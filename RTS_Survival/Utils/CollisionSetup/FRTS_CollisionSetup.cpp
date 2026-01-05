#include "FRTS_CollisionSetup.h"

#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


FRTS_CollisionSetup::FRTS_CollisionSetup()
{
}

FRTS_CollisionSetup::~FRTS_CollisionSetup()
{
}

void FRTS_CollisionSetup::ForceBuildingPlacementResponseOnActor(
	AActor* Actor,
	ECollisionResponse Response,
	bool bRecurseAttachedActors = false)
{
	TSet<const AActor*> Visited;
	ForceBuildingPlacementResponse_Internal(Actor, Response, Visited, bRecurseAttachedActors);
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
			DeveloperSettings::GamePlay::Formation::LetPlayerTanksCollide
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
		// Make sure to ignore enemy tanks.
		MovementMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Ignore);
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

void FRTS_CollisionSetup::SetupPlayerArmorMeshCollision(UBoxComponent* ArmorMesh)
{
	if (IsValid(ArmorMesh))
	{
		ArmorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ArmorMesh->SetCollisionObjectType(COLLISION_OBJ_PLAYER);
		ArmorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

		ArmorMesh->SetReceivesDecals(false);

		ArmorMesh->SetGenerateOverlapEvents(true);
		// Set block on armor and overlap on projectile.
		// like here: https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview?application_version=5.3
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);

		// Make sure traces can hit this armor.
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);

		// For general traces.
		ArmorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Armor Mesh for Player Armor Collision Setup."
		"\n see RTSFunctionLibrary::SetupPlayerArmorMeshCollision");
}

void FRTS_CollisionSetup::SetupEnemyArmorMeshCollision(UBoxComponent* ArmorMesh)
{
	if (IsValid(ArmorMesh))
	{
		ArmorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ArmorMesh->SetCollisionObjectType(COLLISION_OBJ_ENEMY);
		ArmorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		ArmorMesh->SetReceivesDecals(false);
		// Make sure this mesh does not interact with weapons.
		ArmorMesh->SetGenerateOverlapEvents(true);
		// Set block on armor and overlap on projectile.
		// like here: https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview?application_version=5.3
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);

		// Make sure traces can hit this armor.
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);

		// For general traces.
		ArmorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Armor Mesh for Enemy Armor Collision Setup."
		"\n see RTSFunctionLibrary::SetupEnemyArmorMeshCollision");
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
	ArmorMesh->SetCollisionObjectType(OwningPlayer == 1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY);
	ArmorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArmorMesh->SetReceivesDecals(false);

	// Make sure we can query with visibility.
	ArmorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// if this is owned by player 1 make sure Player traces hit otherwise enemy traces will be set to hit.
	if (OwningPlayer == 1)
	{
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
	}
	else
	{
		ArmorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
	}

	// Overlap with building previews.
	const ECollisionResponse ResponseToBuildingPlacement =
		OwningPlayer == 1 ? ECR_Ignore : ECR_Overlap;
	ArmorMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ResponseToBuildingPlacement);
	// Overlap with triggers.
	ArmorMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ArmorMesh->SetCanEverAffectNavigation(bAffectNavMesh);
}

void FRTS_CollisionSetup::SetupCollisionForMeshAttachedToTracks(UMeshComponent* MeshComponent, bool bIsPlayer1)
{
	if (IsValid(MeshComponent))
	{
		// No physics, will mess with calculations for tracks otherwise.
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetReceivesDecals(false);
		MeshComponent->SetGenerateOverlapEvents(true);
		const ECollisionChannel MyCollisionChannel = bIsPlayer1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
		MeshComponent->SetCollisionObjectType(MyCollisionChannel);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Only react to visibility traces.
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		SetupCollisionWithBuildingPreview_ForOwningPlayer(MeshComponent, bIsPlayer1);

		//  Do not block destructible actors; overlap them to drive crush logic
		MeshComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid Mesh Component for Track Collision Setup."
		"\n see RTSFunctionLibrary::SetupCollisionForMeshAttachedToTracks");
}

void FRTS_CollisionSetup::SetupCollisionForNomadicMount(UMeshComponent* MeshComponent, const bool bOwnedByPlayer1)
{
	if (!IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportError("Meshcomponent is not valid: SetupCollisionForNomadicMount");
		return;
	}

	// Make sure this can never be treated as 'WorldStatic' by accident
	MeshComponent->SetMobility(EComponentMobility::Movable);

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetReceivesDecals(false);
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->SetCanEverAffectNavigation(false);

	const ECollisionChannel Obj = bOwnedByPlayer1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
	MeshComponent->SetCollisionObjectType(Obj);

	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	// Block enemy vehicles.
	MeshComponent->SetCollisionResponseToChannel(
		bOwnedByPlayer1 ? COLLISION_OBJ_ENEMY : COLLISION_OBJ_PLAYER, ECR_Block);
	// Overlap allied vehicles.
	const ECollisionChannel AllyObj = bOwnedByPlayer1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
	MeshComponent->SetCollisionResponseToChannel(AllyObj, ECR_Overlap);

	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// BlockWeapons. 
	MeshComponent->SetCollisionResponseToChannel(
		bOwnedByPlayer1 ? COLLISION_TRACE_PLAYER : COLLISION_TRACE_ENEMY, ECR_Block);

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
		CapsuleComponent->SetCollisionObjectType(OwningPlayer == 1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY);
		CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Generate events for triggers and evasion.
		CapsuleComponent->SetGenerateOverlapEvents(true);
		// Make sure traces can hit this capsule.
		CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// overlap player or enemy Objects depending on owner.
		if (OwningPlayer == 1)
		{
			CapsuleComponent->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
		}
		else
		{
			CapsuleComponent->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
		}
		// Block the enemy weapons.
		CapsuleComponent->SetCollisionResponseToChannel(
			OwningPlayer == 1 ? COLLISION_TRACE_PLAYER : COLLISION_TRACE_ENEMY, ECR_Block);
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
		ScavengeableObjectMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
		ScavengeableObjectMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
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
	DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
	DestructibleEnvActorMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
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
	// Block weapons of opposite player.
	WallMesh->SetCollisionResponseToChannel(OwningPlayer == 1 ? COLLISION_TRACE_PLAYER : COLLISION_TRACE_ENEMY,
	                                        ECR_Block);
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
	const int32 OwningPlayer, const bool bAlliedProjectilesHitObject)
{
	if(not IsValid(FieldConstructionMesh))
	{
		return ;
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
	// Block weapons of opposite player.
	FieldConstructionMesh->SetCollisionResponseToChannel(OwningPlayer == 1 ? COLLISION_TRACE_PLAYER : COLLISION_TRACE_ENEMY,
	                                        ECR_Block);
	if(bAlliedProjectilesHitObject)
	{
		// Also block allied projectiles.
		FieldConstructionMesh->SetCollisionResponseToChannel(OwningPlayer == 1 ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER,
			ECR_Block);
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
	ObstacleMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ResponseToWeaponTraces);
	ObstacleMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ResponseToWeaponTraces);
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
	// If allied with player one then make sure to ignore traces that hit enemy as otherwise player units can hit this object.
	const ECollisionChannel NewCollisionChannel = NewOwningPlayer == 1 ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER;
	// Set response.
	const ECollisionResponse ResponseToThisPlayer = bIsAllied ? ECR_Ignore : ECR_Block;
	MeshToChangeCollisionOn->SetCollisionResponseToChannel(NewCollisionChannel, ResponseToThisPlayer);
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
		const ECollisionChannel BuildingCollisionChannel = OwningPlayer == 1
			                                                   ? COLLISION_OBJ_PLAYER
			                                                   : COLLISION_OBJ_ENEMY;
		BuildingMesh->SetCollisionObjectType(BuildingCollisionChannel);
		BuildingMesh->SetGenerateOverlapEvents(true);
		BuildingMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		// Block visbility. 
		BuildingMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		// Block Destructible
		BuildingMesh->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
		// overlap building placement.
		BuildingMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		if (OwningPlayer == 1)
		{
			// block projectiles that hit player objects.
			BuildingMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
		}
		else
		{
			// block projectiles that hit enemy objects.
			BuildingMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
		}
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
	const ECollisionChannel TraceChannelAllowHits = OwningPlayer == 1 ? COLLISION_TRACE_PLAYER : COLLISION_TRACE_ENEMY;
	BxpMesh->SetCollisionResponseToChannel(TraceChannelAllowHits, ECR_Block);
	BxpMesh->SetCanEverAffectNavigation(bAffectNavmesh);
	BxpMesh->SetReceivesDecals(false);
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
	const ECollisionResponse ResponseToBuildingPlacement = OwningPlayer == 1 ? ECR_Ignore : ECR_Overlap;
	Component->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ResponseToBuildingPlacement);
}
