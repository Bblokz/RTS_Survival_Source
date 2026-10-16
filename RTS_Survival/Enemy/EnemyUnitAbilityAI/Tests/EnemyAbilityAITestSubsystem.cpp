// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAbilityAITestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatController.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyUnitAbilityAIComponent.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyAbilityAITest, Log, All);

namespace EnemyAbilityAITestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("EnemyAbilityAIValidate");
	// Game seconds the run observes the map; -EnemyAbilityAITestSeconds=<s>.
	const TCHAR* const ObserveSecondsSwitch = TEXT("EnemyAbilityAITestSeconds=");
	// Game seconds after which all formations are ordered to retreat; -EnemyAbilityAIRetreatAt=<s>.
	const TCHAR* const RetreatAtSwitch = TEXT("EnemyAbilityAIRetreatAt=");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	constexpr float DefaultObserveGameSeconds = 900.f;
	constexpr float DefaultRetreatAtGameSeconds = 600.f;
	constexpr int32 IronmanDifficultyPercentage = 100;
	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 90.0;
	constexpr float SampleIntervalGameSeconds = 1.f;
	// A formation whose unit threw a grenade must advance or end within this time, unless it holds in a fight.
	constexpr float FormationProgressBudgetSeconds = 150.f;
	// A waypoint arrival this long after the unit's grenade still counts as the leg the grenade interrupted.
	constexpr float ArrivalAfterGrenadeWindowSeconds = 90.f;
	// Units count as arrived within the move acceptance radius; further away the arrival came too early.
	constexpr float MaxArrivalDistance = 1500.f;
	constexpr float RetreatBudgetGameSeconds = 300.f;
	constexpr float CounterAttackDelaySeconds = 5.f;
	constexpr float DiagnosticIntervalGameSeconds = 30.f;
	constexpr uint8 EnemyPlayerIndex = 2;
	constexpr uint8 HumanPlayerIndex = 1;

	// Game seconds after the start at which the ability wave is sent; -EnemyAbilityAIWaveAt=<s>.
	const TCHAR* const WaveAtSwitch = TEXT("EnemyAbilityAIWaveAt=");
	constexpr float DefaultWaveAtGameSeconds = 15.f;
	constexpr int32 WaveMaxFormationWidth = 3;
	constexpr float WaveFormationOffsetMultiplier = 1.f;
	constexpr float WaveHelpOffsetMltMax = 1.2f;
	constexpr float WaveHelpOffsetMltMin = 1.f;
	// The formation may leave a fight after this long, so it also moves on while abilities are used.
	constexpr float WaveMaxAttackSecondsAtWaypoint = 30.f;
	constexpr int32 WaveMaxNavTries = 3;
	constexpr float WaveProjectionScale = 1.f;
	constexpr float WaveSpawnSpacing = 700.f;
	const FVector NavProjectionExtent(5000.f, 5000.f, 5000.f);
}

bool UEnemyAbilityAITestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), EnemyAbilityAITestPrivate::ValidateSwitch);
}

void UEnemyAbilityAITestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace EnemyAbilityAITestPrivate;
	Super::OnWorldBeginPlay(InWorld);
	M_ObserveGameSeconds = DefaultObserveGameSeconds;
	M_RetreatAtGameSeconds = DefaultRetreatAtGameSeconds;
	FParse::Value(FCommandLine::Get(), ObserveSecondsSwitch, M_ObserveGameSeconds);
	FParse::Value(FCommandLine::Get(), RetreatAtSwitch, M_RetreatAtGameSeconds);
	M_InjectWaveAtGameSeconds = DefaultWaveAtGameSeconds;
	FParse::Value(FCommandLine::Get(), WaveAtSwitch, M_InjectWaveAtGameSeconds);
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EEnemyAbilityAITestPhase::WaitingForGameStart);
	UE_LOG(LogRTSEnemyAbilityAITest, Display,
	       TEXT("RTS_ENEMY_ABILITY_AI_TEST started: observe %.0f s, retreat at %.0f s (game time)"),
	       M_ObserveGameSeconds, M_RetreatAtGameSeconds);
}

void UEnemyAbilityAITestSubsystem::Deinitialize()
{
	UnbindFromEnemyAI();
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UEnemyAbilityAITestSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EEnemyAbilityAITestPhase::Inactive || M_Phase == EEnemyAbilityAITestPhase::Done)
	{
		return;
	}
	if (M_Phase != EEnemyAbilityAITestPhase::WaitingForGameStart)
	{
		KeepGameRunning();
		ForceHardestDifficulty();
	}
	switch (M_Phase)
	{
	case EEnemyAbilityAITestPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EEnemyAbilityAITestPhase::Observing:
		TickObserving();
		break;
	case EEnemyAbilityAITestPhase::Retreating:
		TickRetreating();
		break;
	default:
		break;
	}
}

TStatId UEnemyAbilityAITestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyAbilityAITestSubsystem, STATGROUP_Tickables);
}

void UEnemyAbilityAITestSubsystem::EnterPhase(const EEnemyAbilityAITestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

float UEnemyAbilityAITestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UEnemyAbilityAITestSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

void UEnemyAbilityAITestSubsystem::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST check=%s %s %s (game t=%.1f s)"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details, GetGameSeconds());
}

void UEnemyAbilityAITestSubsystem::Finish()
{
	LogSummary();
	Check(M_BehavioursUsed + M_GrenadesThrown > 0, TEXT("enemy units used abilities"),
	      FString::Printf(TEXT("behaviours=%d grenades=%d"), M_BehavioursUsed, M_GrenadesThrown));
	Check(M_GrenadesInFormations > 0, TEXT("a formation unit threw a grenade, so the formation rules were exercised"),
	      FString::Printf(TEXT("grenades in formations=%d"), M_GrenadesInFormations));
	Check(M_AbilitiesByRetreatingUnits == 0, TEXT("no ability used by a retreating unit"),
	      FString::Printf(TEXT("count=%d"), M_AbilitiesByRetreatingUnits));
	Check(M_GrenadesWhileWalkingToWaypoint == 0, TEXT("no grenade while walking to a formation waypoint"),
	      FString::Printf(TEXT("count=%d"), M_GrenadesWhileWalkingToWaypoint));
	Check(M_EarlyArrivalsAfterGrenade == 0, TEXT("no early waypoint arrival after a grenade"),
	      FString::Printf(TEXT("arrivals after grenade=%d early=%d"), M_ArrivalsAfterGrenade,
	                      M_EarlyArrivalsAfterGrenade));
	Check(M_StalledFormations == 0, TEXT("formations keep going after a grenade"),
	      FString::Printf(TEXT("advanced=%d ended=%d holding in combat=%d stalled=%d"),
	                      M_FormationsAdvancedAfterGrenade, M_FormationsEndedAfterGrenade, M_FormationsHoldingInCombat,
	                      M_StalledFormations));
	Check(bM_HasOrderedRetreat && bM_HasRetreatArrived, TEXT("ordered retreat arrived"),
	      FString::Printf(TEXT("ordered=%d units=%d arrived=%d after %.1f s"), bM_HasOrderedRetreat ? 1 : 0,
	                      M_RetreatingUnitCount, bM_HasRetreatArrived ? 1 : 0, M_RetreatSecondsTaken));
	M_Phase = EEnemyAbilityAITestPhase::Done;
	UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	UnbindFromEnemyAI();
	FPlatformMisc::RequestExit(false, TEXT("EnemyAbilityAITest"));
}

void UEnemyAbilityAITestSubsystem::LogSummary() const
{
	UE_LOG(LogRTSEnemyAbilityAITest, Display,
	       TEXT("RTS_ENEMY_ABILITY_AI_TEST summary: behaviours=%d grenades=%d grenades_in_formations=%d "
		       "waypoint_advances=%d arrivals_after_grenade=%d max_arrival_distance=%.0f retreating_units=%d"),
	       M_BehavioursUsed, M_GrenadesThrown, M_GrenadesInFormations, M_WaypointAdvancesSeen,
	       M_ArrivalsAfterGrenade, M_MaxArrivalDistance, M_RetreatingUnitCount);
}

bool UEnemyAbilityAITestSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	return true;
}

void UEnemyAbilityAITestSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(EnemyAbilityAITestPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UEnemyAbilityAITestSubsystem::ForceHardestDifficulty()
{
	if (not M_MissionManager.IsValid())
	{
		M_MissionManager = Cast<AMissionManager>(UGameplayStatics::GetActorOfClass(this, AMissionManager::StaticClass()));
	}
	if (not M_MissionManager.IsValid() || not M_MissionManager->GetIsGameDifficultyInitialized())
	{
		return;
	}
	// The difficulty picker sets Normal; squads and tanks both use abilities from Ironman.
	if (M_MissionManager->GetCurrentGameDifficulty().DifficultyLevel != ERTSGameDifficulty::Ironman)
	{
		M_MissionManager->SetMissionDifficulty(EnemyAbilityAITestPrivate::IronmanDifficultyPercentage,
		                                       ERTSGameDifficulty::Ironman);
	}
}

bool UEnemyAbilityAITestSubsystem::TryBindToEnemyAI()
{
	M_EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not M_EnemyController.IsValid())
	{
		return false;
	}
	UEnemyUnitAbilityAIComponent* AbilityAI = M_EnemyController->GetEnemyUnitAbilityAIComponent();
	UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController();
	UEnemyRetreatController* RetreatController = M_EnemyController->GetEnemyRetreatController();
	if (not IsValid(AbilityAI) || not IsValid(FormationController) || not IsValid(RetreatController))
	{
		return false;
	}
	M_AbilityUsedHandle = AbilityAI->OnEnemyUnitAbilityUsedForTest.AddUObject(
		this, &UEnemyAbilityAITestSubsystem::OnEnemyUnitAbilityUsed);
	M_WaypointReachedHandle = FormationController->OnFormationUnitReachedWaypointForTest.AddUObject(
		this, &UEnemyAbilityAITestSubsystem::OnFormationUnitReachedWaypoint);
	M_RetreatArrivedHandle = RetreatController->OnRetreatAllUnitsReachedForTest.AddUObject(
		this, &UEnemyAbilityAITestSubsystem::OnRetreatArrived);
	return true;
}

void UEnemyAbilityAITestSubsystem::UnbindFromEnemyAI()
{
	if (not M_EnemyController.IsValid())
	{
		return;
	}
	if (UEnemyUnitAbilityAIComponent* AbilityAI = M_EnemyController->GetEnemyUnitAbilityAIComponent())
	{
		AbilityAI->OnEnemyUnitAbilityUsedForTest.Remove(M_AbilityUsedHandle);
	}
	if (UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController())
	{
		FormationController->OnFormationUnitReachedWaypointForTest.Remove(M_WaypointReachedHandle);
	}
	if (UEnemyRetreatController* RetreatController = M_EnemyController->GetEnemyRetreatController())
	{
		RetreatController->OnRetreatAllUnitsReachedForTest.Remove(M_RetreatArrivedHandle);
	}
}

void UEnemyAbilityAITestSubsystem::TickWaitingForGameStart()
{
	using namespace EnemyAbilityAITestPrivate;
	const double RealSeconds = FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
	if (RealSeconds < HudBuildSeconds)
	{
		return;
	}
	if (TryStartGame() && TryBindToEnemyAI())
	{
		UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST game started"));
		EnterPhase(EEnemyAbilityAITestPhase::Observing);
		return;
	}
	if (RealSeconds > MaxStartSeconds)
	{
		Check(false, TEXT("game started and enemy AI found"));
		Finish();
	}
}

void UEnemyAbilityAITestSubsystem::TickObserving()
{
	const float NowSeconds = GetGameSeconds();
	if (NowSeconds >= M_NextSampleGameSeconds)
	{
		M_NextSampleGameSeconds = NowSeconds + EnemyAbilityAITestPrivate::SampleIntervalGameSeconds;
		SampleFormations();
	}
	if (NowSeconds >= M_NextDiagnosticGameSeconds)
	{
		M_NextDiagnosticGameSeconds = NowSeconds + EnemyAbilityAITestPrivate::DiagnosticIntervalGameSeconds;
		LogDiagnostics();
	}
	if (not bM_HasInjectedWave && GetGameSecondsInPhase() >= M_InjectWaveAtGameSeconds)
	{
		bM_HasInjectedWave = true;
		InjectAbilityWave();
	}
	if (not bM_HasOrderedRetreat && GetIsRetreatDue(NowSeconds) && TryOrderRetreat())
	{
		EnterPhase(EEnemyAbilityAITestPhase::Retreating);
		return;
	}
	if (NowSeconds >= M_ObserveGameSeconds)
	{
		Finish();
	}
}

void UEnemyAbilityAITestSubsystem::TickRetreating()
{
	SampleFormations();
	if (bM_HasRetreatArrived)
	{
		Finish();
		return;
	}
	if (GetGameSecondsInPhase() > EnemyAbilityAITestPrivate::RetreatBudgetGameSeconds)
	{
		UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST retreat did not arrive in %.0f s"),
		       EnemyAbilityAITestPrivate::RetreatBudgetGameSeconds);
		Finish();
	}
}

bool UEnemyAbilityAITestSubsystem::TryOrderRetreat()
{
	UEnemyFormationController* FormationController = M_EnemyController.IsValid()
		                                                 ? M_EnemyController->GetEnemyFormationController()
		                                                 : nullptr;
	if (not IsValid(FormationController))
	{
		return false;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	if (Formations.IsEmpty() || Formations[0].FormationWaypoints.IsEmpty())
	{
		if (not bM_HasInjectedRetreatWave)
		{
			// The first wave reached its destination or died; send another one to retreat.
			bM_HasInjectedRetreatWave = true;
			InjectAbilityWave();
		}
		return false;
	}
	for (const FFormationData& Formation : Formations)
	{
		M_RetreatingUnitCount += Formation.FormationUnits.Num();
	}
	const FVector CounterAttackLocation = Formations[0].FormationWaypoints.Last();
	M_EnemyController->RetreatAllFormations(CounterAttackLocation, EPostRetreatCounterStrategy::Attack,
	                                        EnemyAbilityAITestPrivate::CounterAttackDelaySeconds,
	                                        EnemyAbilityAITestPrivate::RetreatBudgetGameSeconds);
	bM_HasOrderedRetreat = true;
	UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST ordered %d formations (%d units) to retreat"),
	       Formations.Num(), M_RetreatingUnitCount);
	return true;
}

bool UEnemyAbilityAITestSubsystem::GetIsRetreatDue(const float NowSeconds) const
{
	// First see the grenade formations advance, end or hold in a fight; then test the retreat.
	const bool bHasJudgedGrenadeFormations = M_FirstFormationGrenadeSeconds >= 0.f && M_FormationWatches.IsEmpty();
	return bHasJudgedGrenadeFormations || NowSeconds >= M_RetreatAtGameSeconds;
}

void UEnemyAbilityAITestSubsystem::InjectAbilityWave()
{
	using namespace EnemyAbilityAITestPrivate;
	FVector TargetLocation;
	FVector SpawnLocation;
	if (not M_EnemyController.IsValid() || not TryGetPlayerTargetLocation(TargetLocation)
		|| not TryGetEnemySpawnLocation(SpawnLocation))
	{
		Check(false, TEXT("ability wave could be sent"), TEXT("no player units, enemy spawn or enemy controller"));
		return;
	}
	const TArray<FTrainingOption> UnitOptions = {
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_SU_85)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_SU_85)),
		FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_SU_100)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Rus_Mosin)),
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Rus_Mosin))
	};
	const FVector SideDirection = FVector::CrossProduct((TargetLocation - SpawnLocation).GetSafeNormal2D(),
	                                                    FVector::UpVector);
	TArray<FAttackWaveElement> WaveElements;
	for (int32 OptionIndex = 0; OptionIndex < UnitOptions.Num(); ++OptionIndex)
	{
		FAttackWaveElement Element;
		const float SideOffset = WaveSpawnSpacing * (OptionIndex - UnitOptions.Num() / 2);
		Element.SpawnLocation = SpawnLocation + SideDirection * SideOffset;
		ProjectOntoNavMesh(Element.SpawnLocation);
		Element.UnitOptions = {UnitOptions[OptionIndex]};
		WaveElements.Add(Element);
	}
	const FVector MidWaypoint = (SpawnLocation + TargetLocation) * 0.5f;
	const TArray<FVector> Waypoints = {MidWaypoint, TargetLocation};
	const FRotator FinalDirection = (TargetLocation - SpawnLocation).GetSafeNormal2D().Rotation();
	M_EnemyController->AddToWaveSupply(WaveElements.Num());
	M_EnemyController->CreateSingleAttackMoveWave(
		EEnemyWaveType::Wave_NoOwningBuilding, WaveElements, Waypoints, FinalDirection, WaveMaxFormationWidth, 0.f,
		nullptr, WaveFormationOffsetMultiplier, WaveHelpOffsetMltMax, WaveHelpOffsetMltMin,
		WaveMaxAttackSecondsAtWaypoint, WaveMaxNavTries, WaveProjectionScale);
	UE_LOG(LogRTSEnemyAbilityAITest, Display,
	       TEXT("RTS_ENEMY_ABILITY_AI_TEST sent ability wave of %d units from %s to %s"), WaveElements.Num(),
	       *SpawnLocation.ToCompactString(), *TargetLocation.ToCompactString());
}

bool UEnemyAbilityAITestSubsystem::TryGetPlayerTargetLocation(FVector& OutLocation) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return false;
	}
	// Infantry first: the Mosin molotovs only go for soldiers; tanks are the fallback target.
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const ASquadUnit* Soldier : GameUnitManager->GetSquadUnitsOfPlayer(EnemyAbilityAITestPrivate::HumanPlayerIndex))
	{
		if (IsValid(Soldier))
		{
			Sum += Soldier->GetActorLocation();
			++Count;
		}
	}
	if (Count == 0)
	{
		for (const ATankMaster* Tank : GameUnitManager->GetPlayerTanks(EnemyAbilityAITestPrivate::HumanPlayerIndex))
		{
			if (IsValid(Tank))
			{
				Sum += Tank->GetActorLocation();
				++Count;
			}
		}
	}
	if (Count == 0)
	{
		return false;
	}
	OutLocation = Sum / Count;
	return ProjectOntoNavMesh(OutLocation);
}

bool UEnemyAbilityAITestSubsystem::TryGetEnemySpawnLocation(FVector& OutLocation) const
{
	// The map's own enemy units stand in enemy territory; the wave spawns among them.
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (TActorIterator<ATankMaster> TankIterator(GetWorld()); TankIterator; ++TankIterator)
	{
		const URTSComponent* RTSComponent = TankIterator->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == EnemyAbilityAITestPrivate::EnemyPlayerIndex)
		{
			Sum += TankIterator->GetActorLocation();
			++Count;
		}
	}
	OutLocation = Count > 0 ? Sum / Count : M_EnemyController->GetActorLocation();
	return ProjectOntoNavMesh(OutLocation);
}

bool UEnemyAbilityAITestSubsystem::ProjectOntoNavMesh(FVector& InOutLocation) const
{
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation ProjectedLocation;
	if (not NavigationSystem || not NavigationSystem->ProjectPointToNavigation(
		InOutLocation, ProjectedLocation, EnemyAbilityAITestPrivate::NavProjectionExtent))
	{
		return false;
	}
	InOutLocation = ProjectedLocation.Location;
	return true;
}

void UEnemyAbilityAITestSubsystem::SampleFormations()
{
	UEnemyFormationController* FormationController = M_EnemyController.IsValid()
		                                                 ? M_EnemyController->GetEnemyFormationController()
		                                                 : nullptr;
	if (not IsValid(FormationController))
	{
		return;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	TMap<int32, int32> WaypointIndexByFormation;
	TSet<int32> FormationsInCombat;
	for (FFormationData& Formation : Formations)
	{
		WaypointIndexByFormation.Add(Formation.FormationID, Formation.CurrentWaypointIndex);
		const int32* LastSeenIndex = M_LastSeenWaypointIndexByFormation.Find(Formation.FormationID);
		M_WaypointAdvancesSeen += LastSeenIndex && Formation.CurrentWaypointIndex > *LastSeenIndex ? 1 : 0;
		M_LastSeenWaypointIndexByFormation.Add(Formation.FormationID, Formation.CurrentWaypointIndex);
		const bool bHasUnitInCombat = Formation.FormationUnits.ContainsByPredicate([](const FFormationUnitData& Unit)
		{
			return Unit.Unit.IsValid() && Unit.Unit->GetIsUnitInCombat();
		});
		if (bHasUnitInCombat)
		{
			FormationsInCombat.Add(Formation.FormationID);
		}
	}
	ResolveFormationWatches(WaypointIndexByFormation, FormationsInCombat);
}

void UEnemyAbilityAITestSubsystem::ResolveFormationWatches(const TMap<int32, int32>& WaypointIndexByFormation,
                                                           const TSet<int32>& FormationsInCombat)
{
	const float NowSeconds = GetGameSeconds();
	for (auto WatchIterator = M_FormationWatches.CreateIterator(); WatchIterator; ++WatchIterator)
	{
		const FEnemyAbilityAITestFormationWatch& Watch = WatchIterator.Value();
		const int32* CurrentIndex = WaypointIndexByFormation.Find(Watch.FormationID);
		if (not CurrentIndex)
		{
			// Reached its final destination, was removed for a retreat, or lost all its units.
			++M_FormationsEndedAfterGrenade;
			WatchIterator.RemoveCurrent();
			continue;
		}
		if (*CurrentIndex > Watch.WaypointIndexAtAbility)
		{
			++M_FormationsAdvancedAfterGrenade;
			WatchIterator.RemoveCurrent();
			continue;
		}
		if (NowSeconds - Watch.AbilityGameSeconds < EnemyAbilityAITestPrivate::FormationProgressBudgetSeconds)
		{
			continue;
		}
		// Attack-move formations may hold a waypoint while they fight; anything else has stalled.
		const bool bIsHoldingInCombat = FormationsInCombat.Contains(Watch.FormationID);
		M_FormationsHoldingInCombat += bIsHoldingInCombat ? 1 : 0;
		M_StalledFormations += bIsHoldingInCombat ? 0 : 1;
		UE_LOG(LogRTSEnemyAbilityAITest, Display,
		       TEXT("RTS_ENEMY_ABILITY_AI_TEST formation %d still at waypoint %d %.0f s after a grenade (%s)"),
		       Watch.FormationID, *CurrentIndex, NowSeconds - Watch.AbilityGameSeconds,
		       bIsHoldingInCombat ? TEXT("holding in combat") : TEXT("STALLED"));
		WatchIterator.RemoveCurrent();
	}
}

void UEnemyAbilityAITestSubsystem::OnEnemyUnitAbilityUsed(AActor* UnitActor, const EAbilityID Ability,
                                                          const FVector& AimLocation)
{
	const bool bIsGrenade = Ability == EAbilityID::IdThrowGrenade;
	M_BehavioursUsed += bIsGrenade ? 0 : 1;
	M_GrenadesThrown += bIsGrenade ? 1 : 0;
	// Checked here independently of the ability AI's own gates.
	if (GetIsUnitRetreating(UnitActor))
	{
		++M_AbilitiesByRetreatingUnits;
		UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST %s used an ability while retreating"),
		       *GetNameSafe(UnitActor));
	}
	if (not bIsGrenade)
	{
		return;
	}
	const UEnemyFormationController* FormationController = M_EnemyController.IsValid()
		                                                       ? M_EnemyController->GetEnemyFormationController()
		                                                       : nullptr;
	if (IsValid(FormationController) && FormationController->GetIsUnitMovingToFormationWaypoint(UnitActor))
	{
		++M_GrenadesWhileWalkingToWaypoint;
	}
	M_LastGrenadeSecondsByUnit.Add(UnitActor, GetGameSeconds());
	int32 WaypointIndex = 0;
	const int32 FormationID = FindFormationOfUnit(UnitActor, WaypointIndex);
	if (FormationID == INDEX_NONE)
	{
		return;
	}
	++M_GrenadesInFormations;
	if (M_FirstFormationGrenadeSeconds < 0.f)
	{
		M_FirstFormationGrenadeSeconds = GetGameSeconds();
	}
	if (not M_FormationWatches.Contains(FormationID))
	{
		FEnemyAbilityAITestFormationWatch Watch;
		Watch.FormationID = FormationID;
		Watch.WaypointIndexAtAbility = WaypointIndex;
		Watch.AbilityGameSeconds = GetGameSeconds();
		M_FormationWatches.Add(FormationID, Watch);
	}
	UE_LOG(LogRTSEnemyAbilityAITest, Display,
	       TEXT("RTS_ENEMY_ABILITY_AI_TEST %s threw a grenade in formation %d at waypoint %d (aim %s)"),
	       *GetNameSafe(UnitActor), FormationID, WaypointIndex, *AimLocation.ToCompactString());
}

void UEnemyAbilityAITestSubsystem::OnFormationUnitReachedWaypoint(AActor* UnitActor, const FVector& WaypointDestination,
                                                                  const int32 FormationID)
{
	if (not IsValid(UnitActor))
	{
		return;
	}
	const float* GrenadeSeconds = M_LastGrenadeSecondsByUnit.Find(UnitActor);
	const bool bIsAfterGrenade = GrenadeSeconds
		&& GetGameSeconds() - *GrenadeSeconds <= EnemyAbilityAITestPrivate::ArrivalAfterGrenadeWindowSeconds;
	if (not bIsAfterGrenade)
	{
		return;
	}
	const float Distance = FVector::Dist2D(UnitActor->GetActorLocation(), WaypointDestination);
	M_MaxArrivalDistance = FMath::Max(M_MaxArrivalDistance, Distance);
	++M_ArrivalsAfterGrenade;
	if (Distance > EnemyAbilityAITestPrivate::MaxArrivalDistance)
	{
		++M_EarlyArrivalsAfterGrenade;
		UE_LOG(LogRTSEnemyAbilityAITest, Display,
		       TEXT("RTS_ENEMY_ABILITY_AI_TEST %s counted as arrived in formation %d while %.0f cm from its waypoint"),
		       *UnitActor->GetName(), FormationID, Distance);
	}
}

void UEnemyAbilityAITestSubsystem::OnRetreatArrived(const int32 RetreatID, const float SecondsTaken)
{
	bM_HasRetreatArrived = true;
	M_RetreatSecondsTaken = SecondsTaken;
	UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST retreat %d arrived after %.1f s"),
	       RetreatID, SecondsTaken);
}

bool UEnemyAbilityAITestSubsystem::GetIsUnitRetreating(const AActor* UnitActor) const
{
	if (not M_EnemyController.IsValid())
	{
		return false;
	}
	const UEnemyRetreatController* RetreatController = M_EnemyController->GetEnemyRetreatController();
	if (IsValid(RetreatController) && RetreatController->GetIsUnitRetreating(UnitActor))
	{
		return true;
	}
	const UEnemyDirectControlComponent* DirectControl = M_EnemyController->GetEnemyDirectControlComponent();
	return IsValid(DirectControl) && DirectControl->GetRetreatGroupUnitsToExclude().Contains(UnitActor);
}

void UEnemyAbilityAITestSubsystem::LogDiagnostics()
{
	int32 EnemyUnits = 0;
	int32 EnemyUnitsInCombat = 0;
	int32 EnemyUnitsAttacking = 0;
	for (TActorIterator<AActor> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		const bool bIsUnit = Actor->IsA<ASquadController>() || Actor->IsA<ATankMaster>();
		const URTSComponent* RTSComponent = bIsUnit ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		ICommands* Commands = Cast<ICommands>(Actor);
		if (not IsValid(RTSComponent) || not Commands
			|| RTSComponent->GetOwningPlayer() != EnemyAbilityAITestPrivate::EnemyPlayerIndex)
		{
			continue;
		}
		++EnemyUnits;
		EnemyUnitsInCombat += Commands->GetIsUnitInCombat() ? 1 : 0;
		EnemyUnitsAttacking += Commands->GetActiveCommandID() == EAbilityID::IdAttack ? 1 : 0;
	}
	const UEnemyUnitAbilityAIComponent* AbilityAI = M_EnemyController.IsValid()
		                                                ? M_EnemyController->GetEnemyUnitAbilityAIComponent()
		                                                : nullptr;
	const FString Difficulty = M_MissionManager.IsValid() && M_MissionManager->GetIsGameDifficultyInitialized()
		                           ? UEnum::GetValueAsString(M_MissionManager->GetCurrentGameDifficulty().DifficultyLevel)
		                           : TEXT("not set");
	UE_LOG(LogRTSEnemyAbilityAITest, Display,
	       TEXT("RTS_ENEMY_ABILITY_AI_TEST diag: difficulty=%s roster=%d enemy_units=%d in_combat=%d attacking=%d "
		       "behaviours=%d grenades=%d"),
	       *Difficulty, IsValid(AbilityAI) ? AbilityAI->GetRosterCount() : -1, EnemyUnits, EnemyUnitsInCombat,
	       EnemyUnitsAttacking, M_BehavioursUsed, M_GrenadesThrown);
	if (IsValid(AbilityAI))
	{
		const int32* Rejections = AbilityAI->GetStats().GrenadeRejections;
		UE_LOG(LogRTSEnemyAbilityAITest, Display,
		       TEXT("RTS_ENEMY_ABILITY_AI_TEST grenade rejections: not_idle_or_attacking=%d not_in_combat=%d no_target=%d "
			       "too_few_soldiers=%d wrong_target=%d target_nearly_dead=%d too_few_in_blast=%d claimed=%d "
			       "out_of_reach=%d order_refused=%d"),
		       Rejections[0], Rejections[1], Rejections[2], Rejections[3], Rejections[4], Rejections[5], Rejections[6],
		       Rejections[7], Rejections[8], Rejections[9]);
	}
	LogEnemyCardsOnce();
}

void UEnemyAbilityAITestSubsystem::LogEnemyCardsOnce()
{
	if (bM_HasLoggedEnemyCards)
	{
		return;
	}
	TSet<FString> LoggedClasses;
	for (TActorIterator<AActor> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		const URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>();
		ICommands* Commands = Cast<ICommands>(Actor);
		if (not IsValid(RTSComponent) || not Commands
			|| RTSComponent->GetOwningPlayer() != EnemyAbilityAITestPrivate::EnemyPlayerIndex)
		{
			continue;
		}
		const FString ClassName = Actor->GetClass()->GetName();
		if (LoggedClasses.Contains(ClassName))
		{
			continue;
		}
		LoggedClasses.Add(ClassName);
		FString CardText;
		for (const FUnitAbilityEntry& Entry : Commands->GetUnitAbilityEntries())
		{
			if (Entry.AbilityId != EAbilityID::IdNoAbility)
			{
				CardText += Global_GetAbilityIDAsString(Entry.AbilityId) + FString::Printf(TEXT("(%d) "), Entry.CustomType);
			}
		}
		UE_LOG(LogRTSEnemyAbilityAITest, Display, TEXT("RTS_ENEMY_ABILITY_AI_TEST enemy card %s: %s"), *ClassName,
		       *CardText);
	}
	bM_HasLoggedEnemyCards = LoggedClasses.Num() > 0;
}

int32 UEnemyAbilityAITestSubsystem::FindFormationOfUnit(const AActor* UnitActor, int32& OutWaypointIndex) const
{
	const UEnemyFormationController* FormationController = M_EnemyController.IsValid()
		                                                       ? M_EnemyController->GetEnemyFormationController()
		                                                       : nullptr;
	if (not IsValid(FormationController))
	{
		return INDEX_NONE;
	}
	TArray<FFormationData> Formations;
	FormationController->GetActiveFormationData(Formations);
	for (const FFormationData& Formation : Formations)
	{
		const bool bHasUnit = Formation.FormationUnits.ContainsByPredicate([UnitActor](const FFormationUnitData& Unit)
		{
			return Unit.Unit.IsValid() && Unit.Unit->GetOwnerActor() == UnitActor;
		});
		if (bHasUnit)
		{
			OutWaypointIndex = Formation.CurrentWaypointIndex;
			return Formation.FormationID;
		}
	}
	return INDEX_NONE;
}

#else

bool UEnemyAbilityAITestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UEnemyAbilityAITestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UEnemyAbilityAITestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UEnemyAbilityAITestSubsystem::Tick(float DeltaTime)
{
}

TStatId UEnemyAbilityAITestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyAbilityAITestSubsystem, STATGROUP_Tickables);
}

#endif
