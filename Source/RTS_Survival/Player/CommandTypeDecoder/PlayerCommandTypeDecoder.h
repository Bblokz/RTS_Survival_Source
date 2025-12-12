#pragma once

#include "CoreMinimal.h"
#include "TargetUnion/TargetUnion.h"

#include "PlayerCommandTypeDecoder.generated.h"

enum class EAllUnitType : uint8;
class UNiagaraComponent;
class UNiagaraSystem;
// Forward declarations
enum class ECommandType : uint8;


/**
* @brief: Contains all possible effects to place on the map for selected units.
* is used to show the queued abilities of the units
*/ 
UENUM(BlueprintType)
enum class EPlacementEffect : uint8
{
	Effect_RegularClick,
	Effect_Movement,
	Effect_Attack,
	Effect_HarvestResource,
	Effect_Patrol,
	Effect_Rotate,
	Effect_PickUpItem,
	Effect_Repair,
};


UCLASS()
class RTS_SURVIVAL_API UPlayerCommandTypeDecoder : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerCommandTypeDecoder(const FObjectInitializer& ObjectInitializer);

	/** @return The command type associated with what actor was clicked,
	 * if applicable a valid target is stored in the OutTarget uniion
	 * @param ClickedActor The actor clicked
	 * @param OutTarget The target pointer is set in this union. */
	ECommandType DecodeTargetedActor(AActor*& ClickedActor, union FTargetUnion& OutTarget) const;

	/** @return The effect type for this command; use when the effect is not bound by an action button. */
	EPlacementEffect DecodeCommandTypeIntoEffect(ECommandType CommandType) const;

	/**
	 * @brief 
	 * @param ClickSystems Niagara systems that destroy themselves, one time click effect.
	 * @param PlaceSystems Niagara systems that are placed on the map, and are only destroyed
	 * when the selection changes the action queues are reset.
	 */
	UFUNCTION(BlueprintCallable)
	void InitCommandTypeDecoder(
		TMap<EPlacementEffect, UNiagaraSystem*>  ClickSystems,
		TMap<EPlacementEffect, UNiagaraSystem*> PlaceSystems);


	/**
	 * @brief Creates the placement effect for the command and possibly also the 
	 * @param bResetAllPlacementEffects If true the spawned placement effects prior to this
	 * function call are destroyed
	 * @param AmountCommandsExe How many commands were executed.
	 * @param Location Where to spawn the niagara effect.
	 * @param CommandType The type of commands to infer the effect from.
	 * @param bAlsoCreateClickEffect Whether to not only create the permanent effect
	 * but also the click effect.
	 */
	void CreatePlacementEffectIfCommandExecuted(
		const bool bResetAllPlacementEffects,
		const uint32 AmountCommandsExe,
		const FVector& Location,
		const EPlacementEffect CommandType,
		const bool bAlsoCreateClickEffect);

	// Remove all placement effects from the map.
	void ResetAllPlacementEffects();

	void SpawnPlacementEffectsForPrimarySelected(
		EAllUnitType PrimaryUnitType,
		AActor* PrimarySelectedUnit);


private:
	UPROPERTY()
	TMap<EPlacementEffect, UNiagaraSystem*> M_TClickVFX;

	UPROPERTY()
	TMap<EPlacementEffect, UNiagaraSystem*> M_TPlaceVFX;

	TArray<UNiagaraComponent*> M_TSpawnedSystems;

	void SpawnPlacementSystemAtLocation(
		const UWorld* World,
		EPlacementEffect Effect,
		const FVector& Location,
		const FRotator& Rotation);

	void SpawnClickSystemAtLocation(
		const UWorld* World,
		EPlacementEffect Effect,
		const FVector& Location,
		const FRotator& Rotation);

	FRotator GetRotationFromClickedLocation(const FVector& Location) const;

	//----------------------------------------------------------------------
	// --------------------- START DECODE HELPERS ---------------------
	//----------------------------------------------------------------------


	/** @brief If a turret was clicked and it has a parent, redirect ClickedActor to the parent. */
	void Decode_TurretParentRedirect(AActor*& ClickedActor) const;

	/**
	 * @brief Decode when the actor is a character.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to AlliedCharacter or EnemyCharacter when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_Character(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	bool Decode_CaptureActor(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const; 

	/**
	 * @brief Decode when the actor is an HP actor (buildings/HP-based actors).
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to AlliedBuilding/AlliedActor/EnemyActor when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_HpActor(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;
	static bool Decode_CargoActor(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType);

	/**
	 * @brief Decode when the actor is an HP actor and specifically an aircraft building (airfield, carrier).
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to AlliedAirfield Actor if matched.
	 * @return 
	 */
	bool Decode_HpPawn_IntoAircraftBuilding(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	/**
	 * @brief Decode when the actor is an HP pawn.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to AlliedActor or EnemyActor when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_HpPawn(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	/**
	 * @brief Decode when the actor is a pickup item.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to PickupItem when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_Item(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	/**
	 * @brief Decode when the actor is a scavengeable object.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to ScavengeObject when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_Scavengeable(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	/**
	 * @brief Decode when the actor is a resource node.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to HarvestResource when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_Resource(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;

	/**
	 * @brief Decode when the actor is a destructible environment actor.
	 * @param ClickedActor Candidate actor (not null).
	 * @param OutTarget Target union to fill when matched.
	 * @param OutType Set to DestructableEnvActor when matched.
	 * @return true if this decoder handled the actor.
	 */
	bool Decode_DestructibleEnv(AActor* ClickedActor, FTargetUnion& OutTarget, ECommandType& OutType) const;


	static bool GetIsActorAlliedToPlayer(AActor* ClickedActor);
	static bool GetIsActorNeutral(AActor* ClickedActor);
	static bool IsOfFaction(AActor* ClickedActor, const uint8 OwningPlayer);

	//----------------------------------------------------------------------
	// --------------------- END DECODE HELPERS ---------------------
	//----------------------------------------------------------------------
};
