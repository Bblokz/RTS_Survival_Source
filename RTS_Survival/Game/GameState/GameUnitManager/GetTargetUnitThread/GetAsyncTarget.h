#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Delegates/Delegate.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "Templates/Function.h"

namespace AsyncTargetConstants
{
	// Weapons ask for 3 to 8 targets; searches and answers of that size need no heap allocation.
	inline constexpr int32 InlineTargetsPerRequest = 8;
}

/** One targetable unit as the async target thread sees it. */
struct FAsyncTargetUnit
{
	FVector Location = FVector::ZeroVector;

	// Only resolved on the game thread; the thread hands it back untouched and never tests it.
	TWeakObjectPtr<AActor> Actor;

	ETargetPreference TargetType = ETargetPreference::None;

	// Bit of the player that owns the unit (FRTSPlayers::GetPlayerBit).
	uint8 OwnerBit = 0;

	// Bits of the players whose units see this unit.
	uint8 VisibleToMask = 0;
};

/**
 * @brief Every unit that at least one player can target right now; built on the game thread, read by the thread.
 * Units are grouped by owning player so a request only walks the units of the players it fights.
 */
struct FAsyncTargetSnapshot
{
	TArray<FAsyncTargetUnit> Units;

	// Units of player P are Units[FirstUnitOfPlayer[P]] up to, but not including, Units[FirstUnitOfPlayer[P + 1]].
	int32 FirstUnitOfPlayer[RTSPlayers::PlayerArraySize + 1] = {};

	// Per viewing player: bits of the owners with at least one unit that this player sees. A request that has
	// nothing to find (a weapon far from any enemy) is answered from this without walking any unit.
	uint8 VisibleOwnersOfViewer[RTSPlayers::PlayerArraySize] = {};
};

/** Targets found for one request, closest first. Weak: a unit can be gone before the answer reaches the game thread. */
using FAsyncTargetResult = TArray<TWeakObjectPtr<AActor>, TInlineAllocator<AsyncTargetConstants::InlineTargetsPerRequest>>;

/**
 * @brief One search for the closest targets around a location.
 * Who the requester is and who it fights is copied in on the game thread: the thread never reads FRTSPlayers
 * relations, which can change while a request waits.
 */
struct FAsyncTargetRequest
{
	FVector SearchLocation = FVector::ZeroVector;

	float SearchRadius = 0.f;

	// Targets closer than this radius are excluded.
	float MinimumSearchRadius = 0.f;

	int32 NumTargets = 0;

	ETargetPreference TargetPreference = ETargetPreference::None;

	// Normalized player that asks (FRTSPlayers::NormalizePlayer).
	uint8 RequestingPlayer = 0;

	uint8 RequesterBit = 0;

	// Bits of the players the requester fights; only their units are searched.
	uint8 HostileMask = 0;

	TFunction<void(const FAsyncTargetResult&)> Callback;
};

/**
 * @brief A thread as runnable class that processes actor target requests asynchronously.
 */
class FGetAsyncTarget : public FRunnable
{
public:
	FGetAsyncTarget();

	virtual ~FGetAsyncTarget();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	/**
	 * @brief Schedules an update to the units the thread searches.
	 * @param NewSnapshot The new snapshot of all targetable units; replaces the previous one as a whole.
	 */
	void ScheduleUpdateActorData(FAsyncTargetSnapshot&& NewSnapshot);

	/**
	 * @brief Schedules an update to the detailed unit data used for strategic AI processing.
	 * @param NewDetailedUnitStates Latest detailed unit snapshot for all players.
	 */
	void ScheduleUpdateDetailedActorData(TArray<FAsyncDetailedUnitState>&& NewDetailedUnitStates);

	/**
	 * @brief Adds a strategic AI request batch to the async queue.
	 * @param RequestBatch Aggregated strategic AI requests to process.
	 * @param Callback Callback invoked on the game thread with batch results.
	 */
	void AddStrategicAIRequest(
		const FStrategicAIRequestBatch& RequestBatch,
		TFunction<void(const FStrategicAIResultBatch&)> Callback);


	/**
	 * @brief Adds a target request to the queue.
	 * @param Request Where to search, for whom and what to call with the result.
	 * @note Every request is answered with exactly one call of its callback on the game thread, also when no
	 * target was found.
	 */
	void AddTargetRequest(FAsyncTargetRequest&& Request);

private:
	/** Structure representing a strategic AI request batch */
	struct FStrategicAIRequest
	{
		FStrategicAIRequestBatch RequestBatch;
		TFunction<void(const FStrategicAIResultBatch&)> Callback;
	};

	/** Keeps the latest accepted base footprint available for construction requests without rescanning units. */
	struct FEnemyBaseClusterAsyncCache
	{
		void UpdateFromResult(const FResultEnemyBaseClusters& Result);

		TArray<FEnemyBasePointCoreBuildings> CoreBuildingsByBase;
		TArray<FEnemyBasePointSatelliteBuildings> SatelliteBuildingsByBase;
	};

	/** The targets of one request, waiting to be handed to its callback on the game thread. */
	struct FTargetAnswer
	{
		TFunction<void(const FAsyncTargetResult&)> Callback;
		FAsyncTargetResult Targets;
	};

	/** Latest snapshot of the targetable units of all players. */
	FAsyncTargetSnapshot M_Snapshot;

	/** Detailed unit state snapshot used by strategic AI requests. */
	TArray<FAsyncDetailedUnitState> M_DetailedUnitStates;

	// Latest async base-cluster result per requesting player, used to keep its construction candidates outside
	// its bases. One per player: each enemy controller asks about its own bases.
	FEnemyBaseClusterAsyncCache M_EnemyBaseClusterAsyncCacheOfPlayer[RTSPlayers::PlayerArraySize];

	/** Queue of target requests */
	TQueue<FAsyncTargetRequest> M_RequestQueue;

	/** Queue of pending actor data updates */
	TQueue<FAsyncTargetSnapshot> M_PendingSnapshots;

	/** Queue of pending detailed unit data updates */
	TQueue<TArray<FAsyncDetailedUnitState>> M_PendingDetailedUnitStateUpdates;

	/** Queue of strategic AI requests */
	TQueue<FStrategicAIRequest> M_StrategicAIRequestQueue;


	/** Flag to signal the thread to stop */
	FThreadSafeBool bM_StopThread;

	/**
	 * @brief Processes the queued target requests.
	 */
	void ProcessTargetRequests();

	/**
	 * @brief Processes queued strategic AI requests.
	 */
	void ProcessStrategicAIRequests();

	FRunnableThread* M_Thread;
};
