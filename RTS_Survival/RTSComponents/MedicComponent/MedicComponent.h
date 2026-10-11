// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "MedicComponent.generated.h"

class ASquadController;
class ASquadUnit;
class UAnimatedIconWidgetPoolManager;
class UMedicPatientSubsystem;
struct FOverlapResult;
enum class EAbilityID : uint8;

/** What a medic is doing for its patient; Idle when it has none. */
UENUM(BlueprintType)
enum class EMedicHealState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	MovingToPatient UMETA(DisplayName = "Moving To Patient"),
	Healing UMETA(DisplayName = "Healing")
};

/**
 * @brief How a medic finds, follows and heals its patients. Blueprint read write, so the constructor of a
 * derived medic unit can adjust the defaults of its medic component.
 */
USTRUCT(BlueprintType)
struct FMedicHealSettings
{
	GENERATED_BODY()

	// An idle medic without a command looks for injured allied squad units by itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic")
	bool bAutoHealWhenIdle = true;

	// Radius around the medic of the async overlap that finds allied squad units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "100.0", Units = "cm"))
	float SearchRadius = 1200.0f;

	// How often an idle medic searches for a patient.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "0.2", Units = "s"))
	float SearchPeriod = 1.0f;

	// Health given to the patient with every heal tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "0.0"))
	float HealAmountPerTick = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "0.1", Units = "s"))
	float HealInterval = 1.0f;

	// A medic that heals by itself follows a moving patient no further than this from where its squad stood,
	// on average, when it started healing; past it the medic lets go and looks for another patient.
	// Does not limit a heal order of the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxFollowDistance = 1500.0f;

	// The medic stops this far from its patient: on the side it came from, or, for a patient in cover, on the
	// side away from what the cover faces, to stay out of the line of fire.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic", meta = (ClampMin = "0.0", Units = "cm"))
	float PatientStandOffDistance = 80.0f;

	// Where the healing icon appears, relative to the patient.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic")
	FVector HealIconOffset = FVector(0.0f, 0.0f, 120.0f);
};

/**
 * @brief Add to a squad unit to make it a medic; ASquadUnit_Medic has one by default.
 * An idle medic finds injured allied squad units with an async overlap, walks up to the one that needs it
 * most, and heals it. The squad's IdHeal command points it at a squad of the player's choice instead.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UMedicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMedicComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Medic")
	FMedicHealSettings HealSettings;

	// Called by the owning squad unit once its components exist.
	void SetSquadOwner(ASquadUnit* OwnerUnit);

	/**
	 * @brief Starts this medic's part of its squad's heal command: it heals the injured soldiers of the
	 * patient's squad, the given soldier first, until that squad needs no more healing.
	 * @param OrderedPatient The allied squad unit the player pointed at.
	 * @return False when nobody of that squad can be healed by this medic right now.
	 */
	bool StartOrderedHeal(ASquadUnit* OrderedPatient);

	// Ends whatever healing is going on and gives the unit back to its squad, without reporting to the squad.
	void StopHealing();

	// Called before the squad starts any command: that command has the unit from here on.
	void OnSquadCommandStarting(EAbilityID AbilityStarting);

	// The player cancelled healing: the medic stays put until its squad gets another command.
	void SuppressAutoHealUntilNextCommand();

	/**
	 * @brief Called by the owning unit when a walk it made for the medic component is over.
	 * @param bWasAborted The walk was cut off, which sending the medic after a moving patient does to the
	 * previous walk; the medic then waits for its next update instead of judging where it stands.
	 */
	void OnFinishedMovementForHeal(bool bWasAborted);

	void OnOwnerDied();

	EMedicHealState GetHealState() const { return M_HealState; }
	ASquadUnit* GetPatient() const { return M_Patient.Get(); }
	bool GetIsOrderedHeal() const { return bM_IsOrderedHeal; }
	bool GetIsAutoHealSuppressed() const { return bM_IsAutoHealSuppressed; }
	FVector GetStandLocation() const { return M_StandLocation; }

#if RTS_WITH_MAP_TESTS
	// Test-only: where the medic's follow distance is measured from, once it started healing by itself.
	FVector Debug_GetAutoHealAnchor() const { return M_AutoHealAnchor; }
#endif

	/**
	 * @brief Where this medic wants to stand to heal a patient.
	 * @param Patient The soldier to heal.
	 * @param MedicStartLocation Where the medic set off from; the side of an uncovered patient it stops on.
	 * @return Navigable location at the stand-off distance from the patient; behind a patient that is in cover.
	 */
	FVector GetStandLocationForPatient(const ASquadUnit& Patient, const FVector& MedicStartLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_OwnerSquadUnit;

	bool GetIsValidOwnerSquadUnit() const;

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_Patient;

	// The squad a heal order of the player was given for; its injured soldiers are healed one after the other.
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_OrderedPatientSquad;

	UPROPERTY()
	TWeakObjectPtr<UMedicPatientSubsystem> M_PatientSubsystem;

	bool GetIsValidPatientSubsystem() const;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedIconWidgetPoolManager> M_AnimatedIconWidgetPoolManager;

	bool GetIsValidAnimatedIconWidgetPoolManager() const;

	EMedicHealState M_HealState = EMedicHealState::Idle;

	bool bM_IsOrderedHeal = false;

	bool bM_IsAutoHealSuppressed = false;

	bool bM_IsSearchPending = false;

	// Set while the medic asks its unit to walk: that ends the unit's previous walk, which reports back at once.
	bool bM_IsStartingWalk = false;

	// The medic took its unit's weapon away for the heal animation and has to give it back.
	bool bM_HasDisabledWeapon = false;

	// Cleared when the icon pool turns out to be missing, so that is reported once and not on every heal tick.
	bool bM_ShowHealIcon = true;

	// Where the medic's squad stood when the medic started healing by itself; kept until the squad's next
	// command, so a chain of patients cannot lead the medic across the map.
	bool bM_HasAutoHealAnchor = false;
	FVector M_AutoHealAnchor = FVector::ZeroVector;

	// Where the medic was when it set off for its patient, where it wants to stand, and where the patient was
	// when that was decided: a patient that moves on from there is followed.
	FVector M_MedicStartLocation = FVector::ZeroVector;
	FVector M_StandLocation = FVector::ZeroVector;
	FVector M_PatientLocationAtApproach = FVector::ZeroVector;

	int32 M_FailedApproachCount = 0;

	float M_NextHealWorldSeconds = 0.0f;

	// A patient the medic gave up on is not picked again before this time.
	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_SkippedPatient;
	float M_SkippedPatientUntilWorldSeconds = 0.0f;

	FTimerHandle M_UpdateTimerHandle;

	void StartUpdateTimer(float UpdateInterval, float FirstDelay);
	void UpdateMedic();
	void UpdateIdle();
	void UpdatePatient();
	void UpdateMovingToPatient(const ASquadUnit& Patient);
	void UpdateHealing(ASquadUnit& Patient);

	bool GetMayAutoHeal() const;
	bool GetIsPatientBeyondFollowDistance(const ASquadUnit& Patient) const;

	/**
	 * @brief Decides whether a soldier can be healed by this medic at all.
	 * @param Candidate The soldier to judge; may be null or dead.
	 * @return True for a living, injured soldier of the medic's own player that is not inside a vehicle.
	 */
	bool GetIsValidPatient(ASquadUnit* Candidate) const;
	bool GetIsPatientSkipped(const ASquadUnit* Candidate) const;

	void StartPatientSearch();
	void OnPatientSearchCompleted(const TArray<FOverlapResult>& Overlaps);

	/**
	 * @brief Picks who to heal: a soldier no other medic tends to first, then the one with the least health,
	 * then the closest one.
	 * @param Candidates Soldiers to choose from; invalid and unhealable ones are skipped.
	 * @param bLimitToFollowDistance Leaves out soldiers further from the auto heal anchor than the medic follows.
	 * @return The patient, or null when none of the candidates can be healed.
	 */
	ASquadUnit* SelectBestPatient(const TArray<ASquadUnit*>& Candidates, bool bLimitToFollowDistance) const;

	ASquadUnit* SelectNextOrderedPatient() const;

	void StartHealingPatient(ASquadUnit& Patient, bool bIsOrderedHeal);
	void StartApproach();
	void OnApproachEnded();
	void BeginHealing();
	void ApplyHealTick(ASquadUnit& Patient);
	void ShowHealIcon(ASquadUnit& Patient);
	void FacePatient(const ASquadUnit& Patient) const;
	void UpdateHealAnimation(const ASquadUnit& Patient) const;
	void StopHealAnimation() const;
	void SetOwnerWeaponDisabled(bool bDisabled);

	// The patient is healed, gone or out of reach: moves on to the next one or ends the healing.
	void OnPatientDone();
	void GiveUpOnPatient();
	void FinishHealing();
	void ReleasePatient();
	void ReturnOwnerToIdle();

	bool GetIsAtStandLocation() const;
	bool GetHasPatientMovedSinceApproach(const ASquadUnit& Patient, float Tolerance) const;
};
