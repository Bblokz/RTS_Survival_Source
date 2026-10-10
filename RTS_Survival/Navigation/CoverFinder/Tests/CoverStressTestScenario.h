#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"

class ASquadUnit;
class USquadUnitAnimInstance;
class UCharacterMovementComponent;
class UWeaponState;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ECoverStressTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	FindingSoldierInCover,
	WaitingForRelocation,
	WaitingForSprayFromNewCover,
	WaitingForCalm
};

/** What the tested soldier's weapon and movement were before his stress, to compare against during and after. */
struct FCoverStressTestBaseline
{
	int32 MagCapacity = 0;
	int32 Accuracy = 0;
	float BaseCooldown = 0.0f;
	float ReloadSpeed = 0.0f;
	float MaxWalkSpeed = 0.0f;
};

/**
 * @brief Stresses one player soldier that holds crouch cover and follows him through it: the weapon change and
 * the spray animation on the frame the stress starts, the run to cover further from his enemy at half time,
 * spraying again from there, and everything back to normal at the end.
 * Started with -CoverFinderValidateStress on a map where player squads take crouch cover by themselves.
 */
struct FCoverStressTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

	/**
	 * @brief Gives a soldier the crouch spray assets when his animation Blueprint has none assigned yet, so the
	 * playing of them can still be tested.
	 * @return How many crouch spray animations the animation Blueprint had assigned itself.
	 */
	static int32 GiveSoldierCrouchSprayAnimationsWhenMissing(USquadUnitAnimInstance& UnitAnimation);

private:
	TWeakObjectPtr<ASquadUnit> M_Soldier;
	TWeakObjectPtr<ASquadUnit> M_Enemy;
	FRTSCoverPoint M_FirstCoverPoint;
	FCoverStressTestBaseline M_Baseline;
	FVector M_EnemyStartLocation = FVector::ZeroVector;
	FVector M_EnemyPost = FVector::ZeroVector;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;
	ECoverStressTestPhase M_Phase = ECoverStressTestPhase::Inactive;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ECoverStressTestPhase NewPhase, float TimeoutSeconds);
	bool GetHasPhaseTimedOut(const UWorld& World) const;
	void Finish(const UWorld& World);

	ASquadUnit* FindPlayerSoldierInCrouchCover(UWorld& World) const;

	// Enemy squads on the map call their soldiers back; the one the soldier is stressed by is kept at its post.
	bool PlaceEnemyAtPost(UWorld& World);
	void KeepEnemyAtPost(UWorld& World);

	/**
	 * @brief Hurts the soldier, lightly first and then quickly, and checks what the start of his stress changed.
	 * @return False when the soldier could not be stressed, which ends the test.
	 */
	bool StressSoldier(UWorld& World, ASquadUnit& Soldier);
	void RecordBaseline(const UWeaponState& SoldierWeapon, const UCharacterMovementComponent& SoldierMovement);
	void CheckStressedWeapon(const ASquadUnit& Soldier);
	void TickFindingSoldier(UWorld& World);
	void TickWaitingForRelocation(UWorld& World);
	void TickWaitingForSprayFromNewCover(UWorld& World);
	void TickWaitingForCalm(UWorld& World);
};
