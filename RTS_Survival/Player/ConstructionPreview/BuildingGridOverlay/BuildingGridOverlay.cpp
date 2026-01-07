// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BuildingGridOverlay.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/DeveloperSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

namespace GridOverlay_Constants
{
	static constexpr int32 MaxAllowedGridSize = 1024;
}

ABuildingGridOverlay::ABuildingGridOverlay()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Root component
	TObjectPtr<USceneComponent> Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Instanced grid
	M_GridISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GridISM"));
	M_GridISM->SetupAttachment(RootComponent);
	M_GridISM->SetMobility(EComponentMobility::Movable);
	M_GridISM->SetCastShadow(false);
	M_GridISM->bReceivesDecals = false;
	M_GridISM->SetCanEverAffectNavigation(false);
	M_GridISM->NumCustomDataFloats = 2; // [0]=state (Vacant/Valid/Invalid), [1]=ring distance

	EnsureCollisionSetup();
	TryBuildFromConstructor();
}

void ABuildingGridOverlay::BeginPlay()
{
	Super::BeginPlay();

	if (GetIsValidGridISM() && M_GridISM->GetInstanceCount() == 0)
	{
		RebuildGrid();
	}
	UpdateMaterialParametersWithSettings();
}

void ABuildingGridOverlay::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	EnsureCollisionSetup();
	RebuildGrid();
}

void ABuildingGridOverlay::RebuildGrid()
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	if (not ValidateSettings())
	{
		ClearInstances();
		return;
	}

	if (M_GridISM->GetStaticMesh() != M_GridOverlaySettings.CellMesh)
	{
		M_GridISM->SetStaticMesh(M_GridOverlaySettings.CellMesh);
	}

	BuildInstances();
	EnsureDynamicMaterial();
}

void ABuildingGridOverlay::UpdateOverlaps_ForAllTiles()
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	const int32 InstanceCount = M_GridISM->GetInstanceCount();
	if (InstanceCount <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: UpdateOverlaps_ForAllTiles -> no instances."));
		return;
	}

	for (int32 Index = 0; Index < InstanceCount; ++Index)
	{
		UpdateSingleInstanceOverlapState(Index);
	}

	FlushCustomDataRenderState();

	// Draw debug labels if we have a footprint:
	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		if (M_ConstructPreviewIndices.Num() > 0)
		{
			Debug_DrawConstructionPreviewSquares(0.25f);
		}
	}
}

void ABuildingGridOverlay::UpdateOverlaps_ForTiles(const TArray<int32>& InstanceIndices)
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	const int32 InstanceCount = M_GridISM->GetInstanceCount();
	if (InstanceCount <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: UpdateOverlaps_ForTiles -> no instances."));
		return;
	}

	for (const int32 Index : InstanceIndices)
	{
		if (Index < 0 || Index >= InstanceCount)
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("GridOverlay: Instance index %d out of range [0..%d)."), Index, InstanceCount));
			continue;
		}
		UpdateSingleInstanceOverlapState(Index);
	}

	FlushCustomDataRenderState();

	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		if (M_ConstructPreviewIndices.Num() > 0)
		{
			Debug_DrawConstructionPreviewSquares(0.25f);
		}
	}
}

void ABuildingGridOverlay::SetExtraOverlapActorsToIgnore(const TArray<AActor*>& ActorsToIgnore)
{
	M_ExtraOverlapActorsToIgnore = ActorsToIgnore;
}

void ABuildingGridOverlay::SetConstructionPreviewSquares(const TArray<int32>& InstanceIndices)
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	M_ConstructPreviewIndices = InstanceIndices;
	M_ConstructPreviewIndexSet.Empty(InstanceIndices.Num());
	for (int32 Idx : InstanceIndices)
	{
		M_ConstructPreviewIndexSet.Add(Idx);
	}

	// Initialize visual state: set all to Vacant, then footprint to Valid (green)
	const int32 Count = M_GridISM->GetInstanceCount();
	for (int32 i = 0; i < Count; ++i)
	{
		M_GridISM->SetCustomDataValue(i, 0, TileTypeToCustomData(EGridOverlayTileType::Vacant), false);
	}
	for (int32 Idx : InstanceIndices)
	{
		if (Idx >= 0 && Idx < Count)
		{
			M_GridISM->SetCustomDataValue(Idx, 0, TileTypeToCustomData(EGridOverlayTileType::Valid), false);
		}
	}
	FlushCustomDataRenderState();

	// Recompute overlaps for the footprint set right away to override to Invalid if needed.
	UpdateOverlaps_ForTiles(InstanceIndices);
	// Compute ring distances from the footprint and write them to custom data index 1
	ComputeAndWriteRings_FromFootprint();
	// Update the falloff parameters in the material
	UpdateFallOffFootprintParameters();
}

bool ABuildingGridOverlay::AreConstructionPreviewSquaresValid() const
{
	// Valid if none of the footprint indices are overlapped.
	for (int32 Idx : M_ConstructPreviewIndices)
	{
		if (M_OverlappedIndexSet.Contains(Idx))
		{
			return false;
		}
	}
	return true;
}

// ---------- core safety / validation ----------

bool ABuildingGridOverlay::GetIsValidGridISM() const
{
	if (IsValid(M_GridISM))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_GridISM"), TEXT("GetIsValidGridISM"), this);
	return false;
}

bool ABuildingGridOverlay::ValidateSettings() const
{
	if (!(M_GridOverlaySettings.CellSize > KINDA_SMALL_NUMBER))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("GridOverlay: Invalid CellSize %.3f. Must be > 0."),
			                M_GridOverlaySettings.CellSize));
		return false;
	}

	if (M_GridOverlaySettings.MaxSize <= 0 ||
		M_GridOverlaySettings.MaxSize > GridOverlay_Constants::MaxAllowedGridSize)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("GridOverlay: Invalid MaxSize %d. Expected [1..%d]."),
			                M_GridOverlaySettings.MaxSize,
			                GridOverlay_Constants::MaxAllowedGridSize));
		return false;
	}

	if (M_GridOverlaySettings.InstanceGap < 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: InstanceGap must be >= 0."));
		return false;
	}

	if (!IsValid(M_GridOverlaySettings.CellMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: CellMesh is null. Assign a flat square tile mesh."));
		return false;
	}

	float MeshX = 0.f, MeshY = 0.f;
	if (!GetMeshXYSize(M_GridOverlaySettings.CellMesh, MeshX, MeshY))
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: Failed to read mesh bounds for CellMesh."));
		return false;
	}
	if (MeshX <= KINDA_SMALL_NUMBER || MeshY <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("GridOverlay: CellMesh has degenerate XY bounds (%.3f x %.3f)."), MeshX, MeshY));
		return false;
	}

	// Ensure the step (cell + gap) is positive
	const float Step = M_GridOverlaySettings.CellSize + M_GridOverlaySettings.InstanceGap;
	if (!(Step > KINDA_SMALL_NUMBER))
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: (CellSize + InstanceGap) must be > 0."));
		return false;
	}

	// Optional heads-up if shrink would eliminate the box (we still clamp later)
	if (M_PerTileShrink * 2.f >= Step)
	{
		RTSFunctionLibrary::PrintString(
			TEXT("GridOverlay: Warning — PerTileShrink >= half of (CellSize + InstanceGap); "
				"overlap boxes will be minimal."), FColor::Yellow);
	}

	return true;
}

bool ABuildingGridOverlay::GetMeshXYSize(const UStaticMesh* InMesh, float& OutSizeX, float& OutSizeY) const
{
	if (not IsValid(InMesh))
	{
		OutSizeX = 0.f;
		OutSizeY = 0.f;
		return false;
	}

	const FBoxSphereBounds Bounds = InMesh->GetBounds();
	const FVector FullSize = Bounds.BoxExtent * 2.f;
	OutSizeX = FMath::Abs(FullSize.X);
	OutSizeY = FMath::Abs(FullSize.Y);
	return true;
}

// ---------- build / material ----------

void ABuildingGridOverlay::ClearInstances() const
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	M_GridISM->ClearInstances();
}

void ABuildingGridOverlay::BuildInstances()
{
	if (!GetIsValidGridISM()) { return; }

	if (M_GridISM->GetStaticMesh() != M_GridOverlaySettings.CellMesh)
	{
		M_GridISM->SetStaticMesh(M_GridOverlaySettings.CellMesh);
	}

	ClearInstances();

	float MeshSizeX = 0.f, MeshSizeY = 0.f;
	if (!GetMeshXYSize(M_GridOverlaySettings.CellMesh, MeshSizeX, MeshSizeY))
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: Could not compute mesh XY size during BuildInstances."));
		return;
	}
	if (MeshSizeX <= KINDA_SMALL_NUMBER || MeshSizeY <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("GridOverlay: Degenerate mesh bounds X=%.3f Y=%.3f during BuildInstances."), MeshSizeX, MeshSizeY));
		return;
	}

	const float S = M_GridOverlaySettings.CellSize; // visual tile size
	const float Gap = M_GridOverlaySettings.InstanceGap; // spacing between tiles
	const float Step = S + Gap; // center-to-center spacing

	// Scale each instance so the visible quad is exactly CellSize (gap comes from Step, not scale)
	const FVector InstanceScale(S / MeshSizeX, S / MeshSizeY, 1.f);

	const int32 N = M_GridOverlaySettings.MaxSize;

	// World-aligned grid using the *step* (cell + gap)
	const FVector ActorWorld = GetActorLocation();
	const float HalfSpan = (static_cast<float>(N) - 1.f) * 0.5f;
	M_MinCellX = FMath::FloorToInt(ActorWorld.X / Step - HalfSpan);
	M_MinCellY = FMath::FloorToInt(ActorWorld.Y / Step - HalfSpan);

	M_GridISM->PreAllocateInstancesMemory(N * N);

	for (int32 Iy = 0; Iy < N; ++Iy)
	{
		for (int32 Ix = 0; Ix < N; ++Ix)
		{
			const int32 CellX = M_MinCellX + Ix;
			const int32 CellY = M_MinCellY + Iy;

			const float WorldX = static_cast<float>(CellX) * Step;
			const float WorldY = static_cast<float>(CellY) * Step;

			const FVector LocalOffset(WorldX - ActorWorld.X, WorldY - ActorWorld.Y, M_ZOffset);
			const FTransform InstanceXform(FRotator::ZeroRotator, LocalOffset, InstanceScale);

			const int32 InstanceIdx = M_GridISM->AddInstance(InstanceXform);

			M_GridISM->SetCustomDataValue(InstanceIdx, 0,
			                              TileTypeToCustomData(EGridOverlayTileType::Vacant), false);
			M_GridISM->SetCustomDataValue(InstanceIdx, 1, 0.f, false); // ring distance
		}
	}

	FlushCustomDataRenderState();
}

void ABuildingGridOverlay::TryBuildFromConstructor()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (not GetIsValidGridISM())
	{
		return;
	}

	if (not IsValid(M_GridOverlaySettings.CellMesh))
	{
		return;
	}

	if (not ValidateSettings())
	{
		return;
	}

	if (M_GridISM->GetStaticMesh() != M_GridOverlaySettings.CellMesh)
	{
		M_GridISM->SetStaticMesh(M_GridOverlaySettings.CellMesh);
	}

	BuildInstances();
}

bool ABuildingGridOverlay::EnsureDynamicMaterial()
{
	if (not GetIsValidGridISM())
	{
		return false;
	}

	if (not IsValid(M_GridOverlaySettings.GridOverlayMaterial))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("GridOverlay: GridOverlayMaterial is null. Assign a material in settings."));
		return false;
	}

	if (not IsValid(M_GridMID))
	{
		M_GridMID = UMaterialInstanceDynamic::Create(M_GridOverlaySettings.GridOverlayMaterial, this);
		if (not IsValid(M_GridMID))
		{
			RTSFunctionLibrary::ReportError(TEXT("GridOverlay: Failed to create dynamic material instance."));
			return false;
		}
		M_GridISM->SetMaterial(0, M_GridMID);
	}

	UpdateMaterialParametersWithSettings();
	return true;
}

void ABuildingGridOverlay::UpdateMaterialParametersWithSettings() const
{
	if (not M_GridMID)
	{
		return;
	}
	M_GridMID->SetVectorParameterValue(M_GridOverlaySettings.MatParam_VacantColor, M_GridOverlaySettings.VacantColor);
	M_GridMID->SetVectorParameterValue(M_GridOverlaySettings.MatParam_ValidColor, M_GridOverlaySettings.ValidColor);
	M_GridMID->SetVectorParameterValue(M_GridOverlaySettings.MatParam_InvalidColor, M_GridOverlaySettings.InvalidColor);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_Opacity, M_GridOverlaySettings.MaterialOpacity);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_UseFalloff,
	                                   M_GridOverlaySettings.bUseFalloff ? 1.f : 0.f);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_InnerSquareRatio,
	                                   M_GridOverlaySettings.InnerSquareRatio);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_InnerSquareOpacityMlt,
	                                   M_GridOverlaySettings.InnerSquareOpacityMlt);
	UpdateFallOffFootprintParameters();
}

void ABuildingGridOverlay::UpdateFallOffFootprintParameters() const
{
	if (not M_GridMID)
	{
		return;
	}
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_FalloffMaxSteps,
	                                   M_GridOverlaySettings.FalloffMaxSteps);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_FalloffExponent,
	                                   M_GridOverlaySettings.FalloffExponent);
	M_GridMID->SetScalarParameterValue(M_GridOverlaySettings.MatParam_FalloffMinOpacity,
	                                   M_GridOverlaySettings.FalloffMinOpacity);
}

// ---------- collision / overlap ----------

void ABuildingGridOverlay::EnsureCollisionSetup()
{
	if (not GetIsValidGridISM())
	{
		return;
	}
	FRTS_CollisionSetup::SetupBuildingPlacementGridOverlay(M_GridISM);
}

bool ABuildingGridOverlay::GetInstanceWorldTransform(const int32 InstanceIndex, FTransform& OutWorld) const
{
	if (not GetIsValidGridISM())
	{
		return false;
	}

	if (InstanceIndex < 0 || InstanceIndex >= M_GridISM->GetInstanceCount())
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("GridOverlay: GetInstanceWorldTransform index %d out of range."), InstanceIndex));
		return false;
	}

	M_GridISM->GetInstanceTransform(InstanceIndex, OutWorld, true);
	return true;
}

ECollisionChannel ABuildingGridOverlay::GetPlacementQueryChannel() const
{
	// We drive placement validity via this channel so individual components
	// can opt-in (Overlap), opt-out (Ignore), or hard-block (Block).
	return COLLISION_OBJ_BUILDING_PLACEMENT;
}


void ABuildingGridOverlay::UpdateSingleInstanceOverlapState(const int32 InstanceIndex)
{
	FTransform WorldXf;
	if (not GetInstanceWorldTransform(InstanceIndex, WorldXf))
	{
		return;
	}

	const FVector Center = WorldXf.GetLocation();
	const FQuat Rot = WorldXf.GetRotation();

	const float S   = M_GridOverlaySettings.CellSize;
	const float Gap = M_GridOverlaySettings.InstanceGap;
	const float Step = S + Gap;

	// Half extents cover the full tile + half gaps around it; then shrink a bit to avoid neighbor bleed.
	const float HalfX = FMath::Max(1.f, (Step * 0.5f) - M_PerTileShrink);
	const float HalfY = FMath::Max(1.f, (Step * 0.5f) - M_PerTileShrink);
	const float HalfZ = FMath::Max(1.f, M_PerTileOverlapHalfHeight);

	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(HalfX, HalfY, HalfZ));

	// Ignore ourselves + extra ignores.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildGridTileOverlap), /*bTraceComplex=*/false);
	Params.bFindInitialOverlaps = true;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(M_GridISM.Get());
	for (AActor* Ignored : M_ExtraOverlapActorsToIgnore)
	{
		if (IsValid(Ignored))
		{
			Params.AddIgnoredActor(Ignored);
		}
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("GridOverlay: World is null in UpdateSingleInstanceOverlapState."));
		return;
	}

	const ECollisionChannel PlacementChannel = GetPlacementQueryChannel();

	// Respect per-component responses:
	// - OverlapAnyTestByChannel: finds components set to ECR_Overlap on this channel.
	// - OverlapBlockingTestByChannel: finds components set to ECR_Block on this channel.
	const bool bOverlaps = World->OverlapAnyTestByChannel(Center, Rot, PlacementChannel, Shape, Params);
	const bool bBlocks   = World->OverlapBlockingTestByChannel(Center, Rot, PlacementChannel, Shape, Params);
	const bool bHasConflict = bOverlaps || bBlocks;

	if (bHasConflict) { M_OverlappedIndexSet.Add(InstanceIndex); }
	else { M_OverlappedIndexSet.Remove(InstanceIndex); }

	const float CustomValue =
		bHasConflict
			? TileTypeToCustomData(EGridOverlayTileType::Invalid)
			: (M_ConstructPreviewIndexSet.Contains(InstanceIndex)
				   ? TileTypeToCustomData(EGridOverlayTileType::Valid)
				   : TileTypeToCustomData(EGridOverlayTileType::Vacant));

	M_GridISM->SetCustomDataValue(InstanceIndex, 0, CustomValue, false);
}

float ABuildingGridOverlay::TileTypeToCustomData(const EGridOverlayTileType TileType)
{
	switch (TileType)
	{
	case EGridOverlayTileType::Vacant: return 0.f;
	case EGridOverlayTileType::Valid: return 1.f;
	case EGridOverlayTileType::Invalid: return 2.f;
	default: return 0.f;
	}
}

void ABuildingGridOverlay::FlushCustomDataRenderState() const
{
	if (IsValid(M_GridISM))
	{
		M_GridISM->MarkRenderStateDirty();
	}
}

// ---------- debug ----------

void ABuildingGridOverlay::Debug_DrawConstructionPreviewSquares(const float TimeSeconds) const
{
	if (M_ConstructPreviewIndices.Num() == 0 || not IsValid(M_GridISM))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (const int32 Idx : M_ConstructPreviewIndices)
	{
		FTransform Xf;
		if (not GetInstanceWorldTransform(Idx, Xf))
		{
			continue;
		}

		const FVector Loc = Xf.GetLocation();
		const bool bBlocked = M_OverlappedIndexSet.Contains(Idx);
		const FString Text = bBlocked ? TEXT("BLOCKED") : TEXT("FREE");
		const FColor Col = bBlocked ? FColor::Red : FColor::Green;

		DrawDebugString(World, Loc, Text, nullptr, Col, TimeSeconds, false);
	}
}

void ABuildingGridOverlay::ComputeAndWriteRings_FromFootprint()
{
	if (not GetIsValidGridISM())
	{
		return;
	}

	const int32 N = M_GridOverlaySettings.MaxSize;
	const int32 Total = N * N;
	const int32 InstanceCount = M_GridISM->GetInstanceCount();

	// If we have no footprint, reset ring values to 0 and bail.
	if (M_ConstructPreviewIndices.Num() == 0)
	{
		for (int32 i = 0; i < InstanceCount; ++i)
		{
			M_GridISM->SetCustomDataValue(i, /*Index=*/1, /*D=*/0.f, /*bMarkRenderStateDirty=*/false);
		}
		FlushCustomDataRenderState();
		return;
	}

	// Manhattan-distance BFS from all footprint tiles.
	TArray<int32> Dist;
	Dist.Init(-1, Total);

	TQueue<int32> Queue;
	for (const int32 SeedIdx : M_ConstructPreviewIndices)
	{
		if (SeedIdx >= 0 && SeedIdx < Total)
		{
			Dist[SeedIdx] = 0;
			Queue.Enqueue(SeedIdx);
		}
	}

	const auto EnqueueIfUnvisited = [&](const int32 X, const int32 Y, const int32 CurD)
	{
		if (X < 0 || X >= N || Y < 0 || Y >= N)
		{
			return;
		}
		const int32 Idx = Y * N + X;
		if (Dist[Idx] != -1)
		{
			return;
		}
		Dist[Idx] = CurD + 1;
		Queue.Enqueue(Idx);
	};

	while (not Queue.IsEmpty())
	{
		int32 CurIdx = INDEX_NONE;
		Queue.Dequeue(CurIdx);

		const int32 CurY = CurIdx / N;
		const int32 CurX = CurIdx - CurY * N;
		const int32 CurD = Dist[CurIdx];

		EnqueueIfUnvisited(CurX + 1, CurY, CurD);
		EnqueueIfUnvisited(CurX - 1, CurY, CurD);
		EnqueueIfUnvisited(CurX, CurY + 1, CurD);
		EnqueueIfUnvisited(CurX, CurY - 1, CurD);
	}

	// Write to per-instance custom data channel 1 (ring distance).
	// Unvisited (shouldn't happen in a connected N×N grid) falls back to 0.
	for (int32 i = 0; i < InstanceCount; ++i)
	{
		const float D = (Dist.IsValidIndex(i) && Dist[i] >= 0) ? static_cast<float>(Dist[i]) : 0.f;
		M_GridISM->SetCustomDataValue(i, /*Index=*/1, D, /*bMarkRenderStateDirty=*/false);
	}

	FlushCustomDataRenderState();
}
