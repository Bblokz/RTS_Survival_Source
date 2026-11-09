// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallHoleDamageComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallHoleDamage, Log, All);

namespace WallHoleDamageConstants
{
	// A Voronoi chunk fills about this share of its bounding box; used to estimate a debris mesh's face area.
	constexpr float DebrisMeshFillShare = 0.7f;
	constexpr float MinPieceScale = 0.15f;
	constexpr float MaxPieceScale = 1.2f;
	constexpr float MinPieceScaleJitter = 0.75f;
	constexpr float MaxPieceScaleJitter = 1.1f;
	// Collapse pieces sit on a grid jittered by this share of the spacing.
	constexpr float CollapseJitterShare = 0.3f;
	// A grown hole must gain at least this much radius to count as damage.
	constexpr float MinRadiusGain = 1.f;
	// Pieces sampled per side of a knocked-out cell.
	constexpr int32 CellPiecesPerSide = 2;
	constexpr float SquareCentimetresPerSquareMetre = 10000.f;
}

UWallHoleDamageComponent::UWallHoleDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AddOnArmorPanelDebris::SystemPath));
}

void UWallHoleDamageComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_FindWallMesh();
	if (not GetIsValidWallMesh())
	{
		return;
	}
	BeginPlay_CacheWallShape();
	BeginPlay_CacheDebrisAreas();
	BeginPlay_BindOwnerDeath();
	if (const ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		M_MaxOwnerHealth = DestructibleOwner->GetCurrentHealth();
	}
	M_LoadedDebrisSystem = M_DebrisSystem.LoadSynchronous();
	(void)GetIsValidDebrisSystem();
	WriteMask();
}

void UWallHoleDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		DestructibleOwner->OnDestructableEnvActorDied.Remove(M_OwnerDiedHandle);
	}
	M_OwnerDiedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWallHoleDamageComponent::BeginPlay_FindWallMesh()
{
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(GetOwner());
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == WallMeshComponentName)
		{
			M_WallMesh = MeshComponent;
			return;
		}
	}
	RTSFunctionLibrary::ReportError(TEXT("UWallHoleDamageComponent: no static mesh component named ")
		+ WallMeshComponentName.ToString() + TEXT(" on ") + GetNameSafe(GetOwner()));
}

void UWallHoleDamageComponent::BeginPlay_CacheWallShape()
{
	const UStaticMesh* const Mesh = M_WallMesh->GetStaticMesh();
	if (not IsValid(Mesh))
	{
		return;
	}
	M_MeshBounds = Mesh->GetBoundingBox();
	const FVector Size = M_MeshBounds.GetSize();
	const float CellSize = FMath::Max(GridSettings.CellSize, 1.f);
	// Same rule as the material: round(size / cell), the last column and row take the remainder.
	M_Columns = FMath::Max(1, FMath::RoundToInt32(Size.X / CellSize));
	M_Rows = FMath::Max(1, FMath::RoundToInt32(Size.Z / CellSize));
	if (M_Columns * M_Rows > WallHoleMask::MaxGridCells)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UWallHoleDamageComponent: %s needs %d x %d cells, at most %d fit the mask; raise CellSize."),
			*GetNameSafe(Mesh), M_Columns, M_Rows, WallHoleMask::MaxGridCells));
		M_Rows = FMath::Max(1, WallHoleMask::MaxGridCells / M_Columns);
	}
}

void UWallHoleDamageComponent::BeginPlay_CacheDebrisAreas()
{
	M_DebrisMeshAreas.Reset();
	for (const TObjectPtr<UStaticMesh>& DebrisMesh : DebrisMeshes)
	{
		const FVector Size = IsValid(DebrisMesh) ? DebrisMesh->GetBoundingBox().GetSize() : FVector::ZeroVector;
		M_DebrisMeshAreas.Add(static_cast<float>(Size.X * Size.Z) * WallHoleDamageConstants::DebrisMeshFillShare);
	}
}

void UWallHoleDamageComponent::BeginPlay_BindOwnerDeath()
{
	ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner());
	if (not IsValid(DestructibleOwner) || not CollapseSettings.bCollapseIntoChunksOnDeath)
	{
		return;
	}
	M_OwnerDiedHandle = DestructibleOwner->OnDestructableEnvActorDied.AddUObject(
		this, &UWallHoleDamageComponent::OnOwnerDied);
}

void UWallHoleDamageComponent::OnOwnerDamaged(const float DamageAmount, const FDamageEvent& DamageEvent,
                                              const AActor* DamageCauser, const float HealthBefore,
                                              const float HealthAfter)
{
	if (bM_HasCollapsed || DamageAmount <= 0.f || not GetIsValidWallMesh())
	{
		return;
	}
	M_MaxOwnerHealth = FMath::Max(M_MaxOwnerHealth, HealthBefore);
	FVector WorldHit;
	FVector ShotDirection;
	bool bIsDirectHit = false;
	if (not WallDamageShared::ResolveHitLocation(DamageEvent, DamageCauser, WorldHit, ShotDirection, bIsDirectHit))
	{
		return;
	}
	M_LastShotDirection = ShotDirection.IsNearlyZero() ? M_LastShotDirection : ShotDirection;
	// The wall dies from this hit: the collapse throws everything.
	if (HealthAfter <= 0.f)
	{
		return;
	}
	const FVector MeshSpaceHit = M_WallMesh->GetComponentTransform().InverseTransformPosition(WorldHit);
	const float PlaneDistance = FMath::Max(0.f, static_cast<float>(FMath::Abs(MeshSpaceHit.Y - M_MeshBounds.GetCenter().Y)
		- M_MeshBounds.GetExtent().Y));
	const float HoleRadius = WallDamageShared::ResolveHoleRadius(HoleSettings, DamageAmount, DamageCauser, PlaneDistance);
	const float HealthFractionAfter = M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f;
	const float Removed = ApplyHit(WorldHit, ShotDirection, HoleRadius, GetAllowedRemovedArea(HealthFractionAfter));
	UE_LOG(LogWallHoleDamage, Verbose, TEXT("RTS_WALL_HOLES hit %s radius=%.0f direct=%d removed=%.2f m2 holes=%d cells=%d"),
	       *GetNameSafe(GetOwner()), HoleRadius, bIsDirectHit ? 1 : 0,
	       Removed / WallHoleDamageConstants::SquareCentimetresPerSquareMetre, M_Holes.Num(), GetHiddenCellCount());
}

float UWallHoleDamageComponent::Debug_FireTestHit(const FVector WorldLocation, const FVector ShotDirection,
                                                  const float CalibreMm)
{
	if (bM_HasCollapsed || not GetIsValidWallMesh())
	{
		return 0.f;
	}
	M_LastShotDirection = ShotDirection.GetSafeNormal();
	const float Radius = WallDamageShared::ResolveHoleRadiusFromCalibre(HoleSettings, CalibreMm, 0.f);
	// No health cap: the test drives the wall directly.
	const float Removed = ApplyHit(WorldLocation, M_LastShotDirection, Radius, TNumericLimits<float>::Max());
	UE_LOG(LogWallHoleDamage, Display, TEXT("RTS_WALL_HOLES test hit %s calibre=%.0f removed=%.2f m2 holes=%d cells=%d"),
	       *GetNameSafe(GetOwner()), CalibreMm, Removed / WallHoleDamageConstants::SquareCentimetresPerSquareMetre,
	       M_Holes.Num(), GetHiddenCellCount());
	return Removed / WallHoleDamageConstants::SquareCentimetresPerSquareMetre;
}

float UWallHoleDamageComponent::ApplyHit(const FVector& WorldHit, const FVector& ShotDirection, const float HoleRadius,
                                         const float AllowedArea)
{
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector MeshSpaceHit = MeshTransform.InverseTransformPosition(WorldHit);
	const FVector2D FacePoint(MeshSpaceHit.X, MeshSpaceHit.Z);
	// Splash past the wall's end or top reaches nothing.
	const FBox2D Face(FVector2D(M_MeshBounds.Min.X, M_MeshBounds.Min.Z), FVector2D(M_MeshBounds.Max.X, M_MeshBounds.Max.Z));
	const float AvailableArea = AllowedArea - GetRemovedAreaEstimate();
	if (HoleRadius <= 0.f || AvailableArea <= 0.f || Face.ComputeSquaredDistanceToPoint(FacePoint) > FMath::Square(HoleRadius))
	{
		return 0.f;
	}
	const float CappedRadius = FMath::Min(HoleRadius, FMath::Sqrt(AvailableArea / UE_PI));
	if (CappedRadius < GridSettings.MinHoleRadius)
	{
		return 0.f;
	}
	TArray<FWallDebrisPiece> Pieces;
	const float Removed = CutHole(FacePoint, CappedRadius, Pieces);
	if (Removed <= 0.f)
	{
		return 0.f;
	}
	SwapToDamagedMaterials();
	WriteMask();
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, DebrisMeshes, Pieces,
	                                    MeshTransform.GetRotation(), WorldHit, ShotDirection, 1.f);
	return Removed;
}

float UWallHoleDamageComponent::CutHole(const FVector2D& FacePoint, const float Radius,
                                        TArray<FWallDebrisPiece>& OutPieces)
{
	const float CellBreakRadius = GridSettings.CellSize * GridSettings.CellBreakRadiusShare;
	// A hit inside an existing hole widens it instead of taking a slot.
	for (FWallHole& Hole : M_Holes)
	{
		const float Distance = static_cast<float>(FVector2D::Distance(FacePoint, Hole.Centre));
		if (Distance >= Hole.Radius)
		{
			continue;
		}
		const float OldRadius = Hole.Radius;
		const float NewRadius = FMath::Min(HoleSettings.MaxHoleRadius,
		                                   FMath::Max(OldRadius, Distance + Radius * GridSettings.GrowRadiusShare));
		if (NewRadius < OldRadius + WallHoleDamageConstants::MinRadiusGain)
		{
			return 0.f;
		}
		Hole.Radius = NewRadius;
		AddDiscPieces(Hole.Centre, OldRadius, NewRadius, OutPieces);
		float Removed = UE_PI * (FMath::Square(NewRadius) - FMath::Square(OldRadius));
		if (NewRadius >= CellBreakRadius)
		{
			Removed += HideCell(GetCellIndexAt(Hole.Centre), OutPieces);
		}
		return Removed;
	}
	float Removed = 0.f;
	if (M_Holes.Num() >= WallHoleMask::MaxHoles)
	{
		Removed += BakeOldestHoleIntoCells(OutPieces);
	}
	FWallHole& NewHole = M_Holes.AddDefaulted_GetRef();
	NewHole.Centre = FacePoint;
	NewHole.Radius = Radius;
	AddDiscPieces(FacePoint, 0.f, Radius, OutPieces);
	Removed += UE_PI * FMath::Square(Radius);
	// A big shell takes out the whole cell it hit, not only a round hole.
	if (Radius >= CellBreakRadius)
	{
		Removed += HideCell(GetCellIndexAt(FacePoint), OutPieces);
	}
	return Removed;
}

float UWallHoleDamageComponent::BakeOldestHoleIntoCells(TArray<FWallDebrisPiece>& OutPieces)
{
	if (M_Holes.IsEmpty())
	{
		return 0.f;
	}
	// The oldest hole gives up its slot: it becomes the cells it covers, so the wall keeps the damage.
	const FWallHole Oldest = M_Holes[0];
	const float Reach = Oldest.Radius + GridSettings.CellSize * GridSettings.BakeReachShare;
	float Removed = HideCell(GetCellIndexAt(Oldest.Centre), OutPieces);
	for (int32 CellIndex = 0; CellIndex < M_Columns * M_Rows; ++CellIndex)
	{
		if (FVector2D::Distance(GetCellCentre(CellIndex), Oldest.Centre) <= Reach)
		{
			Removed += HideCell(CellIndex, OutPieces);
		}
	}
	M_Holes.RemoveAt(0);
	return Removed;
}

float UWallHoleDamageComponent::HideCell(const int32 CellIndex, TArray<FWallDebrisPiece>& OutPieces)
{
	if (CellIndex < 0 || CellIndex >= M_Columns * M_Rows || GetIsCellHidden(CellIndex))
	{
		return 0.f;
	}
	// Debris only for the parts of the cell that were still standing.
	const int32 Column = CellIndex % M_Columns;
	const int32 Row = CellIndex / M_Columns;
	const float MinX = static_cast<float>(M_MeshBounds.Min.X) + Column * GridSettings.CellSize;
	const float MinZ = static_cast<float>(M_MeshBounds.Min.Z) + Row * GridSettings.CellSize;
	const float MaxX = Column == M_Columns - 1 ? static_cast<float>(M_MeshBounds.Max.X) : MinX + GridSettings.CellSize;
	const float MaxZ = Row == M_Rows - 1 ? static_cast<float>(M_MeshBounds.Max.Z) : MinZ + GridSettings.CellSize;
	const float CellArea = (MaxX - MinX) * (MaxZ - MinZ);
	constexpr int32 PiecesPerSide = WallHoleDamageConstants::CellPiecesPerSide;
	for (int32 SampleIndex = 0; SampleIndex < PiecesPerSide * PiecesPerSide; ++SampleIndex)
	{
		const float U = (SampleIndex % PiecesPerSide + 0.5f) / PiecesPerSide;
		const float V = (SampleIndex / PiecesPerSide + 0.5f) / PiecesPerSide;
		const FVector2D Sample(FMath::Lerp(MinX, MaxX, U), FMath::Lerp(MinZ, MaxZ, V));
		if (not GetIsFacePointRemoved(Sample))
		{
			AddPiece(Sample, CellArea / (PiecesPerSide * PiecesPerSide), OutPieces);
		}
	}
	M_CellWords[CellIndex / WallHoleMask::CellsPerGridFloat] |= 1u << (CellIndex % WallHoleMask::CellsPerGridFloat);
	return CellArea;
}

void UWallHoleDamageComponent::AddDiscPieces(const FVector2D& Centre, const float InnerRadius, const float OuterRadius,
                                             TArray<FWallDebrisPiece>& OutPieces) const
{
	const float RingArea = UE_PI * (FMath::Square(OuterRadius) - FMath::Square(InnerRadius));
	const int32 PieceCount = FMath::Clamp(FMath::RoundToInt32(RingArea / FMath::Max(GridSettings.AreaPerDebrisPiece, 1.f)),
	                                      1, GridSettings.MaxPiecesPerHit);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		// Uniform over the ring's area.
		const float Distance = FMath::Sqrt(FMath::FRandRange(FMath::Square(InnerRadius), FMath::Square(OuterRadius)));
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		AddPiece(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance, RingArea / PieceCount, OutPieces);
	}
}

void UWallHoleDamageComponent::AddPiece(const FVector2D& FacePoint, const float PieceArea,
                                        TArray<FWallDebrisPiece>& OutPieces) const
{
	using namespace WallHoleDamageConstants;
	if (DebrisMeshes.IsEmpty() || M_DebrisMeshAreas.Num() != DebrisMeshes.Num())
	{
		return;
	}
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	FWallDebrisPiece& Piece = OutPieces.AddDefaulted_GetRef();
	Piece.MeshIndex = FMath::RandRange(0, DebrisMeshes.Num() - 1);
	Piece.WorldCentre = MeshTransform.TransformPosition(FVector(FacePoint.X, M_MeshBounds.GetCenter().Y, FacePoint.Y));
	const float MeshArea = FMath::Max(M_DebrisMeshAreas[Piece.MeshIndex], 1.f);
	const float Scale = FMath::Sqrt(PieceArea / MeshArea) * FMath::FRandRange(MinPieceScaleJitter, MaxPieceScaleJitter);
	Piece.Scale = FMath::Clamp(Scale, MinPieceScale, MaxPieceScale) * static_cast<float>(MeshTransform.GetMaximumAxisScale());
}

int32 UWallHoleDamageComponent::GetCellIndexAt(const FVector2D& FacePoint) const
{
	const int32 Column = FMath::Clamp(FMath::FloorToInt32((FacePoint.X - M_MeshBounds.Min.X) / GridSettings.CellSize),
	                                  0, M_Columns - 1);
	const int32 Row = FMath::Clamp(FMath::FloorToInt32((FacePoint.Y - M_MeshBounds.Min.Z) / GridSettings.CellSize),
	                               0, M_Rows - 1);
	return Column + Row * M_Columns;
}

FVector2D UWallHoleDamageComponent::GetCellCentre(const int32 CellIndex) const
{
	const int32 Column = CellIndex % M_Columns;
	const int32 Row = CellIndex / M_Columns;
	const double MinX = M_MeshBounds.Min.X + Column * GridSettings.CellSize;
	const double MinZ = M_MeshBounds.Min.Z + Row * GridSettings.CellSize;
	const double MaxX = Column == M_Columns - 1 ? M_MeshBounds.Max.X : MinX + GridSettings.CellSize;
	const double MaxZ = Row == M_Rows - 1 ? M_MeshBounds.Max.Z : MinZ + GridSettings.CellSize;
	return FVector2D((MinX + MaxX) * 0.5, (MinZ + MaxZ) * 0.5);
}

bool UWallHoleDamageComponent::GetIsCellHidden(const int32 CellIndex) const
{
	return (M_CellWords[CellIndex / WallHoleMask::CellsPerGridFloat]
		& (1u << (CellIndex % WallHoleMask::CellsPerGridFloat))) != 0;
}

int32 UWallHoleDamageComponent::GetHiddenCellCount() const
{
	int32 Count = 0;
	for (const uint32 Word : M_CellWords)
	{
		Count += FMath::CountBits(Word);
	}
	return Count;
}

bool UWallHoleDamageComponent::GetIsFacePointRemoved(const FVector2D& FacePoint) const
{
	if (GetIsCellHidden(GetCellIndexAt(FacePoint)))
	{
		return true;
	}
	for (const FWallHole& Hole : M_Holes)
	{
		if (FVector2D::DistSquared(FacePoint, Hole.Centre) < FMath::Square(Hole.Radius))
		{
			return true;
		}
	}
	return false;
}

float UWallHoleDamageComponent::GetRemovedAreaEstimate() const
{
	float Area = GridSettings.CellSize * GridSettings.CellSize * GetHiddenCellCount();
	for (const FWallHole& Hole : M_Holes)
	{
		Area += UE_PI * FMath::Square(Hole.Radius);
	}
	return Area;
}

float UWallHoleDamageComponent::GetAllowedRemovedArea(const float HealthFractionAfter) const
{
	const float LostHealth01 = 1.f - FMath::Clamp(HealthFractionAfter, 0.f, 1.f);
	const FVector Size = M_MeshBounds.GetSize();
	return LostHealth01 * HoleSettings.MaxHiddenShareWhileAlive * static_cast<float>(Size.X * Size.Z);
}

void UWallHoleDamageComponent::WriteMask() const
{
	using namespace WallHoleMask;
	if (not GetIsValidWallMesh())
	{
		return;
	}
	for (int32 Word = 0; Word < MaxGridFloats; ++Word)
	{
		M_WallMesh->SetCustomPrimitiveDataFloat(FirstGridPrimitiveDataIndex + Word, static_cast<float>(M_CellWords[Word]));
	}
	for (int32 HoleIndex = 0; HoleIndex < MaxHoles; ++HoleIndex)
	{
		const bool bIsUsed = M_Holes.IsValidIndex(HoleIndex);
		const int32 First = FirstHolePrimitiveDataIndex + HoleIndex * FloatsPerHole;
		M_WallMesh->SetCustomPrimitiveDataFloat(First, bIsUsed ? static_cast<float>(M_Holes[HoleIndex].Centre.X) : 0.f);
		M_WallMesh->SetCustomPrimitiveDataFloat(First + 1, bIsUsed ? static_cast<float>(M_Holes[HoleIndex].Centre.Y) : 0.f);
		// Radius 0 switches the slot off in the material.
		M_WallMesh->SetCustomPrimitiveDataFloat(First + 2, bIsUsed ? M_Holes[HoleIndex].Radius : 0.f);
	}
}

void UWallHoleDamageComponent::SwapToDamagedMaterials()
{
	// Intact walls keep their opaque materials; masked surfaces lose early-Z in the depth and shadow passes.
	if (bM_HasSwappedToDamagedMaterials || not GetIsValidWallMesh())
	{
		return;
	}
	bM_HasSwappedToDamagedMaterials = true;
	for (int32 SlotIndex = 0; SlotIndex < DamagedMaterials.Num(); ++SlotIndex)
	{
		if (IsValid(DamagedMaterials[SlotIndex]))
		{
			M_WallMesh->SetMaterial(SlotIndex, DamagedMaterials[SlotIndex]);
		}
	}
}

void UWallHoleDamageComponent::OnOwnerDied()
{
	CollapseIntoPieces();
}

void UWallHoleDamageComponent::CollapseIntoPieces()
{
	using namespace WallHoleDamageConstants;
	if (bM_HasCollapsed || not GetIsValidWallMesh())
	{
		return;
	}
	bM_HasCollapsed = true;
	const float Spacing = FMath::Max(GridSettings.CollapsePieceSpacing, 1.f);
	const FVector Size = M_MeshBounds.GetSize();
	const int32 Columns = FMath::Max(1, FMath::RoundToInt32(Size.X / Spacing));
	const int32 Rows = FMath::Max(1, FMath::RoundToInt32(Size.Z / Spacing));
	const float PieceArea = static_cast<float>(Size.X * Size.Z) / (Columns * Rows);
	TArray<FWallDebrisPiece> Pieces;
	for (int32 SampleIndex = 0; SampleIndex < Columns * Rows; ++SampleIndex)
	{
		const float Jitter = Spacing * CollapseJitterShare;
		const FVector2D Sample(
			M_MeshBounds.Min.X + (SampleIndex % Columns + 0.5) * Size.X / Columns + FMath::FRandRange(-Jitter, Jitter),
			M_MeshBounds.Min.Z + (SampleIndex / Columns + 0.5) * Size.Z / Rows + FMath::FRandRange(-Jitter, Jitter));
		if (not GetIsFacePointRemoved(Sample))
		{
			AddPiece(Sample, PieceArea, Pieces);
		}
	}
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector Centre = M_WallMesh->Bounds.Origin;
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, DebrisMeshes, Pieces,
	                                    MeshTransform.GetRotation(), Centre, M_LastShotDirection,
	                                    CollapseSettings.DeathSpeedScale);
	// Nothing of the wall is left to draw or to hit.
	M_WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_WallMesh->SetVisibility(false, false);
	WallDamageShared::PlayCollapseSound(*GetOwner(), CollapseSettings, Centre);
	GetOwner()->SetLifeSpan(FMath::Max(CollapseSettings.DestroyOwnerDelaySeconds, UE_KINDA_SMALL_NUMBER));
	UE_LOG(LogWallHoleDamage, Display, TEXT("RTS_WALL_HOLES collapse %s threw=%d"), *GetNameSafe(GetOwner()),
	       Pieces.Num());
}

bool UWallHoleDamageComponent::GetIsValidWallMesh() const
{
	if (IsValid(M_WallMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_WallMesh",
		"GetIsValidWallMesh",
		GetOwner());
	return false;
}

bool UWallHoleDamageComponent::GetIsValidDebrisSystem() const
{
	if (IsValid(M_LoadedDebrisSystem))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_LoadedDebrisSystem",
		"GetIsValidDebrisSystem",
		GetOwner());
	return false;
}
