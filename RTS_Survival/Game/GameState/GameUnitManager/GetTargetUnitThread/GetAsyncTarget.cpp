#include "GetAsyncTarget.h"
#include "Async/Async.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIHelpers.h"

FGetAsyncTarget::FGetAsyncTarget()
	: bM_StopThread(false)
{
	M_Thread = FRunnableThread::Create(this, TEXT("AsyncGetTargetThread"));
}

void FGetAsyncTarget::FEnemyBaseClusterAsyncCache::UpdateFromResult(const FResultEnemyBaseClusters& Result)
{
	CoreBuildingsByBase = Result.BasePoints;
	SatelliteBuildingsByBase = Result.SatelliteBasePoints;
}

FGetAsyncTarget::~FGetAsyncTarget()
{
	if (M_Thread)
	{
		// Waits for the thread to leave Run: it reads the members that are destroyed right after this.
		constexpr bool bWaitForThread = true;
		M_Thread->Kill(bWaitForThread);
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
		FAsyncTargetSnapshot NewSnapshot;
		while (M_PendingSnapshots.Dequeue(NewSnapshot))
		{
			M_Snapshot = MoveTemp(NewSnapshot);
		}

		TArray<FAsyncDetailedUnitState> NewDetailedUnitStates;
		while (M_PendingDetailedUnitStateUpdates.Dequeue(NewDetailedUnitStates))
		{
			M_DetailedUnitStates = MoveTemp(NewDetailedUnitStates);
		}


		// Process target requests
		ProcessTargetRequests();
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

void FGetAsyncTarget::ScheduleUpdateActorData(FAsyncTargetSnapshot&& NewSnapshot)
{
	// Enqueue the new actor data for processing on the thread
	M_PendingSnapshots.Enqueue(MoveTemp(NewSnapshot));
}

void FGetAsyncTarget::ScheduleUpdateDetailedActorData(TArray<FAsyncDetailedUnitState>&& NewDetailedUnitStates)
{
	M_PendingDetailedUnitStateUpdates.Enqueue(MoveTemp(NewDetailedUnitStates));
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

void FGetAsyncTarget::AddTargetRequest(FAsyncTargetRequest&& Request)
{
	Request.MinimumSearchRadius = FMath::Max(Request.MinimumSearchRadius, 0.0f);

	// Enqueue the request
	M_RequestQueue.Enqueue(MoveTemp(Request));
}

namespace AsyncTargetSearch
{
	/** A unit inside the search range of a request; refers to the snapshot by index. */
	struct FTargetCandidate
	{
		double DistanceSquared = 0.0;
		int32 UnitIndex = INDEX_NONE;
	};

	/** The closest candidates found so far, closest first, never more than the request asked for. */
	using FClosestCandidates = TArray<FTargetCandidate, TInlineAllocator<AsyncTargetConstants::InlineTargetsPerRequest>>;

	/** The ranges of one request squared once, so no unit needs a square root. */
	struct FTargetSearchRange
	{
		double MaxDistanceSquared = 0.0;
		double MaxAircraftDistanceSquared = 0.0;
		double MinDistanceSquared = 0.0;
		int32 MaxTargets = 1;
	};

	FTargetSearchRange GetSearchRange(const FAsyncTargetRequest& Request)
	{
		const double SearchRadius = FMath::Max(Request.SearchRadius, 0.0f);
		const double MinimumSearchRadius = FMath::Max(Request.MinimumSearchRadius, 0.0f);
		FTargetSearchRange Range;
		Range.MaxDistanceSquared = FMath::Square(SearchRadius);
		// Aircraft fly high and far, so anti-air reaches them further out; ground targets keep the requested radius.
		Range.MaxAircraftDistanceSquared = FMath::Square(
			SearchRadius * DeveloperSettings::GameBalance::Ranges::AntiAirRangeMlt);
		Range.MinDistanceSquared = FMath::Square(MinimumSearchRadius);
		Range.MaxTargets = FMath::Max(Request.NumTargets, 1);
		return Range;
	}

	bool GetCanUseActorForTargetRequest(
		const ETargetPreference ActorType,
		const ETargetPreference RequestTargetPreference)
	{
		if (ActorType != ETargetPreference::Aircraft)
		{
			return true;
		}

		return RequestTargetPreference == ETargetPreference::Aircraft;
	}

	/**
	 * @param OutDistanceSquared Squared distance from the search location to the unit; only set when in range.
	 * @return Whether the requester sees the unit, may target its type and has it inside the search range.
	 */
	bool GetIsUnitCandidateForRequest(
		const FAsyncTargetUnit& Unit,
		const FAsyncTargetRequest& Request,
		const FTargetSearchRange& Range,
		double& OutDistanceSquared)
	{
		if ((Unit.VisibleToMask & Request.RequesterBit) == 0)
		{
			return false;
		}
		if (not GetCanUseActorForTargetRequest(Unit.TargetType, Request.TargetPreference))
		{
			return false;
		}

		const double DistanceSquared = FVector::DistSquared(Request.SearchLocation, Unit.Location);
		const double MaxDistanceSquared = Unit.TargetType == ETargetPreference::Aircraft
			                                  ? Range.MaxAircraftDistanceSquared
			                                  : Range.MaxDistanceSquared;
		if (DistanceSquared > MaxDistanceSquared || DistanceSquared < Range.MinDistanceSquared)
		{
			return false;
		}

		OutDistanceSquared = DistanceSquared;
		return true;
	}

	/** Keeps the list sorted and at most MaxCandidates long: a few shifts instead of sorting every unit in range. */
	void InsertClosestCandidate(
		FClosestCandidates& Closest,
		const int32 MaxCandidates,
		const double DistanceSquared,
		const int32 UnitIndex)
	{
		if (Closest.Num() >= MaxCandidates)
		{
			if (DistanceSquared >= Closest.Last().DistanceSquared)
			{
				return;
			}
			// The farthest makes room first: inserting into a full inline array would move it to the heap.
			Closest.Pop(EAllowShrinking::No);
		}

		int32 InsertIndex = Closest.Num();
		while (InsertIndex > 0 && Closest[InsertIndex - 1].DistanceSquared > DistanceSquared)
		{
			--InsertIndex;
		}

		FTargetCandidate NewCandidate;
		NewCandidate.DistanceSquared = DistanceSquared;
		NewCandidate.UnitIndex = UnitIndex;
		Closest.Insert(NewCandidate, InsertIndex);
	}

	void GatherCandidatesOfOwner(
		const FAsyncTargetSnapshot& Snapshot,
		const FAsyncTargetRequest& Request,
		const FTargetSearchRange& Range,
		const int32 OwnerPlayer,
		FClosestCandidates& PreferredCandidates,
		FClosestCandidates& OtherCandidates)
	{
		const int32 FirstUnit = FMath::Max(Snapshot.FirstUnitOfPlayer[OwnerPlayer], 0);
		const int32 EndUnit = FMath::Min(Snapshot.FirstUnitOfPlayer[OwnerPlayer + 1], Snapshot.Units.Num());
		for (int32 UnitIndex = FirstUnit; UnitIndex < EndUnit; ++UnitIndex)
		{
			const FAsyncTargetUnit& Unit = Snapshot.Units[UnitIndex];
			double DistanceSquared = 0.0;
			if (not GetIsUnitCandidateForRequest(Unit, Request, Range, DistanceSquared))
			{
				continue;
			}

			// Categorize actors based on the requested target preference.
			FClosestCandidates& Candidates = Unit.TargetType == Request.TargetPreference
				                                 ? PreferredCandidates
				                                 : OtherCandidates;
			InsertClosestCandidate(Candidates, Range.MaxTargets, DistanceSquared, UnitIndex);
		}
	}

	void AppendTargetsOfCandidates(
		const FAsyncTargetSnapshot& Snapshot,
		const FClosestCandidates& Candidates,
		const int32 MaxTargets,
		FAsyncTargetResult& OutTargets)
	{
		for (const FTargetCandidate& Candidate : Candidates)
		{
			if (OutTargets.Num() >= MaxTargets)
			{
				return;
			}
			OutTargets.Add(Snapshot.Units[Candidate.UnitIndex].Actor);
		}
	}

	/**
	 * @brief Finds the closest units the requester fights and sees, preferred target type first.
	 * @param Snapshot Units of all players, grouped by owner.
	 * @param Request Where to search and for whom.
	 * @param OutTargets Up to the requested number of targets, closest first within preferred and other.
	 */
	void FindClosestTargets(
		const FAsyncTargetSnapshot& Snapshot,
		const FAsyncTargetRequest& Request,
		FAsyncTargetResult& OutTargets)
	{
		if (Request.RequestingPlayer >= RTSPlayers::PlayerArraySize)
		{
			return;
		}
		// Owners the requester fights that have a unit it sees: none for every weapon far from any enemy.
		const uint8 OwnersToSearch = Snapshot.VisibleOwnersOfViewer[Request.RequestingPlayer] & Request.HostileMask;
		if (OwnersToSearch == 0)
		{
			return;
		}

		const FTargetSearchRange Range = GetSearchRange(Request);
		FClosestCandidates PreferredCandidates;
		FClosestCandidates OtherCandidates;
		for (int32 OwnerPlayer = RTSPlayers::HumanPlayer; OwnerPlayer <= RTSPlayers::MaxPlayers; ++OwnerPlayer)
		{
			if (not FRTSPlayers::IsPlayerInMask(OwnerPlayer, OwnersToSearch))
			{
				continue;
			}
			GatherCandidatesOfOwner(Snapshot, Request, Range, OwnerPlayer, PreferredCandidates, OtherCandidates);
		}

		// Collect the top N targets, filling from preferred targets first.
		AppendTargetsOfCandidates(Snapshot, PreferredCandidates, Range.MaxTargets, OutTargets);
		// If more targets are needed, add actors from the other targets.
		AppendTargetsOfCandidates(Snapshot, OtherCandidates, Range.MaxTargets, OutTargets);
	}
}

void FGetAsyncTarget::ProcessTargetRequests()
{
	// All answers of this pass go to the game thread in one task instead of one task per request.
	TArray<FTargetAnswer> Answers;
	FAsyncTargetRequest Request;
	while (M_RequestQueue.Dequeue(Request))
	{
		FTargetAnswer& Answer = Answers.AddDefaulted_GetRef();
		AsyncTargetSearch::FindClosestTargets(M_Snapshot, Request, Answer.Targets);
		Answer.Callback = MoveTemp(Request.Callback);
	}

	if (Answers.IsEmpty())
	{
		return;
	}

	// Invoke the callbacks on the game thread with the collected targets; every request gets its call, also
	// when its target list is empty.
	AsyncTask(ENamedThreads::GameThread,
		[Answers = MoveTemp(Answers)]()
		{
			for (const FTargetAnswer& Answer : Answers)
			{
				if (Answer.Callback)
				{
					Answer.Callback(Answer.Targets);
				}
			}
		});
}

void FGetAsyncTarget::ProcessStrategicAIRequests()
{
	FStrategicAIRequest Request;
	while (M_StrategicAIRequestQueue.Dequeue(Request))
	{
		FStrategicAIResultBatch Results;

		// Who asks and who that player fights; stamped on the game thread when the batch was sent.
		const FStrategicAIRequester& Requester = Request.RequestBatch.Requester;
		// Each requesting player has its own base footprint: its construction requests must stay outside its own
		// bases, not those another controller found.
		FEnemyBaseClusterAsyncCache& BaseClusterCache =
			M_EnemyBaseClusterAsyncCacheOfPlayer[FRTSPlayers::NormalizePlayer(Requester.RequestingPlayer)];

		Results.FindClosestFlankableEnemyHeavyResults.Reserve(
			Request.RequestBatch.FindClosestFlankableEnemyHeavyRequests.Num());
		for (const FFindClosestFlankableEnemyHeavy& FindRequest : Request.RequestBatch.FindClosestFlankableEnemyHeavyRequests)
		{
			Results.FindClosestFlankableEnemyHeavyResults.Add(
				FStrategicAIHelpers::BuildClosestFlankableEnemyHeavyResult(FindRequest, M_DetailedUnitStates, Requester));
		}

		Results.PlayerUnitCountsResults.Reserve(
			Request.RequestBatch.GetPlayerUnitCountsAndBaseRequests.Num());
		for (const FGetPlayerUnitCountsAndBase& CountRequest : Request.RequestBatch.GetPlayerUnitCountsAndBaseRequests)
		{
			Results.PlayerUnitCountsResults.Add(
				FStrategicAIHelpers::BuildPlayerUnitCountsResult(CountRequest, M_DetailedUnitStates, Requester));
		}

		Results.AlliedTanksToRetreatResults.Reserve(
			Request.RequestBatch.FindAlliedTanksToRetreatRequests.Num());
		for (const FFindAlliedTanksToRetreat& RetreatRequest : Request.RequestBatch.FindAlliedTanksToRetreatRequests)
		{
			Results.AlliedTanksToRetreatResults.Add(
				FStrategicAIHelpers::BuildAlliedTanksToRetreatResult(RetreatRequest, M_DetailedUnitStates, Requester));
		}

		Results.EnemyBaseClustersResults.Reserve(
			Request.RequestBatch.FindEnemyBaseClustersRequests.Num());
		for (const FFindEnemyBaseClusters& BaseClusterRequest : Request.RequestBatch.FindEnemyBaseClustersRequests)
		{
			FResultEnemyBaseClusters BaseClusterResult = FStrategicAIHelpers::BuildEnemyBaseClustersResult(
				BaseClusterRequest,
				M_DetailedUnitStates,
				Requester);
			BaseClusterCache.UpdateFromResult(BaseClusterResult);
			Results.EnemyBaseClustersResults.Add(MoveTemp(BaseClusterResult));
		}

		Results.LocationsUnderPlayerAttackResults.Reserve(
			Request.RequestBatch.FindLocationsUnderPlayerAttackRequests.Num());
		for (const FFindLocationsUnderPlayerAttack& AttackLocationRequest : Request.RequestBatch.FindLocationsUnderPlayerAttackRequests)
		{
			Results.LocationsUnderPlayerAttackResults.Add(
				FStrategicAIHelpers::BuildLocationsUnderPlayerAttackResult(
					AttackLocationRequest, M_DetailedUnitStates, Requester));
		}

		Results.PlayerUnitBulkLocationsResults.Reserve(
			Request.RequestBatch.FindPlayerUnitBulkLocationsRequests.Num());
		for (const FFindPlayerUnitBulkLocations& BulkLocationsRequest :
			Request.RequestBatch.FindPlayerUnitBulkLocationsRequests)
		{
			Results.PlayerUnitBulkLocationsResults.Add(
				FStrategicAIHelpers::BuildPlayerUnitBulkLocationsResult(
					BulkLocationsRequest, M_DetailedUnitStates, Requester));
		}

		Results.ConstructionLocationsResults.Reserve(
			Request.RequestBatch.FindConstructionLocationsRequests.Num());
		for (const FFindConstructionLocations& ConstructionLocationsRequest :
			Request.RequestBatch.FindConstructionLocationsRequests)
		{
			Results.ConstructionLocationsResults.Add(
				FStrategicAIHelpers::BuildConstructionLocationsResult(
					ConstructionLocationsRequest,
					BaseClusterCache.CoreBuildingsByBase,
					BaseClusterCache.SatelliteBuildingsByBase));
		}

		Results.MineLocationsResults.Reserve(
			Request.RequestBatch.FindMineLocationsRequests.Num());
		for (const FFindMineLocations& MineLocationsRequest : Request.RequestBatch.FindMineLocationsRequests)
		{
			Results.MineLocationsResults.Add(
				FStrategicAIHelpers::BuildMineLocationsResult(MineLocationsRequest));
		}

		TFunction<void(const FStrategicAIResultBatch&)> Callback = MoveTemp(Request.Callback);
		AsyncTask(ENamedThreads::GameThread,
			[Callback = MoveTemp(Callback), Results = MoveTemp(Results)]() mutable
			{
				Callback(Results);
			});
	}
}
