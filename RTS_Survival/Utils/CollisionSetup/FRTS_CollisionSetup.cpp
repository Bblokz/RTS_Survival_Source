#include "FRTS_CollisionSetup.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


FRTS_CollisionSetup::FRTS_CollisionSetup()
{
}

FRTS_CollisionSetup::~FRTS_CollisionSetup()
{
}

void FRTS_CollisionSetup::SetupShieldCollision(UStaticMeshComponent* ShieldMesh, const int32 OwningPlayer)
{
	if (not IsValid(ShieldMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupShieldCollision received an invalid shield mesh."));
		return;
	}

	ShieldMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShieldMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	FRTSPlayerCollision::SetWeaponHitResponses(ShieldMesh, OwningPlayer);
	ShieldMesh->SetGenerateOverlapEvents(false);
	ShieldMesh->SetCanEverAffectNavigation(false);
	ShieldMesh->SetSimulatePhysics(false);
	ShieldMesh->SetReceivesDecals(false);
}

void FRTS_CollisionSetup::SetupAircraftComponentCollision(UPrimitiveComponent* AircraftComponent, const int32 OwningPlayer,
                                                       const bool bReceivesWeaponHits)
{
	if (not IsValid(AircraftComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupAircraftComponentCollision received an invalid component."));
		return;
	}
	const ECollisionResponse VisibilityResponse = AircraftComponent->GetCollisionResponseToChannel(ECC_Visibility);
	const ECollisionResponse CameraResponse = AircraftComponent->GetCollisionResponseToChannel(ECC_Camera);
	const bool bEnableQueries = bReceivesWeaponHits
		|| AircraftComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	AircraftComponent->SetSimulatePhysics(false);
	AircraftComponent->SetGenerateOverlapEvents(false);
	AircraftComponent->SetCanEverAffectNavigation(false);
	AircraftComponent->SetCollisionEnabled(bEnableQueries ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	AircraftComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	// We use the camera trace channel to get info from units the player is looking at.
	AircraftComponent->SetCollisionResponseToChannel(ECC_Visibility, VisibilityResponse);
	AircraftComponent->SetCollisionResponseToChannel(ECC_Camera, CameraResponse);
	if (not bReceivesWeaponHits)
	{
		return;
	}
	AircraftComponent->SetCollisionObjectType(FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer));
	// Traces and projectiles of hostile players hit the aircraft; those of its own player and allies fly through it.
	FRTSPlayerCollision::SetWeaponHitResponses(AircraftComponent, OwningPlayer);
}

void FRTS_CollisionSetup::ForceBuildingPlacementResponseOnActor(
	AActor* Actor,
	ECollisionResponse Response,
	bool bRecurseAttachedActors = false)
{
	TSet<const AActor*> Visited;
	ForceBuildingPlacementResponse_Internal(Actor, Response, Visited, bRecurseAttachedActors);
}

void FRTS_CollisionSetup::SetupVehicleMovementMeshCollision(UMeshComponent* MovementMesh, const int32 OwningPlayer)
{
	if (FRTSPlayers::IsHumanPlayer(OwningPlayer))
	{
		SetupPlayerVehicleMovementMeshCollision(MovementMesh);
	}
	else
	{
		SetupEnemyVehicleMovementMeshCollision(MovementMesh);
	}
	// Weapons do not hit the chassis but area queries do; the owner bit tells them whose vehicle it is.
	FRTSPlayerCollision::SetOwnerMaskFilter(MovementMesh, OwningPlayer);
}

void FRTS_CollisionSetup::SetupNomadicMovementMeshCollision(UMeshComponent* MovementMesh, const int32 OwningPlayer)
{
	if (FRTSPlayers::IsHumanPlayer(OwningPlayer))
	{
		SetupNomadicMvtPlayer(MovementMesh);
	}
	else
	{
		SetupNomadicMvtEnemy(MovementMesh);
	}
	FRTSPlayerCollision::SetOwnerMaskFilter(MovementMesh, OwningPlayer);
}

void FRTS_CollisionSetup::SetupPlayerVehicleMovementMeshCollision(UMeshComponent* MovementMesh)
{
	if (IsValid(MovementMesh))
	{
		MovementMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MovementMesh->SetGenerateOverlapEvents(true);
		MovementMesh->SetCanEverAffectNavigation(false);
		MovementMesh->SetReceivesDecals(false);
		MovementMesh->SetSimulatePhysics(true);
		// for possible collisions with other tanks of the player.
		MovementMesh->SetCollisionObjectType(COLLISION_OBJ_PLAYER);
		MovementMesh->SetCollisionResponseToAllChannels(ECR_Block);
		constexpr ECollisionResponse ResponseToOtherPlayerVehicle =
			DeveloperSettings::GamePlay::Formation::LetPlayerTanksCollide
				? ECR_Block
				: ECR_Overlap;
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ResponseToOtherPlayerVehicle);
		// Make sure this mesh does not interact with weapons.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		// Make sure to block enemy tanks.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Block);
		// Ignore pickup items.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PICKUPITEM, ECR_Ignore);
		// Ignore building preview.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Ignore);
		// Ignore camera.
		MovementMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Ignore landscape traces.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);
		// Make sure to overlap with triggers.
		MovementMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		//  Do not block destructible actors; overlap them to drive crush logic
		MovementMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
	}
}

void FRTS_CollisionSetup::SetupVehicleHullBlockerCollision(UBoxComponent* HullBlocker, const uint8 OwningPlayer,
                                                           const bool bPassesThroughAllies)
{
	if (not IsValid(HullBlocker))
	{
		RTSFunctionLibrary::ReportError("Invalid hull blocker for vehicle hull blocker collision setup.");
		return;
	}

	const bool bIsPlayerVehicle = FRTSPlayers::IsHumanPlayer(OwningPlayer);
	const ECollisionChannel OwnSideHullChannel = bIsPlayerVehicle ? ECC_Vehicle : COLLISION_OBJ_ENEMY_HULL;
	const ECollisionChannel OtherSideHullChannel = bIsPlayerVehicle ? COLLISION_OBJ_ENEMY_HULL : ECC_Vehicle;
	HullBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Hull channels of their own: hull boxes only ever meet other hull boxes. The welded box also changes which
	// channel Chaos files the whole vehicle under; see COLLISION_OBJ_ENEMY_HULL for why its ini default is Block.
	HullBlocker->SetCollisionObjectType(OwnSideHullChannel);
	HullBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	HullBlocker->SetCollisionResponseToChannel(OtherSideHullChannel, ECR_Block);
	// A collision needs both boxes to block: a harvester that ignores its own side passes all allied hulls.
	HullBlocker->SetCollisionResponseToChannel(OwnSideHullChannel, bPassesThroughAllies ? ECR_Ignore : ECR_Block);
	HullBlocker->SetGenerateOverlapEvents(false);
	HullBlocker->SetCanEverAffectNavigation(false);
	HullBlocker->SetReceivesDecals(false);
	HullBlocker->SetHiddenInGame(true);
}

void FRTS_CollisionSetup::SetupHarvesterMovementMeshCollision(UMeshComponent* MovementMesh, const uint8 OwningPlayer)
{
	if (not IsValid(MovementMesh))
	{
		RTSFunctionLibrary::ReportError("Invalid Movement Mesh for Harvester Collision Setup."
			"\n see FRTS_CollisionSetup::SetupHarvesterMovementMeshCollision");
		return;
	}
	SetupVehicleMovementMeshCollision(MovementMesh, OwningPlayer);
	// Allied chassis and soldiers are on the object channel of the own side. Overlap instead of block: the chassis
	// drive through each other while soldiers keep getting the overlap that makes them step aside.
	constexpr ECollisionResponse ResponseToAlliedVehicle =
		DeveloperSettings::GamePlay::Formation::LetHarvestersCollideWithAllies ? ECR_Block : ECR_Overlap;
	MovementMesh->SetCollisionResponseToChannel(
		FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer),
		ResponseToAlliedVehicle);
}

void FRTS_CollisionSetup::SetupNomadicMvtPlayer(UMeshComponent* MovementMesh)
{
	if (IsValid(MovementMesh))
	{
		MovementMesh->ComponentTags.Add(FName("NomadicVehicleMovementMesh"));
		MovementMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MovementMesh->SetReceivesDecals(false);
		MovementMesh->SetCanEverAffectNavigation(false);
		MovementMesh->SetGenerateOverlapEvents(true);
		MovementMesh->SetSimulatePhysics(true);
		// for possible collisions with other tanks of the player.
		MovementMesh->SetCollisionObjectType(COLLISION_OBJ_PLAYER);
		MovementMesh->SetCollisionResponseToAllChannels(ECR_Block);
		constexpr ECollisionResponse ResponseToOtherPlayerVehicle =
			DeveloperSettings::GamePlay::Formation::LetPlayerNomadicVehiclesCollide
				? ECR_Block
				: ECR_Ignore;
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ResponseToOtherPlayerVehicle);
		// Make sure this mesh does not interact with weapons.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// Make sure to block enemy tanks.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Block);
		// Ignore pickup items.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PICKUPITEM, ECR_Ignore);
		// Ignore building preview; as we can build on top of the nomadic vehicle.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Ignore);
		// Ignore camera.
		MovementMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Ignore landscape traces.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);
		//  Do not block destructible actors; overlap them to drive crush logic
		MovementMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		// --- IMPORTANT: never hard-block infantry (CharacterMovement) ---
		MovementMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Movement Mesh for Player Vehicle Collision Setup."
		"\n see RTSFunctionLibrary::SetupPlayerVehicleMovementMeshCollision");
}

void FRTS_CollisionSetup::SetupNomadicMvtEnemy(UMeshComponent* MovementMesh)
{
	if (IsValid(MovementMesh))
	{
		MovementMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MovementMesh->SetGenerateOverlapEvents(true);
		MovementMesh->SetReceivesDecals(false);
		MovementMesh->SetCanEverAffectNavigation(false);
		// for possible collisions with other tanks of the player.
		MovementMesh->SetCollisionObjectType(COLLISION_OBJ_ENEMY);
		MovementMesh->SetCollisionResponseToAllChannels(ECR_Block);
		// Make sure this mesh does not interact with weapons.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		// Make sure to block enemy tanks.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Block);
		// Ignore pickup items.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PICKUPITEM, ECR_Ignore);
		// Ignore building preview; as we can build on top of the nomadic vehicle.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Ignore);
		// Ignore camera.
		MovementMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Ignore landscape traces.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);


		// Do not block destructible actors; overlap them to drive crush logic
		MovementMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);

		// --- IMPORTANT: never hard-block infantry (CharacterMovement) ---
		MovementMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Movement Mesh for Enemy Vehicle Collision Setup."
		"\n see RTSFunctionLibrary::SetupEnemyVehicleMovementMeshCollision");
}


void FRTS_CollisionSetup::SetupBuildingPlacementGridOverlay(UInstancedStaticMeshComponent* GridISM)
{
	if (not IsValid(GridISM))
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupBuildingPlacementGridOverlay: invalid GridISM."));
		return;
	}

	GridISM->SetReceivesDecals(false);
	GridISM->SetCanEverAffectNavigation(false);

	// Object type and responses:
	GridISM->SetCollisionObjectType(COLLISION_OBJ_BUILDING_PLACEMENT);
	GridISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GridISM->SetCollisionResponseToAllChannels(ECR_Ignore);
	GridISM->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);

	// We want overlap events (even if we also drive logic via queries).
	GridISM->SetGenerateOverlapEvents(true);
}


void FRTS_CollisionSetup::SetupEnemyVehicleMovementMeshCollision(UMeshComponent* MovementMesh)
{
	if (IsValid(MovementMesh))
	{
		MovementMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MovementMesh->SetGenerateOverlapEvents(true);
		MovementMesh->SetReceivesDecals(false);
		MovementMesh->SetCanEverAffectNavigation(false);
		// for possible collisions with other tanks of the player.
		MovementMesh->SetCollisionObjectType(COLLISION_OBJ_ENEMY);
		MovementMesh->SetCollisionResponseToAllChannels(ECR_Block);
		// Make sure this mesh does not interact with weapons.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		// Other enemy hulls are the only enemy objects that block this channel back, so only those collide.
		constexpr ECollisionResponse ResponseToOtherEnemyVehicle =
			DeveloperSettings::GamePlay::Formation::LetEnemyTanksCollide
				? ECR_Block
				: ECR_Ignore;
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ResponseToOtherEnemyVehicle);
		// Make sure to block player tanks.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Block);
		// Ignore pickup items.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PICKUPITEM, ECR_Ignore);
		// Overlap with building preview.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		// Ignore camera.
		MovementMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Ignore landscape traces.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);

		//  Do not block destructible actors; overlap them to drive crush logic
		MovementMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Movement Mesh for Enemy Vehicle Collision Setup."
		"\n see RTSFunctionLibrary::SetupEnemyVehicleMovementMeshCollision");
}

void FRTS_CollisionSetup::SetupArmorMeshCollision(UBoxComponent* ArmorMesh, const int32 OwningPlayer)
{
	if (not IsValid(ArmorMesh))
	{
		RTSFunctionLibrary::ReportError("Invalid Armor Mesh for Armor Collision Setup."
			"\n see FRTS_CollisionSetup::SetupArmorMeshCollision");
		return;
	}
	ArmorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArmorMesh->SetCollisionObjectType(FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer));
	ArmorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	ArmorMesh->SetReceivesDecals(false);

	ArmorMesh->SetGenerateOverlapEvents(true);
	// Set block on armor and overlap on projectile.
	// like here: https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview?application_version=5.3
	ArmorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);

	// Make sure the traces of hostile players can hit this armor.
	FRTSPlayerCollision::SetWeaponHitResponses(ArmorMesh, OwningPlayer);

	// For general traces.
	ArmorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void FRTS_CollisionSetup::SetupArmorCalculationMeshCollision(UMeshComponent* ArmorMesh, const uint8 OwningPlayer,
                                                             const bool bAffectNavMesh)
{
	if (not IsValid(ArmorMesh))
	{
		RTSFunctionLibrary::ReportError("Provided invalid armor mesh to setup Armor Calculation collisions");
		return;
	}
	ArmorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArmorMesh->SetCollisionObjectType(FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer));
	ArmorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArmorMesh->SetReceivesDecals(false);

	// Make sure we can query with visibility.
	ArmorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// The traces of players hostile to the owner hit the armor; its own and allied traces pass through.
	FRTSPlayerCollision::SetWeaponHitResponses(ArmorMesh, OwningPlayer);

	// Overlap with building previews.
	const ECollisionResponse ResponseToBuildingPlacement =
		FRTSPlayers::IsHumanPlayer(OwningPlayer) ? ECR_Ignore : ECR_Overlap;
	ArmorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ResponseToBuildingPlacement);
	// Overlap with triggers.
	ArmorMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ArmorMesh->SetCanEverAffectNavigation(bAffectNavMesh);
}

void FRTS_CollisionSetup::SetupTrackAttachedMeshCollision(UMeshComponent* MeshComponent, const int32 OwningPlayer)
{
	if (IsValid(MeshComponent))
	{
		// No physics, will mess with calculations for tracks otherwise.
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetReceivesDecals(false);
		MeshComponent->SetGenerateOverlapEvents(true);
		MeshComponent->SetCollisionObjectType(FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer));
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Only react to visibility traces.
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		SetupCollisionWithBuildingPreview_ForOwningPlayer(MeshComponent, OwningPlayer);

		//  Do not block destructible actors; overlap them to drive crush logic
		MeshComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		// Weapons pass through but area queries find the mesh on its object channel; the bit tells them whose it is.
		FRTSPlayerCollision::SetOwnerMaskFilter(MeshComponent, OwningPlayer);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Mesh Component for Track Collision Setup."
		"\n see FRTS_CollisionSetup::SetupTrackAttachedMeshCollision");
}

void FRTS_CollisionSetup::SetupTrainMeshesCollision(const TArray<UMeshComponent*>& MeshComponents,
                                                    const int32 OwningPlayer)
{
	const ECollisionChannel MyCollisionChannel = FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}


		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetReceivesDecals(false);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetCollisionObjectType(MyCollisionChannel);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		SetupCollisionWithBuildingPreview_ForOwningPlayer(MeshComponent, OwningPlayer);
		MeshComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		FRTSPlayerCollision::SetWeaponHitResponses(MeshComponent, OwningPlayer);
	}
}

void FRTS_CollisionSetup::SetupNomadicMountCollision(UMeshComponent* MeshComponent, const int32 OwningPlayer)
{
	if (!IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportError("Meshcomponent is not valid: SetupNomadicMountCollision");
		return;
	}

	// Make sure this can never be treated as 'WorldStatic' by accident
	MeshComponent->SetMobility(EComponentMobility::Movable);

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetReceivesDecals(false);
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->SetCanEverAffectNavigation(false);

	const bool bOwnedByHuman = FRTSPlayers::IsHumanPlayer(OwningPlayer);
	const ECollisionChannel OwnSideObjectChannel = FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer);
	MeshComponent->SetCollisionObjectType(OwnSideObjectChannel);

	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Block the vehicles of the other side.
	MeshComponent->SetCollisionResponseToChannel(
		bOwnedByHuman ? COLLISION_OBJ_ENEMY : COLLISION_OBJ_PLAYER, ECR_Block);
	// Overlap the vehicles of the own side.
	MeshComponent->SetCollisionResponseToChannel(OwnSideObjectChannel, ECR_Overlap);

	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// BlockWeapons.
	FRTSPlayerCollision::SetWeaponHitResponses(MeshComponent, OwningPlayer);

	//  Do not block destructible actors; overlap them to drive crush logic
	MeshComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
}

void FRTS_CollisionSetup::SetupCollisionForHullMountedWeapon(UMeshComponent* MeshComponent)
{
	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportError("Meshcomponent is not valid: SetupCollisionForHullMountedWeapon");
		return;
	}
	MeshComponent->SetReceivesDecals(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
}

void FRTS_CollisionSetup::SetupCollisionForTeamWeaponMeshes(const TArray<UMeshComponent*>& WeaponMeshes,
                                                            const uint8 OwningPlayer)
{
	for (UMeshComponent* WeaponMesh : WeaponMeshes)
	{
		if (not IsValid(WeaponMesh))
		{
			RTSFunctionLibrary::ReportError("WeaponMesh is invalid in SetupCollisionForTeamWeaponMeshes");
			continue;
		}

		WeaponMesh->SetReceivesDecals(false);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		WeaponMesh->SetGenerateOverlapEvents(true);
		WeaponMesh->SetCanEverAffectNavigation(false);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// Block landscape object.
		WeaponMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Block);
		WeaponMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		WeaponMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

		FRTSPlayerCollision::SetWeaponHitResponses(WeaponMesh, OwningPlayer);
	}
}

void FRTS_CollisionSetup::SetupInfantryWeaponCollision(UStaticMeshComponent* WeaponMesh)
{
	if (IsValid(WeaponMesh))
	{
		// no collision.
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		// no overlap and do not affect navigation.
		WeaponMesh->SetGenerateOverlapEvents(false);
		WeaponMesh->SetCanEverAffectNavigation(false);
		WeaponMesh->SetReceivesDecals(false);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Weapon Mesh for Infantry Weapon Collision Setup."
		"\n see RTSFunctionLibrary::SetupInfantryWeaponCollision");
}

void FRTS_CollisionSetup::SetupInfantryCapsuleCollision(UCapsuleComponent* CapsuleComponent, const int OwningPlayer)
{
	if (IsValid(CapsuleComponent))
	{
		CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
		const ECollisionChannel OwnSideObjectChannel = FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer);
		CapsuleComponent->SetCollisionObjectType(OwnSideObjectChannel);
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Generate events for triggers and evasion.
		CapsuleComponent->SetGenerateOverlapEvents(true);
		// Make sure traces can hit this capsule.
		CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// overlap player or enemy Objects depending on owner.
		CapsuleComponent->SetCollisionResponseToChannel(OwnSideObjectChannel, ECR_Overlap);
		// Block the weapons of hostile players.
		FRTSPlayerCollision::SetWeaponHitResponses(CapsuleComponent, OwningPlayer);
		// // block world  static to not fall through floor.
		CapsuleComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		// overlap with pickup items.
		CapsuleComponent->SetCollisionResponseToChannel(COLLISION_OBJ_PICKUPITEM, ECR_Overlap);
		// overlap with building preview.
		SetupCollisionWithBuildingPreview_ForOwningPlayer(CapsuleComponent, OwningPlayer);
		CapsuleComponent->SetCanEverAffectNavigation(false);
		// overlap with triggers.
		CapsuleComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Capsule Component for Infantry Capsule Collision Setup."
		"\n see RTSFunctionLibrary::SetupInfantryCapsuleCollision");
}

void FRTS_CollisionSetup::SetupTriggerOverlapCollision(UPrimitiveComponent* TriggerComponent,
                                                       ETriggerOverlapLogic TriggerLogic)
{
	if (IsValid(TriggerComponent))
	{
		TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
		TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerComponent->SetGenerateOverlapEvents(true);
		TriggerComponent->SetCanEverAffectNavigation(false);

		if (TriggerLogic == ETriggerOverlapLogic::OverlapPlayer || TriggerLogic == ETriggerOverlapLogic::OverlapBoth)
		{
			TriggerComponent->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
		}
		if (TriggerLogic == ETriggerOverlapLogic::OverlapEnemy || TriggerLogic == ETriggerOverlapLogic::OverlapBoth)
		{
			TriggerComponent->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
		}
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid trigger component provided for overlap setup."
		"\nFRTS_CollisionSetup::SetupTriggerOverlapCollision");
}

void FRTS_CollisionSetup::SetupFieldMineTriggerCollision(USphereComponent* TriggerSphere)
{
	if (not IsValid(TriggerSphere))
	{
		RTSFunctionLibrary::ReportError("Invalid trigger sphere provided for field mine collision setup.");
		return;
	}

	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->SetCanEverAffectNavigation(false);

	// Both sides: the mine's overlap handler lets only units hostile to its owner set it off.
	TriggerSphere->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
	TriggerSphere->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
}

void FRTS_CollisionSetup::SetupScavengeableObjectCollision(UStaticMeshComponent* ScavengeableObjectMesh)
{
	if (IsValid(ScavengeableObjectMesh))
	{
		// Id as world destructible.
		ScavengeableObjectMesh->SetCollisionObjectType(ECC_Destructible);
		ScavengeableObjectMesh->SetReceivesDecals(false);
		// set to query only.
		ScavengeableObjectMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		// block everything.
		ScavengeableObjectMesh->SetCollisionResponseToAllChannels(ECR_Block);
		// ignore landscape, camera.
		ScavengeableObjectMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);
		ScavengeableObjectMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// overlap building placement.
		ScavengeableObjectMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		// Get hit by player and enemy traces.
		FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(ScavengeableObjectMesh);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Scavengeable Object Mesh for Scavengeable Object Collision Setup."
		"\n see RTSFunctionLibrary::SetupScavengeableObjectCollision");
}

void FRTS_CollisionSetup::SetupResourceMeshCollision(UMeshComponent* ResourceMesh)
{
	if (not IsValid(ResourceMesh))
	{
		RTSFunctionLibrary::ReportError("Provided invalid resource mesh for resource mesh collision setup.");
		return;
	}
	// set to destructible.
	ResourceMesh->SetCollisionObjectType(ECC_Destructible);
	ResourceMesh->SetReceivesDecals(false);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	ResourceMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Make sure we visible traces for identifying this resource when it is clicked by the player.
	ResourceMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ResourceMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	// Make sure to block buildings.
	ResourceMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	ResourceMesh->SetGenerateOverlapEvents(true);
	// Make sure it can be hit by weapons:
	FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(ResourceMesh);
}

void FRTS_CollisionSetup::SetupResourceGeometryComponentCollision(UGeometryCollectionComponent* GeometryComponent)
{
	if (not IsValid(GeometryComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Provided invalid geometry component for resource geometry component collision setup.");
		return;
	}
	// Disable collision, this will be re-enabled only when needed by the FRTS_Collapse library.
	GeometryComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	GeometryComponent->SetReceivesDecals(false);
}

void FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(UMeshComponent* DestructibleEnvActorMesh,
                                                                 const bool bOverlapTanks)
{
	if (not IsValid(DestructibleEnvActorMesh))
	{
		RTSFunctionLibrary::ReportError(
			"Provided invalid destructible environment actor mesh for destructible environment actor mesh collision setup.");
		return;
	}
	DestructibleEnvActorMesh->SetCollisionObjectType(ECC_Destructible);
	DestructibleEnvActorMesh->SetReceivesDecals(false);
	// Do not affect nav with component; expect blueprint to use some nav modifier.
	DestructibleEnvActorMesh->SetCanEverAffectNavigation(false);
	DestructibleEnvActorMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	DestructibleEnvActorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Overlap with buildings to block placement.
	DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	// Block visibility and camera for identifying this destructible when clicked by the player.
	DestructibleEnvActorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Block weapons.
	DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(DestructibleEnvActorMesh);
	if (bOverlapTanks)
	{
		DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
		DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
		DestructibleEnvActorMesh->SetGenerateOverlapEvents(true);
	}
}

void FRTS_CollisionSetup::SetupWallActorMeshCollision(UMeshComponent* WallMesh, const bool bAffectNavigation,
                                                      const uint8 OwningPlayer)
{
	if (not IsValid(WallMesh))
	{
		RTSFunctionLibrary::ReportError(
			"Provided invalid WallActor mesh for SetupWallActorMeshCollision.");
		return;
	}
	WallMesh->SetCollisionObjectType(ECC_Destructible);
	WallMesh->SetReceivesDecals(false);
	WallMesh->SetCanEverAffectNavigation(bAffectNavigation);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	WallMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Overlap with buildings to block placement.
	WallMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	// Block visibility for identifying this destructible when clicked by the player.
	WallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Block the weapons of hostile players.
	FRTSPlayerCollision::SetWeaponHitResponses(WallMesh, OwningPlayer);
}

void FRTS_CollisionSetup::SetupDestructibleEnvActorGeometryComponentCollision(
	UGeometryCollectionComponent* GeometryComponent)
{
	if (not IsValid(GeometryComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Provided invalid geometry component for DestructibleEnvActor geometry component collision setup.");
		return;
	}
	// Disable collision, this will be re-enabled only when needed by the FRTS_Collapse library.
	GeometryComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	GeometryComponent->SetSimulatePhysics(false);
}

void FRTS_CollisionSetup::SetupFieldConstructionMeshCollision(UMeshComponent* FieldConstructionMesh,
                                                              const int32 OwningPlayer,
                                                              const bool bAlliedProjectilesHitObject,
                                                              const bool bOverlapTanks)
{
	if (not IsValid(FieldConstructionMesh))
	{
		return;
	}
	FieldConstructionMesh->SetCollisionObjectType(ECC_Destructible);
	FieldConstructionMesh->SetReceivesDecals(false);
	FieldConstructionMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	FieldConstructionMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	FieldConstructionMesh->SetCanEverAffectNavigation(false);
	// Overlap with buildings to block placement.
	FieldConstructionMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	// Block visibility for identifying this destructible when clicked by the player.
	FieldConstructionMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	if (bAlliedProjectilesHitObject)
	{
		// Also block allied projectiles.
		FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(FieldConstructionMesh);
	}
	else
	{
		// Block the weapons of hostile players.
		FRTSPlayerCollision::SetWeaponHitResponses(FieldConstructionMesh, OwningPlayer);
	}
	if (bOverlapTanks)
	{
		FieldConstructionMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
		FieldConstructionMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
		FieldConstructionMesh->SetGenerateOverlapEvents(true);
	}
}

void FRTS_CollisionSetup::SetupObstacleCollision(UMeshComponent* ObstacleMesh, const bool bBlockWeapons)
{
	if (not IsValid(ObstacleMesh))
	{
		return;
	}
	// Set to world static.
	ObstacleMesh->SetCollisionObjectType(ECC_WorldStatic);
	ObstacleMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryOnly);
	ObstacleMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Overlap with buildings.
	ObstacleMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	const ECollisionResponse ResponseToWeaponTraces = bBlockWeapons ? ECR_Block : ECR_Ignore;
	FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(ObstacleMesh, ResponseToWeaponTraces);
}

void FRTS_CollisionSetup::SetupGroundEnvActorCollision(UMeshComponent* GroundEnvActorMesh, const bool bAllowBuilding,
                                                       const bool bAffectNavigation)
{
	if (not IsValid(GroundEnvActorMesh))
	{
		return;
	}
	// Set to world static.
	GroundEnvActorMesh->SetCollisionObjectType(ECC_WorldStatic);
	GroundEnvActorMesh->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	GroundEnvActorMesh->SetCollisionResponseToAllChannels(ECR_Block);
	const ECollisionResponse ResponseToBuildingPlacement = bAllowBuilding ? ECR_Overlap : ECR_Ignore;
	GroundEnvActorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ResponseToBuildingPlacement);
	GroundEnvActorMesh->SetCanEverAffectNavigation(bAffectNavigation);
}

void FRTS_CollisionSetup::SetupSplineNoCollision(UMeshComponent* SplineMesh)
{
	if(not IsValid(SplineMesh))
	{
		return;
	}
	SplineMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	// ignore everything.
	SplineMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void FRTS_CollisionSetup::SetupVehicleRampCollision(UPrimitiveComponent* RampComponent)
{
	if (not IsValid(RampComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid ramp component for vehicle ramp collision setup.");
		return;
	}
	// Physics only: no trace, click or sweep ever hits the ramp, so infantry movement (sweeps), weapons and the
	// landscape traces keep seeing the trench itself.
	RampComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	// PhysicsBody: vehicle chassis block every object type but a few, infantry capsules ignore this one. Not
	// WorldDynamic, which player chassis only overlap (triggers).
	RampComponent->SetCollisionObjectType(ECC_PhysicsBody);
	RampComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	// The chassis of both sides. Chaos files an enemy chassis with its welded hull box under the hull channel (see
	// COLLISION_OBJ_ENEMY_HULL), so that one must block too or enemy vehicles drop through the ramp.
	RampComponent->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Block);
	RampComponent->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Block);
	RampComponent->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY_HULL, ECR_Block);
	RampComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	RampComponent->SetGenerateOverlapEvents(false);
	RampComponent->SetCanEverAffectNavigation(false);
	RampComponent->SetReceivesDecals(false);
}

void FRTS_CollisionSetup::SetupPickupCollision(UBoxComponent* PickupBox, UMeshComponent* PickupMesh)
{
	if (IsValid(PickupMesh))
	{
		PickupMesh->SetCollisionObjectType(COLLISION_OBJ_PICKUPITEM);
		PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Overlap with building placement.
		PickupMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		// overlap with player units.
		PickupMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Overlap);
		// block trace and camera
		PickupMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		PickupMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		PickupMesh->SetGenerateOverlapEvents(true);
		PickupMesh->SetCanEverAffectNavigation(false);
	}
	if (IsValid(PickupBox))
	{
		PickupBox->SetCollisionObjectType(COLLISION_OBJ_PICKUPITEM);
		PickupBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PickupBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		PickupBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Block buildings.
		PickupBox->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		PickupBox->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Overlap);
		PickupBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		PickupBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		PickupBox->SetGenerateOverlapEvents(true);
		PickupBox->SetCanEverAffectNavigation(false);
	}
}

void FRTS_CollisionSetup::UpdateGarrisonCollisionForNewOwner(const int32 NewOwningPlayer, const bool bIsAllied,
                                                             UMeshComponent* MeshToChangeCollisionOn)
{
	if (not IsValid(MeshToChangeCollisionOn))
	{
		return;
	}
	// The mesh keeps blocking both weapon channels. While it is allied with a player it carries that player's owner
	// bit, so the weapon queries of that player and its allies skip it and every other player still hits it.
	if (bIsAllied)
	{
		FRTSPlayerCollision::SetOwnerMaskFilter(MeshToChangeCollisionOn, NewOwningPlayer);
		return;
	}
	FRTSPlayerCollision::ClearOwnerMaskFilter(MeshToChangeCollisionOn);
}

void FRTS_CollisionSetup::SetupStaticBuildingPreviewCollision(UStaticMeshComponent* BuildingPreviewMesh,
                                                              const bool bUseCollision)
{
	if (IsValid(BuildingPreviewMesh))
	{
		if (not bUseCollision)
		{
			BuildingPreviewMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
			BuildingPreviewMesh->SetGenerateOverlapEvents(false);
			return;
		}
		BuildingPreviewMesh->SetCollisionObjectType(ECC_WorldStatic);
		BuildingPreviewMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BuildingPreviewMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
		BuildingPreviewMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		BuildingPreviewMesh->SetCanEverAffectNavigation(false);
		BuildingPreviewMesh->SetGenerateOverlapEvents(true);
		// ignore landscape, camera.
		BuildingPreviewMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Ignore);
		BuildingPreviewMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Building Preview Mesh for Building Preview Collision Setup."
		"\n see RTSFunctionLibrary::SetupStaticBuildingPreviewCollision");
}

void FRTS_CollisionSetup::SetupBuildingCollision(UStaticMeshComponent* BuildingMesh, int32 OwningPlayer)
{
	if (IsValid(BuildingMesh))
	{
		BuildingMesh->SetCollisionObjectType(FRTSPlayerCollision::GetOwnerObjectChannel(OwningPlayer));
		BuildingMesh->SetGenerateOverlapEvents(true);
		BuildingMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Block visbility. 
		BuildingMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// Block Destructible
		BuildingMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
		// overlap building placement.
		BuildingMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		// block the projectiles of hostile players.
		FRTSPlayerCollision::SetWeaponHitResponses(BuildingMesh, OwningPlayer);
		BuildingMesh->SetReceivesDecals(false);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Building Mesh for Building Collision Setup."
		"\n see RTSFunctionLibrary::SetupBuildingCollision");
}

void FRTS_CollisionSetup::SetupBuildingExpansionCollision(
	UStaticMeshComponent* BxpMesh,
	int32 OwningPlayer,
	bool bAffectNavmesh)
{
	if (not IsValid(BxpMesh))
	{
		RTSFunctionLibrary::ReportError(
			"Provided invalid building expansion mesh for building expansion collision setup.");
		return;
	}
	BxpMesh->SetCollisionObjectType(ECC_WorldStatic);
	// Ignore everything.
	BxpMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Set visible to camera and visibility traces.
	BxpMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// Overlap with building placement to not allow building on top of other buildings.
	BxpMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	FRTSPlayerCollision::SetWeaponHitResponses(BxpMesh, OwningPlayer);
	BxpMesh->SetCanEverAffectNavigation(bAffectNavmesh);
	BxpMesh->SetReceivesDecals(true);
}

void FRTS_CollisionSetup::ForceBuildingPlacementResponse_Internal(
	AActor* Actor,
	ECollisionResponse Response,
	TSet<const AActor*>& Visited,
	const bool bRecurseAttachedActors)
{
	if (!Actor || Visited.Contains(Actor)) return;
	Visited.Add(Actor);

	// 1) tweak all primitive comps on this actor
	TArray<UPrimitiveComponent*> PrimComps;
	Actor->GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* C : PrimComps)
	{
		if (!C) continue;
		if (C->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
		C->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, Response);
	}

	// 2) recurse into ChildActorComponents
	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		if (!CAC) continue;
		if (AActor* Child = CAC->GetChildActor())
		{
			ForceBuildingPlacementResponse_Internal(Child, Response, Visited, bRecurseAttachedActors);
		}
	}

	// 3) (optional) recurse into attached actors too
	if (bRecurseAttachedActors)
	{
		TArray<AActor*> Attached;
		Actor->GetAttachedActors(Attached);
		for (AActor* A : Attached)
		{
			ForceBuildingPlacementResponse_Internal(A, Response, Visited, bRecurseAttachedActors);
		}
	}
}

void FRTS_CollisionSetup::SetupCollisionWithBuildingPreview_ForOwningPlayer(UPrimitiveComponent* Component,
                                                                            const int32 OwningPlayer)
{
	if (not Component)
	{
		return;
	}
	const ECollisionResponse ResponseToBuildingPlacement =
		FRTSPlayers::IsHumanPlayer(OwningPlayer) ? ECR_Ignore : ECR_Overlap;
	Component->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ResponseToBuildingPlacement);
}
