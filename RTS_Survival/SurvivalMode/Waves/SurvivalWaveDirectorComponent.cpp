// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalWaveDirectorComponent.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/Formation/FormationData.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticHelpers/StochasticHelpers.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalEdgeSpawnZone.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalStrategicAICoordinator.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveSettings.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

#define LOCTEXT_NAMESPACE "SurvivalWaveDirector"

namespace SurvivalWaveDirectorConstants
{
	constexpr uint8 EnemyPlayerIndex = 2;
	constexpr float SecondsPerMinute = 60.f;
	// Tolerance when matching a formation's last waypoint to the point the director sent it to.
	constexpr float FinalWaypointMatchTolerance = 1.f;
	// Two groups never share a final waypoint closer than this.
	constexpr float FinalWaypointMinSeparation = 50.f;
	constexpr float FinalWaypointNudge = 75.f;
	constexpr int32 MaxFinalWaypointNudges = 8;
	constexpr float BaseProjectionScale = 3.f;
	constexpr float MinimumWaveIntervalSeconds = 10.f;
	// A path end closer than this to the final waypoint is replaced by it instead of adding a tiny last segment.
	constexpr float FinalWaypointSnapDistance = 300.f;
	// Extra edges: weight multiplier at 0 degrees separation from an already picked edge.
	constexpr float MinAngularSpreadFactor = 0.1f;
	// Used only when a profile curve has no keys.
	constexpr float FallbackWaveIntervalSeconds = 90.f;
	constexpr float FallbackWaveBudget = 30.f;
	constexpr float FallbackMaxUnitsPerWave = 10.f;
}

USurvivalWaveDirectorComponent::USurvivalWaveDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USurvivalWaveDirectorComponent::InitWaves(ASurvivalModeDirector* Director)
{
	M_Director = Director;
	if (not GetIsValidDirector())
	{
		return;
	}

	M_StrategicAICoordinator = NewObject<USurvivalStrategicAICoordinator>(this);
	M_StrategicAICoordinator->InitCoordinator(Director, GetWaveSettings());

	M_PhaseChangedHandle = Director->OnPhaseChanged.AddUObject(
		this, &USurvivalWaveDirectorComponent::OnDirectorPhaseChanged);
}

void USurvivalWaveDirectorComponent::StartWaves()
{
	if (M_Schedule.bRunning)
	{
		return;
	}
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	UWorld* World = GetWorld();
	if (not WaveSettings or not IsValid(World))
	{
		return;
	}

	M_Random.Initialize(FMath::Rand());
	M_Schedule = FSurvivalWaveScheduleState();
	M_Schedule.bRunning = true;
	M_Schedule.WavesStartWorldTime = GetWorldTime();
	M_Schedule.NextWaveWorldTime = M_Schedule.WavesStartWorldTime + WaveSettings->FirstWaveDelaySeconds;
	M_Schedule.NextGroupUpdateWorldTime = M_Schedule.WavesStartWorldTime;
	UpdateTechStage(0.f);
	RegisterUnitListeners();
	M_RoundNumber = 0;
	M_RoundsSurvived = 0;
	BeginRound();
	M_Schedule.NextWaveWorldTime = M_Schedule.WavesStartWorldTime + WaveSettings->FirstWaveDelaySeconds;

	if (GetIsValidStrategicAICoordinator())
	{
		M_StrategicAICoordinator->OnWavesStarted();
	}

	World->GetTimerManager().SetTimer(
		M_SchedulerTimerHandle,
		this,
		&USurvivalWaveDirectorComponent::OnSchedulerTick,
		WaveSettings->SchedulerTickSeconds,
		true);
}

void USurvivalWaveDirectorComponent::StopWaves()
{
	M_Schedule.bRunning = false;
	M_RoundState = ESurvivalRoundState::Idle;
	M_Schedule.bWaitingForUnitCap = false;
	StopSchedulerTimer();
	// Formations already on their way keep fighting; they are simply no longer re-aimed.
	M_TrackedGroups.Reset();
	if (IsValid(M_StrategicAICoordinator))
	{
		M_StrategicAICoordinator->OnWavesStopped();
	}
}

float USurvivalWaveDirectorComponent::GetSecondsUntilNextWave() const
{
	if (not M_Schedule.bRunning or M_Schedule.bWaitingForUnitCap)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(M_Schedule.NextWaveWorldTime - GetWorldTime()));
}

float USurvivalWaveDirectorComponent::GetAliveEnemyLoad() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not WaveSettings or not IsValid(GameUnitManager))
	{
		return 0.f;
	}
	using SurvivalWaveDirectorConstants::EnemyPlayerIndex;
	// Only wave units count: the Soviet bases' garrisons and trained units would otherwise starve the waves.
	(void)EnemyPlayerIndex;
	int32 TankCount = 0;
	for (const TWeakObjectPtr<AActor>& Tank : M_AllWaveTanks)
	{
		TankCount += Tank.IsValid() and RTSFunctionLibrary::RTSIsValid(Tank.Get()) ? 1 : 0;
	}
	int32 SoldierCount = 0;
	for (const TWeakObjectPtr<AActor>& SquadActor : M_AllWaveSquads)
	{
		if (ASquadController* Squad = Cast<ASquadController>(SquadActor.Get()))
		{
			SoldierCount += Squad->GetSquadUnitsChecked().Num();
		}
	}
	float Load = TankCount + SoldierCount * WaveSettings->SoldierLoadWeight;
	// Groups whose units have not spawned yet would otherwise be invisible to the cap.
	for (const FSurvivalTrackedWaveGroup& EachGroup : M_TrackedGroups)
	{
		if (not EachGroup.bFormationSeen)
		{
			Load += EachGroup.PlannedLoad;
		}
	}
	return Load;
}

float USurvivalWaveDirectorComponent::GetAliveEnemyLoadCap() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	if (not WaveSettings or not Profile)
	{
		return 0.f;
	}
	return Profile->AliveEnemyLoadCap * WaveSettings->PerformanceLoadCapMultiplier;
}

void USurvivalWaveDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSchedulerTimer();
	if (M_Director.IsValid() and M_PhaseChangedHandle.IsValid())
	{
		M_Director->OnPhaseChanged.Remove(M_PhaseChangedHandle);
	}
	M_PhaseChangedHandle.Reset();
	if (IsValid(M_StrategicAICoordinator))
	{
		M_StrategicAICoordinator->Shutdown();
	}
	OnWaveIncoming.Clear();
	OnTechStageReached.Clear();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- Validity / lookups

bool USurvivalWaveDirectorComponent::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}

bool USurvivalWaveDirectorComponent::GetIsValidStrategicAICoordinator() const
{
	if (IsValid(M_StrategicAICoordinator))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_StrategicAICoordinator", "GetIsValidStrategicAICoordinator", this);
	return false;
}

USurvivalWaveSettings* USurvivalWaveDirectorComponent::GetWaveSettings() const
{
	if (not GetIsValidDirector())
	{
		return nullptr;
	}
	const USurvivalModeSettings* ModeSettings = M_Director->GetSettings();
	if (not IsValid(ModeSettings) or not IsValid(ModeSettings->WaveSettings))
	{
		RTSFunctionLibrary::ReportError(
			"Survival mode settings have no WaveSettings asset; no Soviet waves can be launched."
			"\nAt USurvivalWaveDirectorComponent::GetWaveSettings");
		return nullptr;
	}
	return ModeSettings->WaveSettings.Get();
}

AEnemyController* USurvivalWaveDirectorComponent::GetEnemyController()
{
	if (M_EnemyController.IsValid())
	{
		return M_EnemyController.Get();
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(EnemyController))
	{
		if (bM_ReportedMissingEnemyController)
		{
			return nullptr;
		}
		bM_ReportedMissingEnemyController = true;
		RTSFunctionLibrary::ReportError(
			"Survival waves need an AEnemyController in the level; none was found."
			"\nAt USurvivalWaveDirectorComponent::GetEnemyController");
		return nullptr;
	}
	M_EnemyController = EnemyController;
	return EnemyController;
}

const FSurvivalWaveDifficultyProfile* USurvivalWaveDirectorComponent::GetProfile() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return nullptr;
	}
	return &WaveSettings->GetProfileForDifficulty(M_Director->GetDifficulty().DifficultyLevel);
}

float USurvivalWaveDirectorComponent::GetSurvivalMinute() const
{
	if (M_Director.IsValid())
	{
		const float DirectorSeconds = M_Director->GetElapsedSurvivalSeconds();
		if (DirectorSeconds > 0.f)
		{
			return DirectorSeconds / SurvivalWaveDirectorConstants::SecondsPerMinute;
		}
	}
	if (M_Schedule.WavesStartWorldTime < 0.0)
	{
		return 0.f;
	}
	return static_cast<float>(GetWorldTime() - M_Schedule.WavesStartWorldTime)
		/ SurvivalWaveDirectorConstants::SecondsPerMinute;
}

double USurvivalWaveDirectorComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

void USurvivalWaveDirectorComponent::OnDirectorPhaseChanged(const ESurvivalPhase OldPhase,
                                                            const ESurvivalPhase NewPhase)
{
	if (NewPhase != ESurvivalPhase::GracePeriod or not GetIsValidStrategicAICoordinator())
	{
		return;
	}
	M_StrategicAICoordinator->OnGracePeriodStarted();
}

// ---------------------------------------------------------------- Scheduling

void USurvivalWaveDirectorComponent::OnSchedulerTick()
{
	if (not M_Schedule.bRunning)
	{
		return;
	}
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return;
	}
	const double Now = GetWorldTime();
	UpdateTechStage(GetSurvivalMinute());

	if (Now >= M_Schedule.NextGroupUpdateWorldTime)
	{
		M_Schedule.NextGroupUpdateWorldTime = Now + WaveSettings->RetargetCheckIntervalSeconds;
		UpdateTrackedGroups();
	}
	UpdateRound();
}

void USurvivalWaveDirectorComponent::UpdateTechStage(const float Minute)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	if (not WaveSettings or not Profile)
	{
		return;
	}
	const EEnemyAITechLevel Stage = FSurvivalWaveComposer::GetTechStageAtMinute(
		*WaveSettings, Minute, Profile->TechPaceMultiplier);
	if (Stage == M_Schedule.CurrentTechStage)
	{
		return;
	}
	M_Schedule.CurrentTechStage = Stage;
	OnTechStageReached.Broadcast(Stage, WaveSettings->GetTechStageAnnouncement(Stage));
}

void USurvivalWaveDirectorComponent::ScheduleNextWave(const float Minute)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	if (not WaveSettings or not Profile)
	{
		return;
	}
	using SurvivalWaveDirectorConstants::MinimumWaveIntervalSeconds;
	const float BaseInterval = USurvivalWaveSettings::EvaluateCurveAtMinute(
		Profile->WaveIntervalSeconds, Minute, SurvivalWaveDirectorConstants::FallbackWaveIntervalSeconds);
	const float Variance = WaveSettings->WaveIntervalVarianceFraction;
	// Inside a round the groups come faster than the old wave interval; the round length sets the total.
	const float Interval = BaseInterval * WaveSettings->Rounds.GroupIntervalScale
		* M_Random.FRandRange(1.f - Variance, 1.f + Variance);
	M_Schedule.NextWaveWorldTime = GetWorldTime() + FMath::Max(Interval, MinimumWaveIntervalSeconds);
}

void USurvivalWaveDirectorComponent::TryLaunchWave()
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return;
	}
	FSurvivalWavePlan Plan;
	if (not BuildWavePlan(Plan))
	{
		if (M_Schedule.bWaitingForUnitCap and not bM_LoggedCapWait)
		{
			bM_LoggedCapWait = true;
			UE_LOG(LogTemp, Log, TEXT("Survival wave group waits for the unit cap (load %.1f / cap %.1f)."),
			       GetAliveEnemyLoad(), GetAliveEnemyLoadCap());
		}
		M_Schedule.NextWaveWorldTime = GetWorldTime() + WaveSettings->CapRetrySeconds;
		return;
	}
	M_Schedule.bWaitingForUnitCap = false;
	bM_LoggedCapWait = false;

	const int32 LaunchedUnits = LaunchWavePlan(Plan);
	if (LaunchedUnits <= 0 and Plan.GlobalAbilitiesCast <= 0)
	{
		RTSFunctionLibrary::ReportError(
			"Survival wave could not launch a single unit (no edge spawn points or no enemy controller); "
			"retrying.\nAt USurvivalWaveDirectorComponent::TryLaunchWave");
		M_Schedule.NextWaveWorldTime = GetWorldTime() + WaveSettings->CapRetrySeconds;
		return;
	}

	M_WavesLaunched++;
	if (Plan.KindDefinition == &WaveSettings->MilestoneWave)
	{
		M_Schedule.LastMilestoneIndex = GetMilestoneIndex(Plan.Minute);
	}
	const bool bIsSpecial = Plan.KindDefinition and Plan.KindDefinition->Kind != ESurvivalWaveKind::Regular
		and Plan.KindDefinition->Kind != ESurvivalWaveKind::Milestone;
	M_Schedule.WavesSinceSpecial = bIsSpecial ? 0 : M_Schedule.WavesSinceSpecial + 1;
	RememberEdges(Plan.Edges);
	UE_LOG(LogTemp, Log, TEXT("Survival wave %d group: %d units over %d edges, budget %d, minute %.1f, load %.1f/%.1f."),
	       M_RoundNumber, LaunchedUnits, Plan.Edges.Num(), Plan.Budget, Plan.Minute, GetAliveEnemyLoad(),
	       GetAliveEnemyLoadCap());
	BroadcastAnnouncement(Plan, LaunchedUnits);
	ScheduleNextWave(Plan.Minute);
}

bool USurvivalWaveDirectorComponent::BuildWavePlan(FSurvivalWavePlan& OutPlan)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	if (not WaveSettings or not Profile)
	{
		return false;
	}
	OutPlan.Minute = GetSurvivalMinute();
	OutPlan.BaseLocation = M_Director->GetCurrentPlayerBaseLocation();
	OutPlan.KindDefinition = PickWaveKind(OutPlan.Minute, *Profile);

	FSurvivalWaveCompositionRequest Request;
	Request.SurvivalMinute = OutPlan.Minute;
	Request.Budget = GetWaveBudget(*OutPlan.KindDefinition, OutPlan.Minute, *Profile);
	Request.MaxUnits = FMath::Max(1, FMath::RoundToInt(
		USurvivalWaveSettings::EvaluateCurveAtMinute(
			Profile->MaxUnitsPerWave, OutPlan.Minute, SurvivalWaveDirectorConstants::FallbackMaxUnitsPerWave)
		* OutPlan.KindDefinition->MaxUnitsMultiplier));
	Request.TechPaceMultiplier = Profile->TechPaceMultiplier;
	Request.KindDefinition = OutPlan.KindDefinition;
	OutPlan.Budget = FSurvivalWaveComposer::ComposeWave(*WaveSettings, Request, M_Random, OutPlan.Units);

	if (not TrimWaveToUnitCap(OutPlan.Units))
	{
		M_Schedule.bWaitingForUnitCap = true;
		return false;
	}

	const int32 EdgeCount = GetEdgeCountForWave(OutPlan.Minute, *Profile, *OutPlan.KindDefinition);
	OutPlan.Edges = PickEdges(OutPlan.Minute, OutPlan.BaseLocation, EdgeCount);
	if (OutPlan.Edges.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No ASurvivalEdgeSpawnZone is eligible for the next wave; place edge spawn zones in the survival map."
			"\nAt USurvivalWaveDirectorComponent::BuildWavePlan");
		return false;
	}
	return true;
}

const FSurvivalWaveKindDefinition* USurvivalWaveDirectorComponent::PickWaveKind(
	const float Minute, const FSurvivalWaveDifficultyProfile& Profile)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	// Committed in TryLaunchWave once the wave really launched, so a wave delayed by the cap keeps its milestone.
	if (GetMilestoneIndex(Minute) > M_Schedule.LastMilestoneIndex)
	{
		return &WaveSettings->MilestoneWave;
	}
	if (M_Schedule.WavesSinceSpecial < WaveSettings->MinWavesBetweenSpecials)
	{
		return &WaveSettings->RegularWave;
	}

	const float Chance = USurvivalWaveSettings::EvaluateCurveAtMinute(Profile.SpecialWaveChance, Minute, 0.f);
	if (Chance <= 0.f)
	{
		return &WaveSettings->RegularWave;
	}
	const bool bForced = M_Schedule.WavesSinceSpecial >= WaveSettings->MaxWavesBetweenSpecials;
	if (not bForced and M_Random.FRand() >= Chance)
	{
		return &WaveSettings->RegularWave;
	}

	float TotalWeight = 0.f;
	TArray<const FSurvivalWaveKindDefinition*> Allowed;
	for (const FSurvivalWaveKindDefinition& EachDefinition : WaveSettings->SpecialWaves)
	{
		if (GetIsSpecialWaveAllowed(EachDefinition, Minute, Profile))
		{
			Allowed.Add(&EachDefinition);
			TotalWeight += EachDefinition.Weight;
		}
	}
	float Roll = M_Random.FRandRange(0.f, TotalWeight);
	for (const FSurvivalWaveKindDefinition* EachDefinition : Allowed)
	{
		Roll -= EachDefinition->Weight;
		if (Roll <= 0.f)
		{
			return EachDefinition;
		}
	}
	return Allowed.IsEmpty() ? &WaveSettings->RegularWave : Allowed.Last();
}

bool USurvivalWaveDirectorComponent::GetIsSpecialWaveAllowed(const FSurvivalWaveKindDefinition& Definition,
                                                             const float Minute,
                                                             const FSurvivalWaveDifficultyProfile& Profile)
{
	if (Definition.Weight <= 0.f)
	{
		return false;
	}
	const float SafePace = FMath::Max(Profile.GrowthPaceMultiplier, KINDA_SMALL_NUMBER);
	if (Minute < Definition.MinMinute / SafePace)
	{
		return false;
	}
	if (not Definition.bRequiresGlobalAbility)
	{
		return true;
	}
	return GetHasAnyReadyGlobalAbility(Definition);
}

int32 USurvivalWaveDirectorComponent::GetMilestoneIndex(const float Minute) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings or WaveSettings->MilestoneEveryMinutes <= 0.f)
	{
		return 0;
	}
	return FMath::FloorToInt(Minute / WaveSettings->MilestoneEveryMinutes);
}

float USurvivalWaveDirectorComponent::GetWaveBudget(const FSurvivalWaveKindDefinition& Kind, const float Minute,
                                                    const FSurvivalWaveDifficultyProfile& Profile)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const float BaseBudget = USurvivalWaveSettings::EvaluateCurveAtMinute(
		Profile.WaveBudget, Minute, SurvivalWaveDirectorConstants::FallbackWaveBudget);
	const float Variance = WaveSettings->BudgetVarianceFraction;
	float Budget = BaseBudget * Kind.BudgetMultiplier * M_Random.FRandRange(1.f - Variance, 1.f + Variance);
	if (WaveSettings->bScaleBudgetByDifficultyPercentage)
	{
		const float PercentageScale = static_cast<float>(M_Director->GetDifficulty().DifficultyPercentage)
			/ FMath::Max(1, WaveSettings->ReferenceDifficultyPercentage);
		Budget *= FMath::Clamp(PercentageScale, WaveSettings->DifficultyPercentageScaleClamp.X,
		                       WaveSettings->DifficultyPercentageScaleClamp.Y);
	}
	return Budget;
}

bool USurvivalWaveDirectorComponent::TrimWaveToUnitCap(TArray<FSurvivalPlannedWaveUnit>& InOutUnits) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return false;
	}
	const int32 UntrimmedCount = InOutUnits.Num();
	const float AvailableLoad = GetAliveEnemyLoadCap() - GetAliveEnemyLoad();
	// Keep the strongest units: sort expensive first and drop from the back.
	InOutUnits.Sort([](const FSurvivalPlannedWaveUnit& Left, const FSurvivalPlannedWaveUnit& Right)
	{
		return Left.Cost > Right.Cost;
	});
	while (not InOutUnits.IsEmpty() and GetPlannedLoad(InOutUnits) > AvailableLoad)
	{
		InOutUnits.Pop(EAllowShrinking::No);
	}
	const int32 MinimumToLaunch = FMath::Min(WaveSettings->MinUnitsToLaunch, UntrimmedCount);
	return InOutUnits.Num() >= FMath::Max(1, MinimumToLaunch);
}

float USurvivalWaveDirectorComponent::GetPlannedLoad(const TArray<FSurvivalPlannedWaveUnit>& Units) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 0.f;
	}
	float Load = 0.f;
	for (const FSurvivalPlannedWaveUnit& EachUnit : Units)
	{
		Load += EachUnit.Kind == ESurvivalWaveUnitKind::Squad
			        ? WaveSettings->PlannedSquadLoad
			        : WaveSettings->PlannedTankLoad;
	}
	return Load;
}

// ---------------------------------------------------------------- Edges

int32 USurvivalWaveDirectorComponent::GetEdgeCountForWave(const float Minute,
                                                          const FSurvivalWaveDifficultyProfile& Profile,
                                                          const FSurvivalWaveKindDefinition& Kind)
{
	const float EdgesValue = FMath::Max(1.f, USurvivalWaveSettings::EvaluateCurveAtMinute(
		                                    Profile.EdgesPerWave, Minute, 1.f));
	const int32 WholeEdges = FMath::FloorToInt(EdgesValue);
	const float ExtraEdgeChance = EdgesValue - WholeEdges;
	const int32 RolledEdges = WholeEdges + (M_Random.FRand() < ExtraEdgeChance ? 1 : 0);
	return RolledEdges + Kind.ExtraEdges;
}

TArray<ASurvivalEdgeSpawnZone*> USurvivalWaveDirectorComponent::GetEligibleEdges(const float Minute,
	const FVector& BaseLocation) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	TArray<ASurvivalEdgeSpawnZone*> ActiveZones;
	TArray<ASurvivalEdgeSpawnZone*> FarActiveZones;
	TArray<ASurvivalEdgeSpawnZone*> EnabledZones;
	if (not WaveSettings or not GetIsValidDirector())
	{
		return ActiveZones;
	}
	const float MinDistanceSquared = FMath::Square(WaveSettings->MinEdgeDistanceFromBase);
	for (const TWeakObjectPtr<ASurvivalEdgeSpawnZone>& EachWeakZone : M_Director->GetEdgeSpawnZones())
	{
		ASurvivalEdgeSpawnZone* Zone = EachWeakZone.Get();
		if (not IsValid(Zone) or not Zone->GetIsEnabled())
		{
			continue;
		}
		EnabledZones.Add(Zone);
		if (not Zone->GetIsActiveAtMinute(Minute))
		{
			continue;
		}
		ActiveZones.Add(Zone);
		if (FVector::DistSquared2D(Zone->GetSpawnCenter(), BaseLocation) >= MinDistanceSquared)
		{
			FarActiveZones.Add(Zone);
		}
	}
	// Relax the rules in order rather than launching nothing: distance first, then the minute gate.
	if (not FarActiveZones.IsEmpty())
	{
		return FarActiveZones;
	}
	return ActiveZones.IsEmpty() ? EnabledZones : ActiveZones;
}

TArray<ASurvivalEdgeSpawnZone*> USurvivalWaveDirectorComponent::PickEdges(const float Minute,
                                                                          const FVector& BaseLocation,
                                                                          const int32 EdgeCount)
{
	TArray<ASurvivalEdgeSpawnZone*> Candidates = GetEligibleEdges(Minute, BaseLocation);
	TArray<ASurvivalEdgeSpawnZone*> Picked;
	const int32 WantedCount = FMath::Clamp(EdgeCount, 1, Candidates.Num());
	while (Picked.Num() < WantedCount and not Candidates.IsEmpty())
	{
		float TotalWeight = 0.f;
		TArray<float> Weights;
		Weights.Reserve(Candidates.Num());
		for (const ASurvivalEdgeSpawnZone* EachCandidate : Candidates)
		{
			const float Weight = GetEdgeWeight(*EachCandidate, BaseLocation, Picked);
			Weights.Add(Weight);
			TotalWeight += Weight;
		}
		int32 PickedIndex = Candidates.Num() - 1;
		float Roll = M_Random.FRandRange(0.f, TotalWeight);
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			Roll -= Weights[Index];
			if (Roll <= 0.f)
			{
				PickedIndex = Index;
				break;
			}
		}
		Picked.Add(Candidates[PickedIndex]);
		Candidates.RemoveAtSwap(PickedIndex);
	}
	return Picked;
}

float USurvivalWaveDirectorComponent::GetEdgeWeight(const ASurvivalEdgeSpawnZone& Zone, const FVector& BaseLocation,
                                                    const TArray<ASurvivalEdgeSpawnZone*>& AlreadyPicked) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	float Weight = Zone.GetSelectionWeight();
	if (not WaveSettings)
	{
		return Weight;
	}
	if (GetWasEdgeUsedRecently(Zone))
	{
		Weight *= WaveSettings->RecentEdgeWeightMultiplier;
	}
	if (AlreadyPicked.IsEmpty() or WaveSettings->EdgeAngularSpreadBias <= 0.f)
	{
		return Weight;
	}

	const FVector Direction = (Zone.GetSpawnCenter() - BaseLocation).GetSafeNormal2D();
	float SmallestAngleDegrees = 180.f;
	for (const ASurvivalEdgeSpawnZone* EachPicked : AlreadyPicked)
	{
		const FVector PickedDirection = (EachPicked->GetSpawnCenter() - BaseLocation).GetSafeNormal2D();
		const float CosAngle = FMath::Clamp(FVector::DotProduct(Direction, PickedDirection), -1.f, 1.f);
		SmallestAngleDegrees = FMath::Min(SmallestAngleDegrees, FMath::RadiansToDegrees(FMath::Acos(CosAngle)));
	}
	using SurvivalWaveDirectorConstants::MinAngularSpreadFactor;
	const float SpreadFactor = FMath::Lerp(MinAngularSpreadFactor, 1.f, SmallestAngleDegrees / 180.f);
	return Weight * FMath::Lerp(1.f, SpreadFactor, WaveSettings->EdgeAngularSpreadBias);
}

bool USurvivalWaveDirectorComponent::GetWasEdgeUsedRecently(const ASurvivalEdgeSpawnZone& Zone) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return false;
	}
	const int32 OldestRecentWave = M_WavesLaunched - WaveSettings->RecentEdgeMemory + 1;
	for (const FSurvivalRecentEdgeUse& EachUse : M_RecentEdgeUses)
	{
		if (EachUse.Zone.Get() == &Zone and EachUse.WaveNumber >= OldestRecentWave)
		{
			return true;
		}
	}
	return false;
}

void USurvivalWaveDirectorComponent::RememberEdges(const TArray<ASurvivalEdgeSpawnZone*>& Edges)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const int32 Memory = WaveSettings ? WaveSettings->RecentEdgeMemory : 0;
	const int32 OldestKeptWave = M_WavesLaunched - Memory + 1;
	M_RecentEdgeUses.RemoveAll([OldestKeptWave](const FSurvivalRecentEdgeUse& Use)
	{
		return Use.WaveNumber < OldestKeptWave or not Use.Zone.IsValid();
	});
	for (ASurvivalEdgeSpawnZone* EachEdge : Edges)
	{
		FSurvivalRecentEdgeUse Use;
		Use.Zone = EachEdge;
		Use.WaveNumber = M_WavesLaunched;
		M_RecentEdgeUses.Add(Use);
	}
}

void USurvivalWaveDirectorComponent::DistributeUnitsOverEdges(
	const FSurvivalWavePlan& Plan,
	TArray<TArray<FSurvivalPlannedWaveUnit>>& OutUnitsPerEdge)
{
	OutUnitsPerEdge.SetNum(Plan.Edges.Num());
	if (Plan.Edges.IsEmpty())
	{
		return;
	}
	TArray<FSurvivalPlannedWaveUnit> ShuffledUnits = Plan.Units;
	for (int32 Index = ShuffledUnits.Num() - 1; Index > 0; --Index)
	{
		ShuffledUnits.Swap(Index, M_Random.RandRange(0, Index));
	}

	int32 EdgeCursor = 0;
	for (const FSurvivalPlannedWaveUnit& EachUnit : ShuffledUnits)
	{
		for (int32 Attempt = 0; Attempt < Plan.Edges.Num(); ++Attempt)
		{
			const int32 EdgeIndex = (EdgeCursor + Attempt) % Plan.Edges.Num();
			const int32 MaxUnits = Plan.Edges[EdgeIndex]->GetMaxUnitsPerWave();
			if (MaxUnits > 0 and OutUnitsPerEdge[EdgeIndex].Num() >= MaxUnits)
			{
				continue;
			}
			OutUnitsPerEdge[EdgeIndex].Add(EachUnit);
			EdgeCursor = EdgeIndex + 1;
			break;
		}
	}
}

// ---------------------------------------------------------------- Launching

int32 USurvivalWaveDirectorComponent::LaunchWavePlan(FSurvivalWavePlan& Plan)
{
	if (not GetEnemyController())
	{
		return 0;
	}
	Plan.GlobalAbilitiesCast = CastGlobalAbilities(*Plan.KindDefinition, Plan.BaseLocation);

	EnsureWaveSupplyFor(Plan.Units.Num());
	TArray<TArray<FSurvivalPlannedWaveUnit>> UnitsPerEdge;
	DistributeUnitsOverEdges(Plan, UnitsPerEdge);

	int32 LaunchedUnits = 0;
	for (int32 EdgeIndex = 0; EdgeIndex < Plan.Edges.Num(); ++EdgeIndex)
	{
		if (UnitsPerEdge[EdgeIndex].IsEmpty())
		{
			continue;
		}
		LaunchedUnits += LaunchEdgeGroup(*Plan.Edges[EdgeIndex], UnitsPerEdge[EdgeIndex], Plan.BaseLocation);
	}
	return LaunchedUnits;
}

int32 USurvivalWaveDirectorComponent::LaunchEdgeGroup(ASurvivalEdgeSpawnZone& Zone,
                                                      const TArray<FSurvivalPlannedWaveUnit>& Units,
                                                      const FVector& BaseLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 0;
	}
	TArray<FVector> SpawnPoints;
	if (not Zone.GetSpawnPoints(Units.Num(), WaveSettings->SpawnPointSpacing, SpawnPoints))
	{
		return 0;
	}

	const int32 ChunkSize = FMath::Max(1, WaveSettings->MaxUnitsPerFormation);
	int32 LaunchedUnits = 0;
	for (int32 ChunkStart = 0; ChunkStart < Units.Num(); ChunkStart += ChunkSize)
	{
		const int32 ChunkCount = FMath::Min(ChunkSize, Units.Num() - ChunkStart);
		TArray<FSurvivalPlannedWaveUnit> ChunkUnits;
		TArray<FVector> ChunkPoints;
		ChunkUnits.Reserve(ChunkCount);
		ChunkPoints.Reserve(ChunkCount);
		for (int32 Offset = 0; Offset < ChunkCount; ++Offset)
		{
			ChunkUnits.Add(Units[ChunkStart + Offset]);
			ChunkPoints.Add(SpawnPoints[ChunkStart + Offset]);
		}
		if (LaunchFormationChunk(Zone, ChunkUnits, ChunkPoints, BaseLocation))
		{
			LaunchedUnits += ChunkCount;
		}
	}
	return LaunchedUnits;
}

bool USurvivalWaveDirectorComponent::LaunchFormationChunk(const ASurvivalEdgeSpawnZone& Zone,
                                                          const TArray<FSurvivalPlannedWaveUnit>& Units,
                                                          const TArray<FVector>& SpawnPoints,
                                                          const FVector& BaseLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	if (not WaveSettings or not EnemyController or Units.Num() != SpawnPoints.Num())
	{
		return false;
	}

	TArray<FAttackWaveElement> Elements;
	Elements.Reserve(Units.Num());
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		FAttackWaveElement Element;
		Element.SpawnLocation = SpawnPoints[Index];
		Element.UnitOptions = {Units[Index].Option};
		Elements.Add(Element);
	}

	FVector StagingLocation;
	if (not Zone.GetStagingLocation(WaveSettings->StagingDistance, StagingLocation))
	{
		StagingLocation = SpawnPoints[0];
	}
	const FVector FinalWaypoint = GetUniqueFinalWaypoint(BaseLocation);
	const TArray<FVector> Waypoints = BuildWaypoints(StagingLocation, FinalWaypoint, true);
	const FVector PreviousWaypoint = Waypoints.Num() > 1 ? Waypoints[Waypoints.Num() - 2] : StagingLocation;
	const FRotator FinalDirection(0.f, (FinalWaypoint - PreviousWaypoint).GetSafeNormal2D().Rotation().Yaw, 0.f);

	EnemyController->CreateSingleAttackMoveWave(
		EEnemyWaveType::Wave_NoOwningBuilding,
		Elements,
		Waypoints,
		FinalDirection,
		GetFormationWidth(Units.Num()),
		WaveSettings->WarningLeadSeconds,
		nullptr,
		WaveSettings->FormationOffsetMultiplier,
		WaveSettings->HelpOffsetRadiusMltMax,
		WaveSettings->HelpOffsetRadiusMltMin,
		WaveSettings->MaxAttackTimeBeforeAdvancingToNextWayPoint,
		WaveSettings->MaxTriesFindNavPointForHelpOffset,
		WaveSettings->HelpProjectionScale);

	FSurvivalTrackedWaveGroup Group;
	Group.WaveNumber = M_RoundNumber;
	Group.ExpectedFinalWaypoint = FinalWaypoint;
	Group.AimedBaseLocation = BaseLocation;
	Group.LaunchTime = GetWorldTime();
	Group.PlannedUnitCount = Units.Num();
	Group.PlannedLoad = GetPlannedLoad(Units);
	M_TrackedGroups.Add(Group);

	FRoundSpawnMarker Marker;
	Marker.SpawnPoints = SpawnPoints;
	Marker.Expected = Units.Num();
	Marker.LaunchTime = GetWorldTime();
	M_RoundSpawnMarkers.Add(Marker);
	return true;
}

void USurvivalWaveDirectorComponent::EnsureWaveSupplyFor(const int32 UnitCount)
{
	AEnemyController* EnemyController = GetEnemyController();
	if (not EnemyController)
	{
		return;
	}
	// The cap is enforced by the director (alive load); supply only has to never block a planned unit.
	// Supply is reserved per unit when a delayed wave fires, so groups that have not formed yet still need theirs.
	int32 NeededSupply = UnitCount;
	for (const FSurvivalTrackedWaveGroup& EachGroup : M_TrackedGroups)
	{
		if (not EachGroup.bFormationSeen)
		{
			NeededSupply += EachGroup.PlannedUnitCount;
		}
	}
	if (EnemyController->GetEnemyWaveSupply() < NeededSupply)
	{
		EnemyController->SetEnemyWaveSupply(NeededSupply);
	}
}

int32 USurvivalWaveDirectorComponent::CastGlobalAbilities(const FSurvivalWaveKindDefinition& Kind,
                                                          const FVector& BaseLocation)
{
	if (Kind.GlobalAbilityCasts <= 0 or Kind.GlobalAbilities.IsEmpty())
	{
		return 0;
	}
	AEnemyController* EnemyController = GetEnemyController();
	if (not EnemyController)
	{
		return 0;
	}
	UGlobalAbilitiesManager* AbilitiesManager = EnemyController->GetEnemyGlobalAbilitiesManager();
	if (not IsValid(AbilitiesManager))
	{
		return 0;
	}

	int32 Casts = 0;
	for (int32 CastIndex = 0; CastIndex < Kind.GlobalAbilityCasts; ++CastIndex)
	{
		TArray<EGlobalAbility> ShuffledAbilities = Kind.GlobalAbilities;
		for (int32 Index = ShuffledAbilities.Num() - 1; Index > 0; --Index)
		{
			ShuffledAbilities.Swap(Index, M_Random.RandRange(0, Index));
		}
		const FVector Offset = M_Random.GetUnitVector().GetSafeNormal2D() * Kind.GlobalAbilityTargetOffset;
		bool bProjected = false;
		FVector Target = RTSFunctionLibrary::GetLocationProjected(
			this, BaseLocation + Offset, true, bProjected, SurvivalWaveDirectorConstants::BaseProjectionScale);
		if (not bProjected)
		{
			Target = BaseLocation;
		}
		for (const EGlobalAbility EachAbilityType : ShuffledAbilities)
		{
			UGlobalAbility* Ability = AbilitiesManager->FindLoadedAbilityByType(EachAbilityType);
			if (AbilitiesManager->TryExecuteAbilityAtLocation(Ability, Target))
			{
				Casts++;
				break;
			}
		}
	}
	return Casts;
}

bool USurvivalWaveDirectorComponent::GetHasAnyReadyGlobalAbility(const FSurvivalWaveKindDefinition& Kind)
{
	AEnemyController* EnemyController = GetEnemyController();
	if (not EnemyController)
	{
		return false;
	}
	const UGlobalAbilitiesManager* AbilitiesManager = EnemyController->GetEnemyGlobalAbilitiesManager();
	if (not IsValid(AbilitiesManager))
	{
		return false;
	}
	const TArray<UGlobalAbility*> ReadyAbilities = AbilitiesManager->GetAvailableEnemyGlobalAbilities();
	for (const EGlobalAbility EachAbilityType : Kind.GlobalAbilities)
	{
		UGlobalAbility* Ability = AbilitiesManager->FindLoadedAbilityByType(EachAbilityType);
		if (Ability and ReadyAbilities.Contains(Ability))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------- Waypoints

FVector USurvivalWaveDirectorComponent::GetUniqueFinalWaypoint(const FVector& BaseLocation)
{
	using namespace SurvivalWaveDirectorConstants;
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const float JitterRadius = WaveSettings ? WaveSettings->FinalWaypointJitterRadius : 0.f;
	const FVector Jitter = M_Random.GetUnitVector().GetSafeNormal2D() * M_Random.FRandRange(0.f, JitterRadius);
	bool bProjected = false;
	FVector Candidate = RTSFunctionLibrary::GetLocationProjected(
		this, BaseLocation + Jitter, true, bProjected, BaseProjectionScale);
	if (not bProjected)
	{
		Candidate = RTSFunctionLibrary::GetLocationProjected(this, BaseLocation, true, bProjected,
		                                                     BaseProjectionScale);
	}
	for (int32 Nudge = 0; Nudge < MaxFinalWaypointNudges and GetIsFinalWaypointInUse(Candidate); ++Nudge)
	{
		Candidate += FVector(FinalWaypointNudge, FinalWaypointNudge * 0.5f, 0.f);
	}
	return Candidate;
}

bool USurvivalWaveDirectorComponent::GetIsFinalWaypointInUse(const FVector& Candidate) const
{
	using SurvivalWaveDirectorConstants::FinalWaypointMinSeparation;
	for (const FSurvivalTrackedWaveGroup& EachGroup : M_TrackedGroups)
	{
		if (FVector::Dist(EachGroup.ExpectedFinalWaypoint, Candidate) < FinalWaypointMinSeparation)
		{
			return true;
		}
	}
	return false;
}

TArray<FVector> USurvivalWaveDirectorComponent::BuildWaypoints(const FVector& Start, const FVector& FinalWaypoint,
                                                               const bool bIncludeStart)
{
	TArray<FVector> Waypoints;
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	if (WaveSettings and EnemyController)
	{
		FStochasticPathFindingSettings PathSettings;
		PathSettings.AveragePathPointDistance = WaveSettings->AveragePathPointDistance;
		PathSettings.MaxIntermediatePathPoints = WaveSettings->MaxIntermediatePathPoints;
		PathSettings.MinimumFinalSegmentDistance = WaveSettings->MinimumFinalSegmentDistance;
		PathSettings.ProjectionScale = WaveSettings->PathProjectionScale;

		FStochasticPathFindingParams Params;
		Params.WorldContextObject = this;
		Params.NavComp = EnemyController->GetEnemyNavigationAIComponent();
		Params.StartLocation = Start;
		Params.TargetLocation = FinalWaypoint;
		Params.Settings = PathSettings;
		Params.bUseStartLocationAsFirstPathPoint = bIncludeStart;
		Waypoints = StochasticHelpers::BuildNavigablePathPoints(Params);
	}

	if (Waypoints.IsEmpty())
	{
		if (bIncludeStart)
		{
			Waypoints.Add(Start);
		}
		Waypoints.Add(FinalWaypoint);
		return Waypoints;
	}
	if (not bIncludeStart and Waypoints.Num() > 1 and Waypoints[0].Equals(Start, 1.f))
	{
		Waypoints.RemoveAt(0);
	}
	// The formation is recognised by its exact last waypoint, so force it.
	if (not Waypoints.Last().Equals(FinalWaypoint, SurvivalWaveDirectorConstants::FinalWaypointMatchTolerance))
	{
		if (FVector::Dist(Waypoints.Last(), FinalWaypoint) < SurvivalWaveDirectorConstants::FinalWaypointSnapDistance)
		{
			Waypoints.Last() = FinalWaypoint;
		}
		else
		{
			Waypoints.Add(FinalWaypoint);
		}
	}
	return Waypoints;
}

int32 USurvivalWaveDirectorComponent::GetFormationWidth(const int32 UnitCount) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	const int32 MinWidth = WaveSettings ? WaveSettings->MinFormationWidth : 2;
	const int32 MaxWidth = WaveSettings ? FMath::Max(MinWidth, WaveSettings->MaxFormationWidth) : 5;
	return FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount))), MinWidth, MaxWidth);
}

// ---------------------------------------------------------------- Tracking and retargeting

void USurvivalWaveDirectorComponent::UpdateTrackedGroups()
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	if (M_TrackedGroups.IsEmpty() or not WaveSettings or not EnemyController or not GetIsValidDirector())
	{
		return;
	}
	const UEnemyFormationController* FormationController = EnemyController->GetEnemyFormationController();
	if (not IsValid(FormationController))
	{
		return;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	const FVector BaseLocation = M_Director->GetCurrentPlayerBaseLocation();
	const double Now = GetWorldTime();

	for (int32 GroupIndex = M_TrackedGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
	{
		FSurvivalTrackedWaveGroup& Group = M_TrackedGroups[GroupIndex];
		const int32 FormationIndex = FindFormationIndexForGroup(Group, Formations);
		if (FormationIndex == INDEX_NONE)
		{
			// Seen and now gone: it arrived (survivors are idle blackboard units of the strategic AI) or died.
			const bool bTimedOut = Now - Group.LaunchTime > WaveSettings->FormationBindTimeoutSeconds;
			if (Group.bFormationSeen or bTimedOut)
			{
				M_TrackedGroups.RemoveAtSwap(GroupIndex);
			}
			continue;
		}
		Group.bFormationSeen = true;
		Group.FormationID = Formations[FormationIndex].FormationID;

		const bool bBaseMoved = FVector::Dist2D(BaseLocation, Group.AimedBaseLocation) >= WaveSettings->RetargetDistance;
		const bool bCooledDown = Now - Group.LastRetargetTime >= WaveSettings->RetargetCooldownSeconds;
		if (not bBaseMoved or not bCooledDown)
		{
			continue;
		}
		if (not RetargetGroup(Group, Formations[FormationIndex], BaseLocation))
		{
			M_TrackedGroups.RemoveAtSwap(GroupIndex);
		}
	}
}

int32 USurvivalWaveDirectorComponent::FindFormationIndexForGroup(const FSurvivalTrackedWaveGroup& Group,
                                                                 const TArray<FFormationData>& Formations) const
{
	using SurvivalWaveDirectorConstants::FinalWaypointMatchTolerance;
	for (int32 Index = 0; Index < Formations.Num(); ++Index)
	{
		const FFormationData& Formation = Formations[Index];
		if (Formation.bIsRandomPatrolWithAttackMoveFormation or Formation.FormationWaypoints.IsEmpty())
		{
			continue;
		}
		if (Formation.FormationWaypoints.Last().Equals(Group.ExpectedFinalWaypoint, FinalWaypointMatchTolerance))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool USurvivalWaveDirectorComponent::RetargetGroup(FSurvivalTrackedWaveGroup& Group, const FFormationData& Formation,
                                                   const FVector& BaseLocation)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	AEnemyController* EnemyController = GetEnemyController();
	if (not WaveSettings or not EnemyController)
	{
		return false;
	}
	UEnemyFormationController* FormationController = EnemyController->GetEnemyFormationController();
	if (not IsValid(FormationController))
	{
		return false;
	}

	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	FVector Centroid = FVector::ZeroVector;
	GetFormationUnits(Formation, Squads, Tanks, Centroid);
	if (Squads.IsEmpty() and Tanks.IsEmpty())
	{
		return false;
	}

	const FVector FinalWaypoint = GetUniqueFinalWaypoint(BaseLocation);
	const TArray<FVector> Waypoints = BuildWaypoints(Centroid, FinalWaypoint, false);
	const FVector PreviousWaypoint = Waypoints.Num() > 1 ? Waypoints[Waypoints.Num() - 2] : Centroid;
	const FRotator FinalDirection(0.f, (FinalWaypoint - PreviousWaypoint).GetSafeNormal2D().Rotation().Yaw, 0.f);

	FAttackMoveWaveSettings AttackMoveSettings;
	AttackMoveSettings.HelpOffsetRadiusMltMax = WaveSettings->HelpOffsetRadiusMltMax;
	AttackMoveSettings.HelpOffsetRadiusMltMin = WaveSettings->HelpOffsetRadiusMltMin;
	AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint =
		WaveSettings->MaxAttackTimeBeforeAdvancingToNextWayPoint;
	AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = WaveSettings->MaxTriesFindNavPointForHelpOffset;
	AttackMoveSettings.ProjectionScale = WaveSettings->HelpProjectionScale;

	// Remove and re-create back to back: the units are never outside a formation, so supply refunds stay exact.
	FormationController->RemoveActiveFormationsByID({Formation.FormationID});
	EnemyController->MoveAttackMoveFormationToLocation(
		Squads,
		Tanks,
		Waypoints,
		FinalDirection,
		GetFormationWidth(Squads.Num() + Tanks.Num()),
		WaveSettings->FormationOffsetMultiplier,
		AttackMoveSettings,
		Centroid);

	Group.ExpectedFinalWaypoint = FinalWaypoint;
	Group.AimedBaseLocation = BaseLocation;
	Group.LastRetargetTime = GetWorldTime();
	Group.FormationID = INDEX_NONE;
	return true;
}

void USurvivalWaveDirectorComponent::GetFormationUnits(const FFormationData& Formation,
                                                       TArray<ASquadController*>& OutSquads,
                                                       TArray<ATankMaster*>& OutTanks, FVector& OutCentroid) const
{
	FVector LocationSum = FVector::ZeroVector;
	int32 UnitCount = 0;
	for (const FFormationUnitData& EachFormationUnit : Formation.FormationUnits)
	{
		if (not EachFormationUnit.IsValidFormationUnit())
		{
			continue;
		}
		AActor* UnitActor = EachFormationUnit.Unit->GetOwnerActor();
		if (not RTSFunctionLibrary::RTSIsValid(UnitActor))
		{
			continue;
		}
		if (ATankMaster* Tank = Cast<ATankMaster>(UnitActor))
		{
			OutTanks.Add(Tank);
		}
		else if (ASquadController* Squad = Cast<ASquadController>(UnitActor))
		{
			OutSquads.Add(Squad);
		}
		else
		{
			continue;
		}
		LocationSum += EachFormationUnit.Unit->GetOwnerLocation();
		UnitCount++;
	}
	OutCentroid = UnitCount > 0 ? LocationSum / UnitCount : FVector::ZeroVector;
}

// ---------------------------------------------------------------- Announcement

void USurvivalWaveDirectorComponent::BroadcastAnnouncement(const FSurvivalWavePlan& Plan, const int32 LaunchedUnits)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	FSurvivalWaveAnnouncement Announcement;
	Announcement.WaveNumber = M_RoundNumber;
	Announcement.Kind = Plan.KindDefinition ? Plan.KindDefinition->Kind : ESurvivalWaveKind::Regular;
	Announcement.KindDisplayName = Plan.KindDefinition ? Plan.KindDefinition->DisplayName : FText::GetEmpty();
	Announcement.bIsSpecial = Announcement.Kind != ESurvivalWaveKind::Regular;
	Announcement.TargetLocation = Plan.BaseLocation;
	Announcement.UnitCount = LaunchedUnits;
	Announcement.Budget = Plan.Budget;
	Announcement.TechStage = M_Schedule.CurrentTechStage;
	Announcement.SecondsUntilSpawn = WaveSettings ? WaveSettings->WarningLeadSeconds : 0.f;
	Announcement.GlobalAbilitiesCast = Plan.GlobalAbilitiesCast;
	for (const ASurvivalEdgeSpawnZone* EachEdge : Plan.Edges)
	{
		Announcement.EdgeNames.Add(EachEdge->GetEdgeDisplayName(Plan.BaseLocation));
		Announcement.EdgeLocations.Add(EachEdge->GetSpawnCenter());
		Announcement.EdgeDirectionsFromBase.Add((EachEdge->GetSpawnCenter() - Plan.BaseLocation).GetSafeNormal2D());
	}
	Announcement.Message = BuildAnnouncementMessage(Announcement);
	M_LastAnnouncement = Announcement;
	OnWaveIncoming.Broadcast(Announcement);
}

FText USurvivalWaveDirectorComponent::BuildAnnouncementMessage(const FSurvivalWaveAnnouncement& Announcement) const
{
	const FText Edges = JoinEdgeNames(Announcement.EdgeNames);
	if (Announcement.Kind == ESurvivalWaveKind::Regular or Announcement.KindDisplayName.IsEmpty())
	{
		return FText::Format(LOCTEXT("WaveIncoming", "Wave {0} incoming from the {1}"),
		                     FText::AsNumber(Announcement.WaveNumber), Edges);
	}
	return FText::Format(LOCTEXT("SpecialWaveIncoming", "Wave {0} ({1}) incoming from the {2}"),
	                     FText::AsNumber(Announcement.WaveNumber), Announcement.KindDisplayName, Edges);
}

FText USurvivalWaveDirectorComponent::JoinEdgeNames(const TArray<FText>& EdgeNames) const
{
	if (EdgeNames.IsEmpty())
	{
		return LOCTEXT("UnknownEdge", "edge");
	}
	if (EdgeNames.Num() == 1)
	{
		return EdgeNames[0];
	}
	TArray<FText> Leading = EdgeNames;
	const FText LastName = Leading.Pop();
	return FText::Format(LOCTEXT("EdgeList", "{0} and {1}"),
	                     FText::Join(LOCTEXT("EdgeSeparator", ", "), Leading), LastName);
}

void USurvivalWaveDirectorComponent::StopSchedulerTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SchedulerTimerHandle);
	}
}

#undef LOCTEXT_NAMESPACE

// ---------------------------------------------------------------- Rounds

float USurvivalWaveDirectorComponent::GetRoundActiveSeconds(const int32 RoundNumber) const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 60.f;
	}
	const FSurvivalRoundSettings& Rounds = WaveSettings->Rounds;
	const float Seconds = Rounds.FirstRoundActiveSeconds + Rounds.ActiveSecondsPerRound * FMath::Max(0, RoundNumber - 1);
	return FMath::Min(Seconds, Rounds.MaxRoundActiveSeconds);
}

void USurvivalWaveDirectorComponent::BeginRound()
{
	++M_RoundNumber;
	M_RoundState = ESurvivalRoundState::Active;
	M_RoundActiveEndTime = GetWorldTime() + GetRoundActiveSeconds(M_RoundNumber);
	M_RoundSpawnMarkers.Reset();
	M_RoundTanks.Reset();
	M_RoundSquads.Reset();
	// The first group of a round comes right away; the warning lead gives the player a moment.
	M_Schedule.NextWaveWorldTime = GetWorldTime();
	UE_LOG(LogTemp, Log, TEXT("Survival round %d started, active for %.0f s."), M_RoundNumber,
	       GetRoundActiveSeconds(M_RoundNumber));
}

void USurvivalWaveDirectorComponent::StartNextRound()
{
	if (not M_Schedule.bRunning or M_RoundState != ESurvivalRoundState::Intermission)
	{
		return;
	}
	BeginRound();
}

void USurvivalWaveDirectorComponent::UpdateRound()
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return;
	}
	PruneRoundUnits();
	const double Now = GetWorldTime();
	switch (M_RoundState)
	{
	case ESurvivalRoundState::Active:
		if (Now >= M_RoundActiveEndTime)
		{
			M_RoundState = ESurvivalRoundState::Clearing;
			M_RoundClearStartTime = Now;
			UE_LOG(LogTemp, Log, TEXT("Survival round %d clearing: %d enemies remaining."), M_RoundNumber,
			       GetEnemiesRemaining());
			return;
		}
		if (Now >= M_Schedule.NextWaveWorldTime)
		{
			TryLaunchWave();
		}
		return;
	case ESurvivalRoundState::Clearing:
		if (GetEnemiesRemaining() == 0 or Now - M_RoundClearStartTime >= WaveSettings->Rounds.ClearTimeoutSeconds)
		{
			FinishRound();
		}
		return;
	case ESurvivalRoundState::Idle:
	case ESurvivalRoundState::Intermission:
	default:
		return;
	}
}

void USurvivalWaveDirectorComponent::FinishRound()
{
	const int32 LeftOver = GetEnemiesRemaining();
	M_RoundState = ESurvivalRoundState::Intermission;
	++M_RoundsSurvived;
	UE_LOG(LogTemp, Log, TEXT("Survival round %d survived (%d enemies left over)."), M_RoundNumber, LeftOver);
	OnRoundSurvived.Broadcast(M_RoundNumber);
}

float USurvivalWaveDirectorComponent::GetRoundActiveSecondsRemaining() const
{
	if (M_RoundState != ESurvivalRoundState::Active)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(M_RoundActiveEndTime - GetWorldTime()));
}

void USurvivalWaveDirectorComponent::RegisterUnitListeners()
{
	if (bM_HasRegisteredUnitListeners)
	{
		return;
	}
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Survival waves found no game unit manager; the enemies-remaining count "
			"only shows units still spawning.");
		return;
	}
	using SurvivalWaveDirectorConstants::EnemyPlayerIndex;
	GameUnitManager->GameUnitDelegates.RegisterListener(this, EnemyPlayerIndex,
	                                                    &USurvivalWaveDirectorComponent::OnEnemyTankChanged);
	GameUnitManager->GameUnitDelegates.RegisterListener(this, EnemyPlayerIndex,
	                                                    &USurvivalWaveDirectorComponent::OnEnemySquadUnitChanged);
	bM_HasRegisteredUnitListeners = true;
}

void USurvivalWaveDirectorComponent::OnEnemyTankChanged(ATankMaster* Tank, const bool bWasAdded)
{
	if (not bWasAdded or not IsValid(Tank) or M_RoundState == ESurvivalRoundState::Idle
		or M_RoundState == ESurvivalRoundState::Intermission)
	{
		return;
	}
	if (TryAttributeToRound(Tank->GetActorLocation()))
	{
		M_RoundTanks.Add(Tank);
		M_AllWaveTanks.Add(Tank);
	}
}

void USurvivalWaveDirectorComponent::OnEnemySquadUnitChanged(ASquadUnit* SquadUnit, const bool bWasAdded)
{
	if (not bWasAdded or not IsValid(SquadUnit) or M_RoundState == ESurvivalRoundState::Idle
		or M_RoundState == ESurvivalRoundState::Intermission)
	{
		return;
	}
	ASquadController* Squad = SquadUnit->GetSquadControllerChecked();
	if (not IsValid(Squad))
	{
		return;
	}
	const bool bAlreadyCounted = M_RoundSquads.ContainsByPredicate([Squad](const TWeakObjectPtr<AActor>& Counted)
	{
		return Counted.Get() == Squad;
	});
	// Every soldier reports; the squad is one planned unit.
	if (not bAlreadyCounted and TryAttributeToRound(SquadUnit->GetActorLocation()))
	{
		M_RoundSquads.Add(Squad);
		M_AllWaveSquads.Add(Squad);
	}
}

bool USurvivalWaveDirectorComponent::TryAttributeToRound(const FVector& Location)
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return false;
	}
	const double Now = GetWorldTime();
	const float RadiusSquared = FMath::Square(WaveSettings->Rounds.AttributionRadius);
	for (FRoundSpawnMarker& Marker : M_RoundSpawnMarkers)
	{
		if (Marker.Attributed >= Marker.Expected
			or Now - Marker.LaunchTime > WaveSettings->Rounds.AttributionWindowSeconds)
		{
			continue;
		}
		for (const FVector& SpawnPoint : Marker.SpawnPoints)
		{
			if (FVector::DistSquared2D(SpawnPoint, Location) <= RadiusSquared)
			{
				++Marker.Attributed;
				return true;
			}
		}
	}
	return false;
}

void USurvivalWaveDirectorComponent::PruneRoundUnits()
{
	auto IsTankGone = [](const TWeakObjectPtr<AActor>& Tank)
	{
		return not Tank.IsValid() or not RTSFunctionLibrary::RTSIsValid(Tank.Get());
	};
	auto IsSquadGone = [](const TWeakObjectPtr<AActor>& SquadActor)
	{
		ASquadController* Squad = Cast<ASquadController>(SquadActor.Get());
		return not IsValid(Squad) or Squad->GetSquadUnitsChecked().IsEmpty();
	};
	M_RoundTanks.RemoveAll(IsTankGone);
	M_RoundSquads.RemoveAll(IsSquadGone);
	M_AllWaveTanks.RemoveAll(IsTankGone);
	M_AllWaveSquads.RemoveAll(IsSquadGone);
}

int32 USurvivalWaveDirectorComponent::GetPendingRoundSpawns() const
{
	const USurvivalWaveSettings* WaveSettings = GetWaveSettings();
	if (not WaveSettings)
	{
		return 0;
	}
	const double Now = GetWorldTime();
	int32 Pending = 0;
	for (const FRoundSpawnMarker& Marker : M_RoundSpawnMarkers)
	{
		if (Now - Marker.LaunchTime <= WaveSettings->Rounds.AttributionWindowSeconds)
		{
			Pending += FMath::Max(0, Marker.Expected - Marker.Attributed);
		}
	}
	return Pending;
}

int32 USurvivalWaveDirectorComponent::GetEnemiesRemaining() const
{
	if (M_RoundState != ESurvivalRoundState::Active and M_RoundState != ESurvivalRoundState::Clearing)
	{
		return 0;
	}
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Tank : M_RoundTanks)
	{
		Alive += Tank.IsValid() and RTSFunctionLibrary::RTSIsValid(Tank.Get()) ? 1 : 0;
	}
	for (const TWeakObjectPtr<AActor>& SquadActor : M_RoundSquads)
	{
		Alive += SquadActor.IsValid() ? 1 : 0;
	}
	return Alive + GetPendingRoundSpawns();
}
