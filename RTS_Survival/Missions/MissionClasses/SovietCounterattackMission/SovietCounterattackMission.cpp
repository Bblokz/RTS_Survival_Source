// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietCounterattackMission.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/AirDrop/EnemyAirDrop.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionClasses/SovietCounterattackMission/SovietCounterattackSettings.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
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
	const int32 PrepareSeconds = FMath::Max(0, Difficulty->SecondsUntilSovietsArrive);
	const int32 SurviveSeconds = M_WaveCount * M_SecondsBetweenWaves;

	SetObjective(Settings->PrepareObjectiveTitle, Settings->PrepareObjectiveDescription);

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
	if (M_TimerWidget.IsValid())
	{
		M_TimerWidget->InitMissionTimer(Settings->SurviveTimerText,
		                                static_cast<float>(M_WaveCount * M_SecondsBetweenWaves),
		                                FMissionTimerLifetimeSettings());
	}
	OnPreparationOver();
}

void USovietCounterattackMission::OnPreparationOver()
{
	UWorld* const World = GetWorld();
	if (M_Phase != EPhase::Preparing || World == nullptr || not IsValid(Settings))
	{
		return;
	}
	M_Phase = EPhase::Waves;
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

	SpawnGroundGroup(Center, ESovietCounterattackSpawn::Center, WaveNumber, Settings->Spawns.CenterSpawnLocation);
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

TArray<FVector> USovietCounterattackMission::BuildRoute(const FVector2D& Offset) const
{
	TArray<FVector> Route;
	if (not IsValid(Settings))
	{
		return Route;
	}
	TArray<FVector> Points = Settings->AttackRoute;
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
	const TArray<FVector> Route = BuildRoute(GetRouteOffset(Spawn));
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
	// Face the first route point that is not the spawn itself (the centre spawn sits on route point 1).
	FVector FacingTarget = GetHQLocation();
	for (const FVector& RoutePoint : Settings->AttackRoute)
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
	const FVector Pad = Settings->Spawns.LandingPadCenters[PadIndex];
	UWorld* const World = GetWorld();
	if (World == nullptr || not FMath::IsNearlyZero(Pad.Z))
	{
		return Pad;
	}
	using SovietCounterattackConstants::PadTraceHalfHeight;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SovietCounterattackPad), false);
	const FVector Start(Pad.X, Pad.Y, PadTraceHalfHeight);
	const FVector End(Pad.X, Pad.Y, -PadTraceHalfHeight);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	bool bProjected = false;
	const FVector Projected = RTSFunctionLibrary::GetLocationProjected(GetMissionManagerChecked(), Pad, true,
	                                                                   bProjected);
	return bProjected ? Projected : Pad;
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
	if (not M_TrainRuns.IsEmpty() || M_TrainQueue.IsEmpty() || World == nullptr || M_Phase == EPhase::Done)
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
