// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimInstance.h"
#include "SquadAnimationEnums/SquadAnimationEnums.h"
#include "SquadUnitAnimInstance.generated.h"


class UNiagaraSystem;
class USelectionComponent;
enum class ESquadWeaponAimOffset : uint8;
enum class ESquadAimPosition : uint8;
enum class ESquadAimPositionMontage : uint8;
enum class ESquadMovementAnimState : uint8;
enum class ESquadWeaponMontage : uint8;

// Struct to hold different Aim Offset references
USTRUCT(BlueprintType)
struct FAimOffsetTypes
{
	GENERATED_BODY()

	FAimOffsetTypes();

	// When the weapon is changed or initialized, this function is called to set the aim offset type.
	void UpdateAOForNewWeapon(const ESquadWeaponAimOffset NewAimOffsetType);

	/**
	 * @brief Updates the aim offset depending on the new aim position and the current aim offset type.
	 * @param NewAimPosition The new aiming position.
	 * @pre Make sure that the ActiveAimOffset is set to the correct aim offset type for the weapon held.
	 */
	void UpdateAOForNewAimPosition(const ESquadAimPosition NewAimPosition);

	// Holds the aim offset for the current AimOffsetType
	UPROPERTY()
	UAimOffsetBlendSpace* M_ActiveAimOffset;

	// Holds the aim offset sequence for the current AimOffsetType.
	UPROPERTY()
	UAnimSequence* M_ActiveAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* RifleAimOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* RifleCrouchAimOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* PistolAimOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* PistolCrouchAimOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* HipAimOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAimOffsetBlendSpace* HipCrouchAimOffset;

	// The aim offset type that is used as determined by the weapon carried.
	UPROPERTY(BlueprintReadOnly, Category = "Aim Offsets")
	ESquadWeaponAimOffset M_AimOffsetType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* RifleAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* RifleCrouchAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* PistolAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* PistolCrouchAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* HipAimOffsetSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim Offsets")
	UAnimSequence* HipCrouchAimOffsetSequence;

private:
	void SetRifle_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition);
	void SetHip_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition);
	void SetPistol_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition);
};

USTRUCT(BlueprintType)
struct FWeaponMontages
{
	GENERATED_BODY()

	FWeaponMontages();

	/**
	 * @param FirePosition What fire position the unit is in, standing, crouch or prone.
	 * @param AimOffsetType The type of aim offset used which determiens what fire montage to play.
	 * @param bIsSingleFire Whether to play the single or burst fire montage type.
	 * @return The montage.
	 */
	UAnimMontage* GetFireMontage(
		const ESquadAimPosition FirePosition,
		const ESquadWeaponAimOffset AimOffsetType, const bool bIsSingleFire);

	UAnimMontage* GetRifleFireMontage(
		const ESquadAimPosition FirePosition,
		const bool bIsSingleFire) const;

	UAnimMontage* GetPistolFireMontage(
		const ESquadAimPosition FirePosition) const;

	UAnimMontage* GetHipFireMontage(
		const ESquadAimPosition FirePosition,
		const bool bIsSingleFire) const;


	/**
	 * @param AimOffsetType The type of aim offset used which determines what reload montage to play.
	 * @return The montage to play.
	 */
	UAnimMontage* GetReloadMontage(
		const ESquadWeaponAimOffset AimOffsetType);

	UAnimMontage* GetSwitchWeaponMontage(
		const ESquadWeaponAimOffset AimOffsetOfNewWeapon);

	// Map of montages associated with each ESquadWeaponMontage enum
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* MontageReloadRifleStanding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* MontageReloadRifleHip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* MontageReloadPistolStanding;


	// ----- Rifle Fire Montages -----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleSingleFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleBurstFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleCrouchSingleFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleCrouchBurstFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleProneSingleFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* RifleProneBurstFireMontage;

	// ----- Hip Fire Montages -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireSingleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireBurstMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireCrouchSingleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireCrouchBurstMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireProneSingleMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HipFireProneBurstMontage;

	// ----- Pistol Fire Montages -----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<UAnimMontage*> PistolSingleFireMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<UAnimMontage*> PistolCrouchSingleFireMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<UAnimMontage*> PistolProneSingleFireMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* SwitchToPistolMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* SwitchToRifleMontage;

	/** Stores the current montage type being played */
	ESquadWeaponMontage ActiveWeaponMontage;

	UAnimMontage* GetRandomStandingPistolFireMontage() const;
	UAnimMontage* GetRandomCrouchPistolFireMontage() const;
	UAnimMontage* GetRandomPronePistolFireMontage() const;
};

USTRUCT(BlueprintType)
struct FAimPositionMontages
{
	GENERATED_BODY()

	FAimPositionMontages();

	// Is set to NoActiveMontage when there is no full body aim position montage active.
	ESquadAimPositionMontage ActiveAimPositionMontage;

	UPROPERTY(BlueprintReadOnly)
	ESquadAimPosition AimPosition = ESquadAimPosition::Standing;

	UAnimMontage* GetToCrouchAimPositionMontage(const ESquadWeaponAimOffset AimOffsetType);

	UAnimMontage* GetToStandingAimPositionMontage(const ESquadWeaponAimOffset AimOffsetType);

	UAnimMontage* GetMiscFullBodyMontage(const ESquadAimPositionMontage MontageType);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* StandingToCrouch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* HipToCrouch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* CrouchToHip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* CrouchToStanding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* CrouchToPistol;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	UAnimMontage* PistolToCrouch;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Misc Full Body Montages")
	UAnimMontage* Welding;
};

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class RTS_SURVIVAL_API USquadUnitAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend class RTS_SURVIVAL_API ULeftFootDownNotify;
	friend class RTS_SURVIVAL_API URightFootDownNotify;

public:
	USquadUnitAnimInstance();

	/**
	 * @brief Called by the SquadUnit that uses this anim instance, updates every so few seconds by providing the velocity
	 * of the actor.
	 * @param VectorSpeed The actor's velocity.
	 */
	void UpdateAnimState(const FVector& VectorSpeed);

	void SetSquadUnitMesh(UMeshComponent* Mesh) { MeshComponent = Mesh; }

	/**
	 * @brief Binds the selection functions for setting the alert state of the unit.
	 * @param SelectionComponent The selection component of which the delegates will be bound.
	 */
	void BindSelectionFunctions(USelectionComponent* SelectionComponent);

	inline ESquadMovementAnimState GetMovementState() const { return MovementState; }

	/**
	 * @brief Updated with the weapon, uses the signed direction angle towards the weapon's target.
	 * @param Angle 
	 */
	inline void SetAimOffsetAngle(const float& Angle)
	{
		AimOffsetAngle = Angle;
		bAimToTarget = true;
	}

	/**
	 * @brief Selects the correct fire animation to play based on the current aim offset type.
	 * @post The montage is played and the type of montage is saved in M_CurrentMontageType.
	 */
	void PlaySingleFireAnim();

	/**
	 * @brief Selects the correct fire animation to play based on the current aim offset type.
	 * @post The montage is played and the type of montage is saved in M_CurrentMontageType.
	 */

	void PlayBurstAnim();
	/**
	 * @brief Determines the reload animation to play based on the current aim offset type.
	 * @post The montage is played and the type of montage is saved in M_CurrentMontageType.
	 */
	void PlayReloadAnim(const float ReloadTime);

	/**
	 * @brief Selects the montage to play to switch to the aim offset provided by the new weapon.
	 * @param NewWeaponAimOffset The aim offset the new weapon uses.
	 */
	void PlaySwitchWeaponMontage(const ESquadWeaponAimOffset NewWeaponAimOffset);

	void PlayWeldingMontage();

	// Stop playing all montages on the unit.
	void StopAllMontages();

	void StopAiming() { bAimToTarget = false; }

	/**
	 * @brief Sets the aim offset variable according to the provided type to use the right blend space in the
	 * derived animation blueprint.
	 * @param AimOffsetType The type of aim offset to use.
	 */
	void SetWeaponAimOffset(ESquadWeaponAimOffset AimOffsetType);

	// attempts to unbind the selection functions from the selection component delegatges.
	void UnitDies() const;

protected:
	// Set by unit selection.
	UPROPERTY(BlueprintReadOnly)
	bool bBeAlert;

	UPROPERTY(BlueprintReadOnly)
	ESquadMovementAnimState MovementState;

	// ----- Aim Offset -----

	// Container for all aim offset assets and the current aim offset type.
	UPROPERTY(EditDefaultsOnly)
	FAimOffsetTypes AimOffsets;

	/**
	 * Threadsafe to allow multiple threads to access the function.
	 * @brief Returns the current aim offset based on the current aim offset type. 
	 * @return The current aim offset.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAimOffsetBlendSpace* GetCurrentAimOffset() const { return AimOffsets.M_ActiveAimOffset; }

	/**
	 * Threadsafe to allow multiple threads to access the function.
	 * @return The current aim offset sequence that is the base pose, based on the current aim offset type.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAnimSequence* GetCurrentAOBaseSequence() const { return AimOffsets.M_ActiveAimOffsetSequence; }

	// ----- Weapon Montages -----

	UPROPERTY(EditDefaultsOnly)
	FWeaponMontages WeaponMontages;

	// Set to true when the weapon has a target to aim at.
	UPROPERTY(BlueprintReadOnly)
	bool bAimToTarget;

	// ----- Aim Position Montages -----

	// Contains aim state and active aim montage to switch between aim positions.
	UPROPERTY(EditDefaultsOnly)
	FAimPositionMontages AimPositionMontages;

	UPROPERTY(BlueprintReadOnly)
	float Speed;

	UPROPERTY(BlueprintReadOnly)
	float AimOffsetAngle;

	UPROPERTY(BlueprintReadOnly)
	float WalkingPlayRate;

	UPROPERTY(BlueprintReadOnly)
	float RunningPlayRate;

	UPROPERTY(BlueprintReadOnly)
	UMeshComponent* MeshComponent;

	// Socket names for the left and right foot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep")
	FName LeftFootSocketName = TEXT("LeftFootSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep")
	FName RightFootSocketName = TEXT("RightFootSocket");

	// Niagara effect for footstep
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footstep")
	UNiagaraSystem* FootstepEffect;

private:
	/**
	 * Plays the montage and binds a function to when the montage ends.
	 * @param SelectedMontage The montage selected to play.
	 * @param bIsWeaponMontage Determines whether we bind the on weapon montage finished function or the
	 * on aim position montage finished function.
	 * @param PlayTime
	 */
	void StartMontage(
		UAnimMontage* SelectedMontage,
		const bool bIsWeaponMontage,
		const float PlayTime = 0);

	/** Called when a weapon montage has finished playing. */
	void OnWeaponMontageFinished(UAnimMontage* Montage, bool bInterrupted);

	/** Called when an aim position montage has finished playing. */
	void OnAimPositionMontageFinished(UAnimMontage* Montage, bool bInterrupted);


	FOnMontageEnded M_MontageEndedDelegate;

	void SetMovementStateWithSpeed(const float& MovementSpeed);

	void OnStartAimingWhileIdle();
	void OnStartWalking();

	void OnUnitSelected();
	void OnUnitDeselected();
};
