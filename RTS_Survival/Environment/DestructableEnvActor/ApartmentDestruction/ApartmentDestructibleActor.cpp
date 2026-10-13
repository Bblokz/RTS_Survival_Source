// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentDestructibleActor.h"

#include "ApartmentFootprintNavModifier.h"
#include "ApartmentGarrisonSelectable.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "NavAreas/NavArea_Null.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

namespace ApartmentDestructibleActor
{
	// The reported hit location can sit slightly inside or outside the wall, so the piece search spans both sides.
	constexpr float PieceTraceBacktrack = 300.f;
	constexpr float PieceTraceOvershoot = 400.f;
	// Keeps panels that start slightly below the actor's origin inside the anchored volume.
	constexpr float AnchorDepthBelowOrigin = 10.f;
	// Slots that end below this height above the origin are the ground floor and never break loose.
	constexpr float GroundFloorPinnedHeight = 60.f;
	// The player whose soldiers make the building selectable.
	constexpr uint8 GarrisonPlayer = 1;
	// The navigation areas start below the ground so uneven terrain under the building is still marked.
	constexpr float NavAreaDepthBelowOrigin = 150.f;
	// Light and medium vehicles stay out of this area; infantry and heavy tanks pass.
	const TCHAR* const DefaultOuterBandNavAreaPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/RTS_NavArea_BlockLightMedium.RTS_NavArea_BlockLightMedium_C");
	// Only heavy tanks pass this area.
	const TCHAR* const DefaultInnerRingNavAreaPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/RTS_NavArea_BlockCharLightMedium.RTS_NavArea_BlockCharLightMedium_C");

	// This actor keeps the health of every part itself. The collection only has to let go of a part when told
	// to, so every part gets the same small strength and a broken part receives far more strain than that.
	constexpr float CollectionBreakThreshold = 1.f;
	constexpr float CollectionBreakStrain = 1000.f;
	// A break is aimed at the centre of mass of one slot; the collection then takes its nearest child.
	constexpr float BreakStrainRadius = 5.f;
	// How far outside a slot the search for the body it belongs to starts.
	constexpr float BodyProbeMargin = 30.f;
	// A slot further than this from where it stood has fallen, whether it was shot or lost its support.
	constexpr float FallenSlotDistance = 20.f;
	constexpr float StructureCheckInterval = 0.5f;
	// Parts above a hole can take a few seconds to come down.
	constexpr float StructureCheckSecondsAfterDamage = 8.f;
	const FName MassToLocalAttribute(TEXT("MassToLocal"));
	constexpr float StoreyHeight = 300.f;
	// More than the cover finder's spacing between points.
	constexpr float ApproachStepPerFloor = 100.f;

	// Where the parts a cover socket depends on are looked for, relative to the socket: the wall in front of
	// the soldier from knee to chest, and the floor right under his feet.
	constexpr float SupportWallNear = 10.f;
	constexpr float SupportWallFar = 130.f;
	constexpr float SupportWallHalfWidth = 45.f;
	constexpr float SupportWallLow = 30.f;
	constexpr float SupportWallHigh = 140.f;
	constexpr float SupportFloorHalfWidth = 30.f;
	constexpr float SupportFloorDepth = 45.f;
	constexpr float SupportFloorTop = 2.f;

	bool GetIsWall(const EApartmentPartType PartType)
	{
		return PartType == EApartmentPartType::ExteriorWall || PartType == EApartmentPartType::InteriorWall;
	}

	bool GetIsFloor(const EApartmentPartType PartType)
	{
		return PartType == EApartmentPartType::Floor || PartType == EApartmentPartType::Roof
			|| PartType == EApartmentPartType::Stairs;
	}
}

float FApartmentPartHealth::GetHealth(const EApartmentPartType PartType) const
{
	switch (PartType)
	{
	case EApartmentPartType::ExteriorWall:
		return ExteriorWall;
	case EApartmentPartType::InteriorWall:
		return InteriorWall;
	case EApartmentPartType::Floor:
		return Floor;
	case EApartmentPartType::Roof:
		return Roof;
	case EApartmentPartType::Stairs:
		return Stairs;
	case EApartmentPartType::Detail:
	default:
		return Detail;
	}
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

	M_OuterBandNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("OuterBandNav"));
	M_InnerRingNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("InnerRingNav"));
	M_InnerCoreNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("InnerCoreNav"));
	OuterBandNavArea = TSoftClassPtr<UNavArea>(FSoftObjectPath(ApartmentDestructibleActor::DefaultOuterBandNavAreaPath));
	InnerRingNavArea = TSoftClassPtr<UNavArea>(FSoftObjectPath(ApartmentDestructibleActor::DefaultInnerRingNavAreaPath));
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
	if (GetIsValidGeometryCollection() && IsValid(ApartmentCollection)
		&& M_GeometryCollection->GetRestCollection() != ApartmentCollection)
	{
		M_GeometryCollection->SetRestCollection(ApartmentCollection);
	}
	RebuildFootprint();
}

void AApartmentDestructibleActor::RebuildFootprint()
{
	using namespace ApartmentDestructibleActor;
	if (not GetIsValidGeometryCollection() || not GetIsValidOuterBandNav() || not GetIsValidInnerRingNav()
		|| not GetIsValidInnerCoreNav() || not GetIsValidCrushSensor())
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
	if (UClass* OuterBandClass = OuterBandNavArea.LoadSynchronous())
	{
		M_OuterBandNav->SetAreaClass(OuterBandClass);
	}
	if (UClass* InnerRingClass = InnerRingNavArea.LoadSynchronous())
	{
		M_InnerRingNav->SetAreaClass(InnerRingClass);
	}
	M_InnerCoreNav->SetAreaClass(InnerCoreNavArea);
	M_OuterBandNav->SetFootprintBoxes(Areas.OuterBandBoxes, -NavAreaDepthBelowOrigin, AreaTop);
	M_InnerRingNav->SetFootprintBoxes(Areas.InnerRingBoxes, -NavAreaDepthBelowOrigin, AreaTop);
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
			"No collection set on apartment " + GetName() + "; it cannot be destroyed and gives no cover.");
		return;
	}

	BeginPlay_SetupWeaponOnlyDamage();
	BeginPlay_BuildSlots();
	BeginPlay_AnchorGroundStorey();
	BeginPlay_BindCoverSockets();
	if (GetIsValidCrushSensor())
	{
		M_CrushSensor->OnComponentBeginOverlap.AddDynamic(this, &AApartmentDestructibleActor::OnCrushSensorBeginOverlap);
	}
	// An apartment placed in the map is read by the cover finder when the map starts; one spawned later
	// has to announce itself.
	bM_CoverSocketsNeedRefresh = true;
	RefreshCoverSocketsIfNeeded();
}

void AApartmentDestructibleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CrushTimerHandle);
		World->GetTimerManager().ClearTimer(M_StructureCheckTimerHandle);
		World->GetTimerManager().ClearTimer(M_Garrison.M_UpdateSelectableTimerHandle);
	}
	if (IsValid(M_Garrison.M_Selectable))
	{
		M_Garrison.M_Selectable->Destroy();
	}
	M_Garrison.M_Selectable = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AApartmentDestructibleActor::OnSoldierCoverSocketChanged(ASquadUnit* SquadUnit, const bool bIsOnSocket)
{
	using namespace ApartmentDestructibleActor;
	if (not IsValid(SquadUnit))
	{
		return;
	}
	const URTSComponent* SoldierRTSComponent = SquadUnit->GetRTSComponent();
	// Only the player's own soldiers make the building his to select.
	if (not IsValid(SoldierRTSComponent) || SoldierRTSComponent->GetOwningPlayer() != GarrisonPlayer)
	{
		return;
	}
	if (bIsOnSocket)
	{
		M_Garrison.M_Soldiers.AddUnique(SquadUnit);
	}
	else
	{
		M_Garrison.M_Soldiers.Remove(SquadUnit);
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (not World->GetTimerManager().IsTimerActive(M_Garrison.M_UpdateSelectableTimerHandle))
	{
		M_Garrison.M_UpdateSelectableTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AApartmentDestructibleActor::UpdateGarrisonSelectable));
	}
}

AActor* AApartmentDestructibleActor::GetSelectionProxyActor() const
{
	return M_Garrison.M_Selectable;
}

int32 AApartmentDestructibleActor::GetGarrisonedSoldierCount() const
{
	int32 SoldierCount = 0;
	for (const TWeakObjectPtr<ASquadUnit>& Soldier : M_Garrison.M_Soldiers)
	{
		SoldierCount += Soldier.IsValid() ? 1 : 0;
	}
	return SoldierCount;
}

void AApartmentDestructibleActor::UpdateGarrisonSelectable()
{
	M_Garrison.M_Soldiers.RemoveAll([](const TWeakObjectPtr<ASquadUnit>& Soldier)
	{
		return not Soldier.IsValid();
	});
	const bool bPlayerIsInside = not M_Garrison.M_Soldiers.IsEmpty();
	if (bPlayerIsInside && not IsValid(M_Garrison.M_Selectable))
	{
		SpawnGarrisonSelectable();
		return;
	}
	if (not bPlayerIsInside && IsValid(M_Garrison.M_Selectable))
	{
		// Destroying it also takes it out of the player's selection.
		M_Garrison.M_Selectable->Destroy();
		M_Garrison.M_Selectable = nullptr;
	}
}

void AApartmentDestructibleActor::SpawnGarrisonSelectable()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidCrushSensor())
	{
		return;
	}
	// The crush sensor spans the footprint; the selectable sits on the ground in its middle.
	FVector SelectableLocation = M_CrushSensor->GetComponentLocation();
	SelectableLocation.Z = GetActorLocation().Z;
	const FTransform SpawnTransform(GetActorRotation(), SelectableLocation);
	AApartmentGarrisonSelectable* Selectable = World->SpawnActorDeferred<AApartmentGarrisonSelectable>(
		AApartmentGarrisonSelectable::StaticClass(), SpawnTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Selectable))
	{
		RTSFunctionLibrary::ReportError("Could not spawn the garrison selectable of apartment " + GetName());
		return;
	}
	Selectable->InitGarrison(this, M_CrushSensor->GetUnscaledBoxExtent());
	Selectable->FinishSpawning(SpawnTransform);
	M_Garrison.M_Selectable = Selectable;
}

void AApartmentDestructibleActor::GatherGarrisonedSquads(TArray<ASquadController*>& OutSquads) const
{
	for (const TWeakObjectPtr<ASquadUnit>& Soldier : M_Garrison.M_Soldiers)
	{
		if (not Soldier.IsValid())
		{
			continue;
		}
		ASquadController* Squad = Soldier->GetSquadControllerChecked();
		if (IsValid(Squad))
		{
			OutSquads.AddUnique(Squad);
		}
	}
}

FVector AApartmentDestructibleActor::GetGarrisonExitLocation(const int32 SquadIndex) const
{
	const float DistanceFromDoor = GarrisonSettings.ExitGatherDistance + GarrisonSettings.ExitSquadSpacing * SquadIndex;
	if (not GarrisonSettings.ExitLocalTransform.Equals(FTransform::Identity))
	{
		const FTransform DoorWorld = GarrisonSettings.ExitLocalTransform * GetActorTransform();
		return DoorWorld.GetLocation() + DoorWorld.GetRotation().GetForwardVector().GetSafeNormal2D() * DistanceFromDoor;
	}
	// No door set: out of the front of the footprint.
	if (not GetIsValidCrushSensor())
	{
		return GetActorLocation();
	}
	const FVector FrontDirection = -GetActorRightVector();
	FVector FootprintCentre = M_CrushSensor->GetComponentLocation();
	FootprintCentre.Z = GetActorLocation().Z;
	return FootprintCentre + FrontDirection * (M_CrushSensor->GetUnscaledBoxExtent().Y + DistanceFromDoor);
}

void AApartmentDestructibleActor::EvacuateGarrison()
{
	// The orders below take soldiers off their sockets, which edits the list of who is inside.
	TArray<ASquadController*> Squads;
	GatherGarrisonedSquads(Squads);
	for (int32 SquadIndex = 0; SquadIndex < Squads.Num(); ++SquadIndex)
	{
		const FVector ExitLocation = GetGarrisonExitLocation(SquadIndex);
		const FRotator FacingAwayFromBuilding = (ExitLocation - GetActorLocation()).GetSafeNormal2D().Rotation();
		Squads[SquadIndex]->MoveToLocation(ExitLocation, true, FacingAwayFromBuilding);
	}
}

void AApartmentDestructibleActor::BeginPlay_SetupWeaponOnlyDamage()
{
	using namespace ApartmentDestructibleActor;
	// By default a breaking piece passes its damage on to every connected neighbour, which chain-reacts through
	// the whole building; only the parts this actor decides on may break.
	FGeometryCollectionDamagePropagationData DamagePropagation = M_GeometryCollection->DamagePropagationData;
	DamagePropagation.bEnabled = false;
	M_GeometryCollection->SetDamagePropagationData(DamagePropagation);
	M_GeometryCollection->DamageThreshold = {CollectionBreakThreshold};
	M_GeometryCollection->RecreatePhysicsState();
}

void AApartmentDestructibleActor::BeginPlay_BuildSlots()
{
	using namespace ApartmentDestructibleActor;
	M_Slots.Reset();
	const UGeometryCollection* RestCollection = M_GeometryCollection->GetRestCollection();
	const TSharedPtr<const FGeometryCollection, ESPMode::ThreadSafe> Collection = RestCollection->GetGeometryCollection();
	if (not Collection.IsValid())
	{
		return;
	}
	const TManagedArray<int32>* SlotIds = Collection->FindAttribute<int32>(
		ApartmentCollectionAttributes::SlotId, FGeometryCollection::TransformGroup);
	const TManagedArray<int32>* PartTypes = Collection->FindAttribute<int32>(
		ApartmentCollectionAttributes::PartType, FGeometryCollection::TransformGroup);
	const int32 RootIndex = RestCollection->GetRootIndex();
	if (SlotIds == nullptr || PartTypes == nullptr || not Collection->Children.IsValidIndex(RootIndex))
	{
		RTSFunctionLibrary::ReportError(
			"The collection of apartment " + GetName() + " was not built by RTS.Apartment.BuildGeometryCollection "
			"(or by an old version of it); the building cannot take damage. Rebuild the collection.");
		return;
	}
	M_RootItemIndex = RootIndex;
	TArray<FTransform> GlobalTransforms;
	GeometryCollectionAlgo::GlobalMatrices(Collection->Transform, Collection->Parent, GlobalTransforms);
	const TManagedArray<FTransform>* MassToLocal = Collection->FindAttribute<FTransform>(
		MassToLocalAttribute, FGeometryCollection::TransformGroup);
	const TArray<FTransform3f>& StandingTransforms = M_GeometryCollection->GetComponentSpaceTransforms3f();
	for (const int32 ClusterIndex : Collection->Children[RootIndex])
	{
		FApartmentSlot Slot;
		Slot.ClusterTransformIndex = ClusterIndex;
		Slot.RestTransformLocation = StandingTransforms.IsValidIndex(ClusterIndex)
			                             ? FVector(StandingTransforms[ClusterIndex].GetLocation())
			                             : FVector::ZeroVector;
		Slot.SlotId = (*SlotIds)[ClusterIndex];
		Slot.PartType = static_cast<EApartmentPartType>((*PartTypes)[ClusterIndex]);
		// A slot of one piece hangs under the root as that piece, without a cluster of its own.
		TArray<int32> SlotLeafIndices = Collection->Children[ClusterIndex].Array();
		if (SlotLeafIndices.IsEmpty())
		{
			SlotLeafIndices.Add(ClusterIndex);
		}
		for (const int32 LeafIndex : SlotLeafIndices)
		{
			FBox LeafBounds(ForceInit);
			if (FApartmentFootprint::TryGetPieceBounds(*Collection, LeafIndex, GlobalTransforms, LeafBounds))
			{
				Slot.LocalBounds += LeafBounds;
				Slot.LeafTransformIndices.Add(LeafIndex);
			}
		}
		if (not Slot.LocalBounds.IsValid)
		{
			continue;
		}
		// The collection picks the child nearest to where a break is aimed, so it is aimed at exactly where
		// the collection has this slot: its centre of mass.
		Slot.RestLocation = Slot.LocalBounds.GetCenter();
		if (MassToLocal != nullptr)
		{
			const FVector CentreOfMass = ((*MassToLocal)[ClusterIndex] * GlobalTransforms[ClusterIndex]).GetLocation();
			Slot.RestLocation = Slot.LocalBounds.IsInsideOrOn(CentreOfMass) ? CentreOfMass : Slot.RestLocation;
		}
		Slot.Health = DestructionSettings.PartHealth.GetHealth(Slot.PartType);
		Slot.bIndestructible = Slot.LocalBounds.Max.Z < GroundFloorPinnedHeight
			|| Slot.PartType == EApartmentPartType::Rubble;
		M_Slots.Add(MoveTemp(Slot));
	}
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

void AApartmentDestructibleActor::BeginPlay_BindCoverSockets()
{
	TMap<int32, int32> SlotIndexBySlotId;
	SlotIndexBySlotId.Reserve(M_Slots.Num());
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		SlotIndexBySlotId.Add(M_Slots[SlotIndex].SlotId, SlotIndex);
	}
	M_CoverSocketStates.Reset(CoverSockets.Num());
	for (const FApartmentCoverSocket& CoverSocket : CoverSockets)
	{
		FApartmentCoverSocketState& SocketState = M_CoverSocketStates.AddDefaulted_GetRef();
		for (const int32 SupportSlotId : CoverSocket.SupportSlotIds)
		{
			if (const int32* SlotIndex = SlotIndexBySlotId.Find(SupportSlotId))
			{
				SocketState.SupportSlotIndices.Add(*SlotIndex);
			}
		}
		if (CoverSocket.SupportSlotIds.IsEmpty())
		{
			FindSupportSlots(CoverSocket, SocketState.SupportSlotIndices);
		}
	}
}

void AApartmentDestructibleActor::FindSupportSlots(const FApartmentCoverSocket& CoverSocket,
                                                   TArray<int32>& OutSlotIndices) const
{
	using namespace ApartmentDestructibleActor;
	const FVector SocketLocation = CoverSocket.LocalTransform.GetLocation();
	const FVector Forward = CoverSocket.LocalTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector Right(-Forward.Y, Forward.X, 0.0);
	FBox WallProbe(ForceInit);
	for (const float ForwardDistance : {SupportWallNear, SupportWallFar})
	{
		for (const float SideDistance : {-SupportWallHalfWidth, SupportWallHalfWidth})
		{
			const FVector Corner = SocketLocation + Forward * ForwardDistance + Right * SideDistance;
			WallProbe += Corner + FVector::UpVector * SupportWallLow;
			WallProbe += Corner + FVector::UpVector * SupportWallHigh;
		}
	}
	const FBox FloorProbe(
		SocketLocation - FVector(SupportFloorHalfWidth, SupportFloorHalfWidth, SupportFloorDepth),
		SocketLocation + FVector(SupportFloorHalfWidth, SupportFloorHalfWidth, -SupportFloorTop));
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		const FApartmentSlot& Slot = M_Slots[SlotIndex];
		const bool bIsWallInFront = GetIsWall(Slot.PartType) && Slot.LocalBounds.Intersect(WallProbe);
		const bool bIsFloorBelow = GetIsFloor(Slot.PartType) && Slot.LocalBounds.Intersect(FloorProbe);
		if (bIsWallInFront || bIsFloorBelow)
		{
			OutSlotIndices.Add(SlotIndex);
		}
	}
}

void AApartmentDestructibleActor::GetUsableCoverSockets(TArray<FRTSActorCoverSocket>& OutCoverSockets) const
{
	using namespace ApartmentDestructibleActor;
	const FTransform ActorTransform = GetActorTransform();
	const double GroundHeight = ActorTransform.GetLocation().Z;
	for (int32 SocketIndex = 0; SocketIndex < CoverSockets.Num(); ++SocketIndex)
	{
		// Before the game started nothing is broken yet.
		if (M_CoverSocketStates.IsValidIndex(SocketIndex) && not M_CoverSocketStates[SocketIndex].bUsable)
		{
			continue;
		}
		const FApartmentCoverSocket& CoverSocket = CoverSockets[SocketIndex];
		FRTSActorCoverSocket& UsableSocket = OutCoverSockets.AddDefaulted_GetRef();
		UsableSocket.SocketName = CoverSocket.SocketName;
		UsableSocket.WorldTransform = CoverSocket.LocalTransform * ActorTransform;
		const FVector Facing = UsableSocket.WorldTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
		// Windows above each other would share one spot on the street, and the cover finder keeps only one
		// point per spot; every floor up takes its spot a step further from the wall.
		const int32 FloorsUp = FMath::Max(0, FMath::FloorToInt32(CoverSocket.LocalTransform.GetLocation().Z / StoreyHeight));
		UsableSocket.ApproachLocation = UsableSocket.WorldTransform.GetLocation()
			+ Facing * (CoverApproachDistance + FloorsUp * ApproachStepPerFloor);
		// Sockets on upper floors are taken from the street below them.
		UsableSocket.ApproachLocation.Z = GroundHeight;
	}
}

int32 AApartmentDestructibleActor::GetUsableCoverSocketCount() const
{
	int32 UsableCount = 0;
	for (const FApartmentCoverSocketState& SocketState : M_CoverSocketStates)
	{
		UsableCount += SocketState.bUsable ? 1 : 0;
	}
	return UsableCount;
}

int32 AApartmentDestructibleActor::GetStandingPartCount() const
{
	int32 StandingCount = 0;
	for (const FApartmentSlot& Slot : M_Slots)
	{
		StandingCount += Slot.bBroken ? 0 : 1;
	}
	return StandingCount;
}

void AApartmentDestructibleActor::RefreshCoverSocketsIfNeeded()
{
	if (not bM_CoverSocketsNeedRefresh)
	{
		return;
	}
	bM_CoverSocketsNeedRefresh = false;
	for (FApartmentCoverSocketState& SocketState : M_CoverSocketStates)
	{
		if (not SocketState.bUsable)
		{
			continue;
		}
		for (const int32 SupportSlotIndex : SocketState.SupportSlotIndices)
		{
			if (M_Slots[SupportSlotIndex].bBroken)
			{
				SocketState.bUsable = false;
				break;
			}
		}
	}
	const UWorld* World = GetWorld();
	URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		                                                ? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		                                                : nullptr;
	// No cover finder means a world without infantry cover, such as an editor preview.
	if (IsValid(CoverSubsystem))
	{
		CoverSubsystem->RefreshCoverSocketsOfActor(this);
	}
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
	if (not GetIsValidGeometryCollection())
	{
		return 0;
	}
	const FTransform& ComponentTransform = M_GeometryCollection->GetComponentTransform();
	const FVector PushVelocity = PushDirection * CrushSettings.PushSpeed;
	int32 TouchedPartCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		const FApartmentSlot& Slot = M_Slots[SlotIndex];
		if (Slot.bBroken || Slot.bIndestructible || not Slot.LocalBounds.TransformBy(ComponentTransform).Intersect(WorldBox))
		{
			continue;
		}
		++TouchedPartCount;
		DamageSlot(SlotIndex, CrushSettings.DamagePerUpdate, PushVelocity);
	}
	if (TouchedPartCount > 0)
	{
		StartStructureChecks();
		RefreshCoverSocketsIfNeeded();
	}
	return TouchedPartCount;
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
	const bool bFoundSurface = TryFindHitPiece(ImpactLocation, ShotDirection, PieceHit);
	DamagePartsAround(bFoundSurface ? FVector(PieceHit.ImpactPoint) : ImpactLocation, ShotDirection, DamageAmount);
	return ReportedDamage;
}

void AApartmentDestructibleActor::DamagePartsAround(const FVector& WorldImpact, const FVector& ShotDirection,
                                                    const float DamageAmount)
{
	const FVector LocalImpact = M_GeometryCollection->GetComponentTransform().InverseTransformPosition(WorldImpact);
	const float SplashRadiusSquared = FMath::Square(DestructionSettings.SplashRadius);
	TArray<int32, TInlineAllocator<32>> SlotsInReach;
	int32 HitSlotIndex = INDEX_NONE;
	float HitSlotDistanceSquared = SplashRadiusSquared;
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		const FApartmentSlot& Slot = M_Slots[SlotIndex];
		if (Slot.bBroken || Slot.bIndestructible)
		{
			continue;
		}
		const float DistanceSquared = Slot.LocalBounds.ComputeSquaredDistanceToPoint(LocalImpact);
		if (DistanceSquared > SplashRadiusSquared)
		{
			continue;
		}
		SlotsInReach.Add(SlotIndex);
		if (DistanceSquared <= HitSlotDistanceSquared)
		{
			HitSlotDistanceSquared = DistanceSquared;
			HitSlotIndex = SlotIndex;
		}
	}
	const FVector PushVelocity = ShotDirection * DestructionSettings.BreakingPieceSpeed;
	for (const int32 SlotIndex : SlotsInReach)
	{
		const float DamageShare = SlotIndex == HitSlotIndex ? 1.f : DestructionSettings.SplashDamageShare;
		DamageSlot(SlotIndex, DamageAmount * DamageShare, PushVelocity);
	}
	if (not SlotsInReach.IsEmpty())
	{
		StartStructureChecks();
		RefreshCoverSocketsIfNeeded();
	}
}

void AApartmentDestructibleActor::DamageSlot(const int32 SlotIndex, const float DamageAmount,
                                             const FVector& PushVelocity)
{
	FApartmentSlot& Slot = M_Slots[SlotIndex];
	Slot.Health -= DamageAmount;
	if (Slot.Health <= 0.f)
	{
		BreakSlot(SlotIndex, PushVelocity);
	}
}

void AApartmentDestructibleActor::BreakSlot(const int32 SlotIndex, const FVector& PushVelocity)
{
	using namespace ApartmentDestructibleActor;
	FApartmentSlot& Slot = M_Slots[SlotIndex];
	Slot.bBroken = true;
	bM_CoverSocketsNeedRefresh = true;
	// A ground-storey part is pinned; broken, it has to be able to fall like any other.
	M_GeometryCollection->SetAnchoredByIndex(Slot.ClusterTransformIndex, false);
	for (const int32 LeafIndex : Slot.LeafTransformIndices)
	{
		M_GeometryCollection->SetAnchoredByIndex(LeafIndex, false);
	}
	const FVector WorldLocation = M_GeometryCollection->GetComponentTransform().TransformPosition(Slot.RestLocation);
	const int32 BodyItemIndex = FindBodyItemOfSlot(Slot);
	constexpr int32 NoStrainPropagation = 0;
	M_GeometryCollection->ApplyInternalStrain(BodyItemIndex, WorldLocation, BreakStrainRadius, NoStrainPropagation,
	                                          0.f, CollectionBreakStrain);
	M_GeometryCollection->ApplyBreakingLinearVelocity(BodyItemIndex, PushVelocity);
}

int32 AApartmentDestructibleActor::FindBodyItemOfSlot(const FApartmentSlot& Slot) const
{
	using namespace ApartmentDestructibleActor;
	const FTransform& ComponentTransform = M_GeometryCollection->GetComponentTransform();
	const FVector LocalCentre = Slot.LocalBounds.GetCenter();
	const FVector LocalExtent = Slot.LocalBounds.GetExtent() + FVector(BodyProbeMargin);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ApartmentFindBody), false);
	// A probe through the middle of the slot along each axis; a flat or thin slot is missed along its own plane.
	for (const FVector& Axis : {FVector::UpVector, FVector::ForwardVector, FVector::RightVector})
	{
		const FVector Reach = Axis * (LocalExtent | Axis);
		FHitResult Hit;
		if (M_GeometryCollection->LineTraceComponent(
			Hit,
			ComponentTransform.TransformPosition(LocalCentre + Reach),
			ComponentTransform.TransformPosition(LocalCentre - Reach),
			QueryParams))
		{
			return Hit.Item;
		}
	}
	return M_RootItemIndex;
}

void AApartmentDestructibleActor::StartStructureChecks()
{
	using namespace ApartmentDestructibleActor;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_StructureCheckUntilWorldSeconds = World->GetTimeSeconds() + StructureCheckSecondsAfterDamage;
	if (World->GetTimerManager().IsTimerActive(M_StructureCheckTimerHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_StructureCheckTimerHandle, this, &AApartmentDestructibleActor::CheckStructure,
	                                  StructureCheckInterval, true);
}

void AApartmentDestructibleActor::CheckStructure()
{
	using namespace ApartmentDestructibleActor;
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidGeometryCollection())
	{
		return;
	}
	const TArray<FTransform3f>& CurrentTransforms = M_GeometryCollection->GetComponentSpaceTransforms3f();
	for (FApartmentSlot& Slot : M_Slots)
	{
		if (Slot.bBroken || not CurrentTransforms.IsValidIndex(Slot.ClusterTransformIndex))
		{
			continue;
		}
		const FVector CurrentLocation(CurrentTransforms[Slot.ClusterTransformIndex].GetLocation());
		if (FVector::DistSquared(CurrentLocation, Slot.RestTransformLocation) > FMath::Square(FallenSlotDistance))
		{
			Slot.bBroken = true;
			bM_CoverSocketsNeedRefresh = true;
		}
	}
	RefreshCoverSocketsIfNeeded();
	if (World->GetTimeSeconds() >= M_StructureCheckUntilWorldSeconds)
	{
		World->GetTimerManager().ClearTimer(M_StructureCheckTimerHandle);
	}
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

bool AApartmentDestructibleActor::GetIsValidOuterBandNav() const
{
	if (IsValid(M_OuterBandNav))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_OuterBandNav",
		"GetIsValidOuterBandNav",
		this
	);

	return false;
}

bool AApartmentDestructibleActor::GetIsValidInnerRingNav() const
{
	if (IsValid(M_InnerRingNav))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_InnerRingNav",
		"GetIsValidInnerRingNav",
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
