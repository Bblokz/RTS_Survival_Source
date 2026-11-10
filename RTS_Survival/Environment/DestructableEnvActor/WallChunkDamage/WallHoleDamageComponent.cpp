// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallHoleDamageComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
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
	// A hit inside a hole grows it by at least this share of the hit's own radius, so every hit visibly bites.
	constexpr float MinGrowShareOfHit = 0.25f;
	// A hit on a hole already at MaxHoleRadius still chips its rim: pieces from the outer band of this share.
	constexpr float CappedRimInnerShare = 0.8f;
	// Closed shapes: pieces start this far around the struck point (share of the hole radius) and this far outside it.
	constexpr float AnchoredPieceSpreadShare = 0.6f;
	constexpr float AnchoredPieceSurfaceOffset = 8.f;
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
	CacheWallShape();
	BeginPlay_CacheDebrisAreas();
	BeginPlay_BindOwnerDeath();
	BeginPlay_BindOwnerHealing();
	if (const ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		NoteOwnerHealth(DestructibleOwner->GetCurrentHealth());
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
	else if (AHPActorObjectsMaster* const BuildingOwner = Cast<AHPActorObjectsMaster>(GetOwner()))
	{
		BuildingOwner->OnUnitDies.Remove(M_OwnerDiedHandle);
	}
	M_OwnerDiedHandle.Reset();
	if (M_OwnerHealth.IsValid())
	{
		M_OwnerHealth->GetOnHealthHealed().Remove(M_OwnerHealedHandle);
	}
	M_OwnerHealedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWallHoleDamageComponent::BeginPlay_BindOwnerHealing()
{
	UHealthComponent* const OwnerHealth = GetOwner()->FindComponentByClass<UHealthComponent>();
	// Environment walls without a health component never heal.
	if (not IsValid(OwnerHealth))
	{
		return;
	}
	M_OwnerHealth = OwnerHealth;
	M_OwnerHealedHandle = OwnerHealth->GetOnHealthHealed().AddUObject(
		this, &UWallHoleDamageComponent::OnOwnerHealthHealed);
}

void UWallHoleDamageComponent::OnOwnerHealthHealed(const float HealthAfter, const float MaxHealth)
{
	OnOwnerHealed(HealthAfter, MaxHealth);
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

void UWallHoleDamageComponent::EnsureWallShapeIsCurrent()
{
	if (M_WallMesh->GetStaticMesh() == M_CachedShapeMesh.Get())
	{
		return;
	}
	// Damage on the old mesh does not map onto the new one.
	M_Holes.Reset();
	M_CellOrder.Reset();
	FMemory::Memzero(M_CellWords);
	CacheWallShape();
	WriteMask();
}

void UWallHoleDamageComponent::CacheWallShape()
{
	const UStaticMesh* const Mesh = M_WallMesh->GetStaticMesh();
	M_CachedShapeMesh = Mesh;
	if (not IsValid(Mesh))
	{
		return;
	}
	M_MeshBounds = Mesh->GetBoundingBox();
	if (not GetHasGrid())
	{
		return;
	}
	const FVector Size = M_MeshBounds.GetSize();
	const double AlongSize = GridSettings.GridAxis == EWallHoleGridAxis::AlongY ? Size.Y : Size.X;
	const float CellSize = FMath::Max(GridSettings.CellSize, 1.f);
	// Same rule as the material: round(size / cell), the last column and row take the remainder.
	M_Columns = FMath::Max(1, FMath::RoundToInt32(AlongSize / CellSize));
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
	if (not CollapseSettings.bCollapseIntoChunksOnDeath)
	{
		return;
	}
	if (ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		M_OwnerDiedHandle = DestructibleOwner->OnDestructableEnvActorDied.AddUObject(
			this, &UWallHoleDamageComponent::OnDestructibleOwnerDied);
		return;
	}
	if (AHPActorObjectsMaster* const BuildingOwner = Cast<AHPActorObjectsMaster>(GetOwner()))
	{
		M_OwnerDiedHandle = BuildingOwner->OnUnitDies.AddUObject(this, &UWallHoleDamageComponent::OnBuildingOwnerDied);
	}
}

void UWallHoleDamageComponent::OnOwnerDamaged(const float DamageAmount, const FDamageEvent& DamageEvent,
                                              const AActor* DamageCauser, const float HealthBefore,
                                              const float HealthAfter)
{
	if (bM_HasCollapsed || DamageAmount <= 0.f || not GetIsValidWallMesh() || GetIsOwnerInTransition())
	{
		return;
	}
	NoteOwnerHealth(HealthBefore);
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
	EnsureWallShapeIsCurrent();
	const FVector MeshSpaceHit = M_WallMesh->GetComponentTransform().InverseTransformPosition(WorldHit);
	// Splash outside the wall makes a smaller hole.
	const float OutsideDistance = FMath::Sqrt(static_cast<float>(M_MeshBounds.ComputeSquaredDistanceToPoint(MeshSpaceHit)));
	const float HoleRadius = WallDamageShared::ResolveHoleRadius(HoleSettings, DamageAmount, DamageCauser, OutsideDistance);
	const float HealthFractionAfter = M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f;
	const float Removed = ApplyHit(WorldHit, ShotDirection, HoleRadius, GetAllowedRemovedArea(HealthFractionAfter));
	UE_LOG(LogWallHoleDamage, Verbose, TEXT("RTS_WALL_HOLES hit %s radius=%.0f direct=%d removed=%.2f m2 holes=%d cells=%d"),
	       *GetNameSafe(GetOwner()), HoleRadius, bIsDirectHit ? 1 : 0,
	       Removed / WallHoleDamageConstants::SquareCentimetresPerSquareMetre, M_Holes.Num(), GetHiddenCellCount());
}

void UWallHoleDamageComponent::OnOwnerHealed(const float HealthAfter, const float MaxHealth)
{
	if (bM_HasCollapsed || not GetIsValidWallMesh())
	{
		return;
	}
	SetOwnerMaxHealth(MaxHealth);
	const float AllowedArea = GetAllowedRemovedArea(M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f);
	bool bChanged = false;
	// Newest damage closes first: holes, then the cells they were baked into.
	while (GetRemovedAreaEstimate() > AllowedArea && (not M_Holes.IsEmpty() || not M_CellOrder.IsEmpty()))
	{
		bChanged = true;
		if (not M_Holes.IsEmpty())
		{
			M_Holes.Pop(EAllowShrinking::No);
			continue;
		}
		const int32 CellIndex = M_CellOrder.Pop(EAllowShrinking::No);
		M_CellWords[CellIndex / WallHoleMask::CellsPerGridFloat] &= ~(1u << (CellIndex % WallHoleMask::CellsPerGridFloat));
	}
	if (not bChanged)
	{
		return;
	}
	WriteMask();
	// Fully repaired: back to the opaque materials, so an intact wall pays nothing for the mask again.
	if (M_Holes.IsEmpty() && M_CellOrder.IsEmpty())
	{
		RestoreIntactMaterials();
	}
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
	EnsureWallShapeIsCurrent();
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector MeshSpaceHit = MeshTransform.InverseTransformPosition(WorldHit);
	const float AvailableArea = AllowedArea - GetRemovedAreaEstimate();
	// Splash past the wall reaches nothing.
	if (HoleRadius <= 0.f || AvailableArea <= 0.f
		|| M_MeshBounds.ComputeSquaredDistanceToPoint(MeshSpaceHit) > FMath::Square(HoleRadius))
	{
		return 0.f;
	}
	const float CappedRadius = FMath::Min(HoleRadius, FMath::Sqrt(AvailableArea / UE_PI));
	if (CappedRadius < GridSettings.MinHoleRadius)
	{
		return 0.f;
	}
	TArray<FWallDebrisPiece> Pieces;
	// A splash hole sits on the wall's surface nearest the explosion.
	const FVector SurfacePoint = M_MeshBounds.GetClosestPointTo(MeshSpaceHit);
	const FWallHolePieceAnchor Anchor = MakePieceAnchor(SurfacePoint, MeshTransform, ShotDirection);
	const float Removed = CutHole(GetOutsetHoleCentre(SurfacePoint, MeshTransform, ShotDirection, CappedRadius),
	                              CappedRadius, Anchor, Pieces);
	if (Removed > 0.f)
	{
		SwapToDamagedMaterials();
		WriteMask();
	}
	// A capped hole removes nothing new but still chips pieces off its rim.
	const FVector OutwardDirection = Anchor.GetIsSet()
		                                 ? MeshTransform.TransformVectorNoScale(Anchor.Outward).GetSafeNormal()
		                                 : FVector::ZeroVector;
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, DebrisMeshes, Pieces,
	                                    MeshTransform.GetRotation(), WorldHit, ShotDirection, 1.f, OutwardDirection);
	return Removed;
}

FVector UWallHoleDamageComponent::GetOutsetHoleCentre(const FVector& SurfacePoint, const FTransform& MeshTransform,
                                                     const FVector& ShotDirection, const float Radius) const
{
	if (GridSettings.HoleCentreOutsetShare <= 0.f || ShotDirection.IsNearlyZero())
	{
		return SurfacePoint;
	}
	const FVector MeshSpaceShotDirection = MeshTransform.InverseTransformVectorNoScale(ShotDirection).GetSafeNormal();
	return SurfacePoint - MeshSpaceShotDirection * Radius * GridSettings.HoleCentreOutsetShare;
}

FWallHolePieceAnchor UWallHoleDamageComponent::MakePieceAnchor(const FVector& SurfacePoint,
                                                               const FTransform& MeshTransform,
                                                               const FVector& ShotDirection) const
{
	FWallHolePieceAnchor Anchor;
	if (GetHasGrid())
	{
		return Anchor;
	}
	// Out of the shape: away from its vertical axis, and back toward the shooter.
	FVector Radial = SurfacePoint - M_MeshBounds.GetCenter();
	Radial.Z = 0.0;
	Radial = Radial.GetSafeNormal();
	const FVector TowardShooter = -MeshTransform.InverseTransformVectorNoScale(ShotDirection).GetSafeNormal();
	FVector Outward = (Radial + TowardShooter).GetSafeNormal();
	if (Outward.IsNearlyZero())
	{
		Outward = TowardShooter.IsNearlyZero() ? Radial : TowardShooter;
	}
	Anchor.SurfacePoint = SurfacePoint;
	Anchor.Outward = Outward.IsNearlyZero() ? FVector::UpVector : Outward;
	return Anchor;
}

float UWallHoleDamageComponent::CutHole(const FVector& MeshPoint, const float Radius, const FWallHolePieceAnchor& Anchor,
                                        TArray<FWallDebrisPiece>& OutPieces)
{
	using namespace WallHoleDamageConstants;
	const float CellBreakRadius = GridSettings.CellSize * GridSettings.CellBreakRadiusShare;
	// A hit inside an existing hole widens it instead of taking a slot.
	for (FWallHole& Hole : M_Holes)
	{
		const float Distance = static_cast<float>(FVector::Distance(MeshPoint, Hole.Centre));
		if (Distance >= Hole.Radius)
		{
			continue;
		}
		const float OldRadius = Hole.Radius;
		const float NewRadius = FMath::Min(HoleSettings.MaxHoleRadius,
		                                   FMath::Max3(OldRadius, Distance + Radius * GridSettings.GrowRadiusShare,
		                                               OldRadius + Radius * MinGrowShareOfHit));
		if (NewRadius < OldRadius + MinRadiusGain)
		{
			AddHolePieces(Hole.Centre, OldRadius * CappedRimInnerShare, OldRadius, Anchor, OutPieces);
			return 0.f;
		}
		Hole.Radius = NewRadius;
		AddHolePieces(Hole.Centre, OldRadius, NewRadius, Anchor, OutPieces);
		float Removed = UE_PI * (FMath::Square(NewRadius) - FMath::Square(OldRadius));
		if (GetHasGrid() && NewRadius >= CellBreakRadius)
		{
			Removed += HideCell(GetCellIndexAt(Hole.Centre), OutPieces);
		}
		return Removed;
	}
	float Removed = 0.f;
	if (M_Holes.Num() >= WallHoleMask::MaxHoles)
	{
		Removed += FreeOldestSlot(OutPieces);
	}
	FWallHole& NewHole = M_Holes.AddDefaulted_GetRef();
	NewHole.Centre = MeshPoint;
	NewHole.Radius = Radius;
	AddHolePieces(MeshPoint, 0.f, Radius, Anchor, OutPieces);
	Removed += UE_PI * FMath::Square(Radius);
	// A big shell takes out the whole cell it hit, not only a round hole.
	if (GetHasGrid() && Radius >= CellBreakRadius)
	{
		Removed += HideCell(GetCellIndexAt(MeshPoint), OutPieces);
	}
	return Removed;
}

float UWallHoleDamageComponent::FreeOldestSlot(TArray<FWallDebrisPiece>& OutPieces)
{
	if (GetHasGrid())
	{
		return BakeOldestHoleIntoCells(OutPieces);
	}
	MergeOldestHoleIntoNearest();
	return 0.f;
}

float UWallHoleDamageComponent::BakeOldestHoleIntoCells(TArray<FWallDebrisPiece>& OutPieces)
{
	if (M_Holes.IsEmpty())
	{
		return 0.f;
	}
	// The oldest hole gives up its slot: it becomes the cells it covers, so the wall keeps the damage.
	const FWallHole Oldest = M_Holes[0];
	const FVector2D OldestOnGrid = GetGridPoint(Oldest.Centre);
	const float Reach = Oldest.Radius + GridSettings.CellSize * GridSettings.BakeReachShare;
	float Removed = HideCell(GetCellIndexAt(Oldest.Centre), OutPieces);
	for (int32 CellIndex = 0; CellIndex < M_Columns * M_Rows; ++CellIndex)
	{
		FVector2D CellMin;
		FVector2D CellMax;
		GetCellRect(CellIndex, CellMin, CellMax);
		if (FVector2D::Distance((CellMin + CellMax) * 0.5, OldestOnGrid) <= Reach)
		{
			Removed += HideCell(CellIndex, OutPieces);
		}
	}
	M_Holes.RemoveAt(0);
	return Removed;
}

void UWallHoleDamageComponent::MergeOldestHoleIntoNearest()
{
	if (M_Holes.Num() < 2)
	{
		return;
	}
	// No grid to bake into: the oldest hole joins its nearest neighbour as one sphere around both.
	const FWallHole Oldest = M_Holes[0];
	int32 NearestIndex = 1;
	for (int32 HoleIndex = 2; HoleIndex < M_Holes.Num(); ++HoleIndex)
	{
		if (FVector::DistSquared(M_Holes[HoleIndex].Centre, Oldest.Centre)
			< FVector::DistSquared(M_Holes[NearestIndex].Centre, Oldest.Centre))
		{
			NearestIndex = HoleIndex;
		}
	}
	FWallHole& Nearest = M_Holes[NearestIndex];
	const float Distance = static_cast<float>(FVector::Distance(Nearest.Centre, Oldest.Centre));
	const float EnclosingRadius = FMath::Max3(Oldest.Radius, Nearest.Radius,
	                                          (Distance + Oldest.Radius + Nearest.Radius) * 0.5f);
	const float NewRadius = FMath::Min(EnclosingRadius, HoleSettings.MaxHoleRadius);
	if (Distance > UE_KINDA_SMALL_NUMBER && NewRadius > Nearest.Radius)
	{
		const FVector TowardOldest = (Oldest.Centre - Nearest.Centre) / Distance;
		Nearest.Centre += TowardOldest * FMath::Min(Distance, NewRadius - Nearest.Radius);
	}
	Nearest.Radius = FMath::Max(Nearest.Radius, NewRadius);
	M_Holes.RemoveAt(0);
}

float UWallHoleDamageComponent::HideCell(const int32 CellIndex, TArray<FWallDebrisPiece>& OutPieces)
{
	if (CellIndex < 0 || CellIndex >= M_Columns * M_Rows || GetIsCellHidden(CellIndex))
	{
		return 0.f;
	}
	FVector2D CellMin;
	FVector2D CellMax;
	GetCellRect(CellIndex, CellMin, CellMax);
	const float CellArea = static_cast<float>((CellMax.X - CellMin.X) * (CellMax.Y - CellMin.Y));
	// Debris only for the parts of the cell that were still standing.
	constexpr int32 PiecesPerSide = WallHoleDamageConstants::CellPiecesPerSide;
	for (int32 SampleIndex = 0; SampleIndex < PiecesPerSide * PiecesPerSide; ++SampleIndex)
	{
		const double U = (SampleIndex % PiecesPerSide + 0.5) / PiecesPerSide;
		const double V = (SampleIndex / PiecesPerSide + 0.5) / PiecesPerSide;
		const FVector Sample = GetMeshPointFromGrid(FVector2D(FMath::Lerp(CellMin.X, CellMax.X, U),
		                                                      FMath::Lerp(CellMin.Y, CellMax.Y, V)));
		if (not GetIsMeshPointRemoved(Sample))
		{
			AddPiece(Sample, CellArea / (PiecesPerSide * PiecesPerSide), OutPieces);
		}
	}
	M_CellWords[CellIndex / WallHoleMask::CellsPerGridFloat] |= 1u << (CellIndex % WallHoleMask::CellsPerGridFloat);
	M_CellOrder.Add(CellIndex);
	return CellArea;
}

void UWallHoleDamageComponent::AddHolePieces(const FVector& Centre, const float InnerRadius, const float OuterRadius,
                                             const FWallHolePieceAnchor& Anchor,
                                             TArray<FWallDebrisPiece>& OutPieces) const
{
	using namespace WallHoleDamageConstants;
	const float RingArea = UE_PI * (FMath::Square(OuterRadius) - FMath::Square(InnerRadius));
	const int32 PieceCount = FMath::Clamp(FMath::RoundToInt32(RingArea / FMath::Max(GridSettings.AreaPerDebrisPiece, 1.f)),
	                                      1, GridSettings.MaxPiecesPerHit);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		AddPiece(GetHolePieceStart(Centre, InnerRadius, OuterRadius, Anchor), RingArea / PieceCount, OutPieces);
	}
}

FVector UWallHoleDamageComponent::GetHolePieceStart(const FVector& Centre, const float InnerRadius,
                                                    const float OuterRadius, const FWallHolePieceAnchor& Anchor) const
{
	using namespace WallHoleDamageConstants;
	if (Anchor.GetIsSet())
	{
		// On the struck surface around the hit, just outside it, so the piece is seen leaving the shape.
		const FVector Tangent = FVector::VectorPlaneProject(FMath::VRand(), Anchor.Outward).GetSafeNormal();
		return Anchor.SurfacePoint + Tangent * FMath::FRandRange(0.f, OuterRadius * AnchoredPieceSpreadShare)
			+ Anchor.Outward * AnchoredPieceSurfaceOffset;
	}
	// Somewhere in the removed shell, kept inside the wall's bounds.
	const float Distance = FMath::Sqrt(FMath::FRandRange(FMath::Square(InnerRadius), FMath::Square(OuterRadius)));
	return M_MeshBounds.GetClosestPointTo(Centre + FMath::VRand() * Distance);
}

void UWallHoleDamageComponent::AddPiece(const FVector& MeshPoint, const float PieceArea,
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
	Piece.WorldCentre = MeshTransform.TransformPosition(MeshPoint);
	const float MeshArea = FMath::Max(M_DebrisMeshAreas[Piece.MeshIndex], 1.f);
	const float Scale = FMath::Sqrt(PieceArea / MeshArea) * FMath::FRandRange(MinPieceScaleJitter, MaxPieceScaleJitter);
	Piece.Scale = FMath::Clamp(Scale, MinPieceScale, MaxPieceScale) * static_cast<float>(MeshTransform.GetMaximumAxisScale());
}

FVector2D UWallHoleDamageComponent::GetGridPoint(const FVector& MeshPoint) const
{
	const double Along = GridSettings.GridAxis == EWallHoleGridAxis::AlongY ? MeshPoint.Y : MeshPoint.X;
	return FVector2D(Along, MeshPoint.Z);
}

FVector UWallHoleDamageComponent::GetMeshPointFromGrid(const FVector2D& GridPoint) const
{
	const FVector Centre = M_MeshBounds.GetCenter();
	return GridSettings.GridAxis == EWallHoleGridAxis::AlongY
		       ? FVector(Centre.X, GridPoint.X, GridPoint.Y)
		       : FVector(GridPoint.X, Centre.Y, GridPoint.Y);
}

int32 UWallHoleDamageComponent::GetCellIndexAt(const FVector& MeshPoint) const
{
	const FVector2D GridPoint = GetGridPoint(MeshPoint);
	const FVector2D GridMin = GetGridPoint(M_MeshBounds.Min);
	const int32 Column = FMath::Clamp(FMath::FloorToInt32((GridPoint.X - GridMin.X) / GridSettings.CellSize),
	                                  0, M_Columns - 1);
	const int32 Row = FMath::Clamp(FMath::FloorToInt32((GridPoint.Y - GridMin.Y) / GridSettings.CellSize),
	                               0, M_Rows - 1);
	return Column + Row * M_Columns;
}

void UWallHoleDamageComponent::GetCellRect(const int32 CellIndex, FVector2D& OutMin, FVector2D& OutMax) const
{
	const int32 Column = CellIndex % M_Columns;
	const int32 Row = CellIndex / M_Columns;
	const FVector2D GridMin = GetGridPoint(M_MeshBounds.Min);
	const FVector2D GridMax = GetGridPoint(M_MeshBounds.Max);
	OutMin = GridMin + FVector2D(Column, Row) * GridSettings.CellSize;
	OutMax.X = Column == M_Columns - 1 ? GridMax.X : OutMin.X + GridSettings.CellSize;
	OutMax.Y = Row == M_Rows - 1 ? GridMax.Y : OutMin.Y + GridSettings.CellSize;
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

bool UWallHoleDamageComponent::GetIsMeshPointRemoved(const FVector& MeshPoint) const
{
	if (GetHasGrid() && GetIsCellHidden(GetCellIndexAt(MeshPoint)))
	{
		return true;
	}
	for (const FWallHole& Hole : M_Holes)
	{
		if (FVector::DistSquared(MeshPoint, Hole.Centre) < FMath::Square(Hole.Radius))
		{
			return true;
		}
	}
	return false;
}

float UWallHoleDamageComponent::GetRemovedAreaEstimate() const
{
	float Area = 0.f;
	for (const int32 CellIndex : M_CellOrder)
	{
		FVector2D CellMin;
		FVector2D CellMax;
		GetCellRect(CellIndex, CellMin, CellMax);
		Area += static_cast<float>((CellMax.X - CellMin.X) * (CellMax.Y - CellMin.Y));
	}
	for (const FWallHole& Hole : M_Holes)
	{
		Area += UE_PI * FMath::Square(Hole.Radius);
	}
	return Area;
}

float UWallHoleDamageComponent::GetFaceArea() const
{
	const FVector Size = M_MeshBounds.GetSize();
	if (GridSettings.GridAxis == EWallHoleGridAxis::AlongX)
	{
		return static_cast<float>(Size.X * Size.Z);
	}
	if (GridSettings.GridAxis == EWallHoleGridAxis::AlongY)
	{
		return static_cast<float>(Size.Y * Size.Z);
	}
	// A tower or block: one side of each axis.
	return static_cast<float>((Size.X + Size.Y) * Size.Z);
}

float UWallHoleDamageComponent::GetAllowedRemovedArea(const float HealthFractionAfter) const
{
	const float LostHealth01 = 1.f - FMath::Clamp(HealthFractionAfter, 0.f, 1.f);
	return LostHealth01 * HoleSettings.MaxHiddenShareWhileAlive * GetFaceArea();
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
		const FVector Centre = bIsUsed ? M_Holes[HoleIndex].Centre : FVector::ZeroVector;
		const int32 First = FirstHolePrimitiveDataIndex + HoleIndex * FloatsPerHole;
		M_WallMesh->SetCustomPrimitiveDataFloat(First, static_cast<float>(Centre.X));
		M_WallMesh->SetCustomPrimitiveDataFloat(First + 1, static_cast<float>(Centre.Y));
		M_WallMesh->SetCustomPrimitiveDataFloat(First + 2, static_cast<float>(Centre.Z));
		// Radius 0 switches the slot off in the material.
		M_WallMesh->SetCustomPrimitiveDataFloat(First + 3, bIsUsed ? M_Holes[HoleIndex].Radius : 0.f);
	}
}

void UWallHoleDamageComponent::SwapToDamagedMaterials()
{
	// Intact walls keep their opaque materials; masked surfaces lose early-Z in the depth and shadow passes.
	// Checked on every hole, not once: building expansions put their own materials back when construction or
	// unpacking finishes.
	if (not GetIsValidWallMesh())
	{
		return;
	}
	M_IntactMaterials.SetNum(FMath::Max(M_IntactMaterials.Num(), DamagedMaterials.Num()));
	for (int32 SlotIndex = 0; SlotIndex < DamagedMaterials.Num(); ++SlotIndex)
	{
		UMaterialInterface* const Damaged = DamagedMaterials[SlotIndex];
		if (IsValid(Damaged) && M_WallMesh->GetMaterial(SlotIndex) != Damaged)
		{
			M_IntactMaterials[SlotIndex] = M_WallMesh->GetMaterial(SlotIndex);
			M_WallMesh->SetMaterial(SlotIndex, Damaged);
		}
	}
}

void UWallHoleDamageComponent::RestoreIntactMaterials()
{
	if (not GetIsValidWallMesh())
	{
		return;
	}
	for (int32 SlotIndex = 0; SlotIndex < M_IntactMaterials.Num(); ++SlotIndex)
	{
		UMaterialInterface* const Intact = M_IntactMaterials[SlotIndex];
		const bool bShowsDamaged = DamagedMaterials.IsValidIndex(SlotIndex)
			&& M_WallMesh->GetMaterial(SlotIndex) == DamagedMaterials[SlotIndex];
		// A slot the owner re-skinned since (construction, unpacking) keeps its new material.
		if (IsValid(Intact) && bShowsDamaged)
		{
			M_WallMesh->SetMaterial(SlotIndex, Intact);
		}
	}
	M_IntactMaterials.Reset();
}

void UWallHoleDamageComponent::GetCollapseSampleBoxes(TArray<FBox>& OutBoxes, TArray<float>& OutWeights) const
{
	OutBoxes.Reset();
	OutWeights.Reset();
	const UStaticMesh* const Mesh = M_WallMesh->GetStaticMesh();
	const UBodySetup* const BodySetup = IsValid(Mesh) ? Mesh->GetBodySetup() : nullptr;
	if (IsValid(BodySetup))
	{
		for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
		{
			OutBoxes.Add(BoxElem.CalcAABB(FTransform::Identity, 1.f));
		}
		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			OutBoxes.Add(ConvexElem.ElemBox);
		}
	}
	// Only boxes inside the render bounds; none at all means the bounds themselves.
	OutBoxes.RemoveAll([this](const FBox& Box) { return not Box.IsValid || not Box.Intersect(M_MeshBounds); });
	if (OutBoxes.IsEmpty())
	{
		OutBoxes.Add(M_MeshBounds);
	}
	for (FBox& Box : OutBoxes)
	{
		Box = Box.Overlap(M_MeshBounds);
		const FVector Size = Box.GetSize();
		OutWeights.Add(static_cast<float>(FMath::Max((Size.X + Size.Y) * Size.Z, 1.0)));
	}
}

bool UWallHoleDamageComponent::GetIsOwnerInTransition() const
{
	const ABuildingExpansion* const BuildingOwner = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(BuildingOwner))
	{
		return false;
	}
	// These states show construction or placement materials and meshes; holes would replace them. Placed expansions
	// that never ran construction keep their other states and do get holes.
	switch (BuildingOwner->GetBuildingExpansionStatus())
	{
	case EBuildingExpansionStatus::BXS_LookingForPlacement:
	case EBuildingExpansionStatus::BXS_BeingBuild:
	case EBuildingExpansionStatus::BXS_BeingPackedUp:
	case EBuildingExpansionStatus::BXS_LookingForUnpackingLocation:
	case EBuildingExpansionStatus::BXS_BeingUnpacked:
		return true;
	default:
		return false;
	}
}

void UWallHoleDamageComponent::OnDestructibleOwnerDied()
{
	CollapseIntoPieces(true);
}

void UWallHoleDamageComponent::OnBuildingOwnerDied()
{
	CollapseIntoPieces(false);
}

void UWallHoleDamageComponent::CollapseIntoPieces(const bool bHideAndDestroyOwner)
{
	using namespace WallHoleDamageConstants;
	if (bM_HasCollapsed || not GetIsValidWallMesh())
	{
		return;
	}
	bM_HasCollapsed = true;
	EnsureWallShapeIsCurrent();
	const float Spacing = FMath::Max(GridSettings.CollapsePieceSpacing, 1.f);
	const int32 PieceBudget = FMath::Clamp(FMath::RoundToInt32(GetFaceArea() / FMath::Square(Spacing)), 1,
	                                       FMath::Max(GridSettings.MaxCollapsePieces, 1));
	const float PieceArea = GetFaceArea() / PieceBudget;
	TArray<FWallDebrisPiece> Pieces;
	TArray<FBox> SampleBoxes;
	TArray<float> SampleWeights;
	GetCollapseSampleBoxes(SampleBoxes, SampleWeights);
	float TotalWeight = 0.f;
	for (const float Weight : SampleWeights)
	{
		TotalWeight += Weight;
	}
	for (int32 SampleIndex = 0; SampleIndex < PieceBudget; ++SampleIndex)
	{
		// Walls: a jittered spread over the face; other shapes: inside a collision element, by its side area.
		float Pick = FMath::FRand() * TotalWeight;
		int32 BoxIndex = 0;
		while (BoxIndex < SampleBoxes.Num() - 1 && Pick > SampleWeights[BoxIndex])
		{
			Pick -= SampleWeights[BoxIndex];
			++BoxIndex;
		}
		const FBox& Box = SampleBoxes[BoxIndex];
		const FVector Random(FMath::FRand(), FMath::FRand(), FMath::FRand());
		FVector Sample = Box.Min + Random * Box.GetSize();
		if (GetHasGrid())
		{
			const FVector2D GridMin = GetGridPoint(M_MeshBounds.Min);
			const FVector2D GridSize = GetGridPoint(M_MeshBounds.Max) - GridMin;
			const int32 Columns = FMath::Max(1, FMath::RoundToInt32(FMath::Sqrt(PieceBudget * GridSize.X / FMath::Max(GridSize.Y, 1.0))));
			const int32 Rows = FMath::Max(1, FMath::DivideAndRoundUp(PieceBudget, Columns));
			const float Jitter = Spacing * CollapseJitterShare;
			Sample = GetMeshPointFromGrid(GridMin + FVector2D(
				(SampleIndex % Columns + 0.5) * GridSize.X / Columns + FMath::FRandRange(-Jitter, Jitter),
				(SampleIndex / Columns % Rows + 0.5) * GridSize.Y / Rows + FMath::FRandRange(-Jitter, Jitter)));
		}
		if (not GetIsMeshPointRemoved(Sample))
		{
			AddPiece(Sample, PieceArea, Pieces);
		}
	}
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector Centre = M_WallMesh->Bounds.Origin;
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, DebrisMeshes, Pieces,
	                                    MeshTransform.GetRotation(), Centre, M_LastShotDirection,
	                                    CollapseSettings.DeathSpeedScale);
	UE_LOG(LogWallHoleDamage, Display, TEXT("RTS_WALL_HOLES collapse %s threw=%d hide=%d"), *GetNameSafe(GetOwner()),
	       Pieces.Num(), bHideAndDestroyOwner ? 1 : 0);
	if (not bHideAndDestroyOwner)
	{
		return;
	}
	// Nothing of the wall is left to draw or to hit.
	M_WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_WallMesh->SetVisibility(false, false);
	WallDamageShared::PlayCollapseSound(*GetOwner(), CollapseSettings, Centre);
	GetOwner()->SetLifeSpan(FMath::Max(CollapseSettings.DestroyOwnerDelaySeconds, UE_KINDA_SMALL_NUMBER));
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
