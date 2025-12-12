#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScavengerComponent.generated.h"

class UNiagaraSystem;
class AScavengeableObject;
class ASquadUnit;
class ASquadController;
class UStaticMesh;
class UCapsuleComponent;

UENUM()
enum class EScavengeState : uint8
{
	None, 
	MoveToObject, 
	MoveToScavengeLocation, 
	Scavenging 
};

/**
 * @brief Component for handling scavenging operations.
 *
 * This component manages state transitions and actions involved in scavenging,
 * including moving towards scavengable objects and performing the scavenging process.
 * @note SetSquadOwner: call in blueprint to set up squad unit owner.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UScavengerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScavengerComponent();

	/**
	 * @brief Sets the current scavenging state.
	 * @param NewState The new scavenging state.
	 */
	void SetScavengeState(EScavengeState NewState);

	/**
	 * @brief Teleports and rotates the owner to face the scavengable object at a specific socket location.
	 * @param Location The target location for teleportation.
	 */
	void TeleportAndRotateAtScavSocket(const FVector& Location);

	/**
	 * @brief Sets the target scavengable object.
	 * @param ScavengeObject Pointer to the scavengable object.
	 */
	void SetTargetScavengeObject(TObjectPtr<AScavengeableObject> ScavengeObject);

	/**
	 * @brief Allows the squad unit to block or unblock the scavengable objects for when moving close to them.
	 * @param bBlock Whether to block or unblock the scavengable objects.
	 */
	void UpdateOwnerBlockScavengeObjects(const bool bBlock) const;

	/**
	 * @brief Retrieves the current scavenging state.
	 * @return The current scavenging state.
	 */
	EScavengeState GetScavengeState() const;

	/**
	 * @brief Sets the owner squad unit.
	 * @param OwnerUnit Pointer to the squad unit owner.
	 */
	void SetSquadOwner(ASquadUnit* OwnerUnit);

	void StartScavenging();
	void TerminateScavenging();

	void MoveToLocationComplete();

protected:
	// Divides the time on the scavengable object to determine the time it takes to scavenge.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	float ScavengeTimeDivider;

	// The equipment that the scavenger uses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	TObjectPtr<UStaticMesh> ScavengeEquipment;

	// The socket to which the ScavengeEquipment can be attached.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	FName ScavengeSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> ScavengeEffect;

	// The name of the socket on the equipment mesh to attach the effect to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName ScavengeEffectSocketName;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	EScavengeState M_ScavengeState;

	FTimerHandle M_TeleportHandle;

	UPROPERTY()
	TObjectPtr<ASquadUnit> M_OwnerSquadUnit;

	// Checks whether the owner squad unit pointer is valid.
	bool GetIsValidOwnerSquadUnit();

	// Checks whether the target scavengable object pointer is valid.
	bool GetIsValidTargetScavengeObject() const;

	// Extracted function to handle move-to-object completion logic.
	void HandleMoveToObjectComplete();

	TObjectPtr<AScavengeableObject> M_TargetScavengeObject;
};
