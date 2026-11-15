// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyWaveSpawner.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Components/BillboardComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/UnitGroupMiniMapMarker.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "EnemyWaveSpawnerVisualComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyWaveSpawnerConstants
{
	const FColor AttackColor(235, 64, 40);
	const FColor PatrolColor(40, 150, 255);
	const FColor ProblemColor(255, 210, 0);

	// The enemy controller and the game difficulty are set up during the first frames of a mission.
	constexpr float MinLaunchDelaySeconds = 0.2f;
	constexpr float LaunchRetryIntervalSeconds = 1.f;
	constexpr int32 MaxLaunchRetries = 30;

	// Units that share a spawn point are placed on rings around it.
	constexpr float SpawnSpreadSpacing = 450.f;
	constexpr int32 SpawnSpreadSlotsPerRing = 8;
	constexpr float SpawnNavProjectionHorizontalExtent = 400.f;
	constexpr float SpawnNavProjectionHalfHeight = 2000.f;

	constexpr float LabelWorldSize = 260.f;
	constexpr float LabelHeight = 700.f;
	// Lies flat and faces up so it reads from the top-down camera.
	const FRotator LabelRotation(90.f, 180.f, 0.f);
}

AEnemyWaveSpawner::AEnemyWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = M_Root;

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

	M_EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (M_EditorSprite)
	{
		M_EditorSprite->SetupAttachment(M_Root);
		M_EditorSprite->bIsScreenSizeScaled = true;
		M_EditorSprite->Sprite = M_AttackSprite;
	}

	M_EditorLabel = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (M_EditorLabel)
	{
		M_EditorLabel->SetupAttachment(M_Root);
		M_EditorLabel->SetRelativeLocation(FVector(0.f, 0.f, EnemyWaveSpawnerConstants::LabelHeight));
		M_EditorLabel->SetRelativeRotation(EnemyWaveSpawnerConstants::LabelRotation);
		M_EditorLabel->SetHorizontalAlignment(EHTA_Center);
		M_EditorLabel->SetVerticalAlignment(EVRTA_TextCenter);
		M_EditorLabel->SetWorldSize(EnemyWaveSpawnerConstants::LabelWorldSize);
		M_EditorLabel->SetHiddenInGame(true);
		M_EditorLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_EditorLabel->SetCastShadow(false);
	}

	M_EditorVisual = CreateEditorOnlyDefaultSubobject<UEnemyWaveSpawnerVisualComponent>(TEXT("Visual"));
	if (M_EditorVisual)
	{
		M_EditorVisual->SetupAttachment(M_Root);
	}
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
		World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
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

void AEnemyWaveSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshEditorVisuals();
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
	M_LaunchRetryCount = 0;
	ScheduleLaunch(FMath::Max(Timing.FirstWaveDelay, EnemyWaveSpawnerConstants::MinLaunchDelaySeconds));
}

void AEnemyWaveSpawner::StopSpawner()
{
	if (not bM_IsRunning)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
	}
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

void AEnemyWaveSpawner::ScheduleLaunch(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyWaveSpawner> WeakThis(this);
	World->GetTimerManager().SetTimer(M_LaunchTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->TryLaunchWave();
		}
	}), DelaySeconds, false);
}

void AEnemyWaveSpawner::TryLaunchWave()
{
	if (not bM_IsRunning)
	{
		return;
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	const bool bHasDifficulty = TryGetGameDifficulty(Difficulty);
	if (not IsValid(EnemyController) || not bHasDifficulty)
	{
		M_LaunchRetryCount++;
		if (M_LaunchRetryCount <= EnemyWaveSpawnerConstants::MaxLaunchRetries)
		{
			ScheduleLaunch(EnemyWaveSpawnerConstants::LaunchRetryIntervalSeconds);
			return;
		}
		if (not IsValid(EnemyController))
		{
			RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " found no enemy controller; it will not spawn.");
			FinishSpawner();
			return;
		}
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " found no game difficulty; spawning for Normal.");
	}
	if (not GetIsOwningBuildingAlive())
	{
		// The building was destroyed before this spawner started; its wave never comes.
		FinishSpawner();
		return;
	}
	LaunchWave(EnemyController, Difficulty);
}

bool AEnemyWaveSpawner::TryGetGameDifficulty(ERTSGameDifficulty& OutDifficulty) const
{
	const AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(this);
	if (IsValid(MissionManager) && MissionManager->GetIsGameDifficultyInitialized())
	{
		OutDifficulty = MissionManager->GetCurrentGameDifficulty().DifficultyLevel;
		return true;
	}
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (IsValid(GameInstance) && GameInstance->GetSelectedGameDifficulty().bIsInitialized)
	{
		OutDifficulty = GameInstance->GetSelectedGameDifficulty().DifficultyLevel;
		return true;
	}
	return false;
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
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LaunchTimerHandle);
	}
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
	if (Entry.UnitOptions.IsEmpty() || WorldSpawnPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Wave spawner " + GetName() + " has a unit entry without unit options; skipped.");
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
		Element.UnitOptions = Entry.UnitOptions;
		Element.SpawnLocation = GetSpreadSpawnLocation(WorldSpawnPoints[PointIndex], InOutUsesPerSpawnPoint[PointIndex]);
		InOutUsesPerSpawnPoint[PointIndex]++;
		OutElements.Add(Element);
	}
}

FVector AEnemyWaveSpawner::GetSpreadSpawnLocation(const FVector& SpawnPoint, const int32 UseIndex) const
{
	using namespace EnemyWaveSpawnerConstants;
	if (UseIndex <= 0)
	{
		return ProjectToNavigationIfPossible(SpawnPoint);
	}
	const int32 Slot = UseIndex - 1;
	const int32 Ring = 1 + Slot / SpawnSpreadSlotsPerRing;
	const float SlotAngleDegrees = 360.f / SpawnSpreadSlotsPerRing;
	// Every other ring is turned half a slot so its units stand between those of the ring inside it.
	const float AngleDegrees = (Slot % SpawnSpreadSlotsPerRing) * SlotAngleDegrees + (Ring % 2) * SlotAngleDegrees * 0.5f;
	const FVector Offset = FRotator(0.f, AngleDegrees, 0.f).Vector() * (Ring * SpawnSpreadSpacing);
	return ProjectToNavigationIfPossible(SpawnPoint + Offset);
}

FVector AEnemyWaveSpawner::ProjectToNavigationIfPossible(const FVector& Location) const
{
	using namespace EnemyWaveSpawnerConstants;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return Location;
	}
	FNavLocation ProjectedLocation;
	const FVector Extent(SpawnNavProjectionHorizontalExtent, SpawnNavProjectionHorizontalExtent,
	                     SpawnNavProjectionHalfHeight);
	if (not NavigationSystem->ProjectPointToNavigation(Location, ProjectedLocation, Extent))
	{
		return Location;
	}
	return ProjectedLocation.Location;
}

TArray<FVector> AEnemyWaveSpawner::GetWorldPoints(const TArray<FVector>& LocalPoints) const
{
	TArray<FVector> WorldPoints;
	WorldPoints.Reserve(LocalPoints.Num());
	const FTransform& ActorTransform = GetActorTransform();
	for (const FVector& LocalPoint : LocalPoints)
	{
		WorldPoints.Add(ActorTransform.TransformPosition(LocalPoint));
	}
	return WorldPoints;
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

void AEnemyWaveSpawner::RefreshEditorVisuals()
{
#if WITH_EDITORONLY_DATA
	const bool bIsPatrol = Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave;
	if (M_EditorSprite)
	{
		M_EditorSprite->SetSprite(bIsPatrol ? M_PatrolSprite : M_AttackSprite);
	}
	if (M_EditorLabel)
	{
		const bool bHasProblem = not GetConfigurationProblem().IsEmpty();
		M_EditorLabel->SetText(FText::FromString(GetEditorLabelText()));
		M_EditorLabel->SetTextRenderColor(bHasProblem ? EnemyWaveSpawnerConstants::ProblemColor : GetBehaviourColor());
	}
	if (M_EditorVisual)
	{
		FEnemyWaveSpawnerVisualData VisualData;
		FillVisualData(VisualData);
		M_EditorVisual->SetVisualData(VisualData);
	}
#endif
}

FColor AEnemyWaveSpawner::GetBehaviourColor() const
{
	return Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave
		       ? EnemyWaveSpawnerConstants::PatrolColor
		       : EnemyWaveSpawnerConstants::AttackColor;
}

FString AEnemyWaveSpawner::GetEditorLabelText() const
{
	const FString Kind = Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave ? TEXT("PATROL") : TEXT("ATTACK");
	const FString Name = SpawnerId.IsNone() ? FString(TEXT("(no id)")) : SpawnerId.ToString();
	FString Label = Kind + TEXT(" ") + Name;
	const FString Problem = GetConfigurationProblem();
	if (not Problem.IsEmpty())
	{
		Label += TEXT("\n! ") + Problem;
	}
	return Label;
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
	if (Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave && Waypoints.Num() < 2)
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

void AEnemyWaveSpawner::FillVisualData(FEnemyWaveSpawnerVisualData& OutVisualData) const
{
	OutVisualData.Color = GetBehaviourColor();
	OutVisualData.Origin = GetActorLocation();
	OutVisualData.SpawnPoints = GetWorldPoints(SpawnPoints);
	OutVisualData.Waypoints = GetWorldPoints(Waypoints);
	OutVisualData.bIsPatrol = Behaviour == EEnemyWaveSpawnerBehaviour::PatrolWave;
	OutVisualData.GuardRadius = OutVisualData.bIsPatrol ? PatrolSettings.Patrol.GuardSphereRadius : 0.f;
	FAttackWaveRequest DirectionRequest;
	BuildRequestMovement(OutVisualData.Waypoints, DirectionRequest);
	OutVisualData.bHasFinalDirection = not OutVisualData.bIsPatrol;
	OutVisualData.FinalDirection = DirectionRequest.FinalWaypointDirection.Vector();
}
