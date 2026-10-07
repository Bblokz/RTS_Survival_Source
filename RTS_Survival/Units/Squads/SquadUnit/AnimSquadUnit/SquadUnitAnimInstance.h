// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimInstance.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "SquadAnimationEnums/SquadAnimationEnums.h"
#include "TeamWeaponCrewMontages/TeamWeaponCrewMontages.h"
#include "SquadUnitAnimInstance.generated.h"


class UNiagaraSystem;
class USelectionComponent;
class UAnimSequence;
enum class ESquadWeaponAimOffset : uint8;
enum class ESquadAimPosition : uint8;
enum class ESquadAimPositionMontage : uint8;
enum class ESquadMovementAnimState : uint8;
enum class ESquadWeaponMontage : uint8;

DECLARE_DELEGATE(FOnSquadUnitDeathMontageFinished);

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
 * @brief Supplies full-body death animations and the team-weapon stance overrides used at death.
 * Designers configure this on the squad unit animation Blueprint defaults.
 */
USTRUCT(BlueprintType)
struct FSquadUnitDeathMontages
{
	GENERATED_BODY()

	/** @return A random valid montage for the requested death stance, or nullptr when none is usable. */
	UAnimMontage* GetRandomDeathMontage(bool bUseCrouchedDeathMontage) const;

	/** Full-body montages used when the unit dies standing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death Montages")
	TArray<TObjectPtr<UAnimMontage>> StandingDeathMontages;

	/** Full-body montages used when the unit dies crouched or prone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death Montages")
	TArray<TObjectPtr<UAnimMontage>> CrouchedDeathMontages;

	/** Team-weapon subtypes whose assigned operators always use a crouched death montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death Montages|Team Weapon")
	TArray<ESquadSubtype> TeamWeaponSubtypesUsingCrouchedDeathMontages;
};

/**
 * @brief Keeps the cover aim offset and its required base pose together for AnimGraph evaluation.
 * The first implementation is configured with the available rifle cover assets.
 */
USTRUCT(BlueprintType)
struct FSquadUnitCoverAimAssets
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UAimOffsetBlendSpace> AimOffset = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UAnimSequence> BaseSequence = nullptr;
};

/**
 * Root travel measured on the shipped high-cover clips, used as the default offsets below.
 * RTS.CoverFinder logs RTS_COVER_ROOT_MOTION for every configured montage when its pose is first used,
 * which gives the numbers to enter after a clip is replaced.
 */
namespace SquadUnitCoverAnimDefaults
{
	// AnimRTS_ExitHighCover* played in reverse walks the root 125 cm toward the cover.
	inline constexpr float StandingEnterStartDepth = -125.0f;
	// Canim_HiCover2AimL ends 46 cm back and 110 cm to the left of the protected pose.
	inline constexpr float StandingLeftExposedDepth = -46.0f;
	inline constexpr float StandingLeftExposedRight = -110.0f;
	// Canim_HiCover2AimR ends 44 cm back and 62 cm to the right of the protected pose.
	inline constexpr float StandingRightExposedDepth = -44.0f;
	inline constexpr float StandingRightExposedRight = 62.0f;
}

/**
 * @brief A capsule position relative to the in-cover location, in the frame of a soldier facing the cover.
 * Designers tune these so the code-side positions match where each cover animation starts or ends.
 */
USTRUCT(BlueprintType)
struct FSquadUnitCoverLocalOffset
{
	GENERATED_BODY()

	// Positive is closer to the cover surface; negative is further out in front of it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover", meta = (Units = "cm"))
	float TowardCover = 0.0f;

	// Positive is the soldier's right-hand side while facing the cover.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover", meta = (Units = "cm"))
	float Right = 0.0f;
};

/**
 * @brief Supplies one side of standing cover, where firing requires leaving the protected pose first.
 * Left and right instances keep their authored root-motion transitions independent.
 */
USTRUCT(BlueprintType)
struct FSquadUnitStandingCoverAnimationSet
{
	GENERATED_BODY()

	FSquadUnitStandingCoverAnimationSet();

	// Where the unit stops before EnterCoverMontage plays, so the clip's root travel ends on the cover point.
	// Set this to minus the travel of the enter clip; zero when the clip is authored in place.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Root Motion")
	FSquadUnitCoverLocalOffset EnterStartOffset;

	// Where ExposeFromCoverMontage leaves the unit. Also the origin of the firing-lane check for this side.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Root Motion")
	FSquadUnitCoverLocalOffset ExposedOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UAnimSequence> ProtectedIdlePose = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	FSquadUnitCoverAimAssets PeekAimAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> EnterCoverMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> ExitCoverMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> ExposeFromCoverMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> ReturnToCoverMontage = nullptr;
};

/**
 * @brief Supplies crouch cover, whose aim offset can fire without a separate expose transition.
 * Crouch cover is intentionally non-sided in the initial implementation.
 */
USTRUCT(BlueprintType)
struct FSquadUnitCrouchCoverAnimationSet
{
	GENERATED_BODY()

	// Where the unit stops before EnterCoverMontage plays. Left at zero: the crouch clip is treated as in place.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Root Motion")
	FSquadUnitCoverLocalOffset EnterStartOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UAnimSequence> ProtectedIdlePose = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	FSquadUnitCoverAimAssets AimAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> EnterCoverMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover|Transitions")
	TObjectPtr<UAnimMontage> ExitCoverMontage = nullptr;
};

/** @brief Groups the three cover pose families configured on the master infantry animation Blueprint. */
USTRUCT(BlueprintType)
struct FSquadUnitCoverAnimationSets
{
	GENERATED_BODY()

	FSquadUnitCoverAnimationSets();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	FSquadUnitStandingCoverAnimationSet StandingLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	FSquadUnitStandingCoverAnimationSet StandingRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cover")
	FSquadUnitCrouchCoverAnimationSet Crouch;
};

/**
 * @brief Keeps cover montage ownership and notify-driven pose state coherent during transitions.
 * Gameplay cover geometry and reservations remain owned outside the animation instance.
 */
USTRUCT()
struct FSquadUnitCoverAnimRuntime
{
	GENERATED_BODY()

	void Reset();

	ESquadIdleAnimationPose M_IdlePose = ESquadIdleAnimationPose::Regular;
	ESquadCoverAnimAction M_Action = ESquadCoverAnimAction::None;
	ESquadCoverAnimAction M_ActiveMontageAction = ESquadCoverAnimAction::None;
	ESquadCoverAnimAction M_NextAction = ESquadCoverAnimAction::None;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> M_ActiveMontage = nullptr;

	// Play time of the active cover montage, so its owner can time out a transition that never ticks off screen.
	float M_ActiveMontageSeconds = 0.0f;
};

DECLARE_DELEGATE_OneParam(FOnSquadCoverAnimActionChanged, ESquadCoverAnimAction);

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
 * @brief Drives squad-unit locomotion and selects the full-body montages used by unit gameplay.
 * Animation Blueprint defaults provide the stance-specific and team-weapon-specific animation assets.
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
	 * @brief Starts the full-body transition into one of the three supported protected cover poses.
	 * @param CoverPose CrouchCover, StandingCoverLeft, or StandingCoverRight.
	 * @return True only when the configured enter montage started.
	 */
	bool EnterCover(ESquadIdleAnimationPose CoverPose);

	/** @return True only when a standing protected pose started its expose montage. */
	bool StartStandingCoverPeek();

	/** @return True only when an exposed standing pose started returning to protection. */
	bool ReturnToStandingCover();

	/** @return True when an exit montage started immediately or was queued after returning to protection. */
	bool ExitCover();

	/** Stops only the active cover montage and restores the regular idle animation family. */
	void CancelCoverAnimation();

	/** @return True from the start of entering cover until exit or cancellation finishes. */
	bool GetIsCoverAnimationActive() const;

	/** @return True when crouch cover or an exposed standing peek currently permits weapon fire. */
	bool GetIsCoverFireAllowed() const;
	ESquadCoverAnimAction GetCoverAnimAction() const { return M_CoverAnimRuntime.M_Action; }

	// Fired on every cover action change so the owning unit can line its capsule up without waiting for a poll.
	FOnSquadCoverAnimActionChanged OnCoverAnimActionChanged;

	/** @return Designer offset of the stop position before the enter montage; zero without an enter montage. */
	FSquadUnitCoverLocalOffset GetCoverEnterStartOffset(ESquadIdleAnimationPose CoverPose) const;

	/**
	 * @return True when the enter montage moves the capsule through extracted root motion. False means the clip
	 * only moves the mesh, so the owner places the capsule on the cover point when the montage starts.
	 */
	bool GetDoesCoverEnterMontageMoveCapsule(ESquadIdleAnimationPose CoverPose) const;

	/**
	 * @brief Lets cover selection test the firing lane from where the expose montage really puts the unit.
	 * @param CoverPose Standing cover or standing peek pose of the wanted side.
	 * @param OutExposedOffset Designer offset of the exposed position.
	 * @return False when that side has no expose montage, so the caller must use its own default step.
	 */
	bool TryGetStandingCoverExposedOffset(
		ESquadIdleAnimationPose CoverPose,
		FSquadUnitCoverLocalOffset& OutExposedOffset) const;

	bool GetIsCoverTransitionMontageActive() const;
	float GetActiveCoverMontageSeconds() const { return M_CoverAnimRuntime.M_ActiveMontageSeconds; }

	/** Ends a cover transition whose montage is not advancing, for example on a mesh that is not rendered. */
	void ForceCompleteCoverTransition();

	/**
	 * @brief Updated with the weapon, uses the signed direction angle towards the weapon's target.
	 * @param Angle 
	 */
	inline void SetAimOffsetAngle(const float& Angle)
	{
		AimOffsetAngle = Angle;
		bAimToTarget = true;
		RefreshCoverGraphPose();
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

	void StopAiming()
	{
		bAimToTarget = false;
		RefreshCoverGraphPose();
	}

	/**
	 * @brief Sets the aim offset variable according to the provided type to use the right blend space in the
	 * derived animation blueprint.
	 * @param AimOffsetType The type of aim offset to use.
	 */
	void SetWeaponAimOffset(ESquadWeaponAimOffset AimOffsetType);

	// attempts to unbind the selection functions from the selection component delegatges.
	void UnitDies();

	/**
	 * @brief Selects and starts the death montage while retaining a safe completion callback.
	 * @param bUseCrouchedDeathMontage Whether to select from the crouched rather than standing array.
	 * @param CompletionDelegate Callback used to continue death teardown after the montage ends.
	 * @param OutExpectedDuration Actual one-pass duration used to continue shortly before the montage ends.
	 * @return True only when a valid montage started playing.
	 */
	bool PlayDeathMontage(
		bool bUseCrouchedDeathMontage,
		const FOnSquadUnitDeathMontageFinished& CompletionDelegate,
		float& OutExpectedDuration);

	/**
	 * @brief Keeps ordinary deaths stance-driven while team-weapon operators use designer subtype overrides.
	 * @param bIsTeamWeaponOperator Whether the dying unit belongs to the controller's operator assignment.
	 * @param TeamWeaponSquadSubtype Subtype of that operator's team weapon; ignored for ordinary units.
	 * @return True when the crouched death montage array should be used.
	 */
	bool GetShouldUseCrouchedDeathMontage(
		bool bIsTeamWeaponOperator,
		ESquadSubtype TeamWeaponSquadSubtype) const;

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

	// Selects the regular or cover-specific pose family inside the AnimGraph's idle branch.
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	ESquadIdleAnimationPose IdleAnimationPose = ESquadIdleAnimationPose::Regular;

	// Drives the single Blend Poses by Enum in the idle branch; derived from IdleAnimationPose in C++.
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	ESquadCoverGraphPose CoverGraphPose = ESquadCoverGraphPose::NotInCover;

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

	/** @return Protected cover idle pose for the current cover family, or nullptr outside cover. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	UAnimSequence* GetCurrentCoverIdlePose() const;

	/** @return Cover aim offset for crouch cover or an exposed standing peek. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	UAimOffsetBlendSpace* GetCurrentCoverAimOffset() const;

	/** @return Base sequence paired with the current cover aim offset. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	UAnimSequence* GetCurrentCoverAimBaseSequence() const;

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

	// Designer-authored cover poses, aim offsets, and full-body transition montages.
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	FSquadUnitCoverAnimationSets CoverAnimations;

	// ----- Team Weapon Crew Montages -----

	// Full body montages played per crew position type while operating a deployed team weapon.
	UPROPERTY(EditDefaultsOnly, Category = "Team Weapon Crew")
	FTeamWeaponCrewMontages TeamWeaponCrewMontages;

	// Full body death montages and the team-weapon subtypes that override the current unit stance.
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	FSquadUnitDeathMontages DeathMontages;

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

	// ----- Cover Animations -----

	/** Named notify on cover-to-aim animations; standing-cover weapons may fire after this point. */
	UFUNCTION()
	void AnimNotify_Cover_AimReady();

	/** Named notify on aim-to-cover animations; switches back to the protected idle pose. */
	UFUNCTION()
	void AnimNotify_Cover_BackInCover();

	/**
	 * @brief Starts a cover montage without sharing the generic or team-weapon completion delegates.
	 * @param Montage Transition to play; a missing montage is logged once and reported as not started.
	 * @param MontageAction Cover action the montage represents while it plays.
	 * @param bStartOnFirstFrame Skips the blend-in for clips that carry an in-place root offset on frame one.
	 * @return True when the montage is playing.
	 */
	bool PlayCoverMontage(
		UAnimMontage* Montage,
		ESquadCoverAnimAction MontageAction,
		bool bStartOnFirstFrame = false);
	bool PlayExitCoverMontage();
	void OnCoverMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void CompleteCoverTransition(ESquadCoverAnimAction CompletedAction, ESquadCoverAnimAction NextAction);
	void SetCoverAnimAction(ESquadCoverAnimAction NewAction);

	// Single place where the idle pose changes, so the graph-facing cover branch can never disagree with it.
	void SetIdleAnimationPose(ESquadIdleAnimationPose NewIdlePose);

	// Called whenever the idle pose or the aiming flag changes; both decide which cover branch the graph plays.
	void RefreshCoverGraphPose();
	void EnterExposedCoverPose();
	void EnterProtectedCoverPose();
	UAnimMontage* GetCoverEnterMontage(ESquadIdleAnimationPose CoverPose) const;
	const FSquadUnitStandingCoverAnimationSet* FindStandingCoverAnimationSet(ESquadIdleAnimationPose CoverPose) const;
	void ClearCoverAnimationRuntime();
	void LogMissingCoverAnimationAssets(ESquadIdleAnimationPose CoverPose) const;

	/** Reports what each cover montage of this pose moves the capsule by, as the starting point for tuning offsets. */
	void LogCoverMontageRootMotion(ESquadIdleAnimationPose CoverPose) const;
	void LogCoverMontageRootMotion(const UAnimMontage* Montage, const TCHAR* MontageRole) const;

	const FSquadUnitStandingCoverAnimationSet* GetStandingCoverAnimationSet() const;
	ESquadIdleAnimationPose GetProtectedStandingCoverPose() const;
	ESquadIdleAnimationPose GetStandingPeekPose() const;

	// ----- Team Weapon Crew Animations -----

	/** Plays the loop montage and binds the loop-ended delegate so the code can re-play it on its natural end. */
	void PlayTeamWeaponCrewLoopMontage(UAnimMontage* LoopMontage, const float PlayRate);
	void OnTeamWeaponCrewLoopMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnTeamWeaponCrewReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

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
	FOnMontageEnded M_DeathMontageEndedDelegate;
	FOnSquadUnitDeathMontageFinished M_DeathMontageCompletionDelegate;

	UPROPERTY(Transient)
	FSquadUnitCoverAnimRuntime M_CoverAnimRuntime;

	FOnMontageEnded M_CoverMontageEndedDelegate;
};
