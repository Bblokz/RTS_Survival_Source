#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "TriggerOverlapLogic.h"

class UCapsuleComponent;
class USphereComponent;
class UStaticMeshComponent;

/** @brief Applies the project's collision rules when units and world objects are initialized or change owners. */
class RTS_SURVIVAL_API FRTS_CollisionSetup
{
public:
	FRTS_CollisionSetup();
	~FRTS_CollisionSetup();

	static void ForceBuildingPlacementResponseOnActor(AActor* Actor, ECollisionResponse Response,
	                                                  bool bRecurseAttachedActors);

	// ----------------- Collision Setup Vehicles -----------------

	/**
	 * @brief Init the collision settings for the movement mesh (chassis) of a vehicle: blocks the vehicles of the
	 * other side of the human / AI split and marks the mesh as owned, so area queries know whose it is.
	 * @param MovementMesh The mesh component to set collision for.
	 * @param OwningPlayer Owner of the vehicle.
	 */
	static void SetupVehicleMovementMeshCollision(UMeshComponent* MovementMesh, int32 OwningPlayer);

	/**
	 * @brief Makes a box around a vehicle hull solid for the hull boxes of other vehicles and nothing else.
	 * The chassis physics is a row of spheres per track, which lets hulls slide into and over each other.
	 * @param HullBlocker Box that is welded to the chassis.
	 * @param OwningPlayer Owner of the vehicle; decides which side's hull channel the box is on.
	 * @param bPassesThroughAllies Harvesters: the box only meets the hulls of the other side.
	 */
	static void SetupVehicleHullBlockerCollision(UBoxComponent* HullBlocker, uint8 OwningPlayer,
	                                             bool bPassesThroughAllies);

	/**
	 * @brief Vehicle movement collision for harvesters: they drive through allied vehicles and other harvesters
	 * (overlapping them, so infantry still makes way) and stay solid for enemy vehicles.
	 * @param MovementMesh The chassis mesh to set collision for.
	 * @param OwningPlayer Owner of the harvester.
	 */
	static void SetupHarvesterMovementMeshCollision(UMeshComponent* MovementMesh, uint8 OwningPlayer);

	// Differs from the vehicle movement mesh collision as it ignores collision with building placements.
	static void SetupNomadicMovementMeshCollision(UMeshComponent* MovementMesh, int32 OwningPlayer);

	/** Configure an InstancedStaticMesh grid overlay for building placement preview. */
	static void SetupBuildingPlacementGridOverlay(UInstancedStaticMeshComponent* GridISM);

	// ----------------- Collision Setup Tanks -----------------
	/** Armor box of a unit: on the object channel of its owner's side and hit by the weapons of hostile players. */
	static void SetupArmorMeshCollision(UBoxComponent* ArmorMesh, int32 OwningPlayer);

	static void SetupArmorCalculationMeshCollision(UMeshComponent* ArmorMesh,
	                                               const uint8 OwningPlayer,
	                                               const bool bAffectNavMesh);

	/**
	 * @brief Restricts an active shield to the weapons that can damage its owner: those of hostile players.
	 * @param ShieldMesh Runtime sphere mesh used by the shield component.
	 * @param OwningPlayer Owner of the shield; its own weapons and those of its allies pass through.
	 */
	static void SetupShieldCollision(UStaticMeshComponent* ShieldMesh, int32 OwningPlayer);

	/**
	 * @brief Lets aircraft fly through units and map boundaries while enemy weapon queries can still hit them.
	 * Preserves authored mouse/camera queries so aircraft selection keeps working.
	 * @param AircraftComponent An aircraft primitive, including components that do not receive weapon hits.
	 * @param OwningPlayer The owner of the aircraft.
	 * @param bReceivesWeaponHits Whether this component provides weapon-hit geometry.
	 */
	static void SetupAircraftComponentCollision(UPrimitiveComponent* AircraftComponent, int32 OwningPlayer,
	                                           bool bReceivesWeaponHits);

	static void SetupTrackAttachedMeshCollision(UMeshComponent* MeshComponent, int32 OwningPlayer);

	/**
	 * @brief Applies train collision in batch while keeping train meshes query-only and non-overlapping.
	 * @param MeshComponents Meshes that should receive train collision setup.
	 * @param OwningPlayer Owner of the train; decides the object channel and whose weapons hit it.
	 */
	static void SetupTrainMeshesCollision(const TArray<UMeshComponent*>& MeshComponents, int32 OwningPlayer);

	static void SetupNomadicMountCollision(UMeshComponent* MeshComponent, int32 OwningPlayer);

	static void SetupCollisionForHullMountedWeapon(UMeshComponent* MeshComponent);

	/**
	 * @brief Configures team-weapon meshes so world interaction and hit traces remain team-aware.
	 * @param WeaponMeshes Mesh components that should receive team-weapon collision settings.
	 * @param OwningPlayer Team owner used to select which weapon-hit trace channel should be blocked.
	 */
	static void SetupCollisionForTeamWeaponMeshes(const TArray<UMeshComponent*>& WeaponMeshes, uint8 OwningPlayer);

	// ----------------- Collision Setup Infantry -----------------
	static void SetupInfantryWeaponCollision(UStaticMeshComponent* WeaponMesh);

	static void SetupInfantryCapsuleCollision(UCapsuleComponent* CapsuleComponent, const int OwningPlayer);

	/**
	 * @brief Configure overlap-only collision for trigger components.
	 * @param TriggerComponent Component to configure collision on.
	 * @param TriggerLogic Target overlap channels for players or enemies.
	 */
	static void SetupTriggerOverlapCollision(UPrimitiveComponent* TriggerComponent, ETriggerOverlapLogic TriggerLogic);

	/**
	 * @brief Overlap-only trigger of a mine. It overlaps the units of every player: hostile and allied AI players
	 * share one object channel and relations can change while the mine lies there, so the mine's overlap handler
	 * decides who sets it off.
	 */
	static void SetupFieldMineTriggerCollision(USphereComponent* TriggerSphere);

	// ----------------- Collision setup static objects -----------------
	// if bnNoCollision is true, the object will have no except for player mouse interactions.
	static void SetupScavengeableObjectCollision(UStaticMeshComponent* ScavengeableObjectMesh);

	static void SetupResourceMeshCollision(UMeshComponent* ResourceMesh);
	static void SetupResourceGeometryComponentCollision(UGeometryCollectionComponent* GeometryComponent);

	static void SetupDestructibleEnvActorMeshCollision(UMeshComponent* DestructibleEnvActorMesh,
	                                                   const bool bOverlapTanks);
	static void SetupWallActorMeshCollision(UMeshComponent* WallMesh, const bool bAffectNavigation,
	                                        const uint8 OwningPlayer);
	static void SetupDestructibleEnvActorGeometryComponentCollision(UGeometryCollectionComponent* GeometryComponent);

	static void SetupFieldConstructionMeshCollision(UMeshComponent* FieldConstructionMesh, const int32 OwningPlayer,
	                                                const bool bAlliedProjectilesHitObject = false, const
	                                                bool bOverlapTanks = false);

	static void SetupObstacleCollision(UMeshComponent* ObstacleMesh, const bool bBlockWeapons);

	static void SetupGroundEnvActorCollision(UMeshComponent* GroundEnvActorMesh, const bool bAllowBuilding,
	                                         const bool bAffectNavigation);

	static void SetupSplineNoCollision(UMeshComponent* SplineMesh);

	/**
	 * @brief Invisible ramp that vehicle chassis drive over (field trench earth banks): physics only, so traces and
	 * infantry capsules (query-only, ignoring its object type) never see it.
	 */
	static void SetupVehicleRampCollision(UPrimitiveComponent* RampComponent);

	static void SetupPickupCollision(UBoxComponent* PickupBox, UMeshComponent* PickupMesh);

	// Will dynamically be allied or not with this player: while allied the mesh carries the owner bit of that player,
	// so the weapon queries of that player and its allies skip it and everyone else still hits it.
	static void UpdateGarrisonCollisionForNewOwner(const int32 NewOwningPlayer, const bool bIsAllied,
	                                               UMeshComponent* MeshToChangeCollisionOn);

	// ----------------- Collision setup Building System and buildings -----------------
	static void SetupStaticBuildingPreviewCollision(UStaticMeshComponent* BuildingPreviewMesh,
	                                                const bool bUseCollision);

	static void SetupBuildingCollision(UStaticMeshComponent* BuildingMesh, int32 OwningPlayer);

	static void SetupBuildingExpansionCollision(UStaticMeshComponent* BxpMesh, int32 OwningPlayer, bool bAffectNavmesh);


	static void ForceBuildingPlacementResponse_Internal(
		AActor* Actor,
		ECollisionResponse Response,
		TSet<const AActor*>& Visited,
		const bool bRecurseAttachedActors);

private:
	// The four below only know the human / AI split of the physics channels; the owner-taking functions call them.
	static void SetupPlayerVehicleMovementMeshCollision(UMeshComponent* MovementMesh);
	static void SetupEnemyVehicleMovementMeshCollision(UMeshComponent* MovementMesh);
	static void SetupNomadicMvtPlayer(UMeshComponent* MovementMesh);
	static void SetupNomadicMvtEnemy(UMeshComponent* MovementMesh);

	static void SetupCollisionWithBuildingPreview_ForOwningPlayer(UPrimitiveComponent* Component,
	                                                              const int32 OwningPlayer);
};
