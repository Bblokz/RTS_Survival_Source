#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "TriggerOverlapLogic.h"

class UCapsuleComponent;
class USphereComponent;

// We use the camera trace channel to ge info from units the player is looking at.
class RTS_SURVIVAL_API FRTS_CollisionSetup
{
public:
	FRTS_CollisionSetup();
	~FRTS_CollisionSetup();
	
	static void ForceBuildingPlacementResponseOnActor(AActor* Actor, ECollisionResponse Response, bool bRecurseAttachedActors);

	// ----------------- Collision Setup Vehicles -----------------
	
	/**
	 * @brief Init the collision settings for a movement mesh component on vehicles for the player.
	 * @param MovementMesh The mesh component to set collision for.
	 */
	static void SetupPlayerVehicleMovementMeshCollision(UMeshComponent* MovementMesh);

	// Differs from player vehicle movement mesh collision as it ignores collision with building placements.
	static void SetupNomadicMvtPlayer(UMeshComponent* MovementMesh);

	static void SetupNomadicMvtEnemy(UMeshComponent* MovementMesh);

		/** Configure an InstancedStaticMesh grid overlay for building placement preview. */
    	static void SetupBuildingPlacementGridOverlay(UInstancedStaticMeshComponent* GridISM);

	
	/**
	 * @brief Init the collision settings for a movement mesh component on vehicles for the enemy.
	 * @param MovementMesh The mesh component to set collision for.
	 */
	static void SetupEnemyVehicleMovementMeshCollision(UMeshComponent* MovementMesh);

	// ----------------- Collision Setup Tanks -----------------
	static void SetupPlayerArmorMeshCollision(UBoxComponent* ArmorMesh);
	
	static void SetupEnemyArmorMeshCollision(UBoxComponent* ArmorMesh);

	static void SetupArmorCalculationMeshCollision(UMeshComponent* ArmorMesh,
	                                               const uint8 OwningPlayer,
	                                               const bool bAffectNavMesh);

	static void SetupCollisionForMeshAttachedToTracks(UMeshComponent* MeshComponent, bool bIsPlayer1);
	
	static void SetupCollisionForNomadicMount(UMeshComponent* MeshComponent, const bool bOwnedByPlayer1);

	static void SetupCollisionForHullMountedWeapon(UMeshComponent* MeshComponent);

	// ----------------- Collision Setup Infantry -----------------
        static void SetupInfantryWeaponCollision(UStaticMeshComponent* WeaponMesh);

        static void SetupInfantryCapsuleCollision(UCapsuleComponent* CapsuleComponent, const int OwningPlayer);

        /**
         * @brief Configure overlap-only collision for trigger components.
         * @param TriggerComponent Component to configure collision on.
         * @param TriggerLogic Target overlap channels for players or enemies.
         */
        static void SetupTriggerOverlapCollision(UPrimitiveComponent* TriggerComponent, ETriggerOverlapLogic TriggerLogic);

	static void SetupFieldMineTriggerCollision(USphereComponent* TriggerSphere, int32 OwningPlayer);

	// ----------------- Collision setup static objects -----------------
	// if bnNoCollision is true, the object will have no except for player mouse interactions.
	static void SetupScavengeableObjectCollision(UStaticMeshComponent* ScavengeableObjectMesh);

	static void SetupResourceMeshCollision(UMeshComponent* ResourceMesh);
	static void SetupResourceGeometryComponentCollision(UGeometryCollectionComponent* GeometryComponent);

	static void SetupDestructibleEnvActorMeshCollision(UMeshComponent* DestructibleEnvActorMesh, const bool bOverlapTanks);
	static void SetupWallActorMeshCollision(UMeshComponent* WallMesh, const bool bAffectNavigation, const uint8 OwningPlayer);
	static void SetupDestructibleEnvActorGeometryComponentCollision(UGeometryCollectionComponent* GeometryComponent);

	static void SetupFieldConstructionMeshCollision(UMeshComponent* FieldConstructionMesh, const int32 OwningPlayer, const bool bAlliedProjectilesHitObject = false, const
	                                                bool bOverlapTanks = false);

	static void SetupObstacleCollision(UMeshComponent* ObstacleMesh, const bool bBlockWeapons);

	static void SetupGroundEnvActorCollision(UMeshComponent* GroundEnvActorMesh, const bool bAllowBuilding, const bool bAffectNavigation);

	static void SetupPickupCollision(UBoxComponent* PickupBox, UMeshComponent* PickupMesh);

	// Will dynamically be allied or not with this player by blocking or ignoring the trace channel used to hit the enemies of that player.
	static void UpdateGarrisonCollisionForNewOwner(const int32 NewOwningPlayer, const bool bIsAllied, UMeshComponent* MeshToChangeCollisionOn);

	// ----------------- Collision setup Building System and buildings -----------------
	static void SetupStaticBuildingPreviewCollision(UStaticMeshComponent* BuildingPreviewMesh, const bool bUseCollision);

	static void SetupBuildingCollision(UStaticMeshComponent* BuildingMesh, int32 OwningPlayer);

	static void SetupBuildingExpansionCollision(UStaticMeshComponent* BxpMesh, int32 OwningPlayer, bool bAffectNavmesh);


	static void ForceBuildingPlacementResponse_Internal(
        AActor* Actor,
        ECollisionResponse Response,
        TSet<const AActor*>& Visited,
        const bool bRecurseAttachedActors);

private:
	static void SetupCollisionWithBuildingPreview_ForOwningPlayer(UPrimitiveComponent* Component,
		const int32 OwningPlayer);
};
