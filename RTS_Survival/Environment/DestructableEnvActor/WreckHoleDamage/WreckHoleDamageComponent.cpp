// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WreckHoleDamageComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "WreckHoleMaterialMap.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckHoleDamage, Log, All);

namespace WreckHoleDamageConstants
{
	// Metal, not concrete: 37 mm -> 15 cm, 75 mm -> 30 cm, 152 mm -> 61 cm.
	constexpr int32 DefaultMinCalibreMm = 30;
	constexpr float DefaultHoleRadiusPerCalibreMm = 0.4f;
	constexpr float DefaultHoleRadiusPerSqrtDamage = 2.5f;
	constexpr float DefaultMaxHoleRadius = 90.f;
	constexpr float DefaultMaxHiddenShareWhileAlive = 0.5f;
	// A bent plate fills about this share of the box of its two largest sides.
	constexpr float DebrisMeshFillShare = 0.6f;
	constexpr float MinPieceScale = 0.08f;
	constexpr float MaxPieceScale = 0.6f;
	constexpr float MinPieceScaleJitter = 0.7f;
	constexpr float MaxPieceScaleJitter = 1.15f;
	// Shards start inside this share of the hole radius, a little off the surface so they don't start buried.
	constexpr float ShardScatterShare = 0.8f;
	constexpr float ShardSurfaceOffset = 4.f;
	// A grown hole must gain at least this much radius to count as damage.
	constexpr float MinRadiusGain = 1.f;
	constexpr float SquareCentimetresPerSquareMetre = 10000.f;
}

UWreckHoleDamageComponent::UWreckHoleDamageComponent()
{
	using namespace WreckHoleDamageConstants;
	PrimaryComponentTick.bCanEverTick = false;
	M_DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AddOnArmorPanelDebris::SystemPath));
	HoleSettings.MinCalibreMm = DefaultMinCalibreMm;
	HoleSettings.HoleRadiusPerCalibreMm = DefaultHoleRadiusPerCalibreMm;
	HoleSettings.HoleRadiusPerSqrtDamage = DefaultHoleRadiusPerSqrtDamage;
	HoleSettings.MaxHoleRadius = DefaultMaxHoleRadius;
	HoleSettings.MaxHiddenShareWhileAlive = DefaultMaxHiddenShareWhileAlive;
}

void UWreckHoleDamageComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_GatherMeshes();
	if (M_MeshStates.IsEmpty())
	{
		return;
	}
	BeginPlay_CacheDebrisAreas();
	M_LoadedDebrisSystem = M_DebrisSystem.LoadSynchronous();
	(void)GetIsValidDebrisSystem();
}

void UWreckHoleDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TSharedPtr<FStreamableHandle>& Handle : M_MaterialLoadHandles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}
	M_MaterialLoadHandles.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWreckHoleDamageComponent::BeginPlay_GatherMeshes()
{
	M_MeshStates.Reset();
	if (not IsValid(MaterialMap))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "MaterialMap", "BeginPlay_GatherMeshes",
		                                                             GetOwner());
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(GetOwner());
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent) || not IsValid(MeshComponent->GetStaticMesh()))
		{
			continue;
		}
		const bool bIsNamed = MeshComponentNames.Contains(MeshComponent->GetFName());
		if (not MeshComponentNames.IsEmpty() && not bIsNamed)
		{
			continue;
		}
		// Instanced meshes share one set of custom primitive data and transform for all instances.
		if (MeshComponent->IsA<UInstancedStaticMeshComponent>() && not bIsNamed)
		{
			continue;
		}
		AddMeshState(*MeshComponent);
	}
	UE_LOG(LogWreckHoleDamage, Verbose, TEXT("RTS_WRECK_HOLES %s tracks %d meshes"), *GetNameSafe(GetOwner()),
	       M_MeshStates.Num());
}

bool UWreckHoleDamageComponent::AddMeshState(UStaticMeshComponent& MeshComponent)
{
	FWreckHoleMeshState State;
	bool bHasDamagedSlot = false;
	for (int32 SlotIndex = 0; SlotIndex < MeshComponent.GetNumMaterials(); ++SlotIndex)
	{
		UMaterialInterface* const SlotMaterial = MeshComponent.GetMaterial(SlotIndex);
		const TSoftObjectPtr<UMaterialInterface> Damaged = MaterialMap->FindDamagedMaterial(SlotMaterial);
		State.DamagedMaterials.Add(Damaged);
		bHasDamagedSlot |= not Damaged.IsNull();
	}
	// Meshes whose materials have no masked copy (glass, nets, foliage) would only take invisible holes.
	if (not bHasDamagedSlot)
	{
		return false;
	}
	State.Mesh = &MeshComponent;
	State.LocalBounds = MeshComponent.GetStaticMesh()->GetBoundingBox();
	const FVector Size = State.LocalBounds.GetSize() * MeshComponent.GetComponentScale().GetAbs();
	State.FaceArea = static_cast<float>(2.0 * (Size.X * Size.Y + Size.X * Size.Z + Size.Y * Size.Z));
	// No mask write yet: unset custom primitive data reads 0, which is "no hole".
	M_MeshStates.Add(MoveTemp(State));
	return true;
}

void UWreckHoleDamageComponent::BeginPlay_CacheDebrisAreas()
{
	M_DebrisMeshAreas.Reset();
	for (const TObjectPtr<UStaticMesh>& DebrisMesh : DebrisMeshes)
	{
		FVector Size = IsValid(DebrisMesh) ? DebrisMesh->GetBoundingBox().GetSize() : FVector::ZeroVector;
		// The two largest sides: plates lie in any plane.
		const double Smallest = FMath::Min3(Size.X, Size.Y, Size.Z);
		const double LargestTwo = Size.X * Size.Y * Size.Z / FMath::Max(Smallest, 1.0);
		M_DebrisMeshAreas.Add(static_cast<float>(LargestTwo) * WreckHoleDamageConstants::DebrisMeshFillShare);
	}
}

void UWreckHoleDamageComponent::OnOwnerDamaged(const float DamageAmount, const FDamageEvent& DamageEvent,
                                               const AActor* DamageCauser, const float HealthBefore,
                                               const float HealthAfter)
{
	if (DamageAmount <= 0.f || M_MeshStates.IsEmpty())
	{
		return;
	}
	NoteOwnerHealth(HealthBefore);
	// The wreck dies from this hit: its own death (sink, geometry collection) takes over.
	if (HealthAfter <= 0.f)
	{
		return;
	}
	FVector WorldHit;
	FVector ShotDirection;
	bool bIsDirectHit = false;
	if (not WallDamageShared::ResolveHitLocation(DamageEvent, DamageCauser, WorldHit, ShotDirection, bIsDirectHit))
	{
		return;
	}
	FWreckStruckSurface Surface;
	if (not FindStruckSurface(WorldHit, bIsDirectHit ? ShotDirection : FVector::ZeroVector, Surface))
	{
		return;
	}
	const float SplashDistance = bIsDirectHit ? 0.f : Surface.DistanceFromHit;
	const float HoleRadius = WallDamageShared::ResolveHoleRadius(HoleSettings, DamageAmount, DamageCauser, SplashDistance);
	const float HealthFractionAfter = M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f;
	const float Removed = ApplyHit(Surface, HoleRadius, GetAllowedRemovedArea(HealthFractionAfter));
	UE_LOG(LogWreckHoleDamage, Verbose, TEXT("RTS_WRECK_HOLES hit %s mesh=%s radius=%.0f direct=%d removed=%.2f m2 holes=%d"),
	       *GetNameSafe(GetOwner()), *GetNameSafe(M_MeshStates[Surface.MeshIndex].Mesh), HoleRadius,
	       bIsDirectHit ? 1 : 0, Removed / WreckHoleDamageConstants::SquareCentimetresPerSquareMetre, GetHoleCount());
}

float UWreckHoleDamageComponent::Debug_FireTestHit(const FVector WorldLocation, const FVector ShotDirection,
                                                   const float CalibreMm)
{
	FWreckStruckSurface Surface;
	if (not FindStruckSurface(WorldLocation, ShotDirection.GetSafeNormal(), Surface))
	{
		UE_LOG(LogWreckHoleDamage, Display, TEXT("RTS_WRECK_HOLES test hit %s: no wreck surface near the hit"),
		       *GetNameSafe(GetOwner()));
		return 0.f;
	}
	const float Radius = WallDamageShared::ResolveHoleRadiusFromCalibre(HoleSettings, CalibreMm, 0.f);
	const float Removed = ApplyHit(Surface, Radius, TNumericLimits<float>::Max());
	UE_LOG(LogWreckHoleDamage, Display, TEXT("RTS_WRECK_HOLES test hit %s mesh=%s calibre=%.0f radius=%.0f removed=%.2f m2 holes=%d"),
	       *GetNameSafe(GetOwner()), *GetNameSafe(M_MeshStates[Surface.MeshIndex].Mesh), CalibreMm, Radius,
	       Removed / WreckHoleDamageConstants::SquareCentimetresPerSquareMetre, GetHoleCount());
	return Removed / WreckHoleDamageConstants::SquareCentimetresPerSquareMetre;
}

bool UWreckHoleDamageComponent::FindStruckSurface(const FVector& WorldHit, const FVector& ShotDirection,
                                                  FWreckStruckSurface& OutSurface) const
{
	// Splash has no direction: look from the explosion towards the nearest mesh.
	const FVector TraceDirection = ShotDirection.IsNearlyZero() ? GetDirectionToNearestMesh(WorldHit) : ShotDirection;
	bool bFound = false;
	for (int32 MeshIndex = 0; MeshIndex < M_MeshStates.Num(); ++MeshIndex)
	{
		FWreckStruckSurface Candidate;
		if (TraceMeshSurface(MeshIndex, WorldHit, TraceDirection, Candidate)
			&& (not bFound || Candidate.DistanceFromHit < OutSurface.DistanceFromHit))
		{
			OutSurface = Candidate;
			bFound = true;
		}
	}
	// Meshes without query collision (or a hit beside the mesh): the nearest bounds.
	return bFound || FindNearestBoundsSurface(WorldHit, OutSurface);
}

bool UWreckHoleDamageComponent::TraceMeshSurface(const int32 MeshIndex, const FVector& WorldHit,
                                                 const FVector& TraceDirection,
                                                 FWreckStruckSurface& OutSurface) const
{
	UStaticMeshComponent* const Mesh = M_MeshStates[MeshIndex].Mesh;
	if (not IsValid(Mesh) || not Mesh->IsVisible() || TraceDirection.IsNearlyZero())
	{
		return false;
	}
	const FVector Start = WorldHit - TraceDirection * Tuning.SurfaceProbeDistance;
	const FVector End = WorldHit + TraceDirection * Tuning.SurfaceProbeDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WreckHoleSurface), true);
	QueryParams.bReturnFaceIndex = false;
	FHitResult SurfaceHit;
	// Complex: the visible triangles, not the simple boxes the shell hit.
	if (not Mesh->LineTraceComponent(SurfaceHit, Start, End, QueryParams))
	{
		return false;
	}
	OutSurface.MeshIndex = MeshIndex;
	OutSurface.WorldPoint = SurfaceHit.ImpactPoint;
	OutSurface.WorldNormal = SurfaceHit.ImpactNormal.IsNearlyZero() ? -TraceDirection : SurfaceHit.ImpactNormal;
	// A back face of the complex mesh: shards must still leave towards the shooter, not into the hull.
	if (FVector::DotProduct(OutSurface.WorldNormal, TraceDirection) > 0.0)
	{
		OutSurface.WorldNormal = -OutSurface.WorldNormal;
	}
	OutSurface.DistanceFromHit = static_cast<float>(FVector::Distance(SurfaceHit.ImpactPoint, WorldHit));
	return true;
}

bool UWreckHoleDamageComponent::FindNearestBoundsSurface(const FVector& WorldHit, FWreckStruckSurface& OutSurface) const
{
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 MeshIndex = 0; MeshIndex < M_MeshStates.Num(); ++MeshIndex)
	{
		const FWreckHoleMeshState& State = M_MeshStates[MeshIndex];
		if (not IsValid(State.Mesh) || not State.Mesh->IsVisible())
		{
			continue;
		}
		const FTransform MeshTransform = State.Mesh->GetComponentTransform();
		FVector LocalNormal;
		const FVector LocalPoint = GetBoundsSurfacePoint(State.LocalBounds, MeshTransform.InverseTransformPosition(WorldHit),
		                                                 LocalNormal);
		const FVector WorldPoint = MeshTransform.TransformPosition(LocalPoint);
		const double Distance = FVector::Distance(WorldPoint, WorldHit);
		if (Distance >= BestDistance || Distance > Tuning.SurfaceProbeDistance)
		{
			continue;
		}
		BestDistance = Distance;
		OutSurface.MeshIndex = MeshIndex;
		OutSurface.WorldPoint = WorldPoint;
		OutSurface.WorldNormal = MeshTransform.TransformVectorNoScale(LocalNormal);
		OutSurface.DistanceFromHit = static_cast<float>(Distance);
	}
	return OutSurface.MeshIndex != INDEX_NONE;
}

FVector UWreckHoleDamageComponent::GetBoundsSurfacePoint(const FBox& Bounds, const FVector& LocalPoint,
                                                         FVector& OutLocalNormal)
{
	const FVector Clamped = Bounds.GetClosestPointTo(LocalPoint);
	if (not Bounds.IsInsideOrOn(LocalPoint))
	{
		const FVector FromCentre = Clamped - Bounds.GetCenter();
		// The face the clamp landed on: the axis where the point sits furthest out, relative to the box size.
		const FVector Relative = FromCentre / Bounds.GetExtent().ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
		const int32 Axis = FMath::Abs(Relative.X) >= FMath::Max(FMath::Abs(Relative.Y), FMath::Abs(Relative.Z))
			                   ? 0
			                   : (FMath::Abs(Relative.Y) >= FMath::Abs(Relative.Z) ? 1 : 2);
		OutLocalNormal = FVector::ZeroVector;
		OutLocalNormal[Axis] = FMath::Sign(Relative[Axis]);
		return Clamped;
	}
	// Inside the box: out through the nearest face, so the hole lands on the surface, not in the volume.
	const FVector ToMin = LocalPoint - Bounds.Min;
	const FVector ToMax = Bounds.Max - LocalPoint;
	int32 BestAxis = 0;
	bool bTowardsMax = ToMax.X < ToMin.X;
	double BestGap = FMath::Min(ToMin.X, ToMax.X);
	for (int32 Axis = 1; Axis < 3; ++Axis)
	{
		const double Gap = FMath::Min(ToMin[Axis], ToMax[Axis]);
		if (Gap < BestGap)
		{
			BestGap = Gap;
			BestAxis = Axis;
			bTowardsMax = ToMax[Axis] < ToMin[Axis];
		}
	}
	FVector SurfacePoint = LocalPoint;
	SurfacePoint[BestAxis] = bTowardsMax ? Bounds.Max[BestAxis] : Bounds.Min[BestAxis];
	OutLocalNormal = FVector::ZeroVector;
	OutLocalNormal[BestAxis] = bTowardsMax ? 1.0 : -1.0;
	return SurfacePoint;
}

FVector UWreckHoleDamageComponent::GetDirectionToNearestMesh(const FVector& WorldHit) const
{
	double BestDistanceSquared = TNumericLimits<double>::Max();
	FVector Direction = -FVector::UpVector;
	for (const FWreckHoleMeshState& State : M_MeshStates)
	{
		if (not IsValid(State.Mesh))
		{
			continue;
		}
		const FVector Centre = State.Mesh->Bounds.Origin;
		const double DistanceSquared = FVector::DistSquared(Centre, WorldHit);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Direction = (Centre - WorldHit).GetSafeNormal(UE_KINDA_SMALL_NUMBER, -FVector::UpVector);
		}
	}
	return Direction;
}

float UWreckHoleDamageComponent::ApplyHit(const FWreckStruckSurface& Surface, const float HoleRadius,
                                          const float AllowedArea)
{
	if (not M_MeshStates.IsValidIndex(Surface.MeshIndex) || HoleRadius <= 0.f)
	{
		return 0.f;
	}
	FWreckHoleMeshState& State = M_MeshStates[Surface.MeshIndex];
	if (not IsValid(State.Mesh))
	{
		return 0.f;
	}
	const float AvailableArea = AllowedArea - GetRemovedAreaEstimate();
	if (AvailableArea <= 0.f)
	{
		return 0.f;
	}
	// Hole sizes are world centimetres; the mask works in mesh space.
	const float CappedWorldRadius = FMath::Min(HoleRadius, FMath::Sqrt(AvailableArea / UE_PI));
	if (CappedWorldRadius < Tuning.MinHoleRadius)
	{
		return 0.f;
	}
	const FTransform MeshTransform = State.Mesh->GetComponentTransform();
	const float WorldScale = GetWorldScale(State);
	const FVector MeshPoint = MeshTransform.InverseTransformPosition(Surface.WorldPoint);
	const float MeshRadius = FMath::Min(CappedWorldRadius / WorldScale, GetZoneRadiusLimit(MeshPoint));
	if (MeshRadius * WorldScale < Tuning.MinHoleRadius)
	{
		return 0.f;
	}
	const FWreckHoleCut Cut = CutHole(State, MeshPoint, MeshRadius, AvailableArea);
	if (Cut.RemovedArea <= 0.f)
	{
		return 0.f;
	}
	WriteMask(State);
	RequestDamagedMaterials(Surface.MeshIndex);
	// Shards come from the ring that was removed, around the hole's centre on the struck surface.
	FWreckStruckSurface ShardSurface = Surface;
	ShardSurface.WorldPoint = MeshTransform.TransformPosition(Cut.MeshCentre);
	TArray<FWallDebrisPiece> Pieces;
	AddShards(ShardSurface, Cut.InnerRadius * WorldScale, Cut.OuterRadius * WorldScale, Pieces);
	// The outward normal doubles as the "shot": WallDamageShared throws pieces out of the face the shot crossed.
	const FQuat PieceRotation = FRotationMatrix::MakeFromY(Surface.WorldNormal).ToQuat();
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, DebrisMeshes, Pieces, PieceRotation,
	                                    ShardSurface.WorldPoint, Surface.WorldNormal, Tuning.DebrisSpeedScale);
	return Cut.RemovedArea;
}

float UWreckHoleDamageComponent::GetZoneRadiusLimit(const FVector& MeshPoint) const
{
	if (MeshSpaceHoleZones.IsEmpty())
	{
		return TNumericLimits<float>::Max();
	}
	float BestLimit = 0.f;
	for (const FBox& Zone : MeshSpaceHoleZones)
	{
		if (not Zone.IsValid || not Zone.IsInsideOrOn(MeshPoint))
		{
			continue;
		}
		// Distance to the nearest face keeps the whole sphere inside the zone.
		const FVector ToMin = MeshPoint - Zone.Min;
		const FVector ToMax = Zone.Max - MeshPoint;
		const float FaceDistance = static_cast<float>(FMath::Min(ToMin.GetMin(), ToMax.GetMin()));
		BestLimit = FMath::Max(BestLimit, FaceDistance);
	}
	return BestLimit;
}

FWreckHoleCut UWreckHoleDamageComponent::CutHole(FWreckHoleMeshState& State, const FVector& MeshPoint,
                                                 const float Radius, const float AvailableArea) const
{
	const float WorldScale = GetWorldScale(State);
	const float AreaScale = FMath::Square(WorldScale);
	const float MaxRadius = HoleSettings.MaxHoleRadius / WorldScale;
	FWreckHoleCut Cut;
	Cut.MeshCentre = MeshPoint;
	// A hit inside an existing hole widens it instead of taking a slot.
	for (FWreckHole& Hole : State.Holes)
	{
		const float Distance = static_cast<float>(FVector::Distance(MeshPoint, Hole.Centre));
		if (Distance >= Hole.Radius)
		{
			continue;
		}
		const float OldRadius = Hole.Radius;
		// The ring may not open more than the health cap still allows.
		const float CapRadius = FMath::Sqrt(FMath::Square(OldRadius) + AvailableArea / (UE_PI * AreaScale));
		const float ZoneRadius = FMath::Max(OldRadius, GetZoneRadiusLimit(Hole.Centre));
		const float NewRadius = FMath::Min3(FMath::Min(MaxRadius, ZoneRadius), CapRadius,
		                                    FMath::Max(OldRadius, Distance + Radius * Tuning.GrowRadiusShare));
		if (NewRadius < OldRadius + WreckHoleDamageConstants::MinRadiusGain / WorldScale)
		{
			return Cut;
		}
		Hole.Radius = NewRadius;
		Cut.MeshCentre = Hole.Centre;
		Cut.InnerRadius = OldRadius;
		Cut.OuterRadius = NewRadius;
		Cut.RemovedArea = UE_PI * (FMath::Square(NewRadius) - FMath::Square(OldRadius)) * AreaScale;
		return Cut;
	}
	if (State.Holes.Num() >= WreckHoleMask::MaxHoles)
	{
		FreeOldestSlot(State);
	}
	FWreckHole& NewHole = State.Holes.AddDefaulted_GetRef();
	NewHole.Centre = MeshPoint;
	NewHole.Radius = Radius;
	Cut.OuterRadius = Radius;
	Cut.RemovedArea = UE_PI * FMath::Square(Radius) * AreaScale;
	return Cut;
}

void UWreckHoleDamageComponent::FreeOldestSlot(FWreckHoleMeshState& State) const
{
	if (State.Holes.Num() < 2)
	{
		State.Holes.Reset();
		return;
	}
	const FWreckHole Oldest = State.Holes[0];
	int32 NearestIndex = 1;
	for (int32 HoleIndex = 2; HoleIndex < State.Holes.Num(); ++HoleIndex)
	{
		if (FVector::DistSquared(State.Holes[HoleIndex].Centre, Oldest.Centre)
			< FVector::DistSquared(State.Holes[NearestIndex].Centre, Oldest.Centre))
		{
			NearestIndex = HoleIndex;
		}
	}
	FWreckHole& Nearest = State.Holes[NearestIndex];
	const float Distance = static_cast<float>(FVector::Distance(Nearest.Centre, Oldest.Centre));
	// Holes that touch merge into one sphere around both; a lone old hole is dropped instead, so a merge never
	// opens intact metal that was never hit.
	if (Distance <= Oldest.Radius + Nearest.Radius)
	{
		const float EnclosingRadius = FMath::Max3(Oldest.Radius, Nearest.Radius,
		                                          (Distance + Oldest.Radius + Nearest.Radius) * 0.5f);
		const float NewRadius = FMath::Min(EnclosingRadius, HoleSettings.MaxHoleRadius / GetWorldScale(State));
		if (Distance > UE_KINDA_SMALL_NUMBER && NewRadius > Nearest.Radius)
		{
			const FVector TowardOldest = (Oldest.Centre - Nearest.Centre) / Distance;
			Nearest.Centre += TowardOldest * FMath::Min(Distance, NewRadius - Nearest.Radius);
		}
		Nearest.Radius = FMath::Max(Nearest.Radius, NewRadius);
	}
	State.Holes.RemoveAt(0);
}

void UWreckHoleDamageComponent::AddShards(const FWreckStruckSurface& Surface, const float InnerRadius,
                                          const float OuterRadius, TArray<FWallDebrisPiece>& OutPieces) const
{
	using namespace WreckHoleDamageConstants;
	const float RingArea = UE_PI * (FMath::Square(OuterRadius) - FMath::Square(InnerRadius));
	if (DebrisMeshes.IsEmpty() || M_DebrisMeshAreas.Num() != DebrisMeshes.Num() || RingArea <= 0.f)
	{
		return;
	}
	const int32 PieceCount = FMath::Clamp(FMath::RoundToInt32(RingArea / FMath::Max(Tuning.AreaPerDebrisPiece, 1.f)),
	                                      1, FMath::Max(Tuning.MaxPiecesPerHit, 1));
	const float PieceArea = RingArea / PieceCount;
	FVector TangentA;
	FVector TangentB;
	Surface.WorldNormal.FindBestAxisVectors(TangentA, TangentB);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		// Somewhere in the removed ring on the struck surface, lifted off it a little.
		const float Distance = FMath::Sqrt(FMath::FRandRange(FMath::Square(InnerRadius), FMath::Square(OuterRadius)))
			* ShardScatterShare;
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		FWallDebrisPiece& Piece = OutPieces.AddDefaulted_GetRef();
		Piece.MeshIndex = FMath::RandRange(0, DebrisMeshes.Num() - 1);
		Piece.WorldCentre = Surface.WorldPoint + (TangentA * FMath::Cos(Angle) + TangentB * FMath::Sin(Angle)) * Distance
			+ Surface.WorldNormal * ShardSurfaceOffset;
		const float MeshArea = FMath::Max(M_DebrisMeshAreas[Piece.MeshIndex], 1.f);
		const float Scale = FMath::Sqrt(PieceArea / MeshArea) * FMath::FRandRange(MinPieceScaleJitter, MaxPieceScaleJitter);
		Piece.Scale = FMath::Clamp(Scale, MinPieceScale, MaxPieceScale);
	}
}

float UWreckHoleDamageComponent::GetRemovedAreaEstimate() const
{
	float Area = 0.f;
	for (const FWreckHoleMeshState& State : M_MeshStates)
	{
		const float AreaScale = FMath::Square(GetWorldScale(State));
		for (const FWreckHole& Hole : State.Holes)
		{
			Area += UE_PI * FMath::Square(Hole.Radius) * AreaScale;
		}
	}
	return Area;
}

float UWreckHoleDamageComponent::GetTotalFaceArea() const
{
	float Area = 0.f;
	for (const FWreckHoleMeshState& State : M_MeshStates)
	{
		Area += State.FaceArea;
	}
	return Area;
}

float UWreckHoleDamageComponent::GetAllowedRemovedArea(const float HealthFractionAfter) const
{
	const float LostHealth01 = 1.f - FMath::Clamp(HealthFractionAfter, 0.f, 1.f);
	return LostHealth01 * HoleSettings.MaxHiddenShareWhileAlive * GetTotalFaceArea();
}

float UWreckHoleDamageComponent::GetWorldScale(const FWreckHoleMeshState& State) const
{
	if (not IsValid(State.Mesh))
	{
		return 1.f;
	}
	return FMath::Max(static_cast<float>(State.Mesh->GetComponentTransform().GetMaximumAxisScale()), UE_KINDA_SMALL_NUMBER);
}

int32 UWreckHoleDamageComponent::GetHoleCount() const
{
	int32 Count = 0;
	for (const FWreckHoleMeshState& State : M_MeshStates)
	{
		Count += State.Holes.Num();
	}
	return Count;
}

void UWreckHoleDamageComponent::WriteMask(const FWreckHoleMeshState& State) const
{
	using namespace WreckHoleMask;
	if (not IsValid(State.Mesh))
	{
		return;
	}
	for (int32 HoleIndex = 0; HoleIndex < MaxHoles; ++HoleIndex)
	{
		const bool bIsUsed = State.Holes.IsValidIndex(HoleIndex);
		const FVector Centre = bIsUsed ? State.Holes[HoleIndex].Centre : FVector::ZeroVector;
		const int32 First = FirstHolePrimitiveDataIndex + HoleIndex * FloatsPerHole;
		State.Mesh->SetCustomPrimitiveDataFloat(First, static_cast<float>(Centre.X));
		State.Mesh->SetCustomPrimitiveDataFloat(First + 1, static_cast<float>(Centre.Y));
		State.Mesh->SetCustomPrimitiveDataFloat(First + 2, static_cast<float>(Centre.Z));
		// Radius 0 switches the slot off in the material.
		State.Mesh->SetCustomPrimitiveDataFloat(First + 3, bIsUsed ? State.Holes[HoleIndex].Radius : 0.f);
	}
}

void UWreckHoleDamageComponent::RequestDamagedMaterials(const int32 MeshIndex)
{
	FWreckHoleMeshState& State = M_MeshStates[MeshIndex];
	// Intact wrecks keep their opaque materials (masked surfaces lose early-Z in the depth and shadow passes).
	if (State.bHasRequestedDamagedMaterials)
	{
		return;
	}
	State.bHasRequestedDamagedMaterials = true;
	// Every path, loaded or not: the handle then holds them all until the wreck ends.
	TArray<FSoftObjectPath> PathsToLoad;
	for (const TSoftObjectPtr<UMaterialInterface>& Damaged : State.DamagedMaterials)
	{
		if (not Damaged.IsNull())
		{
			PathsToLoad.AddUnique(Damaged.ToSoftObjectPath());
		}
	}
	if (PathsToLoad.IsEmpty())
	{
		return;
	}
	// The mask is written already; the hole shows once the masked materials are in (at once when loaded).
	TWeakObjectPtr<UWreckHoleDamageComponent> WeakThis(this);
	const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PathsToLoad, FStreamableDelegate::CreateLambda([WeakThis, MeshIndex]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ApplyDamagedMaterials(MeshIndex);
			}
		}));
	if (Handle.IsValid())
	{
		M_MaterialLoadHandles.Add(Handle);
	}
}

void UWreckHoleDamageComponent::ApplyDamagedMaterials(const int32 MeshIndex)
{
	if (not M_MeshStates.IsValidIndex(MeshIndex) || not IsValid(MaterialMap))
	{
		return;
	}
	const FWreckHoleMeshState& State = M_MeshStates[MeshIndex];
	if (not IsValid(State.Mesh))
	{
		return;
	}
	for (int32 SlotIndex = 0; SlotIndex < State.DamagedMaterials.Num(); ++SlotIndex)
	{
		ApplyDamagedMaterialToSlot(*State.Mesh, SlotIndex, State.DamagedMaterials[SlotIndex]);
	}
}

void UWreckHoleDamageComponent::ApplyDamagedMaterialToSlot(UStaticMeshComponent& Mesh, const int32 SlotIndex,
                                                           const TSoftObjectPtr<UMaterialInterface>& Damaged) const
{
	UMaterialInterface* const DamagedMaterial = Damaged.Get();
	UMaterialInterface* const Current = Mesh.GetMaterial(SlotIndex);
	// Through the map, so a dynamic instance made from the same material still matches; a slot that another
	// effect gave a different material keeps it.
	if (not IsValid(DamagedMaterial) || MaterialMap->FindDamagedMaterial(Current) != Damaged)
	{
		return;
	}
	UMaterialInstanceDynamic* const CurrentDynamic = Cast<UMaterialInstanceDynamic>(Current);
	if (not IsValid(CurrentDynamic))
	{
		Mesh.SetMaterial(SlotIndex, DamagedMaterial);
		return;
	}
	// Keep the runtime parameters (camo, char) of a dynamic slot.
	UMaterialInstanceDynamic* const DamagedDynamic = UMaterialInstanceDynamic::Create(DamagedMaterial, &Mesh);
	DamagedDynamic->CopyParameterOverrides(CurrentDynamic);
	Mesh.SetMaterial(SlotIndex, DamagedDynamic);
}

bool UWreckHoleDamageComponent::GetIsValidDebrisSystem() const
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
