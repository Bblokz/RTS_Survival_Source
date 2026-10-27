#pragma once

#include "CoreMinimal.h"

class ARTSFieldTrench;
struct FCoverTrenchTestScenario;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ECoverRuntimeTrenchTestPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	WaitingForBuiltTrenchCover,
	WaitingForRemovedPiece,
	WaitingForDugTrench,
	SoldierInRuntimeTrench
};

/**
 * @brief Builds field trenches while the game runs and checks that their cargo sockets become trench cover:
 * a trench of finished pieces (End, straight, End) put down at once, a piece taken away and put back, and a
 * trench planned along a path and dug piece by piece. A player squad is then sent into the runtime trench and
 * goes through the stand-up cycle of -CoverFinderValidateTrenchCover on it.
 * Started with -CoverFinderValidateRuntimeTrench on a map with a player squad and an enemy, such as TestFieldTrench.
 */
struct FCoverRuntimeTrenchTestScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem, FCoverTrenchTestScenario& TrenchScenario);

private:
	TWeakObjectPtr<ARTSFieldTrench> M_BuiltTrench;
	TWeakObjectPtr<ARTSFieldTrench> M_DugTrench;
	ECoverRuntimeTrenchTestPhase M_Phase = ECoverRuntimeTrenchTestPhase::Inactive;
	float M_PhaseDeadlineWorldSeconds = 0.0f;
	int32 M_FailedCheckCount = 0;
	int32 M_RemovedPieceSocketCount = 0;

	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void EnterPhase(const UWorld& World, ECoverRuntimeTrenchTestPhase NewPhase, float TimeoutSeconds);
	bool GetHasPhaseTimedOut(const UWorld& World) const;
	void Finish(const UWorld& World, int32 TrenchScenarioFailedChecks);

	/** @return False when the map has no player squad next to which a trench can be put. */
	bool SpawnTrenches(UWorld& World);

	/**
	 * @brief Compares the published trench points of the trench with the cargo sockets of its built pieces.
	 * @param bReport Whether to log the comparison as checks; otherwise it is only polled.
	 * @return True when every socket is published once, snapped and with its walk-to point on the ground above it.
	 */
	bool CheckPublishedCover(
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const ARTSFieldTrench& Trench,
		const TCHAR* Description,
		bool bReport);
};
