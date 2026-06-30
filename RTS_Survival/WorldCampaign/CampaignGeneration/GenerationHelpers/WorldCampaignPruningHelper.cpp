// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCampaignPruningHelper.h"

#include "Containers/Queue.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"

namespace WorldCampaignPruningHelper
{
	namespace
	{
		constexpr float HalfValue = 0.5f;
		constexpr int32 LeafNeighborCountMax = 1;
		constexpr int32 ChainNeighborCount = 2;
		constexpr int32 MinConnectionAnchorCount = 2;
		constexpr int32 MaxUniqueGuidAttempts = 16;

		const FName GeneratedAnchorTag(TEXT("WC_GeneratedAnchor"));
		const FName RepairAnchorTag(TEXT("WC_PruneRepairAnchor"));

		struct FAnchorPair
		{
			AAnchorPoint* AnchorA = nullptr;
			AAnchorPoint* AnchorB = nullptr;
		};

		struct FClosestRepairPair
		{
			AAnchorPoint* ReachableAnchor = nullptr;
			AAnchorPoint* UnreachableAnchor = nullptr;
			float DistanceSquared = TNumericLimits<float>::Max();

			bool GetIsValid() const
			{
				return IsValid(ReachableAnchor) && IsValid(UnreachableAnchor);
			}
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

		TArray<TObjectPtr<AAnchorPoint>> BuildValidUniqueNeighbors(const AAnchorPoint* AnchorPoint)
		{
			TArray<TObjectPtr<AAnchorPoint>> NeighborAnchors;
			if (not IsValid(AnchorPoint))
			{
				return NeighborAnchors;
			}

			for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : AnchorPoint->GetNeighborAnchors())
			{
				AddUniqueAnchor(NeighborAnchors, NeighborAnchor.Get());
			}

			return NeighborAnchors;
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

		bool GetIsPrunableEmptyAnchor(const AAnchorPoint* AnchorPoint, const TSet<FGuid>& GameplayAnchorKeys)
		{
			if (not IsValid(AnchorPoint))
			{
				return false;
			}

			if (AnchorPoint->Tags.Contains(RepairAnchorTag))
			{
				return false;
			}

			return not GetIsGameplayAnchor(AnchorPoint, GameplayAnchorKeys);
		}

		bool TryFindFirstPrunableLeafAnchor(
			const FWorldCampaignPruningContext& Context,
			AAnchorPoint*& OutAnchorPoint)
		{
			OutAnchorPoint = nullptr;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not GetIsPrunableEmptyAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					continue;
				}

				const TArray<TObjectPtr<AAnchorPoint>> NeighborAnchors = BuildValidUniqueNeighbors(AnchorPoint.Get());
				if (NeighborAnchors.Num() > LeafNeighborCountMax)
				{
					continue;
				}

				OutAnchorPoint = AnchorPoint.Get();
				return true;
			}

			return false;
		}

		bool TryFindFirstPrunableChainAnchor(
			const FWorldCampaignPruningContext& Context,
			AAnchorPoint*& OutAnchorPoint,
			FAnchorPair& OutNeighborPair)
		{
			OutAnchorPoint = nullptr;
			OutNeighborPair = FAnchorPair();
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not GetIsPrunableEmptyAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					continue;
				}

				const TArray<TObjectPtr<AAnchorPoint>> NeighborAnchors = BuildValidUniqueNeighbors(AnchorPoint.Get());
				if (NeighborAnchors.Num() != ChainNeighborCount)
				{
					continue;
				}

				OutAnchorPoint = AnchorPoint.Get();
				OutNeighborPair.AnchorA = NeighborAnchors[0].Get();
				OutNeighborPair.AnchorB = NeighborAnchors[1].Get();
				return IsValid(OutNeighborPair.AnchorA) && IsValid(OutNeighborPair.AnchorB);
			}

			return false;
		}

		bool GetConnectionTouchesAnchorKeys(const AConnection* Connection, const TSet<FGuid>& AnchorKeys)
		{
			if (not IsValid(Connection))
			{
				return false;
			}

			for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : Connection->GetConnectedAnchors())
			{
				if (not IsValid(ConnectedAnchor))
				{
					continue;
				}

				if (AnchorKeys.Contains(ConnectedAnchor->GetAnchorKey()))
				{
					return true;
				}
			}

			return false;
		}

		void AddConnectionsTouchingAnchorKeys(
			const TArray<TObjectPtr<AConnection>>& Connections,
			const TSet<FGuid>& AnchorKeys,
			TArray<AConnection*>& OutConnections)
		{
			for (const TObjectPtr<AConnection>& Connection : Connections)
			{
				if (not GetConnectionTouchesAnchorKeys(Connection.Get(), AnchorKeys))
				{
					continue;
				}

				OutConnections.AddUnique(Connection.Get());
			}
		}

		void RemoveDestroyedConnectionsFromArray(
			TArray<TObjectPtr<AConnection>>& Connections,
			const TArray<AConnection*>& ConnectionsToDestroy)
		{
			Connections.RemoveAll([&ConnectionsToDestroy](const TObjectPtr<AConnection>& Connection)
			{
				return not IsValid(Connection) || ConnectionsToDestroy.Contains(Connection.Get());
			});
		}

		void DestroyConnectionsTouchingAnchorKeys(
			const TSet<FGuid>& AnchorKeys,
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			TArray<AConnection*> ConnectionsToDestroy;
			AddConnectionsTouchingAnchorKeys(*Context.CachedConnections, AnchorKeys, ConnectionsToDestroy);
			AddConnectionsTouchingAnchorKeys(*Context.GeneratedConnections, AnchorKeys, ConnectionsToDestroy);

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

			RemoveDestroyedConnectionsFromArray(*Context.CachedConnections, ConnectionsToDestroy);
			RemoveDestroyedConnectionsFromArray(*Context.GeneratedConnections, ConnectionsToDestroy);
		}

		void RemoveDestroyedAnchorsFromArray(
			TArray<TObjectPtr<AAnchorPoint>>& Anchors,
			const TSet<FGuid>& AnchorKeysToDestroy)
		{
			Anchors.RemoveAll([&AnchorKeysToDestroy](const TObjectPtr<AAnchorPoint>& AnchorPoint)
			{
				if (not IsValid(AnchorPoint))
				{
					return true;
				}

				return AnchorKeysToDestroy.Contains(AnchorPoint->GetAnchorKey());
			});
		}

		void DestroyAnchorsByKey(
			const TSet<FGuid>& AnchorKeysToDestroy,
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				if (not AnchorKeysToDestroy.Contains(AnchorPoint->GetAnchorKey()))
				{
					continue;
				}

				AnchorPoint->RemovePromotedWorldObject();
				AnchorPoint->Destroy();
				InOutResult.DestroyedEmptyAnchors++;
				InOutResult.bDidChange = true;
			}

			RemoveDestroyedAnchorsFromArray(*Context.CachedAnchors, AnchorKeysToDestroy);
		}

		bool GetConnectionContainsBothAnchors(
			const AConnection* Connection,
			const AAnchorPoint* AnchorA,
			const AAnchorPoint* AnchorB)
		{
			if (not IsValid(Connection) || not IsValid(AnchorA) || not IsValid(AnchorB))
			{
				return false;
			}

			bool bContainsAnchorA = false;
			bool bContainsAnchorB = false;
			for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : Connection->GetConnectedAnchors())
			{
				if (ConnectedAnchor.Get() == AnchorA)
				{
					bContainsAnchorA = true;
				}

				if (ConnectedAnchor.Get() == AnchorB)
				{
					bContainsAnchorB = true;
				}
			}

			return bContainsAnchorA && bContainsAnchorB;
		}

		bool GetHasConnectionBetween(
			const AAnchorPoint* AnchorA,
			const AAnchorPoint* AnchorB,
			const TArray<TObjectPtr<AConnection>>& Connections)
		{
			if (not IsValid(AnchorA) || not IsValid(AnchorB) || AnchorA == AnchorB)
			{
				return true;
			}

			for (const TObjectPtr<AConnection>& Connection : Connections)
			{
				if (GetConnectionContainsBothAnchors(Connection.Get(), AnchorA, AnchorB))
				{
					return true;
				}
			}

			return false;
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

		void CreateConnectionIfMissing(
			const FWorldCampaignPruningContext& Context,
			AAnchorPoint* AnchorA,
			AAnchorPoint* AnchorB,
			FWorldCampaignPruningResult& InOutResult)
		{
			if (GetHasConnectionBetween(AnchorA, AnchorB, *Context.CachedConnections))
			{
				return;
			}

			SpawnConnectionBetweenAnchors(Context, AnchorA, AnchorB, InOutResult);
		}

		TSet<FGuid> BuildSingleAnchorKeySet(const AAnchorPoint* AnchorPoint)
		{
			TSet<FGuid> AnchorKeys;
			if (not IsValid(AnchorPoint))
			{
				return AnchorKeys;
			}

			AnchorKeys.Add(AnchorPoint->GetAnchorKey());
			return AnchorKeys;
		}

		bool PruneFirstEmptyLeafAnchor(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			AAnchorPoint* AnchorToPrune = nullptr;
			if (not TryFindFirstPrunableLeafAnchor(Context, AnchorToPrune))
			{
				return false;
			}

			const TSet<FGuid> AnchorKeysToPrune = BuildSingleAnchorKeySet(AnchorToPrune);
			DestroyConnectionsTouchingAnchorKeys(AnchorKeysToPrune, Context, InOutResult);
			DestroyAnchorsByKey(AnchorKeysToPrune, Context, InOutResult);
			RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
			return true;
		}

		bool ContractFirstEmptyChainAnchor(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			AAnchorPoint* AnchorToPrune = nullptr;
			FAnchorPair NeighborPair;
			if (not TryFindFirstPrunableChainAnchor(Context, AnchorToPrune, NeighborPair))
			{
				return false;
			}

			const TSet<FGuid> AnchorKeysToPrune = BuildSingleAnchorKeySet(AnchorToPrune);
			DestroyConnectionsTouchingAnchorKeys(AnchorKeysToPrune, Context, InOutResult);
			DestroyAnchorsByKey(AnchorKeysToPrune, Context, InOutResult);
			CreateConnectionIfMissing(Context, NeighborPair.AnchorA, NeighborPair.AnchorB, InOutResult);
			RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
			return true;
		}

		void BuildComponentFromAnchor(
			AAnchorPoint* StartAnchor,
			TSet<FGuid>& InOutVisitedAnchorKeys,
			TArray<TObjectPtr<AAnchorPoint>>& OutComponent)
		{
			if (not IsValid(StartAnchor))
			{
				return;
			}

			TQueue<AAnchorPoint*> AnchorQueue;
			AnchorQueue.Enqueue(StartAnchor);
			InOutVisitedAnchorKeys.Add(StartAnchor->GetAnchorKey());

			while (not AnchorQueue.IsEmpty())
			{
				AAnchorPoint* CurrentAnchor = nullptr;
				AnchorQueue.Dequeue(CurrentAnchor);
				if (not IsValid(CurrentAnchor))
				{
					continue;
				}

				OutComponent.Add(CurrentAnchor);
				const TArray<TObjectPtr<AAnchorPoint>> NeighborAnchors = BuildValidUniqueNeighbors(CurrentAnchor);
				for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : NeighborAnchors)
				{
					if (not IsValid(NeighborAnchor))
					{
						continue;
					}

					const FGuid NeighborKey = NeighborAnchor->GetAnchorKey();
					if (InOutVisitedAnchorKeys.Contains(NeighborKey))
					{
						continue;
					}

					InOutVisitedAnchorKeys.Add(NeighborKey);
					AnchorQueue.Enqueue(NeighborAnchor.Get());
				}
			}
		}

		bool GetComponentHasGameplayAnchor(
			const TArray<TObjectPtr<AAnchorPoint>>& Component,
			const TSet<FGuid>& GameplayAnchorKeys)
		{
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : Component)
			{
				if (GetIsGameplayAnchor(AnchorPoint.Get(), GameplayAnchorKeys))
				{
					return true;
				}
			}

			return false;
		}

		TSet<FGuid> BuildAnchorKeySetFromComponent(const TArray<TObjectPtr<AAnchorPoint>>& Component)
		{
			TSet<FGuid> AnchorKeys;
			AnchorKeys.Reserve(Component.Num());
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : Component)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				AnchorKeys.Add(AnchorPoint->GetAnchorKey());
			}

			return AnchorKeys;
		}

		bool PruneFirstEmptyComponentWithoutGameplay(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			TSet<FGuid> VisitedAnchorKeys;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
				if (VisitedAnchorKeys.Contains(AnchorKey))
				{
					continue;
				}

				TArray<TObjectPtr<AAnchorPoint>> Component;
				BuildComponentFromAnchor(AnchorPoint.Get(), VisitedAnchorKeys, Component);
				if (Component.Num() == 0 || GetComponentHasGameplayAnchor(Component, Context.GameplayAnchorKeys))
				{
					continue;
				}

				const TSet<FGuid> AnchorKeysToPrune = BuildAnchorKeySetFromComponent(Component);
				DestroyConnectionsTouchingAnchorKeys(AnchorKeysToPrune, Context, InOutResult);
				DestroyAnchorsByKey(AnchorKeysToPrune, Context, InOutResult);
				RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
				return true;
			}

			return false;
		}

		void PruneEmptyAnchors(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			bool bDidPruneAnchor = true;
			while (bDidPruneAnchor)
			{
				bDidPruneAnchor = false;
				if (PruneFirstEmptyLeafAnchor(Context, InOutResult))
				{
					bDidPruneAnchor = true;
					continue;
				}

				if (ContractFirstEmptyChainAnchor(Context, InOutResult))
				{
					bDidPruneAnchor = true;
				}
			}

			bool bDidPruneEmptyComponent = true;
			while (bDidPruneEmptyComponent)
			{
				bDidPruneEmptyComponent = PruneFirstEmptyComponentWithoutGameplay(Context, InOutResult);
			}
		}

		TSet<FGuid> BuildReachableAnchorKeys(const AAnchorPoint* PlayerHQAnchor)
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

				const TArray<TObjectPtr<AAnchorPoint>> NeighborAnchors = BuildValidUniqueNeighbors(CurrentAnchor);
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

		int32 CountGameplayAnchors(const FWorldCampaignPruningContext& Context)
		{
			int32 GameplayAnchorCount = 0;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (GetIsGameplayAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					GameplayAnchorCount++;
				}
			}

			return GameplayAnchorCount;
		}

		bool GetAreAllGameplayAnchorsReachable(
			const FWorldCampaignPruningContext& Context,
			const TSet<FGuid>& ReachableAnchorKeys)
		{
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not GetIsGameplayAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
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

		TArray<TObjectPtr<AAnchorPoint>> BuildReachableGameplayAnchors(
			const FWorldCampaignPruningContext& Context,
			const TSet<FGuid>& ReachableAnchorKeys)
		{
			TArray<TObjectPtr<AAnchorPoint>> ReachableGameplayAnchors;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not GetIsGameplayAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					continue;
				}

				if (ReachableAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
				{
					ReachableGameplayAnchors.Add(AnchorPoint);
				}
			}

			return ReachableGameplayAnchors;
		}

		TArray<TArray<TObjectPtr<AAnchorPoint>>> BuildUnreachableGameplayIslands(
			const FWorldCampaignPruningContext& Context,
			const TSet<FGuid>& ReachableAnchorKeys)
		{
			TArray<TArray<TObjectPtr<AAnchorPoint>>> Islands;
			TSet<FGuid> VisitedAnchorKeys = ReachableAnchorKeys;
			for (const TObjectPtr<AAnchorPoint>& AnchorPoint : *Context.CachedAnchors)
			{
				if (not GetIsGameplayAnchor(AnchorPoint.Get(), Context.GameplayAnchorKeys))
				{
					continue;
				}

				const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
				if (ReachableAnchorKeys.Contains(AnchorKey) || VisitedAnchorKeys.Contains(AnchorKey))
				{
					continue;
				}

				TArray<TObjectPtr<AAnchorPoint>> Component;
				BuildComponentFromAnchor(AnchorPoint.Get(), VisitedAnchorKeys, Component);

				TArray<TObjectPtr<AAnchorPoint>> GameplayIsland;
				for (const TObjectPtr<AAnchorPoint>& ComponentAnchor : Component)
				{
					if (not GetIsGameplayAnchor(ComponentAnchor.Get(), Context.GameplayAnchorKeys))
					{
						continue;
					}

					GameplayIsland.Add(ComponentAnchor);
				}

				if (GameplayIsland.Num() > 0)
				{
					Islands.Add(GameplayIsland);
				}
			}

			return Islands;
		}

		FClosestRepairPair FindClosestRepairPairForIsland(
			const TArray<TObjectPtr<AAnchorPoint>>& ReachableGameplayAnchors,
			const TArray<TObjectPtr<AAnchorPoint>>& GameplayIsland)
		{
			FClosestRepairPair ClosestPair;
			for (const TObjectPtr<AAnchorPoint>& ReachableAnchor : ReachableGameplayAnchors)
			{
				if (not IsValid(ReachableAnchor))
				{
					continue;
				}

				for (const TObjectPtr<AAnchorPoint>& UnreachableAnchor : GameplayIsland)
				{
					if (not IsValid(UnreachableAnchor))
					{
						continue;
					}

					const float DistanceSquared = FVector::DistSquared(
						ReachableAnchor->GetActorLocation(),
						UnreachableAnchor->GetActorLocation());
					if (DistanceSquared >= ClosestPair.DistanceSquared)
					{
						continue;
					}

					ClosestPair.ReachableAnchor = ReachableAnchor.Get();
					ClosestPair.UnreachableAnchor = UnreachableAnchor.Get();
					ClosestPair.DistanceSquared = DistanceSquared;
				}
			}

			return ClosestPair;
		}

		FClosestRepairPair FindClosestRepairPair(
			const FWorldCampaignPruningContext& Context,
			const TSet<FGuid>& ReachableAnchorKeys,
			const TArray<TArray<TObjectPtr<AAnchorPoint>>>& Islands)
		{
			const TArray<TObjectPtr<AAnchorPoint>> ReachableGameplayAnchors =
				BuildReachableGameplayAnchors(Context, ReachableAnchorKeys);

			FClosestRepairPair ClosestPair;
			for (const TArray<TObjectPtr<AAnchorPoint>>& Island : Islands)
			{
				const FClosestRepairPair IslandPair =
					FindClosestRepairPairForIsland(ReachableGameplayAnchors, Island);
				if (not IslandPair.GetIsValid())
				{
					continue;
				}

				if (IslandPair.DistanceSquared >= ClosestPair.DistanceSquared)
				{
					continue;
				}

				ClosestPair = IslandPair;
			}

			return ClosestPair;
		}

		TSubclassOf<AAnchorPoint> GetAnchorClassToSpawn(const FWorldCampaignPruningContext& Context)
		{
			if (not IsValid(Context.AnchorClass.Get()))
			{
				return AAnchorPoint::StaticClass();
			}

			return Context.AnchorClass;
		}

		FGuid BuildUniqueRepairAnchorKey(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors)
		{
			const TSet<FGuid> ExistingAnchorKeys = BuildAnchorKeySet(CachedAnchors);
			for (int32 AttemptIndex = 0; AttemptIndex < MaxUniqueGuidAttempts; AttemptIndex++)
			{
				const FGuid CandidateKey = FGuid::NewGuid();
				if (CandidateKey.IsValid() && not ExistingAnchorKeys.Contains(CandidateKey))
				{
					return CandidateKey;
				}
			}

			return FGuid::NewGuid();
		}

		AAnchorPoint* SpawnRepairAnchor(
			const FWorldCampaignPruningContext& Context,
			const FClosestRepairPair& RepairPair,
			FWorldCampaignPruningResult& InOutResult)
		{
			if (not RepairPair.GetIsValid())
			{
				return nullptr;
			}

			const FVector SpawnLocation =
				(RepairPair.ReachableAnchor->GetActorLocation() + RepairPair.UnreachableAnchor->GetActorLocation())
				* HalfValue;
			const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = Context.Owner;
			AAnchorPoint* SpawnedAnchor = Context.World->SpawnActor<AAnchorPoint>(
				GetAnchorClassToSpawn(Context),
				SpawnTransform,
				SpawnParameters);
			if (not IsValid(SpawnedAnchor))
			{
				RTSFunctionLibrary::ReportError(TEXT("World campaign pruning failed to spawn a repair anchor."));
				return nullptr;
			}

			SpawnedAnchor->SetAnchorKey(BuildUniqueRepairAnchorKey(*Context.CachedAnchors), true);
			SpawnedAnchor->InitializeCampaignSettings(Context.CampaignSettings);
			SpawnedAnchor->Tags.AddUnique(GeneratedAnchorTag);
			SpawnedAnchor->Tags.AddUnique(RepairAnchorTag);
			Context.CachedAnchors->Add(SpawnedAnchor);

			InOutResult.SpawnedRepairAnchors++;
			InOutResult.bDidChange = true;
			return SpawnedAnchor;
		}

		bool RepairClosestUnreachableIsland(
			const FWorldCampaignPruningContext& Context,
			const TSet<FGuid>& ReachableAnchorKeys,
			const TArray<TArray<TObjectPtr<AAnchorPoint>>>& Islands,
			FWorldCampaignPruningResult& InOutResult)
		{
			const FClosestRepairPair RepairPair = FindClosestRepairPair(Context, ReachableAnchorKeys, Islands);
			if (not RepairPair.GetIsValid())
			{
				RTSFunctionLibrary::ReportError(
					TEXT("World campaign pruning failed to find a reachable gameplay anchor for repair."));
				return false;
			}

			AAnchorPoint* RepairAnchor = SpawnRepairAnchor(Context, RepairPair, InOutResult);
			if (not IsValid(RepairAnchor))
			{
				return false;
			}

			AConnection* FirstConnection = SpawnConnectionBetweenAnchors(
				Context,
				RepairPair.ReachableAnchor,
				RepairAnchor,
				InOutResult);
			AConnection* SecondConnection = SpawnConnectionBetweenAnchors(
				Context,
				RepairAnchor,
				RepairPair.UnreachableAnchor,
				InOutResult);
			if (not IsValid(FirstConnection) || not IsValid(SecondConnection))
			{
				return false;
			}

			RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
			return true;
		}

		void RepairGameplayReachability(
			const FWorldCampaignPruningContext& Context,
			FWorldCampaignPruningResult& InOutResult)
		{
			const int32 GameplayAnchorCount = CountGameplayAnchors(Context);
			if (GameplayAnchorCount <= 1)
			{
				return;
			}

			for (int32 RepairIndex = 0; RepairIndex < GameplayAnchorCount; RepairIndex++)
			{
				RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
				const TSet<FGuid> ReachableAnchorKeys = BuildReachableAnchorKeys(Context.PlayerHQAnchor);
				if (GetAreAllGameplayAnchorsReachable(Context, ReachableAnchorKeys))
				{
					return;
				}

				const TArray<TArray<TObjectPtr<AAnchorPoint>>> Islands =
					BuildUnreachableGameplayIslands(Context, ReachableAnchorKeys);
				if (Islands.Num() == 0)
				{
					return;
				}

				if (not RepairClosestUnreachableIsland(Context, ReachableAnchorKeys, Islands, InOutResult))
				{
					return;
				}
			}

			RTSFunctionLibrary::ReportError(
				TEXT("World campaign pruning stopped before all gameplay anchors became reachable."));
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
		RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
		PruneEmptyAnchors(Context, Result);
		RepairGameplayReachability(Context, Result);
		CompactInvalidGraphEntries(Context, Result);
		RebuildAnchorConnectionReferences(*Context.CachedAnchors, *Context.CachedConnections);
		return Result;
	}
}
