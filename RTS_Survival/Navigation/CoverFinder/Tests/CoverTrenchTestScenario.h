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
	WaitingForCrouchAgain
};

/**
 * @brief Sends a player squad into a trench and checks the stand-up cover cycle: crouched while nothing is
 * in sight, standing up in place when an enemy appears in front, crouched again when it is gone.
 * Started with -CoverFinderValidateTrenchCover on a map that has trenches of the configured class.
 */
struct FCoverTrenchTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	TWeakObjectPtr<ASquadUnit> M_TrenchSoldier;
	TWeakObjectPtr<ASquadUnit> M_Enemy;
	FRTSCoverPoint M_TrenchPoint;
	FVector M_EnemyStartLocation = FVector::ZeroVector;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;
	ECoverTrenchTestPhase M_Phase = ECoverTrenchTestPhase::Inactive;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ECoverTrenchTestPhase NewPhase, float TimeoutSeconds);
	void Finish(const UWorld& World);

	/** @return False when the map has no trench cover, no player squad that can be planned, or no enemy. */
	bool SendSquadIntoTrench(UWorld& World, URTSCoverFinderWorldSubsystem& CoverSubsystem);

	// Finds the first soldier of any player squad that settled into trench cover.
	ASquadUnit* FindSoldierInTrenchCover(UWorld& World) const;
	void TickWalkingToTrench(UWorld& World);
	void TickWaitingForStandUp(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem);
	void TickWaitingForCrouchAgain(UWorld& World);
	bool GetHasPhaseTimedOut(const UWorld& World) const;
};
