#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Delegates/Delegate.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "Templates/Function.h"

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
	 * @brief Schedules an update to the mapping of IDs to locations.
	 * @param NewActorData The new mapping of Actor IDs to their locations.
	 */
	void ScheduleUpdateActorData(const TMap<uint32, TPair<ETargetPreference, FVector>>& NewEnemyActorData,
	                             const TMap<uint32, TPair<ETargetPreference,
	                                                      FVector>>& NewPlayerActorData);

	/**
	 * @brief Schedules an update to the detailed unit data used for strategic AI processing.
	 * @param NewDetailedUnitStates Latest detailed unit snapshot for both players.
	 */
	void ScheduleUpdateDetailedActorData(const TArray<FAsyncDetailedUnitState>& NewDetailedUnitStates);

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
	 * @param RequestID Unique ID for the request.
	 * @param SearchLocation The location from which to search.
	 * @param SearchRadius The radius within which to search.
	 * @param NumTargets The number of closest targets to find.
	 * @param OwningPlayer The player that owns this request; determines which group of actors
	 * are considered.
	 * @param TargetPreference The preference for the target type.
	 * @param Callback The callback function to invoke with the result.
	 */
	void AddTargetRequest(
		uint32 RequestID,
		const FVector& SearchLocation,
		float SearchRadius,
		int32 NumTargets,
		int32 OwningPlayer,
		ETargetPreference TargetPreference,
		TFunction<void(const TArray<uint32>&)> Callback);

private:
	/** Structure representing a target request */
	struct FTargetRequest
	{
		uint32 RequestID;
		FVector SearchLocation;
		float SearchRadius;
		int32 NumTargets;
		int32 OwningPlayer;
		ETargetPreference TargetPreference;
		TFunction<void(const TArray<uint32>&)> Callback;
	};

	/** Structure representing a strategic AI request batch */
	struct FStrategicAIRequest
	{
		FStrategicAIRequestBatch RequestBatch;
		TFunction<void(const FStrategicAIResultBatch&)> Callback;
	};

	/** Mapping of Actor IDs to their Target-type and location */
	TMap<uint32, TPair<ETargetPreference, FVector>> M_PlayerActorData;
	TMap<uint32, TPair<ETargetPreference, FVector>> M_EnemyActorData;

	/** Detailed unit state snapshot used by strategic AI requests. */
	TArray<FAsyncDetailedUnitState> M_DetailedUnitStates;

	/** Queue of target requests */
	TQueue<FTargetRequest> M_RequestQueue;

	/** Queue of pending actor data updates */
	TQueue<TMap<uint32, TPair<ETargetPreference, FVector>>> M_PendingPlayerActorDataUpdates;

	/** Queue of pending actor data updates */
	TQueue<TMap<uint32, TPair<ETargetPreference, FVector>>> M_PendingEnemyActorDataUpdates;

	/** Queue of pending detailed unit data updates */
	TQueue<TArray<FAsyncDetailedUnitState>> M_PendingDetailedUnitStateUpdates;

	/** Queue of strategic AI requests */
	TQueue<FStrategicAIRequest> M_StrategicAIRequestQueue;


	/** Flag to signal the thread to stop */
	FThreadSafeBool bM_StopThread;

	/**
	 * @brief Processes the queued target requests.
	 */
	void ProcessRequests();

	/**
	 * @brief Processes queued strategic AI requests.
	 */
	void ProcessStrategicAIRequests();

	FRunnableThread* M_Thread;
};
