// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentDestructibleActor.h"

#include "ApartmentFootprintNavModifier.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "NavAreas/NavArea_Null.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "TimerManager.h"
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
	// Pieces centred below this height above the origin are the ground floor and never lose their anchor.
	constexpr float GroundFloorPinnedHeight = 40.f;
	// The navigation areas start below the ground so uneven terrain under the building is still marked.
	constexpr float NavAreaDepthBelowOrigin = 150.f;
	// Only heavy tanks pass this area; lighter units have to go around the building.
	const TCHAR* const DefaultOuterRingNavAreaPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/RTS_NavArea_BlockCharLightMedium.RTS_NavArea_BlockCharLightMedium_C");
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

	M_OuterRingNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("OuterRingNav"));
	M_InnerCoreNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("InnerCoreNav"));
	OuterRingNavArea = TSoftClassPtr<UNavArea>(FSoftObjectPath(ApartmentDestructibleActor::DefaultOuterRingNavAreaPath));
	InnerCoreNavArea = UNavArea_Null::StaticClass();

	M_CrushSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("CrushSensor"));
	M_CrushSensor->SetupAttachment(M_GeometryCollection);
	SetupCrushSensorCollision();
}

void AApartmentDestructibleActor::SetupCrushSensorCollision() const
{
	M_CrushSensor->SetCanEverAffectNavigation(false);
	// Same object type as crushable environment meshes, which vehicles already overlap.
	M_CrushSensor->SetCollisionObjectType(ECC_Destructible);
	M_CrushSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_CrushSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_CrushSensor->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
	M_CrushSensor->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
	M_CrushSensor->SetGenerateOverlapEvents(true);
}

void AApartmentDestructibleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildFootprint();
}

void AApartmentDestructibleActor::RebuildFootprint()
{
	using namespace ApartmentDestructibleActor;
	if (not GetIsValidGeometryCollection() || not GetIsValidOuterRingNav() || not GetIsValidInnerCoreNav()
		|| not GetIsValidCrushSensor())
	{
		return;
	}
	const UGeometryCollection* RestCollection = M_GeometryCollection->GetRestCollection();
	FApartmentFootprintAreas Areas;
	if (RestCollection == nullptr || not FApartmentFootprint::BuildAreas(*RestCollection, FootprintSettings, Areas))
	{
		// No collection assigned yet; the areas follow as soon as one is.
		return;
	}
	const float AreaTop = DestructionSettings.AnchoredGroundStoreyHeight;
	if (UClass* OuterRingClass = OuterRingNavArea.LoadSynchronous())
	{
		M_OuterRingNav->SetAreaClass(OuterRingClass);
	}
	M_InnerCoreNav->SetAreaClass(InnerCoreNavArea);
	M_OuterRingNav->SetFootprintBoxes(Areas.OuterRingBoxes, -NavAreaDepthBelowOrigin, AreaTop);
	M_InnerCoreNav->SetFootprintBoxes(Areas.InnerCoreBoxes, -NavAreaDepthBelowOrigin, AreaTop);

	const FVector2D SensorCentre = Areas.TotalBounds.GetCenter();
	const FVector2D SensorExtent = Areas.TotalBounds.GetExtent();
	const float SensorHalfHeight = AreaTop * 0.5f;
	M_CrushSensor->SetRelativeLocation(FVector(SensorCentre, SensorHalfHeight));
	M_CrushSensor->SetBoxExtent(FVector(SensorExtent, SensorHalfHeight));
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
	if (GetIsValidCrushSensor())
	{
		M_CrushSensor->OnComponentBeginOverlap.AddDynamic(this, &AApartmentDestructibleActor::OnCrushSensorBeginOverlap);
	}
}

void AApartmentDestructibleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CrushTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AApartmentDestructibleActor::OnCrushSensorBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                            AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                                            int32 OtherBodyIndex, bool bFromSweep,
                                                            const FHitResult& SweepResult)
{
	if (not GetPassesCrushGate(OtherActor))
	{
		return;
	}
	M_CrushingVehicles.AddUnique(OtherActor);
	const UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_CrushTimerHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_CrushTimerHandle, this, &AApartmentDestructibleActor::CrushUpdate,
	                                  CrushSettings.UpdateInterval, true);
}

bool AApartmentDestructibleActor::GetPassesCrushGate(const AActor* OtherActor) const
{
	const IRTSNavAgentInterface* NavAgent = Cast<IRTSNavAgentInterface>(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	switch (CrushSettings.CrushDestructionType)
	{
	case ECrushDestructionType::HeavyTank:
		return AgentType == ERTSNavAgents::HeavyTank;
	case ECrushDestructionType::MediumOrHeavyTank:
		return AgentType == ERTSNavAgents::MediumTank || AgentType == ERTSNavAgents::HeavyTank;
	case ECrushDestructionType::AnyTank:
	default:
		return AgentType != ERTSNavAgents::Character && AgentType != ERTSNavAgents::NONE;
	}
}

void AApartmentDestructibleActor::CrushUpdate()
{
	if (not GetIsValidCrushSensor() || not GetIsValidGeometryCollection())
	{
		return;
	}
	// Backwards, because vehicles that left or died are dropped while going through the list.
	for (int32 VehicleIndex = M_CrushingVehicles.Num() - 1; VehicleIndex >= 0; --VehicleIndex)
	{
		const AActor* Vehicle = M_CrushingVehicles[VehicleIndex].Get();
		if (not IsValid(Vehicle) || not M_CrushSensor->IsOverlappingActor(Vehicle))
		{
			M_CrushingVehicles.RemoveAtSwap(VehicleIndex);
			continue;
		}
		CrushUnderVehicle(*Vehicle);
	}
	if (not M_CrushingVehicles.IsEmpty())
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CrushTimerHandle);
	}
}

void AApartmentDestructibleActor::CrushUnderVehicle(const AActor& Vehicle)
{
	FVector VehicleOrigin;
	FVector VehicleExtent;
	constexpr bool bOnlyCollidingComponents = true;
	Vehicle.GetActorBounds(bOnlyCollidingComponents, VehicleOrigin, VehicleExtent);
	const FVector TravelDirection = Vehicle.GetVelocity().GetSafeNormal2D();
	CrushInsideBox(FBox::BuildAABB(VehicleOrigin, VehicleExtent),
	               TravelDirection.IsNearlyZero() ? Vehicle.GetActorForwardVector() : TravelDirection);
}

int32 AApartmentDestructibleActor::CrushInsideBox(const FBox& WorldBox, const FVector& PushDirection)
{
	using namespace ApartmentDestructibleActor;
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidGeometryCollection())
	{
		return 0;
	}
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ApartmentCrush), false);
	World->OverlapMultiByObjectType(Overlaps, WorldBox.GetCenter(), FQuat::Identity,
	                                FCollisionObjectQueryParams(ECC_Destructible),
	                                FCollisionShape::MakeBox(WorldBox.GetExtent()), QueryParams);
	TSet<int32> PiecesInside;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (Overlap.GetComponent() == M_GeometryCollection && Overlap.ItemIndex != INDEX_NONE)
		{
			PiecesInside.Add(Overlap.ItemIndex);
		}
	}
	if (PiecesInside.IsEmpty())
	{
		return 0;
	}
	// Ground-storey panels are pinned; under a vehicle they have to be able to fall like any other. The floor itself
	// stays pinned: a slab lying in the terrain would drop straight through it once it is loose.
	FBox UnanchorBox = WorldBox;
	UnanchorBox.Min.Z = FMath::Max(UnanchorBox.Min.Z, GetActorLocation().Z + GroundFloorPinnedHeight);
	if (UnanchorBox.Min.Z < UnanchorBox.Max.Z)
	{
		M_GeometryCollection->SetAnchoredByBox(UnanchorBox, false);
	}
	const FVector CrushCentre = WorldBox.GetCenter();
	const float CrushRadius = WorldBox.GetExtent().Size2D();
	const FVector PushVelocity = PushDirection * CrushSettings.PushSpeed;
	constexpr int32 NoStrainPropagation = 0;
	for (const int32 PieceIndex : PiecesInside)
	{
		M_GeometryCollection->ApplyInternalStrain(PieceIndex, CrushCentre, CrushRadius, NoStrainPropagation, 0.f,
		                                          CrushSettings.DamagePerUpdate);
		M_GeometryCollection->ApplyBreakingLinearVelocity(PieceIndex, PushVelocity);
	}
	return PiecesInside.Num();
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

bool AApartmentDestructibleActor::GetIsValidOuterRingNav() const
{
	if (IsValid(M_OuterRingNav))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_OuterRingNav",
		"GetIsValidOuterRingNav",
		this
	);

	return false;
}

bool AApartmentDestructibleActor::GetIsValidInnerCoreNav() const
{
	if (IsValid(M_InnerCoreNav))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_InnerCoreNav",
		"GetIsValidInnerCoreNav",
		this
	);

	return false;
}

bool AApartmentDestructibleActor::GetIsValidCrushSensor() const
{
	if (IsValid(M_CrushSensor))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_CrushSensor",
		"GetIsValidCrushSensor",
		this
	);

	return false;
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
