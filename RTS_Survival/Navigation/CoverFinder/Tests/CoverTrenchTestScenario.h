#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"

class ASquadController;
class ASquadUnit;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ECoverTrenchTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	WalkingToTrench,
	WaitingForStandUp,
	WaitingForReloadDuck,
	WaitingForStandUpAfterReload,
	WaitingForCrouchAgain
};

/**
 * @brief Sends a player squad into a trench and checks the stand-up cover cycle: crouched while nothing is
 * in sight, standing up in place when an enemy appears in front, ducking while its weapon reloads and standing
 * up again afterwards, crouched again when the enemy is gone.
 * Started with -CoverFinderValidateTrenchCover on a map that has trenches of the configured class.
 */
struct FCoverTrenchTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	// The squad the test ordered into the trench; other squads on the map take cover by themselves too.
	TWeakObjectPtr<ASquadController> M_OrderedSquad;
	TWeakObjectPtr<ASquadUnit> M_TrenchSoldier;
	TWeakObjectPtr<ASquadUnit> M_Enemy;
	FRTSCoverPoint M_TrenchPoint;
	FVector M_EnemyStartLocation = FVector::ZeroVector;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;

	// The first enemy is usually shot dead while the soldier reloads; one more is brought in to stand up for.
	bool bM_HasBroughtEnemyAfterReload = false;
	ECoverTrenchTestPhase M_Phase = ECoverTrenchTestPhase::Inactive;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ECoverTrenchTestPhase NewPhase, float TimeoutSeconds);
	void Finish(const UWorld& World);

	/** @return False when the map has no trench cover, no player squad that can be planned, or no enemy. */
	bool SendSquadIntoTrench(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem);

	/**
	 * @brief Puts a living enemy soldier straight in front of the tested firing step.
	 * @return False when no enemy soldier is alive any more.
	 */
	bool PlaceLivingEnemyInFrontOfTrench(UWorld& World);

	// Finds the first soldier of any player squad that settled into trench cover.
	ASquadUnit* FindSoldierInTrenchCover(UWorld& World) const;
	void TickWalkingToTrench(UWorld& World);
	void TickWaitingForStandUp(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void TickWaitingForReloadDuck(UWorld& World);
	void TickWaitingForStandUpAfterReload(UWorld& World);
	void TickWaitingForCrouchAgain(UWorld& World);

	// With socket snap on for the trench: the soldier holds it with his feet on the socket.
	void CheckFeetOnSocket();

	// With socket snap on for the trench: a soldier that leaves it is back on the navmesh point and can walk.
	void CheckLeavingSocket();

	// Enemy squads on the map call their soldiers back; the one the test uses is kept at its post.
	void KeepEnemyInFrontOfTrench(UWorld& World);
	bool GetHasPhaseTimedOut(const UWorld& World) const;
};
