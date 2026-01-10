#include "GetAsyncTarget.h"
#include "Async/Async.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIHelpers.h"

FGetAsyncTarget::FGetAsyncTarget()
	: bM_StopThread(false)
{
	M_Thread = FRunnableThread::Create(this, TEXT("AsyncGetTargetThread"));
}

FGetAsyncTarget::~FGetAsyncTarget()
{
	if (M_Thread)
	{
		M_Thread->Kill();
		delete M_Thread;
		M_Thread = nullptr;
	}
}

bool FGetAsyncTarget::Init()
{
	return true;
}

uint32 FGetAsyncTarget::Run()
{
	while (not bM_StopThread)
	{
		// Process any pending actor data updates
		TMap<uint32, TPair<ETargetPreference, FVector>> NewActorData;
		while (M_PendingPlayerActorDataUpdates.Dequeue(NewActorData))
		{
			M_PlayerActorData = MoveTemp(NewActorData);
		}
		while (M_PendingEnemyActorDataUpdates.Dequeue(NewActorData))
		{
			M_EnemyActorData = MoveTemp(NewActorData);
		}

		TArray<FAsyncDetailedUnitState> NewDetailedUnitStates;
		while (M_PendingDetailedUnitStateUpdates.Dequeue(NewDetailedUnitStates))
		{
			M_DetailedUnitStates = MoveTemp(NewDetailedUnitStates);
		}


		// Process target requests
		ProcessRequests();
		ProcessStrategicAIRequests();

		// Sleep to prevent tight loop
		FPlatformProcess::Sleep(DeveloperSettings::Async::AsyncGetTargetThreadUpdateInterval);
	}

	return 0;
}

void FGetAsyncTarget::Stop()
{
	bM_StopThread = true;
}

void FGetAsyncTarget::ScheduleUpdateActorData(const TMap<uint32, TPair<ETargetPreference, FVector>>& NewEnemyActorData,
                                              const TMap<uint32, TPair<ETargetPreference,
                                                                       FVector>>& NewPlayerActorData)
{
	// Enqueue the new actor data for processing on the thread
	M_PendingPlayerActorDataUpdates.Enqueue(NewPlayerActorData);
	M_PendingEnemyActorDataUpdates.Enqueue(NewEnemyActorData);
}

void FGetAsyncTarget::ScheduleUpdateDetailedActorData(const TArray<FAsyncDetailedUnitState>& NewDetailedUnitStates)
{
	M_PendingDetailedUnitStateUpdates.Enqueue(NewDetailedUnitStates);
}

void FGetAsyncTarget::AddStrategicAIRequest(
	const FStrategicAIRequestBatch& RequestBatch,
	TFunction<void(const FStrategicAIResultBatch&)> Callback)
{
	FStrategicAIRequest NewRequest;
	NewRequest.RequestBatch = RequestBatch;
	NewRequest.Callback = MoveTemp(Callback);
	M_StrategicAIRequestQueue.Enqueue(MoveTemp(NewRequest));
}

void FGetAsyncTarget::AddTargetRequest(
	uint32 RequestID,
	const FVector& SearchLocation,
	float SearchRadius,
	int32 NumTargets,
	int32 OwningPlayer,
	ETargetPreference TargetPreference,
	TFunction<void(const TArray<uint32>&)> Callback)
{
	// Create a new target request
	FTargetRequest NewRequest;
	NewRequest.RequestID = RequestID;
	NewRequest.SearchLocation = SearchLocation;
	NewRequest.SearchRadius = SearchRadius;
	NewRequest.NumTargets = NumTargets;
	NewRequest.OwningPlayer = OwningPlayer;
	NewRequest.TargetPreference = TargetPreference;
	NewRequest.Callback = MoveTemp(Callback);

	// Enqueue the request
	M_RequestQueue.Enqueue(MoveTemp(NewRequest));
}

void FGetAsyncTarget::ProcessRequests()
{
    FTargetRequest Request;
    while (M_RequestQueue.Dequeue(Request))
    {
        // Determine which actor data map to search based on the owning player.
        // If OwningPlayer == 1 (player), search M_EnemyActorData.
        // If OwningPlayer != 1 (enemy), search M_PlayerActorData.
        TMap<uint32, TPair<ETargetPreference, FVector>>* ActorDataMap = nullptr;

        if (Request.OwningPlayer == 1)
        {
            // The request is from the player; search enemy data.
            ActorDataMap = &M_EnemyActorData;
        }
        else
        {
            // The request is from an enemy; search player data.
            ActorDataMap = &M_PlayerActorData;
        }

        // Arrays to store actors within range, categorized by preference.
        TArray<TPair<float, uint32>> PreferredTargets;
        TArray<TPair<float, uint32>> OtherTargets;

        // Iterate through the actor data map to find actors within range.
        for (const auto& Pair : *ActorDataMap)
        {
            uint32 ActorID = Pair.Key;
            const TPair<ETargetPreference, FVector>& ActorInfo = Pair.Value;
            const ETargetPreference ActorType = ActorInfo.Key;
            const FVector& ActorLocation = ActorInfo.Value;

            float Distance = FVector::Dist(Request.SearchLocation, ActorLocation);
            if (Distance <= Request.SearchRadius)
            {
                // Categorize actors based on the requested target preference.
                if (ActorType == Request.TargetPreference)
                {
                    PreferredTargets.Emplace(Distance, ActorID);
                }
                else
                {
                    OtherTargets.Emplace(Distance, ActorID);
                }
            }
        }

        // Sort both arrays based on distance to prioritize closer targets.
        PreferredTargets.Sort([](const TPair<float, uint32>& A, const TPair<float, uint32>& B)
        {
            return A.Key < B.Key;
        });

        OtherTargets.Sort([](const TPair<float, uint32>& A, const TPair<float, uint32>& B)
        {
            return A.Key < B.Key;
        });

        // Collect the top N targets, filling from preferred targets first.
        TArray<uint32> ClosestActorIDs;
        int32 RemainingTargets = Request.NumTargets;

        // Add actors from PreferredTargets.
        for (const auto& Target : PreferredTargets)
        {
            ClosestActorIDs.Add(Target.Value);
            RemainingTargets--;
            if (RemainingTargets <= 0)
            {
                break;
            }
        }

        // If more targets are needed, add actors from OtherTargets.
        if (RemainingTargets > 0)
        {
            for (const auto& Target : OtherTargets)
            {
                ClosestActorIDs.Add(Target.Value);
                RemainingTargets--;
                if (RemainingTargets <= 0)
                {
                    break;
                }
            }
        }

        // Capture the callback and IDs for use in the game thread.
        TFunction<void(const TArray<uint32>&)> Callback = MoveTemp(Request.Callback);

        // Invoke the callback on the game thread with the collected target IDs.
        AsyncTask(ENamedThreads::GameThread,
                  [Callback = MoveTemp(Callback), ClosestActorIDs = MoveTemp(ClosestActorIDs)]()
                  {
                      Callback(ClosestActorIDs);
                  });
    }
}

void FGetAsyncTarget::ProcessStrategicAIRequests()
{
	FStrategicAIRequest Request;
	while (M_StrategicAIRequestQueue.Dequeue(Request))
	{
		FStrategicAIResultBatch Results;

		Results.FindClosestFlankableEnemyHeavyResults.Reserve(
			Request.RequestBatch.FindClosestFlankableEnemyHeavyRequests.Num());
		for (const FFindClosestFlankableEnemyHeavy& FindRequest : Request.RequestBatch.FindClosestFlankableEnemyHeavyRequests)
		{
			Results.FindClosestFlankableEnemyHeavyResults.Add(
				FStrategicAIHelpers::BuildClosestFlankableEnemyHeavyResult(FindRequest, M_DetailedUnitStates));
		}

		Results.PlayerUnitCountsResults.Reserve(
			Request.RequestBatch.GetPlayerUnitCountsAndBaseRequests.Num());
		for (const FGetPlayerUnitCountsAndBase& CountRequest : Request.RequestBatch.GetPlayerUnitCountsAndBaseRequests)
		{
			Results.PlayerUnitCountsResults.Add(
				FStrategicAIHelpers::BuildPlayerUnitCountsResult(CountRequest, M_DetailedUnitStates));
		}

		Results.AlliedTanksToRetreatResults.Reserve(
			Request.RequestBatch.FindAlliedTanksToRetreatRequests.Num());
		for (const FFindAlliedTanksToRetreat& RetreatRequest : Request.RequestBatch.FindAlliedTanksToRetreatRequests)
		{
			Results.AlliedTanksToRetreatResults.Add(
				FStrategicAIHelpers::BuildAlliedTanksToRetreatResult(RetreatRequest, M_DetailedUnitStates));
		}

		TFunction<void(const FStrategicAIResultBatch&)> Callback = MoveTemp(Request.Callback);
		AsyncTask(ENamedThreads::GameThread,
			[Callback = MoveTemp(Callback), Results = MoveTemp(Results)]() mutable
			{
				Callback(Results);
			});
	}
}
