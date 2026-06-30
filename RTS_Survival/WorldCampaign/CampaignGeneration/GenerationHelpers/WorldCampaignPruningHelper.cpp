// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCampaignPruningHelper.h"

#include "Containers/Queue.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

namespace WorldCampaignPruningHelper
{
	namespace
	{
		constexpr float HalfValue = 0.5f;
		constexpr int32 MinConnectionAnchorCount = 2;

		struct FContractedEdge
		{
			AAnchorPoint* AnchorA = nullptr;
			AAnchorPoint* AnchorB = nullptr;
			float Weight = 0.f;
		};

		struct FPlayerHQRelayCandidate
		{
			AAnchorPoint* Anchor = nullptr;
			float TargetDistanceSquared = TNumericLimits<float>::Max();
			float HQDistanceSquared = 0.f;
			float LateralDistanceSquared = TNumericLimits<float>::Max();
		};

		struct FPruningGraph
		{
			TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorsByKey;
			TMap<FGuid, TArray<FGuid>> NeighborKeysByAnchorKey;
			TSet<FGuid> KeptAnchorKeys;
			TMap<FGuid, TObjectPtr<AAnchorPoint>> KeptAnchorsByKey;
			TArray<FContractedEdge> ContractedEdges;
		};

		bool GetIsValidContext(const FWorldCampaignPruningContext& Context)
		{
			if (not IsValid(Context.World))
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed: world is invalid."));
				return false;
			}

			if (not IsValid(Context.Owner))
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed: owner is invalid."));
				return false;
			}

			if (not IsValid(Context.PlayerHQAnchor))
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed: player HQ anchor is invalid."));
				return false;
			}

			if (Context.CachedAnchors == nullptr
				|| Context.CachedConnections == nullptr
				|| Context.GeneratedConnections == nullptr)
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed: graph arrays were missing."));
				return false;
			}

			return true;
		}

		bool GetIsGameplayAnchor(const AAnchorPoint* AnchorPoint, const TSet<FGuid>& GameplayAnchorKeys)
		{
			if (not IsValid(AnchorPoint))
			{
				return false;
			}

			if (GameplayAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
			{
				return true;
			}

			return AnchorPoint->GetHasPromotedWorldObject();
		}

		bool ContainsAnchor(const TArray<TObjectPtr<AAnchorPoint>>& Anchors, const AAnchorPoint* AnchorToFind)
		{
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : Anchors)
			{
				if (AnchorPoint.Get() == AnchorToFind)
				{
					return true;
				}
			}

			return false;
		}

		void AddUniqueAnchor(TArray<TObjectPtr<AAnchorPoint>>& InOutAnchors, AAnchorPoint* AnchorPoint)
		{
			if (not IsValid(AnchorPoint))
			{
				return;
			}

			if (ContainsAnchor(InOutAnchors, AnchorPoint))
			{
				return;
			}

			InOutAnchors.Add(AnchorPoint);
		}

		TSet<FGuid> BuildAnchorKeySet(const TArray<TObjectPtr<AAnchorPoint>>& Anchors)
		{
			TSet<FGuid> AnchorKeys;
			AnchorKeys.Reserve(Anchors.Num());
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : Anchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
				if (AnchorKey.IsValid())
				{
					AnchorKeys.Add(AnchorKey);
				}
			}

			return AnchorKeys;
		}

		TSet<AConnection*> BuildConnectionSet(const TArray<TObjectPtr<AConnection>>& Connections)
		{
			TSet<AConnection*> ConnectionSet;
			ConnectionSet.Reserve(Connections.Num());
			for (const TObjectPtr<AConnection>& Connection : Connections)
			{
				if (not IsValid(Connection))
				{
					continue;
				}

				ConnectionSet.Add(Connection.Get());
			}

			return ConnectionSet;
		}

		TArray<TObjectPtr<AAnchorPoint>> GetValidConnectedAnchors(
			const AConnection* Connection,
			const TSet<FGuid>& CachedAnchorKeys)
		{
			TArray<TObjectPtr<AAnchorPoint>> ConnectedAnchors;
			if (not IsValid(Connection))
			{
				return ConnectedAnchors;
			}

			for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : Connection->GetConnectedAnchors())
			{
				if (not IsValid(ConnectedAnchor))
				{
					continue;
				}

				if (not CachedAnchorKeys.Contains(ConnectedAnchor->GetAnchorKey()))
				{
					continue;
				}

				AddUniqueAnchor(ConnectedAnchors, ConnectedAnchor.Get());
			}

			return ConnectedAnchors;
		}

		void RegisterConnectionPair(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB)
		{
			if (not IsValid(Connection) || not IsValid(AnchorA) || not IsValid(AnchorB))
			{
				return;
			}

			if (AnchorA == AnchorB)
			{
				return;
			}

			AnchorA->AddConnection(Connection, AnchorB);
			AnchorB->AddConnection(Connection, AnchorA);
		}

		void AddNeighborKey(TMap<FGuid, TArray<FGuid>>& InOutNeighborKeysByAnchorKey,
		                    const FGuid& AnchorKey,
		                    const FGuid& NeighborKey)
		{
			if (not AnchorKey.IsValid() || not NeighborKey.IsValid() || AnchorKey == NeighborKey)
			{
				return;
			}

			TArray<FGuid>& NeighborKeys = InOutNeighborKeysByAnchorKey.FindOrAdd(AnchorKey);
			NeighborKeys.AddUnique(NeighborKey);
		}

		void AddGraphConnection(FPruningGraph& InOutGraph, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB)
		{
			if (not IsValid(AnchorA) || not IsValid(AnchorB) || AnchorA == AnchorB)
			{
				return;
			}

			const FGuid AnchorKeyA = AnchorA->GetAnchorKey();
			const FGuid AnchorKeyB = AnchorB->GetAnchorKey();
			AddNeighborKey(InOutGraph.NeighborKeysByAnchorKey, AnchorKeyA, AnchorKeyB);
			AddNeighborKey(InOutGraph.NeighborKeysByAnchorKey, AnchorKeyB, AnchorKeyA);
		}

		void BuildPruningGraphAnchors(const FWorldCampaignPruningContext& Context, FPruningGraph& OutGraph)
		{
			OutGraph.AnchorsByKey.Reset();
			OutGraph.KeptAnchorKeys.Reset();
			OutGraph.KeptAnchorsByKey.Reset();
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
				if (not AnchorKey.IsValid())
				{
					continue;
				}

				OutGraph.AnchorsByKey.Add(AnchorKey, AnchorPoint);
				OutGraph.NeighborKeysByAnchorKey.FindOrAdd(AnchorKey);
				if (not GetIsGameplayAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					continue;
				}

				OutGraph.KeptAnchorKeys.Add(AnchorKey);
				OutGraph.KeptAnchorsByKey.Add(AnchorKey, AnchorPoint);
			}
		}

		void BuildPruningGraphConnections(const FWorldCampaignPruningContext& Context, FPruningGraph& OutGraph)
		{
			const TSet<FGuid> CachedAnchorKeys = BuildAnchorKeySet(*Context.CachedAnchors);
			for (const TObjectPtr<AConnection>& Connection : *Context.CachedConnections)
			{
				const TArray<TObjectPtr<AAnchorPoint>> ConnectedAnchors =
					GetValidConnectedAnchors(Connection.Get(), CachedAnchorKeys);
				for (int32 AnchorIndex = 0; AnchorIndex < ConnectedAnchors.Num(); AnchorIndex++)
				{
					for (int32 OtherAnchorIndex = AnchorIndex + 1;
					     OtherAnchorIndex < ConnectedAnchors.Num();
					     OtherAnchorIndex++)
					{
						AddGraphConnection(
							OutGraph,
							ConnectedAnchors[AnchorIndex].Get(),
							ConnectedAnchors[OtherAnchorIndex].Get());
					}
				}
			}
		}

		FPruningGraph BuildPruningGraph(const FWorldCampaignPruningContext& Context)
		{
			FPruningGraph Graph;
			BuildPruningGraphAnchors(Context, Graph);
			BuildPruningGraphConnections(Context, Graph);
			return Graph;
		}

		bool GetShouldEdgeSortBefore(const FContractedEdge& Left, const FContractedEdge& Right)
		{
			if (Left.Weight != Right.Weight)
			{
				return Left.Weight < Right.Weight;
			}

			const FGuid LeftKeyA = IsValid(Left.AnchorA) ? Left.AnchorA->GetAnchorKey() : FGuid();
			const FGuid LeftKeyB = IsValid(Left.AnchorB) ? Left.AnchorB->GetAnchorKey() : FGuid();
			const FGuid RightKeyA = IsValid(Right.AnchorA) ? Right.AnchorA->GetAnchorKey() : FGuid();
			const FGuid RightKeyB = IsValid(Right.AnchorB) ? Right.AnchorB->GetAnchorKey() : FGuid();
			if (AAnchorPoint::IsAnchorKeyLess(LeftKeyA, RightKeyA))
			{
				return true;
			}

			if (AAnchorPoint::IsAnchorKeyLess(RightKeyA, LeftKeyA))
			{
				return false;
			}

			return AAnchorPoint::IsAnchorKeyLess(LeftKeyB, RightKeyB);
		}

		FString BuildEdgeKey(const AAnchorPoint* AnchorA, const AAnchorPoint* AnchorB)
		{
			if (not IsValid(AnchorA) || not IsValid(AnchorB))
			{
				return FString();
			}

			FGuid AnchorKeyA = AnchorA->GetAnchorKey();
			FGuid AnchorKeyB = AnchorB->GetAnchorKey();
			if (AAnchorPoint::IsAnchorKeyLess(AnchorKeyB, AnchorKeyA))
			{
				Swap(AnchorKeyA, AnchorKeyB);
			}

			return AnchorKeyA.ToString(EGuidFormats::DigitsWithHyphens)
				+ TEXT("|")
				+ AnchorKeyB.ToString(EGuidFormats::DigitsWithHyphens);
		}

		void AddContractedEdge(
			FPruningGraph& InOutGraph,
			TSet<FString>& InOutEdgeKeys,
			AAnchorPoint* AnchorA,
			AAnchorPoint* AnchorB)
		{
			if (not IsValid(AnchorA) || not IsValid(AnchorB) || AnchorA == AnchorB)
			{
				return;
			}

			const FString EdgeKey = BuildEdgeKey(AnchorA, AnchorB);
			if (EdgeKey.IsEmpty() || InOutEdgeKeys.Contains(EdgeKey))
			{
				return;
			}

			FContractedEdge ContractedEdge;
			ContractedEdge.AnchorA = AnchorA;
			ContractedEdge.AnchorB = AnchorB;
			ContractedEdge.Weight = FVector::DistSquared(AnchorA->GetActorLocation(), AnchorB->GetActorLocation());
			InOutGraph.ContractedEdges.Add(ContractedEdge);
			InOutEdgeKeys.Add(EdgeKey);
		}

		TSet<FString> BuildContractedEdgeKeys(const TArray<FContractedEdge>& ContractedEdges)
		{
			TSet<FString> EdgeKeys;
			EdgeKeys.Reserve(ContractedEdges.Num());
			for (const FContractedEdge& ContractedEdge : ContractedEdges)
			{
				const FString EdgeKey = BuildEdgeKey(ContractedEdge.AnchorA, ContractedEdge.AnchorB);
				if (EdgeKey.IsEmpty())
				{
					continue;
				}

				EdgeKeys.Add(EdgeKey);
			}

			return EdgeKeys;
		}

		FVector2D GetAnchorLocationXY(const AAnchorPoint* AnchorPoint)
		{
			if (not IsValid(AnchorPoint))
			{
				return FVector2D::ZeroVector;
			}

			const FVector AnchorLocation = AnchorPoint->GetActorLocation();
			return FVector2D(AnchorLocation.X, AnchorLocation.Y);
		}

		float GetAnchorDistanceSquaredXY(const AAnchorPoint* AnchorA, const AAnchorPoint* AnchorB)
		{
			return FVector2D::DistSquared(GetAnchorLocationXY(AnchorA), GetAnchorLocationXY(AnchorB));
		}

		float GetLateralDistanceSquaredToSegmentXY(
			const FVector2D& Point,
			const FVector2D& SegmentStart,
			const FVector2D& SegmentEnd)
		{
			const FVector2D SegmentVector = SegmentEnd - SegmentStart;
			const float SegmentLengthSquared = SegmentVector.SizeSquared();
			if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
			{
				return FVector2D::DistSquared(Point, SegmentStart);
			}

			const FVector2D PointVector = Point - SegmentStart;
			const float Projection = FVector2D::DotProduct(PointVector, SegmentVector) / SegmentLengthSquared;
			const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
			const FVector2D ProjectedPoint = SegmentStart + SegmentVector * ClampedProjection;
			return FVector2D::DistSquared(Point, ProjectedPoint);
		}

		bool GetIsMissionOrEnemyAnchor(const AAnchorPoint* AnchorPoint)
		{
			if (not IsValid(AnchorPoint))
			{
				return false;
			}

			const AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
			return IsValid(Cast<AWorldMissionObject>(PromotedWorldObject))
				|| IsValid(Cast<AWorldEnemyObject>(PromotedWorldObject));
		}

		AAnchorPoint* GetPlayerHQDirectEdgeTarget(
			const FContractedEdge& ContractedEdge,
			const AAnchorPoint* PlayerHQAnchor)
		{
			if (not IsValid(PlayerHQAnchor))
			{
				return nullptr;
			}

			if (ContractedEdge.AnchorA == PlayerHQAnchor)
			{
				return ContractedEdge.AnchorB;
			}

			if (ContractedEdge.AnchorB == PlayerHQAnchor)
			{
				return ContractedEdge.AnchorA;
			}

			return nullptr;
		}

		TMap<FGuid, TArray<FGuid>> BuildContractedNeighborKeysByAnchorKey(
			const TArray<FContractedEdge>& ContractedEdges,
			const FString& EdgeKeyToIgnore)
		{
			TMap<FGuid, TArray<FGuid>> NeighborKeysByAnchorKey;
			for (const FContractedEdge& ContractedEdge : ContractedEdges)
			{
				if (not IsValid(ContractedEdge.AnchorA) || not IsValid(ContractedEdge.AnchorB))
				{
					continue;
				}

				const FString EdgeKey = BuildEdgeKey(ContractedEdge.AnchorA, ContractedEdge.AnchorB);
				if (not EdgeKeyToIgnore.IsEmpty() && EdgeKey == EdgeKeyToIgnore)
				{
					continue;
				}

				AddNeighborKey(
					NeighborKeysByAnchorKey,
					ContractedEdge.AnchorA->GetAnchorKey(),
					ContractedEdge.AnchorB->GetAnchorKey());
				AddNeighborKey(
					NeighborKeysByAnchorKey,
					ContractedEdge.AnchorB->GetAnchorKey(),
					ContractedEdge.AnchorA->GetAnchorKey());
			}

			return NeighborKeysByAnchorKey;
		}

		TSet<FGuid> BuildReachableAnchorKeysFromNeighborMap(
			const TMap<FGuid, TArray<FGuid>>& NeighborKeysByAnchorKey,
			const FGuid& StartAnchorKey)
		{
			TSet<FGuid> ReachableAnchorKeys;
			if (not StartAnchorKey.IsValid())
			{
				return ReachableAnchorKeys;
			}

			TQueue<FGuid> AnchorQueue;
			AnchorQueue.Enqueue(StartAnchorKey);
			ReachableAnchorKeys.Add(StartAnchorKey);

			while (not AnchorQueue.IsEmpty())
			{
				FGuid CurrentAnchorKey;
				AnchorQueue.Dequeue(CurrentAnchorKey);
				const TArray<FGuid>* NeighborKeys = NeighborKeysByAnchorKey.Find(CurrentAnchorKey);
				if (NeighborKeys == nullptr)
				{
					continue;
				}

				for (const FGuid& NeighborKey : *NeighborKeys)
				{
					if (ReachableAnchorKeys.Contains(NeighborKey))
					{
						continue;
					}

					ReachableAnchorKeys.Add(NeighborKey);
					AnchorQueue.Enqueue(NeighborKey);
				}
			}

			return ReachableAnchorKeys;
		}

		TSet<FGuid> BuildReachableContractedAnchorKeys(
			const TArray<FContractedEdge>& ContractedEdges,
			const FGuid& StartAnchorKey,
			const FString& EdgeKeyToIgnore)
		{
			const TMap<FGuid, TArray<FGuid>> NeighborKeysByAnchorKey =
				BuildContractedNeighborKeysByAnchorKey(ContractedEdges, EdgeKeyToIgnore);
			return BuildReachableAnchorKeysFromNeighborMap(NeighborKeysByAnchorKey, StartAnchorKey);
		}

		bool GetIsCandidateBetweenPlayerHQAndTarget(
			const AAnchorPoint* PlayerHQAnchor,
			const AAnchorPoint* TargetAnchor,
			const AAnchorPoint* CandidateAnchor)
		{
			const FVector2D HQLocationXY = GetAnchorLocationXY(PlayerHQAnchor);
			const FVector2D TargetLocationXY = GetAnchorLocationXY(TargetAnchor);
			const FVector2D CandidateLocationXY = GetAnchorLocationXY(CandidateAnchor);
			const FVector2D HQToTarget = TargetLocationXY - HQLocationXY;
			const FVector2D HQToCandidate = CandidateLocationXY - HQLocationXY;
			const float TargetDistanceSquared = HQToTarget.SizeSquared();
			if (TargetDistanceSquared <= KINDA_SMALL_NUMBER)
			{
				return false;
			}

			const float CandidateProjection = FVector2D::DotProduct(HQToCandidate, HQToTarget);
			return CandidateProjection > KINDA_SMALL_NUMBER
				&& CandidateProjection + KINDA_SMALL_NUMBER < TargetDistanceSquared;
		}

		bool GetCanUsePlayerHQRelayAnchor(
			const AAnchorPoint* PlayerHQAnchor,
			const AAnchorPoint* TargetAnchor,
			const AAnchorPoint* CandidateAnchor,
			const TSet<FGuid>& ReachableAnchorKeysWithoutDirectEdge)
		{
			if (not IsValid(PlayerHQAnchor) || not IsValid(TargetAnchor) || not IsValid(CandidateAnchor))
			{
				return false;
			}

			if (CandidateAnchor == PlayerHQAnchor || CandidateAnchor == TargetAnchor)
			{
				return false;
			}

			if (not ReachableAnchorKeysWithoutDirectEdge.Contains(CandidateAnchor->GetAnchorKey()))
			{
				return false;
			}

			if (not GetIsMissionOrEnemyAnchor(CandidateAnchor))
			{
				return false;
			}

			const float TargetHQDistanceSquared = GetAnchorDistanceSquaredXY(PlayerHQAnchor, TargetAnchor);
			const float CandidateHQDistanceSquared = GetAnchorDistanceSquaredXY(PlayerHQAnchor, CandidateAnchor);
			if (CandidateHQDistanceSquared + KINDA_SMALL_NUMBER >= TargetHQDistanceSquared)
			{
				return false;
			}

			const float CandidateTargetDistanceSquared = GetAnchorDistanceSquaredXY(CandidateAnchor, TargetAnchor);
			if (CandidateTargetDistanceSquared + KINDA_SMALL_NUMBER >= TargetHQDistanceSquared)
			{
				return false;
			}

			return GetIsCandidateBetweenPlayerHQAndTarget(PlayerHQAnchor, TargetAnchor, CandidateAnchor);
		}

		bool GetShouldReplaceBestRelayCandidate(
			const FPlayerHQRelayCandidate& Candidate,
			const FPlayerHQRelayCandidate& BestCandidate)
		{
			if (not IsValid(BestCandidate.Anchor))
			{
				return true;
			}

			if (Candidate.TargetDistanceSquared != BestCandidate.TargetDistanceSquared)
			{
				return Candidate.TargetDistanceSquared < BestCandidate.TargetDistanceSquared;
			}

			if (Candidate.HQDistanceSquared != BestCandidate.HQDistanceSquared)
			{
				return Candidate.HQDistanceSquared > BestCandidate.HQDistanceSquared;
			}

			if (Candidate.LateralDistanceSquared != BestCandidate.LateralDistanceSquared)
			{
				return Candidate.LateralDistanceSquared < BestCandidate.LateralDistanceSquared;
			}

			return AAnchorPoint::IsAnchorKeyLess(
				Candidate.Anchor->GetAnchorKey(),
				BestCandidate.Anchor->GetAnchorKey());
		}

		AAnchorPoint* FindBestPlayerHQRelayAnchor(
			const FPruningGraph& Graph,
			const AAnchorPoint* PlayerHQAnchor,
			const AAnchorPoint* TargetAnchor,
			const TSet<FGuid>& ReachableAnchorKeysWithoutDirectEdge)
		{
			FPlayerHQRelayCandidate BestCandidate;
			for (const TPair<FGuid, TObjectPtr<AAnchorPoint>>& AnchorPair : Graph.KeptAnchorsByKey)
			{
				AAnchorPoint* CandidateAnchor = AnchorPair.Value.Get();
				if (not GetCanUsePlayerHQRelayAnchor(
					PlayerHQAnchor,
					TargetAnchor,
					CandidateAnchor,
					ReachableAnchorKeysWithoutDirectEdge))
				{
					continue;
				}

				FPlayerHQRelayCandidate Candidate;
				Candidate.Anchor = CandidateAnchor;
				Candidate.TargetDistanceSquared = GetAnchorDistanceSquaredXY(CandidateAnchor, TargetAnchor);
				Candidate.HQDistanceSquared = GetAnchorDistanceSquaredXY(PlayerHQAnchor, CandidateAnchor);
				Candidate.LateralDistanceSquared = GetLateralDistanceSquaredToSegmentXY(
					GetAnchorLocationXY(CandidateAnchor),
					GetAnchorLocationXY(PlayerHQAnchor),
					GetAnchorLocationXY(TargetAnchor));
				if (GetShouldReplaceBestRelayCandidate(Candidate, BestCandidate))
				{
					BestCandidate = Candidate;
				}
			}

			return BestCandidate.Anchor;
		}

		void RefinePlayerHQDirectEdgesThroughCloserAnchors(
			FPruningGraph& InOutGraph,
			const AAnchorPoint* PlayerHQAnchor)
		{
			if (not IsValid(PlayerHQAnchor))
			{
				return;
			}

			const FGuid PlayerHQAnchorKey = PlayerHQAnchor->GetAnchorKey();
			TSet<FString> EdgeKeys = BuildContractedEdgeKeys(InOutGraph.ContractedEdges);
			for (int32 EdgeIndex = 0; EdgeIndex < InOutGraph.ContractedEdges.Num(); EdgeIndex++)
			{
				FContractedEdge& ContractedEdge = InOutGraph.ContractedEdges[EdgeIndex];
				AAnchorPoint* TargetAnchor = GetPlayerHQDirectEdgeTarget(ContractedEdge, PlayerHQAnchor);
				if (not IsValid(TargetAnchor))
				{
					continue;
				}

				const FString DirectEdgeKey = BuildEdgeKey(PlayerHQAnchor, TargetAnchor);
				const TSet<FGuid> ReachableAnchorKeysWithoutDirectEdge =
					BuildReachableContractedAnchorKeys(
						InOutGraph.ContractedEdges,
						PlayerHQAnchorKey,
						DirectEdgeKey);
				AAnchorPoint* RelayAnchor = FindBestPlayerHQRelayAnchor(
					InOutGraph,
					PlayerHQAnchor,
					TargetAnchor,
					ReachableAnchorKeysWithoutDirectEdge);
				if (not IsValid(RelayAnchor))
				{
					continue;
				}

				InOutGraph.ContractedEdges.RemoveAt(EdgeIndex);
				EdgeKeys.Remove(DirectEdgeKey);
				AddContractedEdge(InOutGraph, EdgeKeys, RelayAnchor, TargetAnchor);
				EdgeIndex--;
			}
		}

		void AddDirectKeptAnchorEdges(FPruningGraph& InOutGraph, TSet<FString>& InOutEdgeKeys)
		{
			for (const TPair<FGuid, TObjectPtr<AAnchorPoint>>& AnchorPair : InOutGraph.KeptAnchorsByKey)
			{
				const TArray<FGuid>* NeighborKeys = InOutGraph.NeighborKeysByAnchorKey.Find(AnchorPair.Key);
				if (NeighborKeys == nullptr)
				{
					continue;
				}

				for (const FGuid& NeighborKey : *NeighborKeys)
				{
					if (not InOutGraph.KeptAnchorKeys.Contains(NeighborKey))
					{
						continue;
					}

					TObjectPtr<AAnchorPoint>* NeighborAnchor = InOutGraph.KeptAnchorsByKey.Find(NeighborKey);
					if (NeighborAnchor == nullptr)
					{
						continue;
					}

					AddContractedEdge(InOutGraph, InOutEdgeKeys, AnchorPair.Value.Get(), NeighborAnchor->Get());
				}
			}
		}

		void BuildEmptyComponent(
			const FGuid& StartAnchorKey,
			const FPruningGraph& Graph,
			TSet<FGuid>& InOutVisitedEmptyAnchorKeys,
			TArray<FGuid>& OutEmptyAnchorKeys,
			TArray<TObjectPtr<AAnchorPoint>>& OutBoundaryKeptAnchors)
		{
			TQueue<FGuid> AnchorQueue;
			AnchorQueue.Enqueue(StartAnchorKey);
			InOutVisitedEmptyAnchorKeys.Add(StartAnchorKey);

			while (not AnchorQueue.IsEmpty())
			{
				FGuid CurrentAnchorKey;
				AnchorQueue.Dequeue(CurrentAnchorKey);
				OutEmptyAnchorKeys.Add(CurrentAnchorKey);

				const TArray<FGuid>* NeighborKeys = Graph.NeighborKeysByAnchorKey.Find(CurrentAnchorKey);
				if (NeighborKeys == nullptr)
				{
					continue;
				}

				for (const FGuid& NeighborKey : *NeighborKeys)
				{
					if (Graph.KeptAnchorKeys.Contains(NeighborKey))
					{
						const TObjectPtr<AAnchorPoint>* BoundaryAnchor = Graph.KeptAnchorsByKey.Find(NeighborKey);
						if (BoundaryAnchor != nullptr)
						{
							AddUniqueAnchor(OutBoundaryKeptAnchors, BoundaryAnchor->Get());
						}

						continue;
					}

					if (InOutVisitedEmptyAnchorKeys.Contains(NeighborKey))
					{
						continue;
					}

					InOutVisitedEmptyAnchorKeys.Add(NeighborKey);
					AnchorQueue.Enqueue(NeighborKey);
				}
			}
		}

		TArray<FContractedEdge> BuildBoundaryCandidateEdges(
			const TArray<TObjectPtr<AAnchorPoint>>& BoundaryKeptAnchors)
		{
			TArray<FContractedEdge> CandidateEdges;
			for (int32 AnchorIndex = 0; AnchorIndex < BoundaryKeptAnchors.Num(); AnchorIndex++)
			{
				AAnchorPoint* AnchorA = BoundaryKeptAnchors[AnchorIndex].Get();
				if (not IsValid(AnchorA))
				{
					continue;
				}

				for (int32 OtherAnchorIndex = AnchorIndex + 1;
				     OtherAnchorIndex < BoundaryKeptAnchors.Num();
				     OtherAnchorIndex++)
				{
					AAnchorPoint* AnchorB = BoundaryKeptAnchors[OtherAnchorIndex].Get();
					if (not IsValid(AnchorB))
					{
						continue;
					}

					FContractedEdge CandidateEdge;
					CandidateEdge.AnchorA = AnchorA;
					CandidateEdge.AnchorB = AnchorB;
					CandidateEdge.Weight = FVector::DistSquared(AnchorA->GetActorLocation(), AnchorB->GetActorLocation());
					CandidateEdges.Add(CandidateEdge);
				}
			}

			CandidateEdges.Sort([](const FContractedEdge& Left, const FContractedEdge& Right)
			{
				return GetShouldEdgeSortBefore(Left, Right);
			});
			return CandidateEdges;
		}

		FGuid FindDisjointSetRoot(TMap<FGuid, FGuid>& InOutParentByAnchorKey, const FGuid& AnchorKey)
		{
			FGuid* ParentKey = InOutParentByAnchorKey.Find(AnchorKey);
			if (ParentKey == nullptr || *ParentKey == AnchorKey)
			{
				return AnchorKey;
			}

			const FGuid RootKey = FindDisjointSetRoot(InOutParentByAnchorKey, *ParentKey);
			InOutParentByAnchorKey.Add(AnchorKey, RootKey);
			return RootKey;
		}

		bool TryUnionDisjointSet(TMap<FGuid, FGuid>& InOutParentByAnchorKey, const FGuid& AnchorKeyA, const FGuid& AnchorKeyB)
		{
			const FGuid RootKeyA = FindDisjointSetRoot(InOutParentByAnchorKey, AnchorKeyA);
			const FGuid RootKeyB = FindDisjointSetRoot(InOutParentByAnchorKey, AnchorKeyB);
			if (RootKeyA == RootKeyB)
			{
				return false;
			}

			InOutParentByAnchorKey.Add(RootKeyB, RootKeyA);
			return true;
		}

		void AddBoundaryMinimumSpanningTreeEdges(
			FPruningGraph& InOutGraph,
			TSet<FString>& InOutEdgeKeys,
			const TArray<TObjectPtr<AAnchorPoint>>& BoundaryKeptAnchors)
		{
			if (BoundaryKeptAnchors.Num() < 2)
			{
				return;
			}

			TMap<FGuid, FGuid> ParentByAnchorKey;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : BoundaryKeptAnchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				ParentByAnchorKey.Add(AnchorPoint->GetAnchorKey(), AnchorPoint->GetAnchorKey());
			}

			const TArray<FContractedEdge> CandidateEdges = BuildBoundaryCandidateEdges(BoundaryKeptAnchors);
			for (const FContractedEdge& CandidateEdge : CandidateEdges)
			{
				if (not IsValid(CandidateEdge.AnchorA) || not IsValid(CandidateEdge.AnchorB))
				{
					continue;
				}

				if (not TryUnionDisjointSet(
					ParentByAnchorKey,
					CandidateEdge.AnchorA->GetAnchorKey(),
					CandidateEdge.AnchorB->GetAnchorKey()))
				{
					continue;
				}

				AddContractedEdge(InOutGraph, InOutEdgeKeys, CandidateEdge.AnchorA, CandidateEdge.AnchorB);
			}
		}

		void AddEmptyComponentContractedEdges(FPruningGraph& InOutGraph, TSet<FString>& InOutEdgeKeys)
		{
			TSet<FGuid> VisitedEmptyAnchorKeys;
			for (const TPair<FGuid, TObjectPtr<AAnchorPoint>>& AnchorPair : InOutGraph.AnchorsByKey)
			{
				if (InOutGraph.KeptAnchorKeys.Contains(AnchorPair.Key)
					|| VisitedEmptyAnchorKeys.Contains(AnchorPair.Key))
				{
					continue;
				}

				TArray<FGuid> EmptyAnchorKeys;
				TArray<TObjectPtr<AAnchorPoint>> BoundaryKeptAnchors;
				BuildEmptyComponent(
					AnchorPair.Key,
					InOutGraph,
					VisitedEmptyAnchorKeys,
					EmptyAnchorKeys,
					BoundaryKeptAnchors);
				AddBoundaryMinimumSpanningTreeEdges(InOutGraph, InOutEdgeKeys, BoundaryKeptAnchors);
			}
		}

		void BuildContractedEdges(FPruningGraph& InOutGraph, const AAnchorPoint* PlayerHQAnchor)
		{
			TSet<FString> EdgeKeys;
			AddDirectKeptAnchorEdges(InOutGraph, EdgeKeys);
			AddEmptyComponentContractedEdges(InOutGraph, EdgeKeys);
			RefinePlayerHQDirectEdgesThroughCloserAnchors(InOutGraph, PlayerHQAnchor);
		}

		TSubclassOf<AConnection> GetConnectionClassToSpawn(const FWorldCampaignPruningContext& Context)
		{
			if (not IsValid(Context.ConnectionClass.Get()))
			{
				return AConnection::StaticClass();
			}

			return Context.ConnectionClass;
		}

		AConnection* SpawnConnectionBetweenAnchors(
			const FWorldCampaignPruningContext& Context,
			AAnchorPoint* AnchorA,
			AAnchorPoint* AnchorB,
			FWorldCampaignPruningResult& InOutResult)
		{
			if (not IsValid(AnchorA) || not IsValid(AnchorB) || AnchorA == AnchorB)
			{
				return nullptr;
			}

			const FVector SpawnLocation = (AnchorA->GetActorLocation() + AnchorB->GetActorLocation()) * HalfValue;
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = Context.Owner;

			AConnection* SpawnedConnection = Context.World->SpawnActor<AConnection>(
				GetConnectionClassToSpawn(Context),
				SpawnLocation,
				FRotator::ZeroRotator,
				SpawnParameters);
			if (not IsValid(SpawnedConnection))
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed to spawn a connection."));
				return nullptr;
			}

			SpawnedConnection->InitializeConnection(AnchorA, AnchorB);
			Context.CachedConnections->Add(SpawnedConnection);
			Context.GeneratedConnections->Add(SpawnedConnection);
			InOutResult.SpawnedConnections++;
			InOutResult.bDidChange = true;
			return SpawnedConnection;
		}

		void DestroyOldConnections(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			TArray<AConnection*> ConnectionsToDestroy;
			for (const TObjectPtr<AConnection>& Connection : *Context.CachedConnections)
			{
				if (not IsValid(Connection))
				{
					continue;
				}

				ConnectionsToDestroy.AddUnique(Connection.Get());
			}

			for (const TObjectPtr<AConnection>& Connection : *Context.GeneratedConnections)
			{
				if (not IsValid(Connection))
				{
					continue;
				}

				ConnectionsToDestroy.AddUnique(Connection.Get());
			}

			for (AConnection* Connection : ConnectionsToDestroy)
			{
				if (not IsValid(Connection))
				{
					continue;
				}

				Connection->Destroy();
				InOutResult.DestroyedConnections++;
				InOutResult.bDidChange = true;
			}

			Context.CachedConnections->Reset();
			Context.GeneratedConnections->Reset();
		}

		void DestroyEmptyAnchors(
			const FWorldCampaignPruningContext& Context,
			const FPruningGraph& Graph,
			FWorldCampaignPruningResult& InOutResult)
		{
			for (const TPair<FGuid, TObjectPtr<AAnchorPoint>>& AnchorPair : Graph.AnchorsByKey)
			{
				if (Graph.KeptAnchorKeys.Contains(AnchorPair.Key))
				{
					continue;
				}

				AAnchorPoint* AnchorPoint = AnchorPair.Value.Get();
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				AnchorPoint->RemovePromotedWorldObject();
				AnchorPoint->Destroy();
				InOutResult.DestroyedEmptyAnchors++;
				InOutResult.bDidChange = true;
			}

			Context.CachedAnchors->Reset();
			for (const TPair<FGuid, TObjectPtr<AAnchorPoint>>& KeptAnchorPair : Graph.KeptAnchorsByKey)
			{
				if (not IsValid(KeptAnchorPair.Value))
				{
					continue;
				}

				Context.CachedAnchors->Add(KeptAnchorPair.Value);
			}

			Context.CachedAnchors->Sort([](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
			{
				if (not IsValid(Left))
				{
					return false;
				}

				if (not IsValid(Right))
				{
					return true;
				}

				return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
			});
		}

		void DestroyGeneratedEmptyAnchorsOutsideGraph(
			const FWorldCampaignPruningContext& Context,
			const FPruningGraph& Graph,
			FWorldCampaignPruningResult& InOutResult)
		{
			if (Context.GeneratedAnchorTag.IsNone())
			{
				return;
			}

			for (TActorIterator<AAnchorPoint> AnchorIterator(Context.World); AnchorIterator; ++AnchorIterator)
			{
				AAnchorPoint* AnchorPoint = *AnchorIterator;
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				if (Graph.KeptAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
				{
					continue;
				}

				if (AnchorPoint->GetHasPromotedWorldObject())
				{
					continue;
				}

				if (not AnchorPoint->Tags.Contains(Context.GeneratedAnchorTag))
				{
					continue;
				}

				AnchorPoint->Destroy();
				InOutResult.DestroyedEmptyAnchors++;
				InOutResult.bDidChange = true;
			}
		}

		void SpawnContractedConnections(
			const FWorldCampaignPruningContext& Context,
			const FPruningGraph& Graph,
			FWorldCampaignPruningResult& InOutResult)
		{
			TArray<FContractedEdge> ContractedEdges = Graph.ContractedEdges;
			ContractedEdges.Sort([](const FContractedEdge& Left, const FContractedEdge& Right)
			{
				return GetShouldEdgeSortBefore(Left, Right);
			});

			for (const FContractedEdge& ContractedEdge : ContractedEdges)
			{
				SpawnConnectionBetweenAnchors(Context, ContractedEdge.AnchorA, ContractedEdge.AnchorB, InOutResult);
			}
		}

		void DestroyStaleOwnedConnectionsOutsideGraph(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			const TSet<AConnection*> CachedConnectionSet = BuildConnectionSet(*Context.CachedConnections);
			for (TActorIterator<AConnection> ConnectionIterator(Context.World); ConnectionIterator; ++ConnectionIterator)
			{
				AConnection* Connection = *ConnectionIterator;
				if (not IsValid(Connection))
				{
					continue;
				}

				if (Connection->GetOwner() != Context.Owner)
				{
					continue;
				}

				if (CachedConnectionSet.Contains(Connection))
				{
					continue;
				}

				Connection->Destroy();
				InOutResult.DestroyedConnections++;
				InOutResult.bDidChange = true;
			}
		}

		TSet<FGuid> BuildReachableKeptAnchorKeys(const AAnchorPoint* PlayerHQAnchor)
		{
			TSet<FGuid> ReachableAnchorKeys;
			if (not IsValid(PlayerHQAnchor))
			{
				return ReachableAnchorKeys;
			}

			TQueue<const AAnchorPoint*> AnchorQueue;
			AnchorQueue.Enqueue(PlayerHQAnchor);
			ReachableAnchorKeys.Add(PlayerHQAnchor->GetAnchorKey());

			while (not AnchorQueue.IsEmpty())
			{
				const AAnchorPoint* CurrentAnchor = nullptr;
				AnchorQueue.Dequeue(CurrentAnchor);
				if (not IsValid(CurrentAnchor))
				{
					continue;
				}

				const TArray<TObjectPtr<AAnchorPoint>>& NeighborAnchors = CurrentAnchor->GetNeighborAnchors();
				for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : NeighborAnchors)
				{
					if (not IsValid(NeighborAnchor))
					{
						continue;
					}

					const FGuid NeighborKey = NeighborAnchor->GetAnchorKey();
					if (ReachableAnchorKeys.Contains(NeighborKey))
					{
						continue;
					}

					ReachableAnchorKeys.Add(NeighborKey);
					AnchorQueue.Enqueue(NeighborAnchor.Get());
				}
			}

			return ReachableAnchorKeys;
		}

		bool GetAreAllKeptAnchorsReachable(
			const TArray<TObjectPtr<AAnchorPoint>>& KeptAnchors,
			const TSet<FGuid>& ReachableAnchorKeys)
		{
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : KeptAnchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				if (ReachableAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
				{
					continue;
				}

				return false;
			}

			return true;
		}

		AAnchorPoint* FindClosestReachableAnchor(
			const TArray<TObjectPtr<AAnchorPoint>>& KeptAnchors,
			const TSet<FGuid>& ReachableAnchorKeys,
			const AAnchorPoint* TargetAnchor)
		{
			AAnchorPoint* ClosestAnchor = nullptr;
			float ClosestDistanceSquared = TNumericLimits<float>::Max();
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : KeptAnchors)
			{
				if (not IsValid(AnchorPoint) || not ReachableAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
				{
					continue;
				}

				const float DistanceSquared = FVector::DistSquared(
					AnchorPoint->GetActorLocation(),
					TargetAnchor->GetActorLocation());
				if (DistanceSquared >= ClosestDistanceSquared)
				{
					continue;
				}

				ClosestAnchor = AnchorPoint.Get();
				ClosestDistanceSquared = DistanceSquared;
			}

			return ClosestAnchor;
		}

		AAnchorPoint* FindClosestUnreachableAnchor(
			const TArray<TObjectPtr<AAnchorPoint>>& KeptAnchors,
			const TSet<FGuid>& ReachableAnchorKeys)
		{
			AAnchorPoint* ClosestUnreachableAnchor = nullptr;
			AAnchorPoint* ClosestReachableAnchor = nullptr;
			float ClosestDistanceSquared = TNumericLimits<float>::Max();
			for (const TObjectPtr<AAnchorPoint>& UnreachableAnchor : KeptAnchors)
			{
				if (not IsValid(UnreachableAnchor)
					|| ReachableAnchorKeys.Contains(UnreachableAnchor->GetAnchorKey()))
				{
					continue;
				}

				AAnchorPoint* ReachableAnchor = FindClosestReachableAnchor(
					KeptAnchors,
					ReachableAnchorKeys,
					UnreachableAnchor.Get());
				if (not IsValid(ReachableAnchor))
				{
					continue;
				}

				const float DistanceSquared = FVector::DistSquared(
					ReachableAnchor->GetActorLocation(),
					UnreachableAnchor->GetActorLocation());
				if (DistanceSquared >= ClosestDistanceSquared)
				{
					continue;
				}

				ClosestReachableAnchor = ReachableAnchor;
				ClosestUnreachableAnchor = UnreachableAnchor.Get();
				ClosestDistanceSquared = DistanceSquared;
			}

			return IsValid(ClosestReachableAnchor) ? ClosestUnreachableAnchor : nullptr;
		}

		void RepairKeptAnchorReachability(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			const int32 MaxRepairAttempts = Context.CachedAnchors->Num();
			for (int32 RepairAttempt = 0; RepairAttempt < MaxRepairAttempts; RepairAttempt++)
			{
				RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
				const TSet<FGuid> ReachableAnchorKeys = BuildReachableKeptAnchorKeys(Context.PlayerHQAnchor);
				if (GetAreAllKeptAnchorsReachable(*Context.CachedAnchors, ReachableAnchorKeys))
				{
					return;
				}

				AAnchorPoint* UnreachableAnchor = FindClosestUnreachableAnchor(*Context.CachedAnchors, ReachableAnchorKeys);
				if (not IsValid(UnreachableAnchor))
				{
					RTSFunctionLibrary::ReportError(
						TEXT("World campaign pruning failed to find an unreachable anchor for repair."));
					return;
				}

				AAnchorPoint* ReachableAnchor = FindClosestReachableAnchor(
					*Context.CachedAnchors,
					ReachableAnchorKeys,
					UnreachableAnchor);
				if (not IsValid(ReachableAnchor))
				{
					RTSFunctionLibrary::ReportError(
						TEXT("World campaign pruning failed to find a reachable anchor for repair."));
					return;
				}

				SpawnConnectionBetweenAnchors(Context, ReachableAnchor, UnreachableAnchor, InOutResult);
			}

			RTSFunctionLibrary::ReportError(
				TEXT("World campaign pruning stopped before all kept anchors became reachable."));
		}

		void CompactInvalidGraphEntries(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			const int32 AnchorCountBefore = Context.CachedAnchors->Num();
			Context.CachedAnchors->RemoveAll([](const TObjectPtr<AAnchorPoint>& AnchorPoint)
			{
				return not IsValid(AnchorPoint);
			});

			const int32 CachedConnectionCountBefore = Context.CachedConnections->Num();
			Context.CachedConnections->RemoveAll([](const TObjectPtr<AConnection>& Connection)
			{
				return not IsValid(Connection);
			});

			const int32 GeneratedConnectionCountBefore = Context.GeneratedConnections->Num();
			Context.GeneratedConnections->RemoveAll([](const TObjectPtr<AConnection>& Connection)
			{
				return not IsValid(Connection);
			});

			if (AnchorCountBefore != Context.CachedAnchors->Num()
				|| CachedConnectionCountBefore != Context.CachedConnections->Num()
				|| GeneratedConnectionCountBefore != Context.GeneratedConnections->Num())
			{
				InOutResult.bDidChange = true;
			}
		}
	}

	void RebuildAnchorConnectionReferences(
		const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
		const TArray<TObjectPtr<AConnection>>& CachedConnections)
	{
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AnchorPoint->ClearConnections();
		}

		const TSet<FGuid> CachedAnchorKeys = BuildAnchorKeySet(CachedAnchors);
		for (const TObjectPtr<AConnection>& Connection : CachedConnections)
		{
			const TArray<TObjectPtr<AAnchorPoint>> ConnectedAnchors =
				GetValidConnectedAnchors(Connection.Get(), CachedAnchorKeys);
			if (ConnectedAnchors.Num() < MinConnectionAnchorCount)
			{
				continue;
			}

			for (int32 AnchorIndex = 0; AnchorIndex < ConnectedAnchors.Num(); AnchorIndex++)
			{
				for (int32 OtherAnchorIndex = AnchorIndex + 1;
				     OtherAnchorIndex < ConnectedAnchors.Num();
				     OtherAnchorIndex++)
				{
					RegisterConnectionPair(
						Connection.Get(),
						ConnectedAnchors[AnchorIndex].Get(),
						ConnectedAnchors[OtherAnchorIndex].Get());
				}
			}
		}

		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AnchorPoint->SortNeighborsByKey();
		}
	}

	FWorldCampaignPruningResult PruneUnusedAnchorsAndRepairConnectivity(
		const FWorldCampaignPruningContext& Context)
	{
		FWorldCampaignPruningResult Result;
		if (not GetIsValidContext(Context))
		{
			return Result;
		}

		CompactInvalidGraphEntries(Context, Result);
		FPruningGraph Graph = BuildPruningGraph(Context);
		BuildContractedEdges(Graph, Context.PlayerHQAnchor);

		DestroyOldConnections(Context, Result);
		DestroyEmptyAnchors(Context, Graph, Result);
		DestroyGeneratedEmptyAnchorsOutsideGraph(Context, Graph, Result);
		SpawnContractedConnections(Context, Graph, Result);
		RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
		RepairKeptAnchorReachability(Context, Result);
		DestroyStaleOwnedConnectionsOutsideGraph(Context, Result);
		CompactInvalidGraphEntries(Context, Result);
		RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
		return Result;
	}
}
