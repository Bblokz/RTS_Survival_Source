// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentDestructibleActor.h"

#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ApartmentDestructibleActor
{
	// The reported hit location can sit slightly inside or outside the wall, so the piece search spans both sides.
	constexpr float PieceTraceBacktrack = 300.f;
	constexpr float PieceTraceOvershoot = 400.f;
	// Keeps panels that start slightly below the actor's origin inside the anchored volume.
	constexpr float AnchorDepthBelowOrigin = 10.f;
}

AApartmentDestructibleActor::AApartmentDestructibleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	M_GeometryCollection = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	SetRootComponent(M_GeometryCollection);
	// Dynamic with a pinned ground storey; a kinematic collection registers breaks but its freed pieces do not fall.
	M_GeometryCollection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	M_GeometryCollection->bEnableDamageFromCollision = false;
	M_GeometryCollection->SetCanEverAffectNavigation(false);
	M_GeometryCollection->SetReceivesDecals(false);
	M_GeometryCollection->SetCollisionObjectType(ECC_Destructible);
	M_GeometryCollection->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	M_GeometryCollection->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Freed pieces land on the ground and on the rest of the building.
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	// Block visibility for identifying this building when clicked by the player.
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Block weapons.
	M_GeometryCollection->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	M_GeometryCollection->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
	M_GeometryCollection->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
}

void AApartmentDestructibleActor::BeginPlay()
{
	Super::BeginPlay();
	if (not GetIsValidGeometryCollection())
	{
		return;
	}
	if (M_GeometryCollection->GetRestCollection() == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"No rest collection set on the geometry component of apartment " + GetName() + "; it cannot be destroyed.");
		return;
	}

	BeginPlay_SetupWeaponOnlyDamage();
	BeginPlay_AnchorGroundStorey();
}

void AApartmentDestructibleActor::BeginPlay_SetupWeaponOnlyDamage()
{
	// By default a breaking piece passes its full damage on to every connected neighbour; with one shared panel
	// health that chain-reacts through the whole building, so only the pieces a weapon damaged may break.
	FGeometryCollectionDamagePropagationData DamagePropagation = M_GeometryCollection->DamagePropagationData;
	DamagePropagation.bEnabled = false;
	M_GeometryCollection->SetDamagePropagationData(DamagePropagation);
	// Panels sit one level below the root; this is the strain a hit must exceed to free one.
	M_GeometryCollection->DamageThreshold = {DestructionSettings.PanelHealth};
	M_GeometryCollection->RecreatePhysicsState();
}

void AApartmentDestructibleActor::BeginPlay_AnchorGroundStorey()
{
	using namespace ApartmentDestructibleActor;
	const FBoxSphereBounds BuildingBounds = M_GeometryCollection->Bounds;
	const double OriginHeight = GetActorLocation().Z;
	const FVector AnchorMin(BuildingBounds.Origin.X - BuildingBounds.BoxExtent.X,
	                        BuildingBounds.Origin.Y - BuildingBounds.BoxExtent.Y,
	                        OriginHeight - AnchorDepthBelowOrigin);
	const FVector AnchorMax(BuildingBounds.Origin.X + BuildingBounds.BoxExtent.X,
	                        BuildingBounds.Origin.Y + BuildingBounds.BoxExtent.Y,
	                        OriginHeight + DestructionSettings.AnchoredGroundStoreyHeight);
	M_GeometryCollection->SetAnchoredByBox(FBox(AnchorMin, AnchorMax), true);
	M_GeometryCollection->SetSimulatePhysics(true);
}

float AApartmentDestructibleActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                              AController* EventInstigator, AActor* DamageCauser)
{
	// Never return 0: weapons read that as a kill, and this building has no single moment of death.
	const float ReportedDamage = FMath::Max(DamageAmount, UE_KINDA_SMALL_NUMBER);
	if (DamageAmount <= 0.f || not GetIsValidGeometryCollection())
	{
		return ReportedDamage;
	}

	const FVector ImpactLocation = GetImpactLocation(DamageEvent, DamageCauser);
	const FVector ShotDirection = GetShotDirection(DamageEvent, DamageCauser, ImpactLocation);
	FHitResult PieceHit;
	if (not TryFindHitPiece(ImpactLocation, ShotDirection, PieceHit))
	{
		return ReportedDamage;
	}

	M_GeometryCollection->ApplyInternalStrain(PieceHit.Item, PieceHit.ImpactPoint, DestructionSettings.StrainRadius,
	                                          DestructionSettings.StrainPropagationDepth,
	                                          DestructionSettings.StrainPropagationFactor, DamageAmount);
	M_GeometryCollection->ApplyBreakingLinearVelocity(PieceHit.Item,
	                                                  ShotDirection * DestructionSettings.BreakingPieceSpeed);
	return ReportedDamage;
}

bool AApartmentDestructibleActor::TryFindHitPiece(const FVector& ImpactLocation, const FVector& ShotDirection,
                                                  FHitResult& OutHit) const
{
	using namespace ApartmentDestructibleActor;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ApartmentFindHitPiece), true);
	return M_GeometryCollection->LineTraceComponent(
		OutHit,
		ImpactLocation - ShotDirection * PieceTraceBacktrack,
		ImpactLocation + ShotDirection * PieceTraceOvershoot,
		QueryParams);
}

FVector AApartmentDestructibleActor::GetImpactLocation(const FDamageEvent& DamageEvent,
                                                       const AActor* DamageCauser) const
{
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		return static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.Location;
	}
	if (IsValid(DamageCauser))
	{
		return DamageCauser->GetActorLocation();
	}
	return M_GeometryCollection->Bounds.Origin;
}

FVector AApartmentDestructibleActor::GetShotDirection(const FDamageEvent& DamageEvent, const AActor* DamageCauser,
                                                      const FVector& ImpactLocation) const
{
	if (IsValid(DamageCauser))
	{
		const FVector TravelDirection = DamageCauser->GetVelocity().GetSafeNormal();
		if (not TravelDirection.IsNearlyZero())
		{
			return TravelDirection;
		}
	}
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FVector EventDirection = FVector(static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection).
			GetSafeNormal();
		if (not EventDirection.IsNearlyZero())
		{
			return EventDirection;
		}
	}
	// Without a known direction the shot is taken to head for the middle of the building.
	const FVector TowardsBuildingCentre = (M_GeometryCollection->Bounds.Origin - ImpactLocation).GetSafeNormal();
	return TowardsBuildingCentre.IsNearlyZero() ? FVector::DownVector : TowardsBuildingCentre;
}

bool AApartmentDestructibleActor::GetIsValidGeometryCollection() const
{
	if (IsValid(M_GeometryCollection))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_GeometryCollection",
		"GetIsValidGeometryCollection",
		this
	);

	return false;
}
