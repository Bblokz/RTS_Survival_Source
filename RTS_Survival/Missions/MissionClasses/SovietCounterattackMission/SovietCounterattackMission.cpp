// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietCounterattackMission.h"

#include "Camera/CameraActor.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Enemy/AirDrop/EnemyAirDrop.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionClasses/SovietCounterattackMission/SovietCounterattackSettings.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerAirDropSubsystem.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Missions/MissionProps/SovietCargoTrain/SovietCargoTrain.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

namespace SovietCounterattackConstants
{
	constexpr float RemainingPollSeconds = 0.5f;
	// Ground spawn keys: GroupId * this + unit index.
	constexpr int32 SpawnKeysPerGroup = 1000;
	// Ground under a landing pad: traced from this far above and below the configured point.
	constexpr float PadTraceHalfHeight = 20000.f;
	constexpr float SpawnZLift = 50.f;
	// Seconds an air drop actor of a finished batch lives on (it is still inside its own broadcast).
	constexpr float FinishedAirDropLifeSpan = 1.f;
	// ID of the dig-in camera tour (handed back in OnCameraTourFinishedNative).
	constexpr int32 DigInTourId = 7301;
	// How often the intro checks whether the tour's first shot is on screen (to start the voice line).
	constexpr float IntroVoiceLinePollSeconds = 0.2f;
	// How often the dig-in positions are checked for dug-in player vehicles.
	constexpr float PositionPollSeconds = 0.5f;
	// Drop shot, after the filmed unit is on the ground: the camera turns to it.
	constexpr float ArrivalUnitLookLift = 150.f;
	// Dig-in markers: the ground under a marker point is traced this far above and below it.
	constexpr float MarkerTraceHalfHeight = 3000.f;
	// Arrival cinematic: how often the filmed aircraft is checked while waiting, and the closing-speed smoothing.
	constexpr float ArrivalPollSeconds = 0.1f;
	constexpr float ArrivalClosingSpeedBlend = 0.35f;
	// Below this closing speed (cm/s) the aircraft is not on its way in; within this 2D distance it is at the pad.
	constexpr float ArrivalMinClosingSpeed = 50.f;
	constexpr float ArrivalAtPadDistance = 1500.f;
	// Drop shot: the camera looks between the pad and the aircraft; the aircraft offset counts up to this length.
	constexpr float ArrivalMaxLookOffset = 5000.f;
	constexpr float ArrivalLookAtInterpSpeed = 2.5f;
	constexpr float ArrivalLookAtLift = 200.f;
	constexpr float ArrivalTrainLookLift = 150.f;
	// The sequence actor and camera outlive the session by this much (it ends inside the player's broadcast).
	constexpr float ArrivalActorLifeSpan = 0.5f;
	// Fog stays lifted a little longer than the cinematic.
	constexpr float ArrivalRevealExtraSeconds = 3.f;

	const FName MarkerSetTextEventName(TEXT("SetText"));

	struct FMarkerSetTextParams
	{
		FText NewText;
	};
}

namespace
{
	FVector GetUnitLocation(const AActor* Unit)
	{
		if (const ICommands* Commands = Cast<ICommands>(Unit))
		{
			return Commands->GetOwnerLocation();
		}
		return IsValid(Unit) ? Unit->GetActorLocation() : FVector::ZeroVector;
	}

	FString GetSpawnName(const ESovietCounterattackSpawn Spawn)
	{
		switch (Spawn)
		{
		case ESovietCounterattackSpawn::Center:
			return TEXT("Center");
		case ESovietCounterattackSpawn::AirDrop:
			return TEXT("AirDrop");
		case ESovietCounterattackSpawn::Train:
			return TEXT("Train");
		default:
			return TEXT("Unknown");
		}
	}
}

// ------------------------------------------------------------------
// Mission lifetime
// ------------------------------------------------------------------

void USovietCounterattackMission::OnMissionStartedNative()
{
	Super::OnMissionStartedNative();
	ClearRuntime(true);
	if (not GetHasValidSettings())
	{
		return;
	}
	Start_Preparation();
}

void USovietCounterattackMission::OnMissionComplete()
{
	M_Phase = EPhase::Done;
	ClearRuntime(false);
	Super::OnMissionComplete();
}

void USovietCounterattackMission::OnMissionFailed()
{
	M_Phase = EPhase::Done;
	ClearRuntime(false);
	Super::OnMissionFailed();
}

void USovietCounterattackMission::OnCleanUpMission()
{
	ClearRuntime(true);
	Super::OnCleanUpMission();
}

void USovietCounterattackMission::ClearRuntime(const bool bDestroyPendingActors)
{
	Arrival_Clear(false);
	Intro_UnbindTakeover();
	Intro_RemoveMarkers();
	if (M_IntroStage != EIntroStage::NotStarted)
	{
		M_IntroStage = EIntroStage::Done;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_IntroVoiceLineTimer);
		World->GetTimerManager().ClearTimer(M_BaseReinforcementsTimer);
	}
	if (UWorld* const World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(M_PrepareTimer);
		Timers.ClearTimer(M_WaveTimer);
		Timers.ClearTimer(M_SurviveEndTimer);
		Timers.ClearTimer(M_RemainingPollTimer);
		for (FPendingTrainRun& Run : M_TrainRuns)
		{
			Timers.ClearTimer(Run.UnloadTimer);
			Timers.ClearTimer(Run.DepartTimer);
		}
	}
	if (M_TimerWidget.IsValid())
	{
		M_TimerWidget->RemoveFromParent();
	}
	M_TimerWidget.Reset();

	AirDrop_ClearBatch(bDestroyPendingActors);
	M_AirQueue.Empty();

	for (FPendingTrainRun& Run : M_TrainRuns)
	{
		ASovietCargoTrain* Train = Run.Train.Get();
		if (not IsValid(Train))
		{
			continue;
		}
		Train->OnArrived.RemoveAll(this);
		Train->OnDeparted.RemoveAll(this);
		if (bDestroyPendingActors)
		{
			Train->Destroy();
		}
		else if (Train->GetHasArrived())
		{
			Train->HideCargo();
			Train->Depart();
		}
	}
	M_TrainRuns.Empty();
	M_TrainQueue.Empty();

	for (const FSovietCounterattackRemainingUnit& Remaining : M_RemainingUnits)
	{
		RemoveCustomMiniMapIcon(Remaining.IconId);
	}
	M_RemainingUnits.Empty();
	for (const FName& MarkerId : M_GroupMarkerIds)
	{
		// Tracking an ID with no units removes its marker.
		TrackUnitGroupOnMiniMap(MarkerId, {}, FVector::ZeroVector);
	}
	M_GroupMarkerIds.Empty();
	M_GroundGroups.Empty();
}

bool USovietCounterattackMission::GetHasValidSettings() const
{
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + " has no Settings data asset.");
		return false;
	}
	if (Settings->AttackRoute.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": the settings' AttackRoute is "
			"empty; groups only drive to the player HQ.");
	}
	return true;
}

const FSovietCounterattackDifficulty* USovietCounterattackMission::GetDifficulty() const
{
	if (not IsValid(Settings))
	{
		return nullptr;
	}
	return &Settings->GetDifficultySettings(GetGameDifficulty().DifficultyLevel);
}

void USovietCounterattackMission::SetObjective(const FText& Title, const FText& Description)
{
	SetMissionObjectiveText(Title, Description);
}

// ------------------------------------------------------------------
// Preparation and waves
// ------------------------------------------------------------------

void USovietCounterattackMission::Start_Preparation()
{
	const FSovietCounterattackDifficulty* Difficulty = GetDifficulty();
	UWorld* const World = GetWorld();
	if (Difficulty == nullptr || World == nullptr)
	{
		return;
	}
	M_Phase = EPhase::Preparing;
	M_WaveCount = FMath::Max(0, Difficulty->WaveCount);
	M_SecondsBetweenWaves = FMath::Max(1, Difficulty->SecondsBetweenWaves);
	M_SpawnedWaveCount = 0;
	M_PrepareSeconds = FMath::Max(0, Difficulty->SecondsUntilSovietsArrive);
	bM_PrepareCountdownStarted = false;
	bM_FirstPositionLinePlayed = false;
	bM_AllPositionsRewardPaid = false;

	SetObjective(Settings->PrepareObjectiveTitle, Settings->PrepareObjectiveDescription);

	if (Settings->DigInIntro.bEnabled)
	{
		// The countdown starts when the intro (takeover, markers, tour) is over.
		Intro_Start();
		Positions_RefreshObjective();
		return;
	}
	Start_PrepareCountdown();
}

void USovietCounterattackMission::Start_PrepareCountdown()
{
	UWorld* const World = GetWorld();
	if (bM_PrepareCountdownStarted || M_Phase != EPhase::Preparing || World == nullptr || not IsValid(Settings))
	{
		return;
	}
	bM_PrepareCountdownStarted = true;
	const int32 PrepareSeconds = M_PrepareSeconds;
	const int32 SurviveSeconds = M_WaveCount * M_SecondsBetweenWaves;

	if (Settings->TimerWidgetClass)
	{
		// One widget for both countdowns: it switches to "Survive" by itself when the preparation runs out, on the
		// same world timer clock as the waves below.
		FMissionTimerLifetimeSettings Lifetimes;
		if (SurviveSeconds > 0)
		{
			FTimerLifeTime Survive;
			Survive.M_TimerText = Settings->SurviveTimerText;
			Survive.M_Time = SurviveSeconds;
			Lifetimes.M_TimerLifetimes.Add(Survive);
		}
		M_TimerWidget = SpawnMissionTimerWidget(Settings->TimerWidgetClass, Settings->PrepareTimerText,
		                                        PrepareSeconds, Lifetimes);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": no TimerWidgetClass in the "
			"settings; the countdown is not shown.");
	}

	if (PrepareSeconds <= 0)
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &USovietCounterattackMission::OnPreparationOver));
		return;
	}
	World->GetTimerManager().SetTimer(M_PrepareTimer, this, &USovietCounterattackMission::OnPreparationOver,
	                                  static_cast<float>(PrepareSeconds), false);
}

void USovietCounterattackMission::SkipPreparation()
{
	UWorld* const World = GetWorld();
	if (M_Phase != EPhase::Preparing || World == nullptr || not IsValid(Settings))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_PrepareTimer);
	Intro_UnbindTakeover();
	M_IntroStage = EIntroStage::Done;
	const float SurviveSeconds = static_cast<float>(M_WaveCount * M_SecondsBetweenWaves);
	if (M_TimerWidget.IsValid())
	{
		M_TimerWidget->InitMissionTimer(Settings->SurviveTimerText, SurviveSeconds, FMissionTimerLifetimeSettings());
	}
	else if (Settings->TimerWidgetClass && SurviveSeconds > 0.f)
	{
		// Skipped before the intro was over: the countdown widget was never made.
		M_TimerWidget = SpawnMissionTimerWidget(Settings->TimerWidgetClass, Settings->SurviveTimerText,
		                                        SurviveSeconds, FMissionTimerLifetimeSettings());
	}
	bM_PrepareCountdownStarted = true;
	OnPreparationOver();
}

void USovietCounterattackMission::OnPreparationOver()
{
	UWorld* const World = GetWorld();
	if (M_Phase != EPhase::Preparing || World == nullptr || not IsValid(Settings))
	{
		return;
	}
	// Last look at the positions (reward when all are held), then they go: they were for the preparation.
	Positions_Poll();
	M_Phase = EPhase::Waves;
	Positions_OnWavesStart();
	Intro_RemoveMarkers();
	SetObjective(Settings->SurviveObjectiveTitle, Settings->SurviveObjectiveDescription);
	if (M_WaveCount <= 0)
	{
		OnSurviveTimerOver();
		return;
	}
	SpawnNextWave();
	if (M_WaveCount > 1)
	{
		World->GetTimerManager().SetTimer(M_WaveTimer, this, &USovietCounterattackMission::SpawnNextWave,
		                                  static_cast<float>(M_SecondsBetweenWaves), true);
	}
	World->GetTimerManager().SetTimer(M_SurviveEndTimer, this, &USovietCounterattackMission::OnSurviveTimerOver,
	                                  static_cast<float>(M_WaveCount * M_SecondsBetweenWaves), false);
}

void USovietCounterattackMission::SpawnNextWave()
{
	const FSovietCounterattackDifficulty* Difficulty = GetDifficulty();
	if (M_Phase != EPhase::Waves || Difficulty == nullptr)
	{
		return;
	}
	if (M_SpawnedWaveCount >= M_WaveCount)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_WaveTimer);
		}
		return;
	}
	++M_SpawnedWaveCount;
	const int32 WaveNumber = M_SpawnedWaveCount;
	if (M_SpawnedWaveCount >= M_WaveCount)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_WaveTimer);
		}
	}

	TArray<FEnemyPlacementUnit> Center;
	TArray<FEnemyPlacementUnit> AirDrop;
	TArray<FEnemyPlacementUnit> Train;
	SplitOverSpawns(RollWaveUnits(*Difficulty, WaveNumber), Center, AirDrop, Train);
	RTSFunctionLibrary::PrintString(FString::Printf(
		TEXT("Soviet counterattack wave %d/%d: %d centre, %d landing field, %d train"), WaveNumber, M_WaveCount,
		Center.Num(), AirDrop.Num(), Train.Num()));

	if (WaveNumber == 1)
	{
		// Before the air batch and the train start: the first train waits for the cinematic.
		Arrival_Arm(AirDrop, not Train.IsEmpty());
	}
	SpawnGroundGroup(Center, ESovietCounterattackSpawn::Center, WaveNumber,
	                 Spawn_GetGroundLocation(Settings->Spawns.CenterSpawnLocation));
	for (const FEnemyPlacementUnit& Unit : AirDrop)
	{
		FSovietCounterattackQueuedAirUnit& Queued = M_AirQueue.AddDefaulted_GetRef();
		Queued.Unit = Unit;
		Queued.WaveNumber = WaveNumber;
	}
	AirDrop_LaunchNextBatchIfIdle();
	Train_Start(Train, WaveNumber);
}

TArray<FEnemyPlacementUnit> USovietCounterattackMission::RollWaveUnits(const FSovietCounterattackDifficulty& Difficulty,
                                                                       const int32 WaveNumber) const
{
	TArray<FEnemyPlacementUnit> Pool;
	for (const ETankSubtype Tank : Difficulty.TankTypes)
	{
		if (Tank != ETankSubtype::Tank_None)
		{
			Pool.Add(FEnemyPlacementUnit::MakeVehicle(Tank));
		}
	}
	for (const ESquadSubtype Squad : Difficulty.SquadTypes)
	{
		if (Squad != ESquadSubtype::Squad_None)
		{
			Pool.Add(FEnemyPlacementUnit::MakeSquad(Squad));
		}
	}
	if (Pool.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": the unit pool of this "
			"difficulty has no tanks and no squads; wave " + FString::FromInt(WaveNumber) + " is empty.");
		return {};
	}
	const int32 Added = (WaveNumber - 1) * Difficulty.UnitsAddedPerWave;
	const int32 Min = FMath::Max(0, Difficulty.MinUnitsFirstWave + Added);
	const int32 Max = FMath::Max(Min, Difficulty.MaxUnitsFirstWave + Added);
	const int32 Count = FMath::RandRange(Min, Max);
	TArray<FEnemyPlacementUnit> Units;
	Units.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Units.Add(Pool[FMath::RandHelper(Pool.Num())]);
	}
	return Units;
}

void USovietCounterattackMission::SplitOverSpawns(TArray<FEnemyPlacementUnit> Units,
                                                  TArray<FEnemyPlacementUnit>& OutCenter,
                                                  TArray<FEnemyPlacementUnit>& OutAirDrop,
                                                  TArray<FEnemyPlacementUnit>& OutTrain) const
{
	// Shuffle so each spawn gets a random mix of the rolled units.
	for (int32 Index = Units.Num() - 1; Index > 0; --Index)
	{
		Units.Swap(Index, FMath::RandRange(0, Index));
	}
	// Leftovers of the even split go to spawns in a random order.
	TArray<int32> SpawnOrder = {0, 1, 2};
	for (int32 Index = SpawnOrder.Num() - 1; Index > 0; --Index)
	{
		SpawnOrder.Swap(Index, FMath::RandRange(0, Index));
	}
	const int32 Base = Units.Num() / 3;
	const int32 Leftover = Units.Num() % 3;
	int32 Counts[3] = {Base, Base, Base};
	for (int32 Index = 0; Index < Leftover; ++Index)
	{
		++Counts[SpawnOrder[Index]];
	}
	TArray<FEnemyPlacementUnit>* Outputs[3] = {&OutCenter, &OutAirDrop, &OutTrain};
	int32 Next = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < 3; ++SpawnIndex)
	{
		for (int32 Count = 0; Count < Counts[SpawnIndex]; ++Count)
		{
			Outputs[SpawnIndex]->Add(Units[Next++]);
		}
	}
	// Without pads the landing field's share comes from the centre.
	if (Settings->Spawns.LandingPadCenters.IsEmpty())
	{
		OutCenter.Append(OutAirDrop);
		OutAirDrop.Empty();
	}
	if (Settings->Spawns.bAllUnitsByTrain && GetCanUseTrain())
	{
		OutTrain.Append(OutCenter);
		OutTrain.Append(OutAirDrop);
		OutCenter.Empty();
		OutAirDrop.Empty();
	}
}

void USovietCounterattackMission::OnSurviveTimerOver()
{
	if (M_Phase != EPhase::Waves)
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WaveTimer);
	}
	if (M_TimerWidget.IsValid())
	{
		M_TimerWidget->RemoveFromParent();
	}
	M_TimerWidget.Reset();
	Remaining_Start();
}

// ------------------------------------------------------------------
// Groups and route
// ------------------------------------------------------------------

FVector USovietCounterattackMission::GetHQLocation() const
{
	const AMissionManager* const MissionManager = GetMissionManagerChecked();
	const ACPPController* const PlayerController = MissionManager
		                                               ? FRTS_Statics::GetRTSController(MissionManager)
		                                               : nullptr;
	ANomadicVehicle* const HQ = IsValid(PlayerController) ? PlayerController->GetPlayerHq() : nullptr;
	if (RTSFunctionLibrary::RTSIsValid(HQ))
	{
		return HQ->GetActorLocation();
	}
	return IsValid(Settings) ? Settings->FallbackHQLocation : FVector::ZeroVector;
}

FVector2D USovietCounterattackMission::GetRouteOffset(const ESovietCounterattackSpawn Spawn) const
{
	if (not IsValid(Settings))
	{
		return FVector2D::ZeroVector;
	}
	switch (Spawn)
	{
	case ESovietCounterattackSpawn::AirDrop:
		return Settings->Spawns.AirDropRouteOffset;
	case ESovietCounterattackSpawn::Train:
		return Settings->Spawns.TrainRouteOffset;
	default:
		return Settings->Spawns.CenterRouteOffset;
	}
}

const TArray<FVector>& USovietCounterattackMission::GetAttackRoutePoints(const ESovietCounterattackSpawn Spawn) const
{
	if (Spawn == ESovietCounterattackSpawn::Train && Settings->bTrainUsesOwnRoute)
	{
		return Settings->TrainAttackRoute;
	}
	return Settings->AttackRoute;
}

TArray<FVector> USovietCounterattackMission::BuildRoute(const ESovietCounterattackSpawn Spawn) const
{
	TArray<FVector> Route;
	if (not IsValid(Settings))
	{
		return Route;
	}
	const FVector2D Offset = GetRouteOffset(Spawn);
	TArray<FVector> Points = GetAttackRoutePoints(Spawn);
	Points.Add(GetHQLocation());
	const AMissionManager* const MissionManager = GetMissionManagerChecked();
	for (const FVector& Point : Points)
	{
		const FVector Shifted = Point + FVector(Offset.X, Offset.Y, 0.f);
		bool bProjected = false;
		const FVector Projected = RTSFunctionLibrary::GetLocationProjected(MissionManager, Shifted, true, bProjected);
		Route.Add(bProjected ? Projected : Shifted);
	}
	return Route;
}

void USovietCounterattackMission::RegisterSpawnedUnit(AActor* Unit)
{
	if (not IsValid(Unit))
	{
		return;
	}
	M_SpawnedUnits.AddUnique(Unit);
	if (M_Phase == EPhase::KillRemaining)
	{
		Remaining_AddUnit(Unit);
	}
}

FName USovietCounterattackMission::MakeGroupMarkerId(const ESovietCounterattackSpawn Spawn, const int32 WaveNumber,
                                                     const int32 GroupId) const
{
	return FName(*FString::Printf(TEXT("SovietCounterattack_%u_W%d_%s_%d"), GetUniqueID(), WaveNumber,
	                              *GetSpawnName(Spawn), GroupId));
}

void USovietCounterattackMission::OrderGroupAfterDelay(const TArray<TWeakObjectPtr<AActor>>& Units,
                                                       const ESovietCounterattackSpawn Spawn, const int32 WaveNumber)
{
	UWorld* const World = GetWorld();
	const float Delay = IsValid(Settings) ? Settings->OrderDelaySeconds : 0.f;
	if (World == nullptr || Delay <= 0.f)
	{
		OrderGroup(Units, Spawn, WaveNumber);
		return;
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Units, Spawn, WaveNumber]()
	{
		OrderGroup(Units, Spawn, WaveNumber);
	}), Delay, false);
}

void USovietCounterattackMission::OrderGroup(const TArray<TWeakObjectPtr<AActor>>& Units,
                                             const ESovietCounterattackSpawn Spawn, const int32 WaveNumber)
{
	if (M_Phase == EPhase::Done)
	{
		return;
	}
	TArray<ASquadController*> Squads;
	TArray<ATankMaster*> Tanks;
	TArray<AActor*> Actors;
	FVector Centroid = FVector::ZeroVector;
	for (const TWeakObjectPtr<AActor>& WeakUnit : Units)
	{
		AActor* const Unit = WeakUnit.Get();
		if (not RTSFunctionLibrary::RTSIsValid(Unit))
		{
			continue;
		}
		if (ASquadController* const Squad = Cast<ASquadController>(Unit))
		{
			Squads.Add(Squad);
		}
		else if (ATankMaster* const Tank = Cast<ATankMaster>(Unit))
		{
			Tanks.Add(Tank);
		}
		else
		{
			continue;
		}
		Actors.Add(Unit);
		Centroid += GetUnitLocation(Unit);
	}
	if (Actors.IsEmpty())
	{
		return;
	}
	Centroid /= Actors.Num();
	AEnemyController* const EnemyController = GetEnemyControllerChecked();
	const TArray<FVector> Route = BuildRoute(Spawn);
	if (not IsValid(EnemyController) || Route.IsEmpty())
	{
		return;
	}
	const FVector Previous = Route.Num() > 1 ? Route[Route.Num() - 2] : Centroid;
	const FRotator FinalDirection(0.f, (Route.Last() - Previous).GetSafeNormal2D().Rotation().Yaw, 0.f);
	EnemyController->MoveAttackMoveFormationToLocation(
		Squads,
		Tanks,
		Route,
		FinalDirection,
		FMath::Clamp(Settings->MaxFormationWidth, 1, Actors.Num()),
		Settings->FormationOffsetMultiplier,
		Settings->AttackMoveSettings,
		Centroid);

	if (M_Phase == EPhase::Waves && Settings->bMarkAttackGroupsOnMiniMap)
	{
		const FName MarkerId = MakeGroupMarkerId(Spawn, WaveNumber, M_NextGroupId++);
		TrackUnitGroupOnMiniMap(MarkerId, Actors, Route.Last());
		M_GroupMarkerIds.Add(MarkerId);
	}
}

// ------------------------------------------------------------------
// Ground spawns
// ------------------------------------------------------------------

TArray<FVector> USovietCounterattackMission::BuildSpawnGrid(const FVector& Center, const int32 Count,
                                                            const FVector& FacingTarget) const
{
	TArray<FVector> Grid;
	if (Count <= 0)
	{
		return Grid;
	}
	const float Spacing = Settings->Spawns.SpawnSpacing;
	FVector Forward = (FacingTarget - Center).GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Right(-Forward.Y, Forward.X, 0.f);
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count))));
	const AMissionManager* const MissionManager = GetMissionManagerChecked();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Row = Index / Columns;
		const int32 Column = Index % Columns;
		const int32 ColumnsInRow = FMath::Min(Columns, Count - Row * Columns);
		// Rows trail behind the front row, away from where the group heads.
		const FVector Offset = Right * ((Column - (ColumnsInRow - 1) * 0.5f) * Spacing) - Forward * (Row * Spacing);
		const FVector Candidate = Center + Offset;
		bool bProjected = false;
		const FVector Projected = RTSFunctionLibrary::GetLocationProjected(MissionManager, Candidate, true,
		                                                                   bProjected);
		Grid.Add((bProjected ? Projected : Candidate) + FVector(0.f, 0.f, SovietCounterattackConstants::SpawnZLift));
	}
	return Grid;
}

void USovietCounterattackMission::SpawnGroundGroup(const TArray<FEnemyPlacementUnit>& Units,
                                                   const ESovietCounterattackSpawn Spawn, const int32 WaveNumber,
                                                   const FVector& Center)
{
	if (Units.IsEmpty() || not IsValid(Settings))
	{
		return;
	}
	ARTSAsyncSpawner* const AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetMissionManagerChecked());
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + " found no async spawner.");
		return;
	}
	// Face the first route point that is not the spawn itself (a spawn may sit on a route point).
	FVector FacingTarget = GetHQLocation();
	for (const FVector& RoutePoint : GetAttackRoutePoints(Spawn))
	{
		if (FVector::Dist2D(RoutePoint, Center) > 2000.f)
		{
			FacingTarget = RoutePoint;
			break;
		}
	}
	const TArray<FVector> Grid = BuildSpawnGrid(Center, Units.Num(), FacingTarget);
	const FRotator Facing(0.f, (FacingTarget - Center).GetSafeNormal2D().Rotation().Yaw, 0.f);

	const int32 GroupId = M_NextGroupId++;
	FSovietCounterattackGroundGroup& Group = M_GroundGroups.AddDefaulted_GetRef();
	Group.GroupId = GroupId;
	Group.Spawn = Spawn;
	Group.WaveNumber = WaveNumber;
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		Group.PendingSpawnKeys.Add(GroupId * SovietCounterattackConstants::SpawnKeysPerGroup + Index);
	}

	TWeakObjectPtr<USovietCounterattackMission> WeakThis(this);
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		const int32 SpawnKey = GroupId * SovietCounterattackConstants::SpawnKeysPerGroup + Index;
		const bool bQueued = AsyncSpawner->AsyncSpawnOptionAtLocation(
			Units[Index].ToTrainingOption(),
			Grid[Index],
			this,
			SpawnKey,
			[WeakThis, GroupId, SpawnKey](const FTrainingOption&, AActor* SpawnedActor, const int32)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnGroundUnitSpawned(GroupId, SpawnKey, SpawnedActor);
				}
			},
			Facing);
		if (not bQueued)
		{
			// The spawner reported why; resolves the key unless its callback already did.
			OnGroundUnitSpawned(GroupId, SpawnKey, nullptr);
		}
	}
}

void USovietCounterattackMission::OnGroundUnitSpawned(const int32 GroupId, const int32 SpawnKey, AActor* SpawnedActor)
{
	FSovietCounterattackGroundGroup* const Group = M_GroundGroups.FindByPredicate(
		[GroupId](const FSovietCounterattackGroundGroup& Each) { return Each.GroupId == GroupId; });
	if (Group == nullptr || Group->PendingSpawnKeys.Remove(SpawnKey) == 0)
	{
		return;
	}
	if (IsValid(SpawnedActor))
	{
		Group->Units.Add(SpawnedActor);
		RegisterSpawnedUnit(SpawnedActor);
	}
	FinishGroundGroupIfDone(GroupId);
}

void USovietCounterattackMission::FinishGroundGroupIfDone(const int32 GroupId)
{
	const int32 GroupIndex = M_GroundGroups.IndexOfByPredicate(
		[GroupId](const FSovietCounterattackGroundGroup& Each) { return Each.GroupId == GroupId; });
	if (GroupIndex == INDEX_NONE || not M_GroundGroups[GroupIndex].PendingSpawnKeys.IsEmpty())
	{
		return;
	}
	const FSovietCounterattackGroundGroup Group = M_GroundGroups[GroupIndex];
	M_GroundGroups.RemoveAt(GroupIndex);
	if (not Group.Units.IsEmpty())
	{
		OrderGroupAfterDelay(Group.Units, Group.Spawn, Group.WaveNumber);
	}
}

// ------------------------------------------------------------------
// Landing field
// ------------------------------------------------------------------

FVector USovietCounterattackMission::AirDrop_GetPadLocation(const int32 PadIndex) const
{
	return Spawn_GetGroundLocation(Settings->Spawns.LandingPadCenters[PadIndex]);
}

FVector USovietCounterattackMission::Spawn_GetGroundLocation(const FVector& Point) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not FMath::IsNearlyZero(Point.Z))
	{
		return Point;
	}
	using SovietCounterattackConstants::PadTraceHalfHeight;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SovietCounterattackGround), false);
	const FVector Start(Point.X, Point.Y, PadTraceHalfHeight);
	const FVector End(Point.X, Point.Y, -PadTraceHalfHeight);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	bool bProjected = false;
	const FVector Projected = RTSFunctionLibrary::GetLocationProjected(GetMissionManagerChecked(), Point, true,
	                                                                   bProjected);
	return bProjected ? Projected : Point;
}

void USovietCounterattackMission::AirDrop_LaunchNextBatchIfIdle()
{
	UWorld* const World = GetWorld();
	if (not M_AirSlots.IsEmpty() || M_AirQueue.IsEmpty() || World == nullptr || not IsValid(Settings)
		|| M_Phase == EPhase::Done)
	{
		return;
	}
	const int32 PadCount = Settings->Spawns.LandingPadCenters.Num();
	if (PadCount <= 0)
	{
		return;
	}
	const int32 BatchSize = FMath::Min(PadCount, M_AirQueue.Num());
	M_AirBatchWave = M_AirQueue[0].WaveNumber;
	bM_AirBatchOrdered = false;
	const FSovietCounterattackAirDropSettings& AirSettings = Settings->AirDrop;

	// All slots exist before any air drop starts: a drop that fails at once must not see a half-made batch.
	M_AirSlots.SetNum(BatchSize);
	TArray<AEnemyAirDrop*> ToFinish;
	TArray<FTransform> Transforms;
	for (int32 PadIndex = 0; PadIndex < BatchSize; ++PadIndex)
	{
		const FTransform SpawnTransform(AirDrop_GetPadLocation(PadIndex));
		AEnemyAirDrop* const AirDrop = World->SpawnActorDeferred<AEnemyAirDrop>(
			AEnemyAirDrop::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (not IsValid(AirDrop))
		{
			RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName()
				+ " could not spawn the air drop of pad " + FString::FromInt(PadIndex) + ".");
			M_AirSlots[PadIndex].bResolved = true;
			M_AirSlots[PadIndex].bFinished = true;
			continue;
		}
		FEnemyAirDropUnitEntry Entry;
		Entry.UnitOptions = {M_AirQueue[PadIndex].Unit};
		Entry.Count = 1;
		FEnemyAirDropUnits Units;
		Units.BaseUnits = {Entry};
		FEnemyAirDropDeliverySettings Delivery;
		Delivery.Delivery = EEnemyAirDropDelivery::CargoPlane;
		Delivery.CargoPlaneAbility = AirSettings.CargoPlaneAbility;
		Delivery.bRopeSlungVehicles = AirSettings.bRopeSlungVehicles;
		Delivery.bWarnPlayer = AirSettings.bWarnPlayer && PadIndex == 0;
		Delivery.bHandUnitsToEnemyAI = false;
		const FName AirDropId(*FString::Printf(TEXT("SovietCounterattack_%u_%d"), GetUniqueID(), ++M_AirDropSerial));
		AirDrop->InitRuntimeAirDrop(AirDropId, Units, Delivery, PadIndex * AirSettings.StaggerSeconds);
		AirDrop->OnUnitDelivered.AddUObject(this, &USovietCounterattackMission::AirDrop_OnUnitDelivered);
		AirDrop->OnAirDropFinished.AddDynamic(this, &USovietCounterattackMission::AirDrop_OnFinished);
		M_AirSlots[PadIndex].AirDrop = AirDrop;
		ToFinish.Add(AirDrop);
		Transforms.Add(SpawnTransform);
	}
	M_AirQueue.RemoveAt(0, BatchSize);
	for (int32 Index = 0; Index < ToFinish.Num(); ++Index)
	{
		ToFinish[Index]->FinishSpawning(Transforms[Index]);
	}
	AirDrop_CheckBatch();
}

FSovietCounterattackAirSlot* USovietCounterattackMission::AirDrop_FindSlot(const AEnemyAirDrop* AirDrop)
{
	return M_AirSlots.FindByPredicate([AirDrop](const FSovietCounterattackAirSlot& Slot)
	{
		return Slot.AirDrop.Get() == AirDrop;
	});
}

void USovietCounterattackMission::AirDrop_OnUnitDelivered(AEnemyAirDrop* AirDrop, AActor* Unit)
{
	FSovietCounterattackAirSlot* const Slot = AirDrop_FindSlot(AirDrop);
	if (Slot == nullptr || not IsValid(Unit))
	{
		return;
	}
	RegisterSpawnedUnit(Unit);
	if (M_AirSlots.IsValidIndex(M_ArrivalPadIndex) && &M_AirSlots[M_ArrivalPadIndex] == Slot)
	{
		Arrival_OnUnitDelivered(Unit);
	}
	if (bM_AirBatchOrdered || Slot->bResolved)
	{
		// Too late for its group (or a second unit): it attacks on its own.
		OrderGroupAfterDelay(TArray<TWeakObjectPtr<AActor>>{TWeakObjectPtr<AActor>(Unit)},
		                     ESovietCounterattackSpawn::AirDrop, M_AirBatchWave);
		return;
	}
	Slot->DeliveredUnit = Unit;
	Slot->bResolved = true;
	AirDrop_CheckBatch();
}

void USovietCounterattackMission::AirDrop_OnFinished(AEnemyAirDrop* AirDrop)
{
	FSovietCounterattackAirSlot* const Slot = AirDrop_FindSlot(AirDrop);
	if (Slot == nullptr)
	{
		return;
	}
	// A drop that finished without a unit was shot down: that pad's unit never comes.
	Slot->bResolved = true;
	Slot->bFinished = true;
	AirDrop_CheckBatch();
}

void USovietCounterattackMission::AirDrop_CheckBatch()
{
	if (M_AirSlots.IsEmpty())
	{
		return;
	}
	bool bAllResolved = true;
	bool bAllFinished = true;
	for (const FSovietCounterattackAirSlot& Slot : M_AirSlots)
	{
		bAllResolved &= Slot.bResolved;
		bAllFinished &= Slot.bFinished;
	}
	if (bAllResolved && not bM_AirBatchOrdered)
	{
		// The batch's units are all on the ground: they attack together.
		bM_AirBatchOrdered = true;
		TArray<TWeakObjectPtr<AActor>> Units;
		for (const FSovietCounterattackAirSlot& Slot : M_AirSlots)
		{
			if (Slot.DeliveredUnit.IsValid())
			{
				Units.Add(Slot.DeliveredUnit);
			}
		}
		if (not Units.IsEmpty())
		{
			OrderGroupAfterDelay(Units, ESovietCounterattackSpawn::AirDrop, M_AirBatchWave);
		}
	}
	if (bAllFinished)
	{
		// Every aircraft has left the pads: the next four may come.
		AirDrop_ClearBatch(false);
		AirDrop_LaunchNextBatchIfIdle();
	}
}

void USovietCounterattackMission::AirDrop_ClearBatch(const bool bDestroyAirDrops)
{
	for (const FSovietCounterattackAirSlot& Slot : M_AirSlots)
	{
		AEnemyAirDrop* const AirDrop = Slot.AirDrop.Get();
		if (not IsValid(AirDrop))
		{
			continue;
		}
		AirDrop->OnUnitDelivered.RemoveAll(this);
		AirDrop->OnAirDropFinished.RemoveAll(this);
		if (bDestroyAirDrops)
		{
			AirDrop->Destroy();
		}
		else
		{
			// Usually called from inside its own finished broadcast.
			AirDrop->SetLifeSpan(SovietCounterattackConstants::FinishedAirDropLifeSpan);
		}
	}
	M_AirSlots.Empty();
}

// ------------------------------------------------------------------
// Train
// ------------------------------------------------------------------

bool USovietCounterattackMission::GetCanUseTrain() const
{
	return IsValid(Settings) && Settings->Train.TrainClass != nullptr && Settings->Train.PathPoints.Num() >= 2;
}

void USovietCounterattackMission::Train_Start(const TArray<FEnemyPlacementUnit>& Units, const int32 WaveNumber)
{
	if (Units.IsEmpty())
	{
		return;
	}
	if (not GetCanUseTrain())
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": no train class or train path "
			"in the settings; the station units spawn without a train.");
		SpawnGroundGroup(Units, ESovietCounterattackSpawn::Train, WaveNumber, Settings->Spawns.StationSpawnLocation);
		return;
	}
	FQueuedTrain& Queued = M_TrainQueue.AddDefaulted_GetRef();
	Queued.Units = Units;
	Queued.WaveNumber = WaveNumber;
	Train_StartNextIfIdle();
}

void USovietCounterattackMission::Train_StartNextIfIdle()
{
	UWorld* const World = GetWorld();
	if (not M_TrainRuns.IsEmpty() || M_TrainQueue.IsEmpty() || World == nullptr || M_Phase == EPhase::Done
		|| bM_HoldTrainForCinematic)
	{
		return;
	}
	const FQueuedTrain Queued = M_TrainQueue[0];
	M_TrainQueue.RemoveAt(0);
	const FSovietCounterattackTrainSettings& TrainSettings = Settings->Train;

	TArray<UStaticMesh*> CargoMeshes;
	int32 SquadCount = 0;
	for (const FEnemyPlacementUnit& Unit : Queued.Units)
	{
		if (Unit.Kind == EEnemyPlacementUnitKind::Squad)
		{
			++SquadCount;
			continue;
		}
		UStaticMesh* const Mesh = TrainSettings.CargoTankMeshes.IsEmpty()
			                          ? nullptr
			                          : TrainSettings.CargoTankMeshes[CargoMeshes.Num() % TrainSettings.CargoTankMeshes.
				                          Num()].Get();
		CargoMeshes.Add(Mesh);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASovietCargoTrain* const Train = World->SpawnActor<ASovietCargoTrain>(
		TrainSettings.TrainClass, TrainSettings.PathPoints[0], FRotator::ZeroRotator, Params);
	if (not IsValid(Train))
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + " could not spawn the train; "
			"its units spawn at the station.");
		SpawnGroundGroup(Queued.Units, ESovietCounterattackSpawn::Train, Queued.WaveNumber,
		                 Settings->Spawns.StationSpawnLocation);
		Train_StartNextIfIdle();
		return;
	}
	FPendingTrainRun& Run = M_TrainRuns.AddDefaulted_GetRef();
	Run.Train = Train;
	Run.Units = Queued.Units;
	Run.WaveNumber = Queued.WaveNumber;
	Train->OnArrived.AddUObject(this, &USovietCounterattackMission::Train_OnArrived);
	Train->OnDeparted.AddUObject(this, &USovietCounterattackMission::Train_OnDeparted);
	// Squads ride in one goods wagon behind the flatcars.
	Train->StartRun(TrainSettings.PathPoints, TrainSettings.StopFrontLocation, CargoMeshes, SquadCount > 0 ? 1 : 0);
}

USovietCounterattackMission::FPendingTrainRun* USovietCounterattackMission::Train_FindRun(
	const ASovietCargoTrain* Train)
{
	return M_TrainRuns.FindByPredicate([Train](const FPendingTrainRun& Run) { return Run.Train.Get() == Train; });
}

void USovietCounterattackMission::Train_OnArrived(ASovietCargoTrain* Train)
{
	FPendingTrainRun* const Run = Train_FindRun(Train);
	UWorld* const World = GetWorld();
	if (Run == nullptr || World == nullptr)
	{
		return;
	}
	const float Delay = Settings->Train.UnloadDelaySeconds;
	const TWeakObjectPtr<ASovietCargoTrain> WeakTrain(Train);
	if (Delay <= 0.f)
	{
		Train_Unload(Train);
		return;
	}
	World->GetTimerManager().SetTimer(Run->UnloadTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakTrain]()
	{
		Train_Unload(WeakTrain.Get());
	}), Delay, false);
}

void USovietCounterattackMission::Train_Unload(ASovietCargoTrain* Train)
{
	FPendingTrainRun* const Run = Train_FindRun(Train);
	UWorld* const World = GetWorld();
	if (Run == nullptr || World == nullptr)
	{
		return;
	}
	const TArray<FEnemyPlacementUnit> Units = Run->Units;
	const int32 WaveNumber = Run->WaveNumber;
	Run->Units.Empty();
	if (IsValid(Train))
	{
		Train->HideCargo();
	}
	const TWeakObjectPtr<ASovietCargoTrain> WeakTrain(Train);
	World->GetTimerManager().SetTimer(Run->DepartTimer, FTimerDelegate::CreateWeakLambda(this, [WeakTrain]()
	{
		if (WeakTrain.IsValid())
		{
			WeakTrain->Depart();
		}
	}), FMath::Max(0.01f, Settings->Train.DepartDelaySeconds), false);
	// Last: the spawn callbacks may run inside this call and touch M_TrainRuns' neighbours.
	SpawnGroundGroup(Units, ESovietCounterattackSpawn::Train, WaveNumber, Settings->Spawns.StationSpawnLocation);
}

void USovietCounterattackMission::Train_OnDeparted(ASovietCargoTrain* Train)
{
	const int32 RunIndex = M_TrainRuns.IndexOfByPredicate(
		[Train](const FPendingTrainRun& Run) { return Run.Train.Get() == Train; });
	if (RunIndex == INDEX_NONE)
	{
		return;
	}
	// A train that left without unloading (should not happen) still delivers its units at the station.
	const FPendingTrainRun Run = M_TrainRuns[RunIndex];
	M_TrainRuns.RemoveAt(RunIndex);
	if (not Run.Units.IsEmpty())
	{
		SpawnGroundGroup(Run.Units, ESovietCounterattackSpawn::Train, Run.WaveNumber,
		                 Settings->Spawns.StationSpawnLocation);
	}
	Train_StartNextIfIdle();
}

// ------------------------------------------------------------------
// Kill the remaining Soviets
// ------------------------------------------------------------------

void USovietCounterattackMission::Remaining_Start()
{
	UWorld* const World = GetWorld();
	M_Phase = EPhase::KillRemaining;
	for (const FName& MarkerId : M_GroupMarkerIds)
	{
		TrackUnitGroupOnMiniMap(MarkerId, {}, FVector::ZeroVector);
	}
	M_GroupMarkerIds.Empty();
	for (const TWeakObjectPtr<AActor>& Unit : M_SpawnedUnits)
	{
		if (RTSFunctionLibrary::RTSIsValidWeak(Unit))
		{
			Remaining_AddUnit(Unit.Get());
		}
	}
	if (World != nullptr)
	{
		World->GetTimerManager().SetTimer(M_RemainingPollTimer, this, &USovietCounterattackMission::Remaining_Poll,
		                                  SovietCounterattackConstants::RemainingPollSeconds, true);
	}
	Remaining_Poll();
}

void USovietCounterattackMission::Remaining_AddUnit(AActor* Unit)
{
	if (not IsValid(Settings) || not RTSFunctionLibrary::RTSIsValid(Unit)
		|| M_RemainingUnits.ContainsByPredicate([Unit](const FSovietCounterattackRemainingUnit& Each)
		{
			return Each.Unit.Get() == Unit;
		}))
	{
		return;
	}
	FSovietCounterattackRemainingUnit& Remaining = M_RemainingUnits.AddDefaulted_GetRef();
	Remaining.Unit = Unit;
	const FName IconId(*FString::Printf(TEXT("SovietCounterattackLeft_%u_%d"), GetUniqueID(),
	                                    M_NextRemainingIconIndex++));
	Remaining.IconId = AddCustomMiniMapIcon(IconId, Settings->RemainingUnitIcon, GetUnitLocation(Unit),
	                                        FRotator::ZeroRotator, FMinimapIconTextPayload());
}

void USovietCounterattackMission::Remaining_Poll()
{
	if (M_Phase != EPhase::KillRemaining)
	{
		return;
	}
	AFowManager* const FowManager = FRTS_Statics::GetFowManager(GetMissionManagerChecked());
	for (int32 Index = M_RemainingUnits.Num() - 1; Index >= 0; --Index)
	{
		const FSovietCounterattackRemainingUnit& Remaining = M_RemainingUnits[Index];
		if (RTSFunctionLibrary::RTSIsValidWeak(Remaining.Unit))
		{
			if (IsValid(FowManager) && not Remaining.IconId.IsNone())
			{
				FowManager->SetCustomMiniMapIconTransform(Remaining.IconId, GetUnitLocation(Remaining.Unit.Get()),
				                                          FRotator::ZeroRotator);
			}
			continue;
		}
		RemoveCustomMiniMapIcon(Remaining.IconId);
		M_RemainingUnits.RemoveAt(Index);
	}
	const int32 Alive = M_RemainingUnits.Num();
	Remaining_RefreshObjective(Alive);
	if (Alive == 0 && GetUndeliveredUnitCount() == 0)
	{
		M_Phase = EPhase::Done;
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_RemainingPollTimer);
		}
		OnMissionComplete();
	}
}

void USovietCounterattackMission::Remaining_RefreshObjective(const int32 AliveCount)
{
	if (not IsValid(Settings))
	{
		return;
	}
	const int32 Undelivered = GetUndeliveredUnitCount();
	const FString Count = FRTSRichTextConverter::MakeRTSRich(FString::FromInt(AliveCount + Undelivered),
	                                                         ERTSRichText::Text_SubTitle);
	const FText Title = FText::FromString(Settings->RemainingObjectiveTitle.ToString() + TEXT(" ") + Count);
	if (Title.EqualTo(MissionWidgetState.MissionTitle))
	{
		return;
	}
	SetObjective(Title, Settings->RemainingObjectiveDescription);
}

// ------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------

TArray<AActor*> USovietCounterattackMission::GetLivingSovietUnits() const
{
	TArray<AActor*> Living;
	for (const TWeakObjectPtr<AActor>& Unit : M_SpawnedUnits)
	{
		if (RTSFunctionLibrary::RTSIsValidWeak(Unit))
		{
			Living.Add(Unit.Get());
		}
	}
	return Living;
}

TArray<FVector> USovietCounterattackMission::GetLivingSovietUnitLocations() const
{
	TArray<FVector> Locations;
	for (const AActor* Unit : GetLivingSovietUnits())
	{
		Locations.Add(GetUnitLocation(Unit));
	}
	return Locations;
}

int32 USovietCounterattackMission::GetUndeliveredUnitCount() const
{
	int32 Count = M_AirQueue.Num();
	for (const FSovietCounterattackAirSlot& Slot : M_AirSlots)
	{
		Count += Slot.bResolved ? 0 : 1;
	}
	for (const FSovietCounterattackGroundGroup& Group : M_GroundGroups)
	{
		Count += Group.PendingSpawnKeys.Num();
	}
	for (const FPendingTrainRun& Run : M_TrainRuns)
	{
		Count += Run.Units.Num();
	}
	for (const FQueuedTrain& Queued : M_TrainQueue)
	{
		Count += Queued.Units.Num();
	}
	return Count;
}

// ------------------------------------------------------------------
// Dig-in intro: Dig In field manual -> markers on the ridges -> camera tour -> prepare countdown
// ------------------------------------------------------------------

void USovietCounterattackMission::Intro_Start()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings))
	{
		return;
	}
	M_IntroStage = EIntroStage::Takeover;
	// Placed first so they already stand on the ridges while the field manual explains digging in.
	Intro_SpawnMarkers();
	Intro_RevealPositions();
	if (not Settings->DigInIntro.bShowDigInTakeover)
	{
		Intro_StartTour();
		return;
	}
	UInfoTakeoverSubsystem* const InfoTakeovers = World->GetSubsystem<UInfoTakeoverSubsystem>();
	if (InfoTakeovers == nullptr)
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": no info takeover subsystem; "
			"the Dig In field manual is not shown.");
		Intro_StartTour();
		return;
	}
	// Bound before queueing: the subsystem can open and finish the request inside the queue call.
	bM_IntroTakeoverQueuing = true;
	bM_IntroTakeoverFinishedWhileQueuing = false;
	M_DigInTakeoverHandle = InfoTakeovers->OnTakeoverRequestFinished.AddUObject(
		this, &USovietCounterattackMission::Intro_OnTakeoverFinished);
	M_DigInTakeoverRequestId = InfoTakeovers->QueueDigInTakeover();
	bM_IntroTakeoverQueuing = false;
	if (M_DigInTakeoverRequestId == INDEX_NONE || bM_IntroTakeoverFinishedWhileQueuing)
	{
		Intro_UnbindTakeover();
		Intro_StartTour();
	}
}

void USovietCounterattackMission::Intro_OnTakeoverFinished(const int32 RequestId,
                                                           const EInfoTakeoverRequestResult Result)
{
	if (bM_IntroTakeoverQueuing)
	{
		bM_IntroTakeoverFinishedWhileQueuing = true;
		return;
	}
	if (RequestId != M_DigInTakeoverRequestId || M_IntroStage != EIntroStage::Takeover)
	{
		return;
	}
	Intro_UnbindTakeover();
	if (Result == EInfoTakeoverRequestResult::Cancelled)
	{
		// The world is ending.
		return;
	}
	Intro_StartTour();
}

void USovietCounterattackMission::Intro_UnbindTakeover()
{
	if (M_DigInTakeoverHandle.IsValid())
	{
		if (UWorld* const World = GetWorld())
		{
			if (UInfoTakeoverSubsystem* const InfoTakeovers = World->GetSubsystem<UInfoTakeoverSubsystem>())
			{
				InfoTakeovers->OnTakeoverRequestFinished.Remove(M_DigInTakeoverHandle);
			}
		}
	}
	M_DigInTakeoverHandle.Reset();
	M_DigInTakeoverRequestId = INDEX_NONE;
}

FVector USovietCounterattackMission::Intro_GetGroundLocation(const FVector& Location) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return Location;
	}
	using SovietCounterattackConstants::MarkerTraceHalfHeight;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SovietCounterattackDigInMarker), false);
	// World static only: a unit standing on the spot must not lift the marker onto its roof.
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	const FVector Start(Location.X, Location.Y, Location.Z + MarkerTraceHalfHeight);
	const FVector End(Location.X, Location.Y, Location.Z - MarkerTraceHalfHeight);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void USovietCounterattackMission::Intro_SpawnMarkers()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings))
	{
		return;
	}
	Intro_RemoveMarkers();
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	UClass* const MarkerClass = Intro.MarkerClass.LoadSynchronous();
	const int32 Count = Intro.MarkerLocations.Num();
	// One slot per position, in MarkerLocations order (a slot stays empty when its marker could not spawn).
	M_DigInMarkers.SetNum(Count);
	M_DigInMarkerIconIds.Init(NAME_None, Count);
	M_DigInPositions.SetNum(Count);
	M_PositionHeld.Init(false, Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Location = Intro_GetGroundLocation(Intro.MarkerLocations[Index]);
		M_DigInPositions[Index] = Location;
		Positions_SetIcon(Index, true);
		if (MarkerClass == nullptr)
		{
			continue;
		}
		FVector ToFacing = Intro.MarkerFacingTarget - Location;
		ToFacing.Z = 0.f;
		const float Yaw = (ToFacing.IsNearlyZero() ? 0.f : ToFacing.Rotation().Yaw) + Intro.MarkerYawOffset;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* const Marker = World->SpawnActor<AActor>(MarkerClass, Location, FRotator(0.f, Yaw, 0.f), Params);
		if (not IsValid(Marker))
		{
			continue;
		}
		// The hint Blueprint gives itself a short life span on BeginPlay; these stay for the whole preparation.
		Marker->SetLifeSpan(0.f);
		M_DigInMarkers[Index] = Marker;
		Positions_SetMarkerText(Index, Intro.MarkerText);
	}
	if (MarkerClass == nullptr && Count > 0)
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": no dig-in marker class (set "
			"DigInIntro.MarkerClass on the settings); only the minimap icons show the dig-in positions.");
	}
	if (Count > 0)
	{
		World->GetTimerManager().SetTimer(M_PositionPollTimer, this, &USovietCounterattackMission::Positions_Poll,
		                                  SovietCounterattackConstants::PositionPollSeconds, true);
	}
}

void USovietCounterattackMission::Positions_SetIcon(const int32 Index, const bool bShow)
{
	if (not M_DigInMarkerIconIds.IsValidIndex(Index) || not M_DigInPositions.IsValidIndex(Index)
		|| not IsValid(Settings))
	{
		return;
	}
	if (not bShow)
	{
		if (M_DigInMarkerIconIds[Index] != NAME_None)
		{
			RemoveCustomMiniMapIcon(M_DigInMarkerIconIds[Index]);
			M_DigInMarkerIconIds[Index] = NAME_None;
		}
		return;
	}
	if (M_DigInMarkerIconIds[Index] != NAME_None)
	{
		return;
	}
	const FName IconId(*FString::Printf(TEXT("SovietCounterattack_DigIn_%u_%d"), GetUniqueID(), Index));
	M_DigInMarkerIconIds[Index] = AddCustomMiniMapIcon(IconId, Settings->DigInIntro.MarkerMiniMapIcon,
	                                                   M_DigInPositions[Index], FRotator::ZeroRotator,
	                                                   FMinimapIconTextPayload());
}

void USovietCounterattackMission::Positions_SetMarkerText(const int32 Index, const FText& Text) const
{
	AActor* const Marker = M_DigInMarkers.IsValidIndex(Index) ? M_DigInMarkers[Index].Get() : nullptr;
	if (not IsValid(Marker) || Text.IsEmpty())
	{
		return;
	}
	if (UFunction* const SetTextEvent = Marker->FindFunction(SovietCounterattackConstants::MarkerSetTextEventName))
	{
		SovietCounterattackConstants::FMarkerSetTextParams TextParams{Text};
		Marker->ProcessEvent(SetTextEvent, &TextParams);
	}
}

void USovietCounterattackMission::Positions_SetHeld(const int32 Index, const bool bHeld)
{
	if (not M_PositionHeld.IsValidIndex(Index) || M_PositionHeld[Index] == bHeld || not IsValid(Settings))
	{
		return;
	}
	M_PositionHeld[Index] = bHeld;
	// Held: the ghost wall and arrow go; the text widget stays and says so.
	Positions_SetMarkerGhostVisible(Index, not bHeld);
	Positions_SetMarkerText(Index, bHeld ? Settings->DigInIntro.MarkerHeldText : Settings->DigInIntro.MarkerText);
	Positions_SetIcon(Index, not bHeld);
}

void USovietCounterattackMission::Positions_SetMarkerGhostVisible(const int32 Index, const bool bVisible) const
{
	AActor* const Marker = M_DigInMarkers.IsValidIndex(Index) ? M_DigInMarkers[Index].Get() : nullptr;
	if (not IsValid(Marker))
	{
		return;
	}
	TArray<UPrimitiveComponent*> Primitives;
	Marker->GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		if (not Primitive->IsA<UWidgetComponent>())
		{
			Primitive->SetVisibility(bVisible, true);
		}
	}
}

int32 USovietCounterattackMission::Positions_FindHeldIndex(const ATankMaster* Tank) const
{
	if (not IsValid(Tank) || not IsValid(Settings) || FRTSPlayers::GetOwningPlayerOfActor(Tank) != 1)
	{
		return INDEX_NONE;
	}
	const UDigInComponent* const DigIn = Tank->FindComponentByClass<UDigInComponent>();
	if (DigIn == nullptr || not DigIn->GetIsDugIn())
	{
		return INDEX_NONE;
	}
	// The closest position in range; two vehicles on one position still hold just that one.
	int32 Closest = INDEX_NONE;
	float ClosestDistanceSquared = FMath::Square(Settings->DigInIntro.PositionRadius);
	for (int32 Index = 0; Index < M_DigInPositions.Num(); ++Index)
	{
		const float DistanceSquared = FVector::DistSquared2D(Tank->GetActorLocation(), M_DigInPositions[Index]);
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			Closest = Index;
		}
	}
	return Closest;
}

int32 USovietCounterattackMission::GetDigInPositionsHeld() const
{
	int32 Held = 0;
	for (const bool bHeld : M_PositionHeld)
	{
		Held += bHeld ? 1 : 0;
	}
	return Held;
}

void USovietCounterattackMission::Positions_Poll()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings) || M_Phase != EPhase::Preparing || M_DigInPositions.IsEmpty())
	{
		if (World != nullptr)
		{
			World->GetTimerManager().ClearTimer(M_PositionPollTimer);
		}
		return;
	}
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	TArray<bool> Held;
	Held.Init(false, M_DigInPositions.Num());
	for (TActorIterator<ATankMaster> It(World); It; ++It)
	{
		const int32 HeldIndex = Positions_FindHeldIndex(*It);
		if (Held.IsValidIndex(HeldIndex))
		{
			Held[HeldIndex] = true;
		}
	}
	const int32 HeldBefore = GetDigInPositionsHeld();
	for (int32 Index = 0; Index < Held.Num(); ++Index)
	{
		Positions_SetHeld(Index, Held[Index]);
	}
	const int32 HeldNow = GetDigInPositionsHeld();
	if (HeldNow != HeldBefore)
	{
		Positions_RefreshObjective();
	}
	if (HeldNow > 0 && not bM_FirstPositionLinePlayed)
	{
		bM_FirstPositionLinePlayed = true;
		if (Intro.FirstPositionHeldLine != nullptr)
		{
			PlayPortrait(Intro.VoiceLinePortrait, Intro.FirstPositionHeldLine);
		}
	}
}

void USovietCounterattackMission::Positions_RefreshObjective()
{
	if (not IsValid(Settings) || M_Phase != EPhase::Preparing)
	{
		return;
	}
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	// Once every position is held the counter has done its job and goes; it comes back if a position is lost again
	// before the waves (the player should know a position needs a vehicle).
	const bool bAllHeld = GetDigInPositionsHeld() >= M_DigInPositions.Num();
	if (not Intro.bEnabled || M_DigInPositions.IsEmpty() || Intro.PositionsObjectiveLine.IsEmpty() || bAllHeld)
	{
		SetObjective(Settings->PrepareObjectiveTitle, Settings->PrepareObjectiveDescription);
		return;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Held"), GetDigInPositionsHeld());
	Args.Add(TEXT("Total"), M_DigInPositions.Num());
	const FText Line = FText::Format(Intro.PositionsObjectiveLine, Args);
	SetObjective(Settings->PrepareObjectiveTitle,
	             FText::Format(FText::FromString(TEXT("{0}\n\n{1}")), Settings->PrepareObjectiveDescription, Line));
}

void USovietCounterattackMission::Positions_OnWavesStart()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_PositionPollTimer);
	}
	if (not IsValid(Settings) || M_DigInPositions.IsEmpty() || bM_AllPositionsRewardPaid)
	{
		return;
	}
	if (GetDigInPositionsHeld() < M_DigInPositions.Num())
	{
		return;
	}
	bM_AllPositionsRewardPaid = true;
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	if (UPlayerResourceManager* const Resources = FRTS_Statics::GetPlayerResourceManager(this))
	{
		for (const FSovietCounterattackResourceReward& Reward : Intro.AllPositionsHeldReward)
		{
			if (Reward.Amount > 0)
			{
				Resources->AddResource(Reward.Resource, Reward.Amount);
			}
		}
	}
	if (Intro.AllPositionsHeldLine != nullptr)
	{
		PlayPortrait(Intro.VoiceLinePortrait, Intro.AllPositionsHeldLine);
	}
	RTSFunctionLibrary::PrintString("Soviet counterattack: every dig-in position held; reward paid.");
}

void USovietCounterattackMission::Intro_RevealPositions()
{
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	if (Intro.RevealRadius <= 0.f)
	{
		return;
	}
	// World time: the paused takeover does not use any of it up. Covers the tour and the whole prepare countdown,
	// so the markers stay in sight until the waves come.
	float Seconds = Intro.TourSettings.ReturnSeconds + Intro.RevealSecondsAfterTour
		+ static_cast<float>(M_PrepareSeconds);
	for (const FMissionCameraTourShot& Shot : Intro.TourShots)
	{
		Seconds += Shot.Seconds;
	}
	for (const FVector& Location : Intro.MarkerLocations)
	{
		RevealAreaForSeconds(Location, Intro.RevealRadius, Seconds);
	}
}

void USovietCounterattackMission::Intro_RemoveMarkers()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_PositionPollTimer);
	}
	for (const TWeakObjectPtr<AActor>& Marker : M_DigInMarkers)
	{
		if (Marker.IsValid())
		{
			Marker->Destroy();
		}
	}
	M_DigInMarkers.Empty();
	for (const FName& IconId : M_DigInMarkerIconIds)
	{
		if (IconId != NAME_None)
		{
			RemoveCustomMiniMapIcon(IconId);
		}
	}
	M_DigInMarkerIconIds.Empty();
}

TArray<AActor*> USovietCounterattackMission::GetDigInMarkers() const
{
	TArray<AActor*> Markers;
	for (const TWeakObjectPtr<AActor>& Marker : M_DigInMarkers)
	{
		if (Marker.IsValid())
		{
			Markers.Add(Marker.Get());
		}
	}
	return Markers;
}

void USovietCounterattackMission::Intro_StartTour()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings) || M_Phase != EPhase::Preparing)
	{
		return;
	}
	M_IntroStage = EIntroStage::Tour;
	const FSovietCounterattackDigInIntroSettings& Intro = Settings->DigInIntro;
	if (Intro.TourShots.IsEmpty())
	{
		Intro_Finish();
		return;
	}
	// A refused tour still reports through OnCameraTourFinishedNative next frame.
	PlayCameraTour(Intro.TourShots, Intro.TourSettings, SovietCounterattackConstants::DigInTourId);
	if (Intro.VoiceLine == nullptr)
	{
		return;
	}
	// The tour's takeover stops a portrait that is already playing, and the tour may wait for the screen: the line
	// starts VoiceLineDelaySeconds after the first shot is on screen.
	World->GetTimerManager().SetTimer(M_IntroVoiceLineTimer, this,
	                                  &USovietCounterattackMission::Intro_WaitForFirstTourShot,
	                                  SovietCounterattackConstants::IntroVoiceLinePollSeconds, true);
}

void USovietCounterattackMission::Intro_WaitForFirstTourShot()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings) || M_IntroStage != EIntroStage::Tour)
	{
		if (World != nullptr)
		{
			World->GetTimerManager().ClearTimer(M_IntroVoiceLineTimer);
		}
		return;
	}
	if (GetCameraTourShotIndex() < 0)
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_IntroVoiceLineTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsValid(Settings) && Settings->DigInIntro.VoiceLine != nullptr)
		{
			PlayPortrait(Settings->DigInIntro.VoiceLinePortrait, Settings->DigInIntro.VoiceLine);
		}
	}), FMath::Max(0.01f, Settings->DigInIntro.VoiceLineDelaySeconds), false);
}

void USovietCounterattackMission::OnCameraTourFinishedNative(const int32 ID, const bool bWasShown)
{
	Super::OnCameraTourFinishedNative(ID, bWasShown);
	if (ID != SovietCounterattackConstants::DigInTourId || M_IntroStage != EIntroStage::Tour)
	{
		return;
	}
	Intro_Finish();
}

void USovietCounterattackMission::Intro_Finish()
{
	M_IntroStage = EIntroStage::Done;
	Start_PrepareCountdown();
}

// ------------------------------------------------------------------
// First wave arrival cinematic
// ------------------------------------------------------------------

void USovietCounterattackMission::Arrival_Arm(const TArray<FEnemyPlacementUnit>& AirUnits, const bool bHasTrain)
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings) || M_ArrivalStage != EArrivalStage::NotStarted)
	{
		return;
	}
	const FSovietCounterattackArrivalCinematicSettings& Cinematic = Settings->ArrivalCinematic;
	const int32 PadCount = Settings->Spawns.LandingPadCenters.Num();
	if (not Cinematic.bEnabled || Cinematic.Sequence.IsNull() || AirUnits.IsEmpty() || PadCount <= 0
		|| not M_AirSlots.IsEmpty() || not M_AirQueue.IsEmpty())
	{
		// Nothing to film at the landing field (or another batch is still out): no cinematic.
		M_ArrivalStage = EArrivalStage::Over;
		Arrival_OnOver();
		return;
	}
	// The first batch flies the queue's first units, one per pad in order: film a pad that brings a vehicle.
	M_ArrivalPadIndex = 0;
	const int32 BatchSize = FMath::Min(PadCount, AirUnits.Num());
	for (int32 Index = 0; Index < BatchSize; ++Index)
	{
		if (AirUnits[Index].Kind == EEnemyPlacementUnitKind::Vehicle)
		{
			M_ArrivalPadIndex = Index;
			break;
		}
	}
	M_ArrivalPadGround = AirDrop_GetPadLocation(M_ArrivalPadIndex);
	M_ArrivalStage = EArrivalStage::Waiting;
	M_ArrivalWaitSeconds = 0.f;
	M_ArrivalClosingSpeed = 0.f;
	M_ArrivalLastPadDistance = -1.f;
	M_ArrivalResult = 0;
	bM_ArrivalUnitDelivered = false;
	bM_ArrivalTrainReleased = not bHasTrain;
	bM_HoldTrainForCinematic = bHasTrain && GetCanUseTrain();
	bM_ArrivalFilmsTrain = bM_HoldTrainForCinematic;
	if (not bM_HoldTrainForCinematic)
	{
		bM_ArrivalTrainReleased = true;
	}
	World->GetTimerManager().SetTimer(M_ArrivalPollTimer, this, &USovietCounterattackMission::Arrival_Poll,
	                                  SovietCounterattackConstants::ArrivalPollSeconds, true);
}

AAircraftMaster* USovietCounterattackMission::Arrival_FindAircraft() const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	AAircraftMaster* Closest = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AAircraftMaster> It(World); It; ++It)
	{
		AAircraftMaster* const Aircraft = *It;
		if (not IsValid(Aircraft) || not FRTSPlayers::IsActorHostileToPlayer(Aircraft, 1))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(Aircraft->GetActorLocation(), M_ArrivalPadGround);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			Closest = Aircraft;
		}
	}
	return Closest;
}

void USovietCounterattackMission::Arrival_Poll()
{
	using namespace SovietCounterattackConstants;
	if (M_ArrivalStage != EArrivalStage::Waiting || not IsValid(Settings))
	{
		return;
	}
	const FSovietCounterattackArrivalCinematicSettings& Cinematic = Settings->ArrivalCinematic;
	M_ArrivalWaitSeconds += ArrivalPollSeconds;
	if (M_ArrivalWaitSeconds > Cinematic.MaxWaitSeconds)
	{
		RTSFunctionLibrary::PrintString("Soviet counterattack: no landing field aircraft came near its pad in time; "
			"no arrival cinematic.");
		Arrival_Clear(true);
		return;
	}
	AAircraftMaster* const Aircraft = Arrival_FindAircraft();
	if (not IsValid(Aircraft))
	{
		return;
	}
	if (M_ArrivalAircraft.Get() != Aircraft)
	{
		M_ArrivalAircraft = Aircraft;
		M_ArrivalLastPadDistance = -1.f;
	}
	const float Distance = FVector::Dist2D(Aircraft->GetActorLocation(), M_ArrivalPadGround);
	if (M_ArrivalLastPadDistance >= 0.f)
	{
		const float Closing = (M_ArrivalLastPadDistance - Distance) / ArrivalPollSeconds;
		M_ArrivalClosingSpeed = FMath::Lerp(M_ArrivalClosingSpeed, Closing, ArrivalClosingSpeedBlend);
	}
	M_ArrivalLastPadDistance = Distance;
	float SecondsToPad = TNumericLimits<float>::Max();
	if (Distance <= ArrivalAtPadDistance)
	{
		SecondsToPad = 0.f;
	}
	else if (M_ArrivalClosingSpeed > ArrivalMinClosingSpeed)
	{
		SecondsToPad = Distance / M_ArrivalClosingSpeed;
	}
	const float TrainShot = bM_ArrivalFilmsTrain ? Cinematic.TrainShotSeconds : 0.f;
	if (not bM_ArrivalTrainReleased && SecondsToPad <= Cinematic.TrainPreRollSeconds + TrainShot
		+ Cinematic.DropLeadSeconds)
	{
		Arrival_ReleaseTrain();
	}
	if (SecondsToPad > TrainShot + Cinematic.DropLeadSeconds || GetIsCameraTourPlaying())
	{
		return;
	}
	if (not Arrival_StartCinematic())
	{
		Arrival_Clear(true);
	}
}

void USovietCounterattackMission::Arrival_ReleaseTrain()
{
	bM_ArrivalTrainReleased = true;
	if (not bM_HoldTrainForCinematic)
	{
		return;
	}
	bM_HoldTrainForCinematic = false;
	Train_StartNextIfIdle();
}

UMovieSceneSequencePlayer* USovietCounterattackMission::Arrival_GetPlayer() const
{
	return IsValid(M_ArrivalSequenceActor) ? M_ArrivalSequenceActor->GetSequencePlayer() : nullptr;
}

float USovietCounterattackMission::Arrival_GetSequenceSeconds() const
{
	const UMovieSceneSequencePlayer* const Player = Arrival_GetPlayer();
	return IsValid(Player) ? static_cast<float>(Player->GetCurrentTime().AsSeconds()) : 0.f;
}

void USovietCounterattackMission::Arrival_SetSequenceLength(const float Seconds) const
{
	if (UMovieSceneSequencePlayer* const Player = Arrival_GetPlayer())
	{
		Player->SetTimeRange(0.f, FMath::Max(0.1f, Seconds));
	}
}

AActor* USovietCounterattackMission::GetArrivalCinematicCamera() const
{
	return M_ArrivalCamera;
}

bool USovietCounterattackMission::Arrival_StartCinematic()
{
	using namespace SovietCounterattackConstants;
	UWorld* const World = GetWorld();
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (World == nullptr || not IsValid(Settings) || not IsValid(PlayerController))
	{
		return false;
	}
	const FSovietCounterattackArrivalCinematicSettings& Cinematic = Settings->ArrivalCinematic;
	ULevelSequence* const Sequence = Cinematic.Sequence.LoadSynchronous();
	if (Sequence == nullptr)
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": the arrival cinematic's "
			"level sequence did not load; no cinematic.");
		return false;
	}

	ASovietCargoTrain* const Train = M_TrainRuns.IsEmpty() ? nullptr : M_TrainRuns[0].Train.Get();
	const bool bFilmTrain = IsValid(Train) && not Train->GetHasArrived() && Cinematic.TrainShotSeconds > 0.f;
	const float TrainShotSeconds = bFilmTrain ? Cinematic.TrainShotSeconds : 0.f;
	const float TotalSeconds = TrainShotSeconds + Cinematic.DropShotMaxSeconds;

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	// Pausing at the end reports OnFinished without an OnStop first, so the session sees a natural finish.
	PlaybackSettings.bPauseAtEnd = true;
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* const Player = ULevelSequencePlayer::CreateLevelSequencePlayer(
		World, Sequence, PlaybackSettings, SequenceActor);
	if (not IsValid(Player) || not IsValid(SequenceActor))
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": could not create the arrival "
			"cinematic's sequence player.");
		return false;
	}
	M_ArrivalSequenceActor = SequenceActor;
	Player->SetTimeRange(0.f, TotalSeconds);

	if (not Arrival_SpawnCamera())
	{
		SequenceActor->Destroy();
		M_ArrivalSequenceActor = nullptr;
		return false;
	}
	if (bFilmTrain)
	{
		Arrival_SetupTrainShot(*Train, TrainShotSeconds);
	}
	else
	{
		M_ArrivalStage = EArrivalStage::DropShot;
	}
	M_ArrivalDropShotStart = TrainShotSeconds;
	bM_ArrivalHasLookAt = false;
	// The mission timer is not part of the HUD the takeover hides.
	if (M_TimerWidget.IsValid() && M_TimerWidget->IsVisible())
	{
		M_ArrivalTimerVisibility = M_TimerWidget->GetVisibility();
		M_TimerWidget->SetVisibility(ESlateVisibility::Collapsed);
		bM_ArrivalHidTimer = true;
	}
	RevealAreaForSeconds(M_ArrivalPadGround, Cinematic.RevealRadius, TotalSeconds + ArrivalRevealExtraSeconds);
	Arrival_TickCamera();

	bool bStarted = false;
	M_ArrivalSession = OnCinematicTakeOverWithSkipping(Player, Cinematic.bAllowSkipping, bStarted);
	if (not bStarted || not IsValid(M_ArrivalSession))
	{
		M_ArrivalSession = nullptr;
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": the arrival cinematic "
			"takeover did not start.");
		M_ArrivalCamera->Destroy();
		M_ArrivalCamera = nullptr;
		SequenceActor->Destroy();
		M_ArrivalSequenceActor = nullptr;
		return false;
	}
	M_ArrivalSession->OnCompletedWithResult.AddDynamic(this, &USovietCounterattackMission::Arrival_OnSessionCompleted);
	if (Cinematic.TimeDilation < 1.f)
	{
		M_ArrivalPreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(World);
		UGameplayStatics::SetGlobalTimeDilation(World, Cinematic.TimeDilation);
		bM_ArrivalChangedTimeDilation = true;
	}
	World->GetTimerManager().ClearTimer(M_ArrivalPollTimer);
	PlayerController->SetViewTargetWithBlend(M_ArrivalCamera, 0.f);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &USovietCounterattackMission::Arrival_TickCamera));
	RTSFunctionLibrary::PrintString(FString::Printf(
		TEXT("Soviet counterattack: arrival cinematic started (train shot %s, pad %d)"),
		bFilmTrain ? TEXT("yes") : TEXT("no"), M_ArrivalPadIndex));
	return true;
}

bool USovietCounterattackMission::Arrival_SpawnCamera()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings))
	{
		return false;
	}
	FActorSpawnParameters CameraParams;
	CameraParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CameraParams.ObjectFlags |= RF_Transient;
	M_ArrivalCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), M_ArrivalPadGround,
	                                                  FRotator::ZeroRotator, CameraParams);
	if (not IsValid(M_ArrivalCamera))
	{
		return false;
	}
	if (UCameraComponent* const CameraComponent = M_ArrivalCamera->GetCameraComponent())
	{
		CameraComponent->SetFieldOfView(Settings->ArrivalCinematic.FieldOfView);
		CameraComponent->SetConstraintAspectRatio(false);
	}
	return true;
}

FVector USovietCounterattackMission::Arrival_GetTrackDirectionAt(const FVector& Location) const
{
	const TArray<FVector>& Path = Settings->Train.PathPoints;
	FVector Direction = FVector::ForwardVector;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index + 1 < Path.Num(); ++Index)
	{
		const FVector Closest = FMath::ClosestPointOnSegment(Location, Path[Index], Path[Index + 1]);
		const float DistanceSquared = FVector::DistSquared(Closest, Location);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Direction = (Path[Index + 1] - Path[Index]).GetSafeNormal2D();
		}
	}
	return Direction;
}

void USovietCounterattackMission::Arrival_SetupTrainShot(const ASovietCargoTrain& Train, const float TrainShotSeconds)
{
	const FSovietCounterattackArrivalCinematicSettings& Cinematic = Settings->ArrivalCinematic;
	// A fixed camera ahead of the front flatcar, beside the rails and high: the train rolls towards it.
	const TArray<FVector> Cargo = Train.GetCargoWorldLocations();
	const FVector Front = Cargo.IsEmpty() ? Train.GetActorLocation() : Cargo[0];
	const FVector Direction = Arrival_GetTrackDirectionAt(Front);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	M_ArrivalTrainCameraLocation = Front + Direction * Cinematic.TrainCameraAhead
		+ Right * Cinematic.TrainCameraSide + FVector(0.f, 0.f, Cinematic.TrainCameraHeight);
	M_ArrivalTrainDirection = Direction;
	M_ArrivalStage = EArrivalStage::TrainShot;
	RevealAreaForSeconds(Front + Direction * (Cinematic.TrainCameraAhead * 0.5f), Cinematic.RevealRadius,
	                     TrainShotSeconds + SovietCounterattackConstants::ArrivalRevealExtraSeconds);
}

void USovietCounterattackMission::Arrival_EnterDropShot()
{
	M_ArrivalStage = EArrivalStage::DropShot;
	bM_ArrivalHasLookAt = false;
}

void USovietCounterattackMission::Arrival_TickCamera()
{
	using namespace SovietCounterattackConstants;
	UWorld* const World = GetWorld();
	const bool bInShot = M_ArrivalStage == EArrivalStage::TrainShot || M_ArrivalStage == EArrivalStage::DropShot;
	if (World == nullptr || not bInShot || not IsValid(M_ArrivalCamera) || not IsValid(Settings))
	{
		return;
	}
	const FSovietCounterattackArrivalCinematicSettings& Cinematic = Settings->ArrivalCinematic;
	const float SequenceSeconds = Arrival_GetSequenceSeconds();
	if (M_ArrivalStage == EArrivalStage::TrainShot && SequenceSeconds >= M_ArrivalDropShotStart)
	{
		Arrival_EnterDropShot();
	}

	FVector CameraLocation;
	FVector LookAt;
	if (M_ArrivalStage == EArrivalStage::TrainShot)
	{
		ASovietCargoTrain* const Train = M_TrainRuns.IsEmpty() ? nullptr : M_TrainRuns[0].Train.Get();
		const TArray<FVector> Cargo = IsValid(Train) ? Train->GetCargoWorldLocations() : TArray<FVector>();
		CameraLocation = M_ArrivalTrainCameraLocation;
		LookAt = (Cargo.IsEmpty() ? (IsValid(Train) ? Train->GetActorLocation() : CameraLocation) : Cargo[0])
			+ M_ArrivalTrainDirection * Cinematic.TrainLookAhead + FVector(0.f, 0.f, ArrivalTrainLookLift);
	}
	else
	{
		const FVector Back = FRotator(0.f, Cinematic.DropCameraYaw, 0.f).Vector();
		CameraLocation = M_ArrivalPadGround - Back * Cinematic.DropCameraDistance
			+ FVector(0.f, 0.f, Cinematic.DropCameraHeight);
		// Re-picked every frame: the aircraft that ends up over the filmed pad is the one to watch, not one that led
		// the batch (aircraft take off staggered).
		M_ArrivalAircraft = Arrival_FindAircraft();
		const FVector PadLook = M_ArrivalPadGround + FVector(0.f, 0.f, ArrivalLookAtLift);
		FVector Desired = PadLook;
		if (M_ArrivalDeliveredUnit.IsValid())
		{
			// The vehicle is down: end on it.
			Desired = GetUnitLocation(M_ArrivalDeliveredUnit.Get()) + FVector(0.f, 0.f, ArrivalUnitLookLift);
		}
		else if (M_ArrivalAircraft.IsValid())
		{
			const FVector Offset = (M_ArrivalAircraft->GetActorLocation() - PadLook).GetClampedToMaxSize(
				ArrivalMaxLookOffset);
			Desired = PadLook + Offset * Cinematic.DropLookAtAircraftWeight;
		}
		Desired.Z = FMath::Min(Desired.Z, M_ArrivalPadGround.Z + Cinematic.DropLookAtMaxHeight);
		LookAt = bM_ArrivalHasLookAt
			         ? FMath::VInterpTo(M_ArrivalLookAt, Desired, World->GetDeltaSeconds(), ArrivalLookAtInterpSpeed)
			         : Desired;
		M_ArrivalLookAt = LookAt;
		bM_ArrivalHasLookAt = true;
	}
	M_ArrivalCamera->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
	if (M_ArrivalSession != nullptr && not M_ArrivalSession->GetHasCompleted())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &USovietCounterattackMission::Arrival_TickCamera));
	}
}

void USovietCounterattackMission::Arrival_OnUnitDelivered(AActor* Unit)
{
	if (bM_ArrivalUnitDelivered || not IsValid(Settings))
	{
		return;
	}
	bM_ArrivalUnitDelivered = true;
	M_ArrivalDeliveredUnit = Unit;
	if (M_ArrivalStage == EArrivalStage::Waiting)
	{
		// Arrived sooner than the closing speed said: show it on the ground now rather than not at all.
		if (not Arrival_StartCinematic())
		{
			Arrival_Clear(true);
			return;
		}
	}
	if (M_ArrivalStage != EArrivalStage::TrainShot && M_ArrivalStage != EArrivalStage::DropShot)
	{
		return;
	}
	const float EndSeconds = FMath::Max(Arrival_GetSequenceSeconds(), M_ArrivalDropShotStart)
		+ Settings->ArrivalCinematic.SecondsAfterDelivery;
	Arrival_SetSequenceLength(EndSeconds);
}

void USovietCounterattackMission::Arrival_OnSessionCompleted(const EMissionCinematicTakeOverResult Result)
{
	M_ArrivalResult = static_cast<int32>(Result);
	RTSFunctionLibrary::PrintString(FString::Printf(TEXT("Soviet counterattack: arrival cinematic ended (%d)"),
	                                                M_ArrivalResult));
	Arrival_Clear(true);
}

void USovietCounterattackMission::Arrival_Clear(const bool bReleaseTrain)
{
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		World->GetTimerManager().ClearTimer(M_ArrivalPollTimer);
	}
	if (M_ArrivalStage != EArrivalStage::NotStarted)
	{
		M_ArrivalStage = EArrivalStage::Over;
	}
	UMissionCinematicTakeOverSession* const Session = M_ArrivalSession;
	M_ArrivalSession = nullptr;
	if (IsValid(Session))
	{
		Session->OnCompletedWithResult.RemoveAll(this);
		if (not Session->GetHasCompleted())
		{
			Session->RequestCancel();
		}
	}
	if (IsValid(M_ArrivalCamera))
	{
		ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
		if (IsValid(PlayerController) && PlayerController->GetViewTarget() == M_ArrivalCamera)
		{
			PlayerController->SetViewTargetWithBlend(PlayerController->GetPawn(), 0.f);
		}
		M_ArrivalCamera->SetLifeSpan(SovietCounterattackConstants::ArrivalActorLifeSpan);
	}
	M_ArrivalCamera = nullptr;
	if (IsValid(M_ArrivalSequenceActor))
	{
		// The session may end inside this actor's player broadcast: let it go a moment later.
		M_ArrivalSequenceActor->SetLifeSpan(SovietCounterattackConstants::ArrivalActorLifeSpan);
	}
	M_ArrivalSequenceActor = nullptr;
	M_ArrivalAircraft.Reset();
	M_ArrivalDeliveredUnit.Reset();
	if (bM_ArrivalChangedTimeDilation && World != nullptr)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, M_ArrivalPreviousTimeDilation);
	}
	bM_ArrivalChangedTimeDilation = false;
	if (bM_ArrivalHidTimer && M_TimerWidget.IsValid())
	{
		M_TimerWidget->SetVisibility(M_ArrivalTimerVisibility);
	}
	bM_ArrivalHidTimer = false;
	if (bReleaseTrain && bM_HoldTrainForCinematic)
	{
		bM_HoldTrainForCinematic = false;
		Train_StartNextIfIdle();
	}
	else if (not bReleaseTrain)
	{
		bM_HoldTrainForCinematic = false;
	}
	// bReleaseTrain = a real end (finished, skipped, timed out, failed to start); false = the mission is cleaned up.
	if (bReleaseTrain)
	{
		Arrival_OnOver();
	}
}

void USovietCounterattackMission::Arrival_OnOver()
{
	if (bM_ArrivalOverHandled || M_Phase == EPhase::Done)
	{
		return;
	}
	bM_ArrivalOverHandled = true;
	const bool bWasShown = M_ArrivalResult != 0;
	const bool bWasSkipped = M_ArrivalResult == static_cast<int32>(EMissionCinematicTakeOverResult::Skipped);
	BP_OnArrivalCinematicOver(bWasShown, bWasSkipped);
	UWorld* const World = GetWorld();
	if (World == nullptr || not IsValid(Settings) || not Settings->BaseReinforcements.bEnabled)
	{
		return;
	}
	const float Delay = Settings->BaseReinforcements.DelayAfterCinematicSeconds;
	if (Delay <= 0.f)
	{
		DropBaseReinforcements();
		return;
	}
	World->GetTimerManager().SetTimer(M_BaseReinforcementsTimer, this,
	                                  &USovietCounterattackMission::DropBaseReinforcements, Delay, false);
}

FVector USovietCounterattackMission::BaseReinforcements_GetLandingPoint() const
{
	const FSovietCounterattackBaseReinforcementSettings& Reinforcements = Settings->BaseReinforcements;
	FVector Point = Reinforcements.FallbackLandingPoint;
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	ANomadicVehicle* const HQ = IsValid(PlayerController) ? PlayerController->GetPlayerHq() : nullptr;
	if (RTSFunctionLibrary::RTSIsValid(HQ))
	{
		Point = HQ->GetActorLocation() + FVector(Reinforcements.LandingOffsetFromHQ.X,
		                                         Reinforcements.LandingOffsetFromHQ.Y, 0.f);
		Point.Z = 0.f;
	}
	const FVector Ground = Spawn_GetGroundLocation(Point);
	// The first aircraft lands on this exact point: keep it on the navmesh (off buildings), like the others.
	bool bProjected = false;
	const FVector Projected = RTSFunctionLibrary::GetLocationProjected(GetMissionManagerChecked(), Ground, true,
	                                                                   bProjected);
	return bProjected ? Projected : Ground;
}

void USovietCounterattackMission::DropBaseReinforcements()
{
	if (bM_BaseReinforcementsDropped || not IsValid(Settings) || M_Phase == EPhase::Done)
	{
		return;
	}
	bM_BaseReinforcementsDropped = true;
	const FSovietCounterattackBaseReinforcementSettings& Reinforcements = Settings->BaseReinforcements;
	const bool bStrikeDivision = GetPlayerFaction() == ERTSFaction::GerStrikeDivision;
	FPlayerAirDropPayload Payload;
	Payload.Vehicles = bStrikeDivision ? Reinforcements.StrikeDivisionVehicles : Reinforcements.BreakthroughVehicles;
	Payload.Vehicles.Remove(ETankSubtype::Tank_None);
	Payload.CargoPlaneAbility = Reinforcements.CargoPlaneAbility;
	if (Reinforcements.VoiceLine != nullptr)
	{
		PlayPortrait(Reinforcements.VoiceLinePortrait, Reinforcements.VoiceLine);
	}
	if (Payload.GetIsEmpty())
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": BaseReinforcements has no "
			"vehicles for the player's faction; nothing is dropped.");
		return;
	}
	UPlayerAirDropSubsystem* const AirDrop = UPlayerAirDropSubsystem::Get(this);
	if (not IsValid(AirDrop))
	{
		RTSFunctionLibrary::ReportError("Soviet counterattack mission " + GetName() + ": no player air drop "
			"subsystem; the base reinforcements are not dropped.");
		return;
	}
	const FVector LandingPoint = BaseReinforcements_GetLandingPoint();
	if (not AirDrop->DeliverPlayerUnitsByAir(LandingPoint, Payload))
	{
		return;
	}
	if (Reinforcements.MiniMapPingSeconds > 0.f)
	{
		ShowReinforcementsOnMiniMap(LandingPoint, Reinforcements.MiniMapPingSeconds);
	}
	RTSFunctionLibrary::PrintString(FString::Printf(TEXT("Soviet counterattack: %d base reinforcements flying in (%s)"),
	                                                Payload.Vehicles.Num(),
	                                                bStrikeDivision ? TEXT("Strike Division") : TEXT("Breakthrough")));
}
