// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyWaveSpawner.h"

#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/UnitGroupMiniMapMarker.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyWaveSpawnerConstants
{
	// The enemy controller and the game difficulty are set up during the first frames of a mission.
	constexpr float MinLaunchDelaySeconds = 0.2f;

	// Editor drawing.
	constexpr float SpawnPointRadius = 120.f;
	constexpr float SpawnPointStemHeight = 400.f;
	constexpr float WaypointRadius = 70.f;
	constexpr float PathLiftZ = 60.f;
	constexpr float PathThickness = 6.f;
	constexpr float OriginLinkThickness = 1.5f;
	constexpr float PatrolDashSize = 120.f;
	constexpr float GuardRingThickness = 2.f;
	constexpr float FinalDirectionLength = 900.f;
}

namespace
{
	FVector LiftPathPoint(const FVector& Point)
	{
		return Point + FVector(0.f, 0.f, EnemyWaveSpawnerConstants::PathLiftZ);
	}
}

AEnemyWaveSpawner::AEnemyWaveSpawner()
{
#if WITH_EDITORONLY_DATA
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> AttackSprite;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> PatrolSprite;

		FConstructorStatics()
			: AttackSprite(TEXT("/Engine/EditorResources/S_TargetPoint"))
			, PatrolSprite(TEXT("/Engine/EditorResources/S_NavP"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
	M_AttackSprite = ConstructorStatics.AttackSprite.Get();
	M_PatrolSprite = ConstructorStatics.PatrolSprite.Get();
#endif
}

void AEnemyWaveSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (Timing.bStartOnBeginPlay)
	{
		StartSpawner();
	}
}

void AEnemyWaveSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_MinimapTimerHandle);
	}
	// Cleared first so the OnWaveEnded fired by StopWave does not broadcast OnSpawnerFinished during teardown.
	const int32 WaveIdToStop = M_ActiveWaveId;
	M_ActiveWaveId = INDEX_NONE;
	bM_IsRunning = false;
	// No validator here: the enemy controller may already be gone while the world tears down.
	AEnemyController* EnemyController = M_EnemyController.Get();
	if (WaveIdToStop != INDEX_NONE && IsValid(EnemyController))
	{
		EnemyController->StopWave(WaveIdToStop);
	}
	RemoveAllMinimapMarkers();
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------
// Start / stop
// ------------------------------------------------------------------

void AEnemyWaveSpawner::StartSpawner()
{
	if (bM_IsRunning)
	{
		return;
	}
	bM_IsRunning = true;
	bM_HasFinished = false;
	WaitForEnemyAI(FMath::Max(Timing.FirstWaveDelay, EnemyWaveSpawnerConstants::MinLaunchDelaySeconds));
}

void AEnemyWaveSpawner::StopSpawner()
{
	if (not bM_IsRunning)
	{
		return;
	}
	CancelWaitForEnemyAI();
	if (M_ActiveWaveId != INDEX_NONE && GetIsValidEnemyController())
	{
		// Fires OnWaveEnded, which finishes the spawner.
		M_EnemyController->StopWave(M_ActiveWaveId);
	}
	FinishSpawner();
}

AEnemyWaveSpawner* AEnemyWaveSpawner::FindEnemyWaveSpawner(const UObject* WorldContextObject,
                                                           const FName SpawnerIdToFind)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World) || SpawnerIdToFind.IsNone())
	{
		RTSFunctionLibrary::ReportError("FindEnemyWaveSpawner needs a valid world context and a spawner ID.");
		return nullptr;
	}
	for (TActorIterator<AEnemyWaveSpawner> It(World); It; ++It)
	{
		if (It->SpawnerId == SpawnerIdToFind)
		{
			return *It;
		}
	}
	RTSFunctionLibrary::ReportError("FindEnemyWaveSpawner found no wave spawner with ID: " + SpawnerIdToFind.ToString());
	return nullptr;
}

// ------------------------------------------------------------------
// Launch
// ------------------------------------------------------------------

void AEnemyWaveSpawner::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	if (not bM_IsRunning)
	{
		return;
	}
	if (not GetIsOwningBuildingAlive())
	{
		// The building was destroyed before this spawner started; its wave never comes.
		FinishSpawner();
		return;
	}
	LaunchWave(EnemyController, Difficulty);
}

void AEnemyWaveSpawner::OnEnemyAINeverReady()
{
	FinishSpawner();
}

bool AEnemyWaveSpawner::GetIsOwningBuildingAlive() const
{
	if (Ownership.WaveType == EEnemyWaveType::Wave_NoOwningBuilding)
	{
		return true;
	}
	// A destroyed building is nulled by garbage collection, so "not set" and "destroyed" look the same here;
	// a building that was never set is flagged on the spawner's editor label instead.
	return RTSFunctionLibrary::RTSIsValid(Ownership.OwningBuilding);
}

void AEnemyWaveSpawner::LaunchWave(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	FAttackWaveRequest Request;
	if (not BuildWaveRequest(Difficulty, Request))
	{
		FinishSpawner();
		return;
	}
	M_EnemyController = EnemyController;
	M_ActiveWaveId = INDEX_NONE;
	const int32 WaveId = EnemyController->StartWaveFromRequest(Request);
	if (WaveId == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " could not start its wave; see the error above.");
		FinishSpawner();
		return;
	}
	if (not bM_IsRunning)
	{
		// The first iteration ran inside StartWaveFromRequest and already ended the wave.
		return;
	}
	M_ActiveWaveId = WaveId;
}

void AEnemyWaveSpawner::FinishSpawner()
{
	CancelWaitForEnemyAI();
	if (not bM_IsRunning)
	{
		return;
	}
	bM_IsRunning = false;
	bM_HasFinished = true;
	M_ActiveWaveId = INDEX_NONE;
	OnSpawnerFinished.Broadcast(this);
}

bool AEnemyWaveSpawner::BuildWaveRequest(const ERTSGameDifficulty Difficulty, FAttackWaveRequest& OutRequest)
{
	const TArray<FVector> WorldSpawnPoints = GetWorldPoints(SpawnPoints);
	if (WorldSpawnPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " has no spawn points.");
		return false;
	}
	OutRequest.WaveElements = BuildWaveElements(Difficulty, WorldSpawnPoints);
	if (OutRequest.WaveElements.IsEmpty())
	{
		// Allowed: a spawner may only have extra units for higher difficulties.
		UE_LOG(LogTemp, Log, TEXT("Wave spawner %s has no units on this difficulty; it does not spawn."), *GetName());
		return false;
	}
	BuildRequestTiming(OutRequest);
	BuildRequestMovement(GetWorldPoints(Waypoints), OutRequest);
	BuildRequestOwnership(OutRequest);
	OutRequest.OnUnitsSpawned.BindUObject(this, &AEnemyWaveSpawner::HandleWaveUnitsSpawned);
	OutRequest.OnWaveEnded.BindUObject(this, &AEnemyWaveSpawner::HandleWaveEnded);
	return true;
}

void AEnemyWaveSpawner::BuildRequestTiming(FAttackWaveRequest& OutRequest) const
{
	switch (Timing.Repeat)
	{
	case EEnemyWaveSpawnerRepeat::Once:
		OutRequest.IterationCount = 1;
		break;
	case EEnemyWaveSpawnerRepeat::FixedCount:
		OutRequest.IterationCount = FMath::Max(Timing.WaveCount, 1);
		break;
	case EEnemyWaveSpawnerRepeat::Infinite:
		OutRequest.IterationCount = INDEX_NONE;
		break;
	}
	// The spawner already waited FirstWaveDelay before launching.
	OutRequest.FirstIterationDelay = 0.f;
	OutRequest.WaveInterval = Timing.WaveInterval;
	OutRequest.IntervalVarianceFraction = Timing.IntervalVarianceFraction;
}

void AEnemyWaveSpawner::BuildRequestMovement(const TArray<FVector>& WorldWaypoints, FAttackWaveRequest& OutRequest) const
{
	OutRequest.Waypoints = WorldWaypoints;
	if (Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave)
	{
		OutRequest.Movement = EAttackWaveMovement::RandomPatrolWithAttackMove;
		OutRequest.MaxFormationWidth = PatrolSettings.Formation.MaxFormationWidth;
		OutRequest.FormationOffsetMultiplier = PatrolSettings.Formation.FormationOffsetMultiplier;
		OutRequest.PatrolSettings = PatrolSettings.Patrol;
		return;
	}
	OutRequest.Movement = EAttackWaveMovement::AttackMove;
	OutRequest.MaxFormationWidth = AttackSettings.Formation.MaxFormationWidth;
	OutRequest.FormationOffsetMultiplier = AttackSettings.Formation.FormationOffsetMultiplier;
	OutRequest.AttackMoveSettings = AttackSettings.CombatSupport;
	OutRequest.FinalWaypointDirection = AttackSettings.FinalWaypointDirection;
	if (AttackSettings.bFaceAlongLastLeg && not WorldWaypoints.IsEmpty())
	{
		const FVector LegStart = WorldWaypoints.Num() >= 2 ? WorldWaypoints[WorldWaypoints.Num() - 2] : GetActorLocation();
		OutRequest.FinalWaypointDirection = FRotator(0.f, (WorldWaypoints.Last() - LegStart).Rotation().Yaw, 0.f);
	}
}

void AEnemyWaveSpawner::BuildRequestOwnership(FAttackWaveRequest& OutRequest) const
{
	OutRequest.WaveType = Ownership.WaveType;
	OutRequest.WaveCreator = Ownership.OwningBuilding.Get();
	OutRequest.PerAffectingBuildingTimerFraction = Ownership.PerDestroyedGeneratorIntervalFraction;
	for (AActor* Generator : Ownership.PowerGenerators)
	{
		// A generator destroyed before launch would fail the wave controller's validation of the whole wave.
		if (RTSFunctionLibrary::RTSIsValid(Generator))
		{
			OutRequest.WaveTimerAffectingBuildings.Add(Generator);
		}
	}
}

TArray<const TArray<FEnemyWaveSpawnerUnitEntry>*> AEnemyWaveSpawner::GetUnitListsForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	FRTSGameDifficulty GameDifficulty;
	GameDifficulty.DifficultyLevel = Difficulty;
	TArray<const TArray<FEnemyWaveSpawnerUnitEntry>*> UnitLists = {&Units.BaseUnits};
	if (GameDifficulty.IsNormalOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsNormal);
	}
	if (GameDifficulty.IsHardOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsHard);
	}
	// Ironman is above Brutal, so it gets the Brutal extras too.
	if (GameDifficulty.IsBrutalOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsBrutal);
	}
	return UnitLists;
}

TArray<FAttackWaveElement> AEnemyWaveSpawner::BuildWaveElements(
	const ERTSGameDifficulty Difficulty,
	const TArray<FVector>& WorldSpawnPoints) const
{
	TArray<FAttackWaveElement> Elements;
	TArray<int32> UsesPerSpawnPoint;
	UsesPerSpawnPoint.Init(0, WorldSpawnPoints.Num());
	int32 NextSpreadPoint = 0;
	const TArray<const TArray<FEnemyWaveSpawnerUnitEntry>*> UnitLists = GetUnitListsForDifficulty(Difficulty);
	for (const TArray<FEnemyWaveSpawnerUnitEntry>* UnitList : UnitLists)
	{
		for (const FEnemyWaveSpawnerUnitEntry& Entry : *UnitList)
		{
			AppendEntryElements(Entry, WorldSpawnPoints, UsesPerSpawnPoint, NextSpreadPoint, Elements);
		}
	}
	return Elements;
}

void AEnemyWaveSpawner::AppendEntryElements(
	const FEnemyWaveSpawnerUnitEntry& Entry,
	const TArray<FVector>& WorldSpawnPoints,
	TArray<int32>& InOutUsesPerSpawnPoint,
	int32& InOutNextSpreadPoint,
	TArray<FAttackWaveElement>& OutElements) const
{
	if (not EnemyPlacement::GetAreUnitsSet(Entry.UnitOptions) || WorldSpawnPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " has a unit entry with no or unpicked units; skipped.");
		return;
	}
	const bool bHasFixedPoint = WorldSpawnPoints.IsValidIndex(Entry.SpawnPointIndex);
	if (not bHasFixedPoint && Entry.SpawnPointIndex != INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " has a unit entry with spawn point index " +
			FString::FromInt(Entry.SpawnPointIndex) + " that does not exist; its units are spread instead.");
	}
	for (int32 UnitIndex = 0; UnitIndex < Entry.Count; ++UnitIndex)
	{
		int32 PointIndex = Entry.SpawnPointIndex;
		if (not bHasFixedPoint)
		{
			PointIndex = InOutNextSpreadPoint % WorldSpawnPoints.Num();
			InOutNextSpreadPoint++;
		}
		FAttackWaveElement Element;
		Element.UnitOptions = EnemyPlacement::ToTrainingOptions(Entry.UnitOptions);
		Element.SpawnLocation = GetSpreadSpawnLocation(WorldSpawnPoints[PointIndex], InOutUsesPerSpawnPoint[PointIndex]);
		InOutUsesPerSpawnPoint[PointIndex]++;
		OutElements.Add(Element);
	}
}

FVector AEnemyWaveSpawner::GetSpreadSpawnLocation(const FVector& SpawnPoint, const int32 UseIndex) const
{
	return EnemyPlacement::ProjectToNavigationIfPossible(GetWorld(),
	                                                     SpawnPoint + EnemyPlacement::GetRingSpreadOffset(UseIndex));
}

bool AEnemyWaveSpawner::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_EnemyController",
		"AEnemyWaveSpawner::GetIsValidEnemyController",
		this);
	return false;
}

// ------------------------------------------------------------------
// Wave callbacks
// ------------------------------------------------------------------

void AEnemyWaveSpawner::HandleWaveUnitsSpawned(const int32 WaveId, const TArray<AActor*>& SpawnedUnits)
{
	if (M_ActiveWaveId != INDEX_NONE && WaveId != M_ActiveWaveId)
	{
		return;
	}
	M_SpawnedWaveCount++;
	if (Minimap.bShowOnMinimap)
	{
		AddMinimapMarker(SpawnedUnits);
	}
	OnWaveUnitsSpawned.Broadcast(this, SpawnedUnits);
}

void AEnemyWaveSpawner::HandleWaveEnded(const int32 WaveId)
{
	// M_ActiveWaveId is still INDEX_NONE when the first iteration ends the wave inside StartWaveFromRequest.
	if (M_ActiveWaveId != INDEX_NONE && WaveId != M_ActiveWaveId)
	{
		return;
	}
	M_ActiveWaveId = INDEX_NONE;
	FinishSpawner();
}

// ------------------------------------------------------------------
// Minimap
// ------------------------------------------------------------------

void AEnemyWaveSpawner::AddMinimapMarker(const TArray<AActor*>& SpawnedUnits)
{
	FEnemyWaveSpawnerMinimapMarker Marker;
	for (AActor* Unit : SpawnedUnits)
	{
		if (IsValid(Unit))
		{
			Marker.Units.Add(Unit);
		}
	}
	if (not RTSUnitGroupMiniMapMarker::UpdateFromUnits(Marker.Units, Marker.Location, Marker.HeadingAnchor,
	                                                   Marker.HeadingYaw))
	{
		return;
	}
	Marker.HeadingAnchor = Marker.Location;
	const TArray<FVector> WorldWaypoints = GetWorldPoints(Waypoints);
	if (not WorldWaypoints.IsEmpty())
	{
		Marker.HeadingYaw = (WorldWaypoints[0] - Marker.Location).Rotation().Yaw;
	}
	const FString IdBase = SpawnerId.IsNone() ? GetName() : SpawnerId.ToString();
	Marker.IconId = FName(*FString::Printf(TEXT("WaveSpawner_%s_%d"), *IdBase, M_NextMarkerIndex++));
	RTSUnitGroupMiniMapMarker::AddIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw, Minimap.HeadingIcon,
	                                    Minimap.BadgeIcon);
	M_MinimapMarkers.Add(Marker);
	EnsureMinimapTimer();
}

void AEnemyWaveSpawner::EnsureMinimapTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_MinimapTimerHandle))
	{
		return;
	}
	TWeakObjectPtr<AEnemyWaveSpawner> WeakThis(this);
	World->GetTimerManager().SetTimer(M_MinimapTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateMinimapMarkers();
		}
	}), RTSUnitGroupMiniMapMarker::UpdateIntervalSeconds, true);
}

void AEnemyWaveSpawner::UpdateMinimapMarkers()
{
	for (int32 MarkerIndex = M_MinimapMarkers.Num() - 1; MarkerIndex >= 0; --MarkerIndex)
	{
		FEnemyWaveSpawnerMinimapMarker& Marker = M_MinimapMarkers[MarkerIndex];
		if (RTSUnitGroupMiniMapMarker::UpdateFromUnits(Marker.Units, Marker.Location, Marker.HeadingAnchor,
		                                               Marker.HeadingYaw))
		{
			RTSUnitGroupMiniMapMarker::MoveIcons(this, Marker.IconId, Marker.Location, Marker.HeadingYaw);
			continue;
		}
		RTSUnitGroupMiniMapMarker::RemoveIcons(this, Marker.IconId);
		M_MinimapMarkers.RemoveAt(MarkerIndex);
	}
	if (M_MinimapMarkers.IsEmpty())
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_MinimapTimerHandle);
		}
	}
}

void AEnemyWaveSpawner::RemoveAllMinimapMarkers()
{
	for (const FEnemyWaveSpawnerMinimapMarker& Marker : M_MinimapMarkers)
	{
		RTSUnitGroupMiniMapMarker::RemoveIcons(this, Marker.IconId);
	}
	M_MinimapMarkers.Empty();
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

FColor AEnemyWaveSpawner::GetPlacementColor() const
{
	return GetIsPatrol() ? EnemyPlacementColors::PatrolWave : EnemyPlacementColors::AttackWave;
}

FString AEnemyWaveSpawner::GetPlacementKindName() const
{
	return GetIsPatrol() ? TEXT("PATROL") : TEXT("ATTACK");
}

UTexture2D* AEnemyWaveSpawner::GetEditorSpriteTexture() const
{
#if WITH_EDITORONLY_DATA
	return GetIsPatrol() ? M_PatrolSprite.Get() : M_AttackSprite.Get();
#else
	return nullptr;
#endif
}

FString AEnemyWaveSpawner::GetConfigurationProblem() const
{
	if (SpawnPoints.IsEmpty())
	{
		return TEXT("no spawn points");
	}
	if (Units.BaseUnits.IsEmpty() && Units.ExtraUnitsNormal.IsEmpty() && Units.ExtraUnitsHard.IsEmpty()
		&& Units.ExtraUnitsBrutal.IsEmpty())
	{
		return TEXT("no units");
	}
	if (GetHasUnsetUnitEntry())
	{
		return TEXT("unit entry with no unit picked");
	}
	if (GetIsPatrol() && Waypoints.Num() < 2)
	{
		return TEXT("patrol needs 2+ waypoints");
	}
	if (Waypoints.IsEmpty())
	{
		return TEXT("no waypoints");
	}
	if (Ownership.WaveType == EEnemyWaveType::Wave_None)
	{
		return TEXT("wave type None");
	}
	if (Ownership.WaveType != EEnemyWaveType::Wave_NoOwningBuilding && Ownership.OwningBuilding == nullptr)
	{
		return TEXT("no owning building");
	}
	return FString();
}

bool AEnemyWaveSpawner::GetHasUnsetUnitEntry() const
{
	const TArray<FEnemyWaveSpawnerUnitEntry>* UnitLists[] = {
		&Units.BaseUnits, &Units.ExtraUnitsNormal, &Units.ExtraUnitsHard, &Units.ExtraUnitsBrutal
	};
	for (const TArray<FEnemyWaveSpawnerUnitEntry>* UnitList : UnitLists)
	{
		for (const FEnemyWaveSpawnerUnitEntry& Entry : *UnitList)
		{
			if (not EnemyPlacement::GetAreUnitsSet(Entry.UnitOptions))
			{
				return true;
			}
		}
	}
	return false;
}

FVector AEnemyWaveSpawner::GetSpawnCentre(const TArray<FVector>& WorldSpawnPoints) const
{
	if (WorldSpawnPoints.IsEmpty())
	{
		return GetActorLocation();
	}
	FVector Sum = FVector::ZeroVector;
	for (const FVector& Point : WorldSpawnPoints)
	{
		Sum += Point;
	}
	return Sum / WorldSpawnPoints.Num();
}

void AEnemyWaveSpawner::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	const TArray<FVector> WorldSpawnPoints = GetWorldPoints(SpawnPoints);
	const TArray<FVector> WorldWaypoints = GetWorldPoints(Waypoints);
	AddSpawnPointShapes(WorldSpawnPoints, OutSummary, OutDetail);
	if (GetIsPatrol())
	{
		AddPatrolLoopShapes(WorldSpawnPoints, WorldWaypoints, OutSummary, OutDetail);
		return;
	}
	AddAttackPathShapes(WorldSpawnPoints, WorldWaypoints, OutSummary, OutDetail);
}

void AEnemyWaveSpawner::AddSpawnPointShapes(const TArray<FVector>& WorldSpawnPoints, FEnemyPlacementShapes& OutSummary,
                                            FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyWaveSpawnerConstants;
	const FColor Color = GetPlacementColor();
	for (const FVector& SpawnPoint : WorldSpawnPoints)
	{
		OutSummary.Spheres.Emplace(SpawnPointRadius, SpawnPoint, FLinearColor(Color));
		OutSummary.Lines.Emplace(SpawnPoint, SpawnPoint + FVector(0.f, 0.f, SpawnPointStemHeight), Color,
		                         PathThickness);
		OutDetail.Lines.Emplace(GetActorLocation(), SpawnPoint, Color, OriginLinkThickness);
	}
}

void AEnemyWaveSpawner::AddAttackPathShapes(const TArray<FVector>& WorldSpawnPoints,
                                            const TArray<FVector>& WorldWaypoints, FEnemyPlacementShapes& OutSummary,
                                            FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyWaveSpawnerConstants;
	const FColor Color = GetPlacementColor();
	FVector Previous = GetSpawnCentre(WorldSpawnPoints);
	for (const FVector& Waypoint : WorldWaypoints)
	{
		OutSummary.ArrowLines.Emplace(LiftPathPoint(Previous), LiftPathPoint(Waypoint), Color);
		OutSummary.Lines.Emplace(LiftPathPoint(Previous), LiftPathPoint(Waypoint), Color, PathThickness);
		OutDetail.Spheres.Emplace(WaypointRadius, Waypoint, FLinearColor(Color),
		                          FDebugRenderSceneProxy::EDrawType::WireMesh);
		Previous = Waypoint;
	}
	if (WorldWaypoints.IsEmpty())
	{
		return;
	}
	FAttackWaveRequest DirectionRequest;
	BuildRequestMovement(WorldWaypoints, DirectionRequest);
	const FVector Final = LiftPathPoint(WorldWaypoints.Last());
	OutDetail.ArrowLines.Emplace(Final, Final + DirectionRequest.FinalWaypointDirection.Vector() * FinalDirectionLength,
	                             EnemyPlacementColors::Facing);
}

void AEnemyWaveSpawner::AddPatrolLoopShapes(const TArray<FVector>& WorldSpawnPoints,
                                            const TArray<FVector>& WorldWaypoints, FEnemyPlacementShapes& OutSummary,
                                            FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyWaveSpawnerConstants;
	const FColor Color = GetPlacementColor();
	if (not WorldWaypoints.IsEmpty())
	{
		// The patrol walks from its spawn to its first point; the points are then visited at random.
		OutDetail.ArrowLines.Emplace(LiftPathPoint(GetSpawnCentre(WorldSpawnPoints)),
		                             LiftPathPoint(WorldWaypoints[0]), Color);
	}
	const float GuardRadius = PatrolSettings.Patrol.GuardSphereRadius;
	for (int32 PointIndex = 0; PointIndex < WorldWaypoints.Num(); ++PointIndex)
	{
		const FVector& Point = WorldWaypoints[PointIndex];
		const FVector& Next = WorldWaypoints[(PointIndex + 1) % WorldWaypoints.Num()];
		if (WorldWaypoints.Num() > 1)
		{
			OutSummary.DashedLines.Emplace(LiftPathPoint(Point), LiftPathPoint(Next), Color, PatrolDashSize);
		}
		OutSummary.Spheres.Emplace(WaypointRadius, Point, FLinearColor(Color),
		                           FDebugRenderSceneProxy::EDrawType::WireMesh);
		OutDetail.AddCircle(LiftPathPoint(Point), GuardRadius, Color, GuardRingThickness);
	}
}
