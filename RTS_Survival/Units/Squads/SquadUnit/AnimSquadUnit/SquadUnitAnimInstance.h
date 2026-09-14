// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimInstance.h"
#include "SquadAnimationEnums/SquadAnimationEnums.h"
#include "TeamWeaponCrewMontages/TeamWeaponCrewMontages.h"
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
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Misc Full Body Montages")
	UAnimMontage* GrenadePullAndThrow = nullptr;
};

/**
 * @brief Runtime state of the team weapon crew animation armed on this squad unit by its team weapon controller.
 * Armed means the operator is settled on a deployed team weapon; the entry decides whether it loops or reacts.
 */
USTRUCT()
struct FSquadUnitTeamWeaponCrewAnimRuntime
{
	GENERATED_BODY()

	void Reset();

	bool GetIsSameAssignment(const ECrewPositionType CrewRole, const ESquadSubtype TeamWeaponSquadSubtype) const
	{
		return bM_IsActive && M_CrewRole == CrewRole && M_TeamWeaponSquadSubtype == TeamWeaponSquadSubtype;
	}

	// Copy of the resolved entry; montages are also referenced by the EditDefaultsOnly struct so GC is safe.
	UPROPERTY()
	FTeamWeaponCrewMontageEntry M_ActiveEntry;

	ECrewPositionType M_CrewRole = ECrewPositionType::None;

	ESquadSubtype M_TeamWeaponSquadSubtype = ESquadSubtype::Squad_None;

	// Armed by the controller; cleared by Stop, StopAllMontages and UnitDies.
	bool bM_IsActive = false;

	// True while a loop montage (Montage or IdleLoopMontage) is expected to be playing.
	bool bM_IsLoopPlaying = false;

	// True while a reaction montage is in flight; suppresses the loop's interrupted-end handling.
	bool bM_IsReactPlaying = false;

	// The loop montage that was started last; needed to stop and to re-play it on its natural end.
	UPROPERTY()
	TObjectPtr<UAnimMontage> M_ActiveLoopMontage = nullptr;
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

	/** @param MontageTime: The time the grenade throw montage should play for. */
	void PlayGrenadeThrowMontage(const float MontageTime);

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
	void UnitDies();

	// ----- Team Weapon Crew Animations -----

	/**
	 * @brief Arms the crew animation so the operator animates only while settled on a deployed team weapon.
	 * Idempotent for the same role and subtype; a re-arm with a different role restarts the resolution.
	 * @param CrewRole Crew position type assigned to this operator.
	 * @param TeamWeaponSquadSubtype Subtype of the team weapon squad used as override key.
	 */
	void StartTeamWeaponCrewAnimation(const ECrewPositionType CrewRole, const ESquadSubtype TeamWeaponSquadSubtype);

	/** @brief Stops loop and reaction montages and returns the unit to its regular animations. */
	void StopTeamWeaponCrewAnimation();

	/**
	 * @brief Plays the reaction montage stretched over the reload so it ends when the weapon is ready again.
	 * @param ReloadTime Flux adjusted reload duration of the team weapon's first weapon in seconds.
	 */
	void OnTeamWeaponReloadStarted(const float ReloadTime);

	/** @return True while armed, even when the resolved entry has no montage (the controller must not re-arm). */
	bool GetIsTeamWeaponCrewAnimationActive() const { return M_TeamWeaponCrewAnimRuntime.bM_IsActive; }

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

	// ----- Team Weapon Crew Montages -----

	// Full body montages played per crew position type while operating a deployed team weapon.
	UPROPERTY(EditDefaultsOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontages TeamWeaponCrewMontages;

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

	// ----- Team Weapon Crew Animations -----

	/** Plays the loop montage and binds the loop-ended delegate so the code can re-play it on its natural end. */
	void PlayTeamWeaponCrewLoopMontage(UAnimMontage* LoopMontage, const float PlayRate);
	void OnTeamWeaponCrewLoopMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnTeamWeaponCrewReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Stops whichever crew montage is playing and clears the crew runtime, without touching other montages. */
	void ClearTeamWeaponCrewAnimationRuntime(const bool bStopPlayingMontages);

	/**
	 * @brief Computes the play rate that makes the reaction montage end exactly when the reload finishes.
	 * @param ReactMontage Montage that will be played; its asset RateScale is compensated for.
	 * @param ReloadTime Flux adjusted reload duration in seconds.
	 * @return Reload synced rate multiplied with the designer play rate of the active entry.
	 */
	float GetTeamWeaponCrewReactPlayRate(const UAnimMontage* ReactMontage, const float ReloadTime) const;

	UPROPERTY()
	FSquadUnitTeamWeaponCrewAnimRuntime M_TeamWeaponCrewAnimRuntime;

	// Dedicated delegates; M_MontageEndedDelegate is rebound by StartMontage and must not be shared.
	FOnMontageEnded M_TeamWeaponCrewLoopMontageEndedDelegate;
	FOnMontageEnded M_TeamWeaponCrewReactMontageEndedDelegate;
};
