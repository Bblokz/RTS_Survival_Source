#pragma once

#include "CoreMinimal.h"

class ASquadController;
class ASquadUnit;
class UMedicComponent;
class URTSCoverFinderWorldSubsystem;
class UWorld;
enum class ECommandQueueError : uint8;

enum class EMedicHealTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	FindingCast,
	AutoWaitingForPatient,
	AutoWaitingForHealing,
	AutoWaitingForHealthGain,
	FollowWaitingForReapproach,
	LeashWaitingForRelease,
	SpreadWaitingForTwoPatients,
	SpreadWaitingForSharedPatient,
	SpreadWaitingForIdle,
	OrderWaitingForSecondPatient,
	OrderWaitingForOrderDone,
	CancelWaitingWhileSuppressed,
	ResumeWaitingForAutoHeal
};

/**
 * @brief Turns soldiers of two player squads into medics and follows them through everything a medic does:
 * picking the most injured soldier, standing behind a patient that is in cover, the heal animation and the
 * health it gives, following a patient that walks off and letting go of it past the follow distance, two
 * medics spreading over two patients, and the squad's heal order with its cancel button.
 * Started with -MedicValidateHeal on a map where player squads take cover by themselves. Not in a shipping build.
 */
struct FMedicHealTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	TWeakObjectPtr<ASquadUnit> M_Medic;
	TWeakObjectPtr<ASquadUnit> M_SecondMedic;
	TWeakObjectPtr<ASquadController> M_MedicSquad;
	TWeakObjectPtr<ASquadController> M_SecondMedicSquad;

	// Two soldiers of one squad; the first one held cover when the test picked it.
	TWeakObjectPtr<ASquadUnit> M_Patient;
	TWeakObjectPtr<ASquadUnit> M_SecondPatient;
	TWeakObjectPtr<ASquadController> M_PatientSquad;

	FVector M_PatientLocationBeforeWalk = FVector::ZeroVector;
	float M_PatientHealthAtHealStart = 0.0f;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	float M_PhaseStartWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;
	int32 M_FrameCounter = 0;
	bool bM_MedicFollowedBeforeRelease = false;
	bool bM_HasHurtSpreadPatients = false;
	EMedicHealTestPhase M_Phase = EMedicHealTestPhase::Inactive;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, EMedicHealTestPhase NewPhase);
	bool GetHasPhaseTimedOut(const UWorld& World) const;
	void Finish(const UWorld& World);

	/** @return False, after failing a check and ending the test, when one of the test's actors is gone. */
	bool GetIsCastAlive(const UWorld& World);

	// Fails the check and ends the test when the phase ran out of time. Returns true in that case.
	bool FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent);

	bool FindCast(UWorld& World);
	ASquadUnit* FindPlayerSoldierInCoverWithSquadMate(UWorld& World, ASquadUnit*& OutSquadMate) const;
	ASquadController* FindNearestMedicSquad(
		UWorld& World,
		const FVector& Location,
		const TArray<const ASquadController*>& SquadsToSkip) const;
	UMedicComponent* MakeMedic(ASquadUnit& Soldier) const;

	// Nobody sees the soldiers of an unattended run, and unseen soldiers move in steps so coarse that they step
	// over their destination again and again. The test's squads are moved every frame, as on screen.
	void KeepCastMovingSmoothly() const;

	// Soldiers the test does not use are kept at full health, so the medics only ever see the test's patients.
	void KeepBystandersHealthy(UWorld& World) const;

	void CheckStandLocation(const UMedicComponent& MedicComponent, const ASquadUnit& Patient);
	void CheckMedicIsHealing(const ASquadUnit& Medic, const ASquadUnit& Patient);

	void TickFindingCast(UWorld& World);
	void TickAutoWaitingForPatient(UWorld& World);
	void TickAutoWaitingForHealing(UWorld& World);
	void TickAutoWaitingForHealthGain(UWorld& World);
	void TickFollowWaitingForReapproach(UWorld& World);
	void TickLeashWaitingForRelease(UWorld& World);
	void TickSpreadWaitingForTwoPatients(UWorld& World);
	void TickSpreadWaitingForSharedPatient(UWorld& World);
	void TickSpreadWaitingForIdle(UWorld& World);
	void TickOrderWaitingForSecondPatient(UWorld& World);
	void TickOrderWaitingForOrderDone(UWorld& World);
	void TickCancelWaitingWhileSuppressed(UWorld& World);
	void TickResumeWaitingForAutoHeal(UWorld& World);

	void StartFollowTest(UWorld& World);
	void StartLeashTest(UWorld& World);
	void StartSpreadTest(UWorld& World);
	void StartOrderTest(UWorld& World);
	void StartCancelTest(UWorld& World);
	void HurtSpreadPatients(UWorld& World);

	// A short walk for the medic's squad; a move order onto the spot it stands on would never complete.
	ECommandQueueError OrderMedicSquadToStepAside(UWorld& World) const;
};
