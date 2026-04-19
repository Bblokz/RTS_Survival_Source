#include "RadixiteGrowthComponent.h"

#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Components/AudioComponent.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NavMesh/NavMeshPath.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSPathFindingHelpers/FRTSPathFindingHelpers.h"

namespace RadixiteGrowthConstants
{
	constexpr float DecalSpawnPitch = -90.f;
	constexpr float DecalSpawnYaw = 0.f;
	constexpr float DecalSpawnRoll = 0.f;
	constexpr float MinProjectionDistance = 1.f;
	constexpr float DecalThickness = 16.f;
}

URadixiteGrowthComponent::URadixiteGrowthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URadixiteGrowthComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitCachedSystems();
	if (not GetIsValidOwnerActor() || not GetIsValidNavigationSystem())
	{
		return;
	}

	BeginPlay_InitRootGrowthNode();
	BeginPlay_InitGrowthTimers();
	BeginPlay_InitOwnerDestructionCallback();
}

void URadixiteGrowthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleDecalGrowth);
		World->GetTimerManager().ClearTimer(M_TimerHandleNodeGrowth);
		World->GetTimerManager().ClearTimer(M_TimerHandleOwnerDestruction);
	}

	for (TPair<TObjectPtr<ADecalActor>, FTimerHandle>& ActiveShrinkTimer : M_ActiveDecalShrinkTimers)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ActiveShrinkTimer.Value);
		}
	}
	M_ActiveDecalShrinkTimers.Empty();

	for (TPair<TObjectPtr<AActor>, FTimerHandle>& ActiveNodeSpawnAnimationTimer : M_ActiveNodeSpawnAnimationTimers)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ActiveNodeSpawnAnimationTimer.Value);
		}
	}
	M_ActiveNodeSpawnAnimationTimers.Empty();

	TryDestroyRuntimeFxCache(M_DecalCreationFxCacheRuntime);
	TryDestroyRuntimeFxCache(M_DecalDestructionFxCacheRuntime);
	TryDestroyRuntimeFxCache(M_NodeCreationFxCacheRuntime);
	TryDestroyRuntimeFxCache(M_NodeDestructionFxCacheRuntime);

	Super::EndPlay(EndPlayReason);
}

void URadixiteGrowthComponent::BeginPlay_InitCachedSystems()
{
	M_OwnerActor = GetOwner();
	if (UWorld* World = GetWorld())
	{
		M_NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	}
}

void URadixiteGrowthComponent::BeginPlay_InitRootGrowthNode()
{
	FRadixiteGrowthNodeRecord RootNode;
	RootNode.NodeId = GetNextNodeId();
	RootNode.bM_IsRootNode = true;
	RootNode.Location = M_OwnerActor->GetActorLocation();
	M_LastKnownOwnerLocation = RootNode.Location;
	RootNode.SpawnedNodeActor = M_OwnerActor;
	M_GrowthNodes.Add(RootNode);
}

void URadixiteGrowthComponent::BeginPlay_InitGrowthTimers()
{
	if (not GetIsValidWorld())
	{
		return;
	}

	const TWeakObjectPtr<URadixiteGrowthComponent> WeakThis(this);

	GetWorld()->GetTimerManager().SetTimer(
		M_TimerHandleDecalGrowth,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->TickDecalGrowth();
		}),
		M_IntervalDecalGrowth,
		true);

	GetWorld()->GetTimerManager().SetTimer(
		M_TimerHandleNodeGrowth,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->TickNodeGrowth();
		}),
		M_IntervalNodeGrowth,
		true);
}

void URadixiteGrowthComponent::BeginPlay_InitOwnerDestructionCallback()
{
	if (not GetIsValidOwnerActor())
	{
		return;
	}

	M_OwnerActor->OnDestroyed.AddUniqueDynamic(this, &URadixiteGrowthComponent::HandleOwnerDestroyed);
}

void URadixiteGrowthComponent::TickDecalGrowth()
{
	CleanupInvalidReferences();
	TrySpawnDecalBranch();
}

void URadixiteGrowthComponent::TickNodeGrowth()
{
	CleanupInvalidReferences();
	TrySpawnGrowthNodeFromPendingBranch();
}

void URadixiteGrowthComponent::TickOwnerDestructionSequence()
{
	FRadixiteGrowthBranchRecord BranchToDestroy;
	if (not TryPopNextDestructionBranch(BranchToDestroy))
	{
		DestroyRemainingGrowthNodesForOwnerDestruction();
		StopOwnerDestructionSequence();
		return;
	}

	for (TObjectPtr<ADecalActor> SpawnedDecal : BranchToDestroy.SpawnedDecals)
	{
		if (not IsValid(SpawnedDecal))
		{
			continue;
		}

		StartDecalShrinkAndDestroy(SpawnedDecal);
	}

	const int32 EndNodeId = BranchToDestroy.EndNodeId;
	if (EndNodeId == INDEX_NONE)
	{
		return;
	}

	const int32 EndNodeIndex = M_GrowthNodes.IndexOfByPredicate([EndNodeId](const FRadixiteGrowthNodeRecord& NodeRecord)
	{
		return NodeRecord.NodeId == EndNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(EndNodeIndex))
	{
		return;
	}

	AActor* NodeActorToDestroy = M_GrowthNodes[EndNodeIndex].SpawnedNodeActor.Get();
	if (not IsValid(NodeActorToDestroy))
	{
		return;
	}

	HandleNodeDestructionOverTime(NodeActorToDestroy);
}

void URadixiteGrowthComponent::CleanupInvalidReferences()
{
	for (FRadixiteGrowthBranchRecord& BranchRecord : M_DecalBranches)
	{
		CleanupInvalidBranchDecals(BranchRecord);
	}

	RemoveBranchesThatNoLongerHaveDecals();

	for (int32 NodeIndex = M_GrowthNodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		const FRadixiteGrowthNodeRecord& NodeRecord = M_GrowthNodes[NodeIndex];
		if (NodeRecord.bM_IsRootNode)
		{
			continue;
		}

		if (NodeRecord.SpawnedNodeActor.IsValid())
		{
			continue;
		}

		const int32 DestroyedNodeId = NodeRecord.NodeId;
		RemoveBranchesForNodeDestruction(DestroyedNodeId, NodeRecord.ConnectedNodeIds.Num());
		RemoveNodeConnections(DestroyedNodeId);
		M_GrowthNodes.RemoveAt(NodeIndex);
	}
}

void URadixiteGrowthComponent::CleanupInvalidBranchDecals(FRadixiteGrowthBranchRecord& BranchRecord) const
{
	for (int32 DecalIndex = BranchRecord.SpawnedDecals.Num() - 1; DecalIndex >= 0; --DecalIndex)
	{
		if (IsValid(BranchRecord.SpawnedDecals[DecalIndex]))
		{
			continue;
		}

		BranchRecord.SpawnedDecals.RemoveAt(DecalIndex);
	}
}

bool URadixiteGrowthComponent::GetHasAtLeastOneValidDecal(const FRadixiteGrowthBranchRecord& BranchRecord) const
{
	for (const TObjectPtr<ADecalActor> SpawnedDecal : BranchRecord.SpawnedDecals)
	{
		if (IsValid(SpawnedDecal))
		{
			return true;
		}
	}

	return false;
}

bool URadixiteGrowthComponent::TrySpawnDecalBranch()
{
	if (GetCurrentSpawnedDecalCount() >= M_MaxDecalGrowth)
	{
		return false;
	}

	TArray<int32> UnsaturatedNodeIds;
	if (not TryGetUnsaturatedNodeIds(UnsaturatedNodeIds))
	{
		return false;
	}

	for (int32 AttemptIndex = 0; AttemptIndex < M_MaxRetries; ++AttemptIndex)
	{
		const int32 RandomNodeIndex = FMath::RandRange(0, UnsaturatedNodeIds.Num() - 1);
		if (TrySpawnDecalBranchForNode(UnsaturatedNodeIds[RandomNodeIndex]))
		{
			return true;
		}
	}

	return false;
}

bool URadixiteGrowthComponent::TrySpawnDecalBranchForNode(const int32 StartNodeId)
{
	const int32 NodeIndex = M_GrowthNodes.IndexOfByPredicate([StartNodeId](const FRadixiteGrowthNodeRecord& NodeRecord)
	{
		return NodeRecord.NodeId == StartNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	FRadixiteGrowthNodeRecord& StartNodeRecord = M_GrowthNodes[NodeIndex];
	float InitialYaw = 0.f;
	int32 InitialYawBucket = INDEX_NONE;
	if (not TryChooseInitialYaw(StartNodeRecord, InitialYaw, InitialYawBucket))
	{
		return false;
	}

	if (M_GrowthDecalOptions.Num() == 0)
	{
		return false;
	}

	const int32 BranchLength = FMath::RandRange(M_MinDecalGrowthsUntilNode, M_MaxDecalGrowthsUntilNode);
	const float SegmentDistance = FMath::FRandRange(M_MinDecalExpansionDistance, M_MaxDecalExpansionDistance);
	const float DirectionSign = FMath::RandBool() ? 1.f : -1.f;

	TArray<FVector> BranchPoints;
	if (not TryBuildBranchPoints(StartNodeRecord.Location, BranchLength, InitialYaw, DirectionSign, SegmentDistance, BranchPoints))
	{
		return false;
	}

	TArray<TObjectPtr<ADecalActor>> SpawnedDecals;
	SpawnedDecals.Reserve(BranchPoints.Num() - 1);

	float SegmentYaw = InitialYaw;
	for (int32 PointIndex = 0; PointIndex < BranchPoints.Num() - 1; ++PointIndex)
	{
		TObjectPtr<ADecalActor> SpawnedDecal = SpawnGrowthDecal(
			BranchPoints[PointIndex],
			BranchPoints[PointIndex + 1],
			SegmentDistance,
			SegmentYaw);
		if (not IsValid(SpawnedDecal))
		{
			for (const TObjectPtr<ADecalActor> SpawnedDecalActor : SpawnedDecals)
			{
				if (not IsValid(SpawnedDecalActor))
				{
					continue;
				}

				SpawnedDecalActor->Destroy();
			}
			return false;
		}

		SpawnedDecals.Add(SpawnedDecal);
		PlayCreationFxAtLocation(SpawnedDecal->GetActorLocation(), false);
		SegmentYaw += DirectionSign * M_DecalRotationOffset;
	}

	RegisterSpawnedBranch(StartNodeId, InitialYaw, InitialYawBucket, BranchPoints.Last(), SpawnedDecals);
	StartNodeRecord.UsedInitialYawOffsets.Add(InitialYawBucket);
	return true;
}

bool URadixiteGrowthComponent::TryBuildBranchPoints(
	const FVector& StartLocation,
	const int32 BranchLength,
	const float InitialYaw,
	const float DirectionSign,
	const float SegmentDistance,
	TArray<FVector>& OutBranchPoints) const
{
	OutBranchPoints.Reset();
	OutBranchPoints.Add(StartLocation);

	float WorkingYaw = InitialYaw;
	FVector CurrentLocation = StartLocation;
	for (int32 SegmentIndex = 0; SegmentIndex < BranchLength; ++SegmentIndex)
	{
		const FVector DirectionVector = FRotator(0.f, WorkingYaw, 0.f).Vector();
		const FVector DesiredPoint = CurrentLocation + (DirectionVector * SegmentDistance);

		FVector ValidatedPoint;
		if (not TryGetValidGrowthPoint(DesiredPoint, ValidatedPoint))
		{
			return false;
		}

		OutBranchPoints.Add(ValidatedPoint);
		CurrentLocation = ValidatedPoint;
		WorkingYaw += DirectionSign * M_DecalRotationOffset;
	}

	return OutBranchPoints.Num() > 1;
}

bool URadixiteGrowthComponent::TryGetValidGrowthPoint(const FVector& DesiredPoint, FVector& OutValidatedPoint) const
{
	if (not GetIsValidWorld() || not GetIsValidNavigationSystem())
	{
		return false;
	}

	const FVector VerticalTraceOffset(0.f, 0.f, M_ZLandscapeTraceOffset * 0.5f);
	const FVector TraceStart = DesiredPoint + VerticalTraceOffset;
	const FVector TraceEnd = DesiredPoint - VerticalTraceOffset;

	FHitResult HitResult;
	FCollisionQueryParams QueryParameters;
	QueryParameters.bTraceComplex = false;
	if (not GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParameters))
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	if (not M_NavigationSystem->ProjectPointToNavigation(HitResult.ImpactPoint, ProjectedLocation, M_ProjectionToNavMeshExtent))
	{
		return false;
	}

	FPathFindingQuery PathFindingQuery;
	if (not TryBuildHighCostQuery(PathFindingQuery, ProjectedLocation.Location))
	{
		return false;
	}

	if (FRTSPathFindingHelpers::IsLocationInHighCostArea(GetWorld(), ProjectedLocation.Location, PathFindingQuery))
	{
		return false;
	}

	OutValidatedPoint = ProjectedLocation.Location;
	return true;
}

bool URadixiteGrowthComponent::TryBuildHighCostQuery(FPathFindingQuery& OutPathFindingQuery, const FVector& QueryLocation) const
{
	if (not GetIsValidOwnerActor() || not GetIsValidNavigationSystem())
	{
		return false;
	}

	ANavigationData* DefaultNavigationData = M_NavigationSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (not IsValid(DefaultNavigationData))
	{
		return false;
	}

	OutPathFindingQuery = FPathFindingQuery(M_OwnerActor.Get(), *DefaultNavigationData, QueryLocation, QueryLocation);
	return true;
}

bool URadixiteGrowthComponent::TrySpawnGrowthNodeFromPendingBranch()
{
	if (GetCurrentSpawnedNodeCount() >= M_GrowthNodeOptions.MaxSpawnedNodes)
	{
		return false;
	}

	for (int32 AttemptIndex = 0; AttemptIndex < M_MaxRetries; ++AttemptIndex)
	{
		TArray<int32> PendingBranchIndices;
		for (int32 BranchIndex = 0; BranchIndex < M_DecalBranches.Num(); ++BranchIndex)
		{
			if (M_DecalBranches[BranchIndex].bM_HasPendingGrowthNode)
			{
				PendingBranchIndices.Add(BranchIndex);
			}
		}
		if (PendingBranchIndices.Num() == 0)
		{
			return false;
		}

		const int32 RandomPendingIndex = FMath::RandRange(0, PendingBranchIndices.Num() - 1);
		if (TrySpawnGrowthNodeForBranch(M_DecalBranches[PendingBranchIndices[RandomPendingIndex]]))
		{
			return true;
		}
	}

	return false;
}

bool URadixiteGrowthComponent::TrySpawnGrowthNodeForBranch(FRadixiteGrowthBranchRecord& BranchToGrow)
{
	if (not BranchToGrow.bM_HasPendingGrowthNode)
	{
		return false;
	}

	const TSubclassOf<AActor> GrowthNodeClass = GetRandomGrowthNodeClass();
	if (not IsValid(GrowthNodeClass.Get()))
	{
		return false;
	}

	if (not GetIsValidWorld())
	{
		return false;
	}

	const float RandomYaw = FMath::FRandRange(M_GrowthNodeOptions.MinRotation, M_GrowthNodeOptions.MaxRotation);
	const FVector SpawnStartLocation = BranchToGrow.PendingNodeLocation - FVector(0.f, 0.f, M_NodeSpawnStartZOffset);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = M_OwnerActor.Get();
	AActor* SpawnedNodeActor = GetWorld()->SpawnActor<AActor>(
		GrowthNodeClass,
		SpawnStartLocation,
		FRotator(0.f, RandomYaw, 0.f),
		SpawnParameters);
	if (not IsValid(SpawnedNodeActor))
	{
		return false;
	}

	SpawnedNodeActor->OnDestroyed.AddUniqueDynamic(this, &URadixiteGrowthComponent::HandleSpawnedGrowthNodeDestroyed);
	StartNodeSpawnVerticalAnimation(SpawnedNodeActor, BranchToGrow.PendingNodeLocation);
	PlayCreationFxAtLocation(SpawnedNodeActor->GetActorLocation(), true);
	RegisterSpawnedGrowthNode(BranchToGrow, SpawnedNodeActor, BranchToGrow.PendingNodeLocation);
	return true;
}

void URadixiteGrowthComponent::RegisterSpawnedBranch(
	const int32 StartNodeId,
	const float InitialYaw,
	const int32 InitialYawBucket,
	const FVector& PendingNodeLocation,
	const TArray<TObjectPtr<ADecalActor>>& SpawnedDecals)
{
	FRadixiteGrowthBranchRecord NewBranch;
	NewBranch.BranchId = GetNextBranchId();
	NewBranch.StartNodeId = StartNodeId;
	NewBranch.InitialYaw = InitialYaw;
	NewBranch.InitialYawBucket = InitialYawBucket;
	NewBranch.PendingNodeLocation = PendingNodeLocation;
	NewBranch.bM_HasPendingGrowthNode = true;
	NewBranch.SpawnedDecals = SpawnedDecals;
	M_DecalBranches.Add(NewBranch);
}

void URadixiteGrowthComponent::RegisterSpawnedGrowthNode(
	FRadixiteGrowthBranchRecord& BranchToGrow,
	AActor* SpawnedNodeActor,
	const FVector& SpawnLocation)
{
	FRadixiteGrowthNodeRecord NewNodeRecord;
	NewNodeRecord.NodeId = GetNextNodeId();
	NewNodeRecord.Location = SpawnLocation;
	NewNodeRecord.SpawnedNodeActor = SpawnedNodeActor;
	NewNodeRecord.ConnectedNodeIds.Add(BranchToGrow.StartNodeId);

	const int32 ParentNodeIndex = M_GrowthNodes.IndexOfByPredicate([&BranchToGrow](const FRadixiteGrowthNodeRecord& NodeRecord)
	{
		return NodeRecord.NodeId == BranchToGrow.StartNodeId;
	});
	if (M_GrowthNodes.IsValidIndex(ParentNodeIndex))
	{
		M_GrowthNodes[ParentNodeIndex].ConnectedNodeIds.Add(NewNodeRecord.NodeId);
	}

	BranchToGrow.EndNodeId = NewNodeRecord.NodeId;
	BranchToGrow.bM_HasPendingGrowthNode = false;
	M_GrowthNodes.Add(NewNodeRecord);
}

void URadixiteGrowthComponent::DestroyBranchDecals(FRadixiteGrowthBranchRecord& BranchRecord) const
{
	for (const TObjectPtr<ADecalActor> SpawnedDecal : BranchRecord.SpawnedDecals)
	{
		if (not IsValid(SpawnedDecal))
		{
			continue;
		}

		SpawnedDecal->Destroy();
	}
}

void URadixiteGrowthComponent::RemoveBranchesForNodeDestruction(const int32 DestroyedNodeId, const int32 ConnectionsCount)
{
	for (int32 BranchIndex = M_DecalBranches.Num() - 1; BranchIndex >= 0; --BranchIndex)
	{
		const FRadixiteGrowthBranchRecord& BranchRecord = M_DecalBranches[BranchIndex];
		const bool bBranchTouchesDestroyedNode =
			BranchRecord.StartNodeId == DestroyedNodeId || BranchRecord.EndNodeId == DestroyedNodeId;
		if (ConnectionsCount <= 1 && bBranchTouchesDestroyedNode)
		{
			RemoveBranchByIndex(BranchIndex);
			continue;
		}

		const bool bStartedAtDestroyedNode = BranchRecord.StartNodeId == DestroyedNodeId;
		if (ConnectionsCount > 1 && bStartedAtDestroyedNode && BranchRecord.bM_HasPendingGrowthNode)
		{
			RemoveBranchByIndex(BranchIndex);
		}
	}
}

void URadixiteGrowthComponent::RemoveBranchesThatNoLongerHaveDecals()
{
	for (int32 BranchIndex = M_DecalBranches.Num() - 1; BranchIndex >= 0; --BranchIndex)
	{
		if (GetHasAtLeastOneValidDecal(M_DecalBranches[BranchIndex]))
		{
			continue;
		}

		RemoveBranchByIndex(BranchIndex);
	}
}

void URadixiteGrowthComponent::RemoveBranchByIndex(const int32 BranchIndex)
{
	if (not M_DecalBranches.IsValidIndex(BranchIndex))
	{
		return;
	}

	FRadixiteGrowthBranchRecord& BranchRecord = M_DecalBranches[BranchIndex];
	ReleaseInitialYawBucket(BranchRecord);
	DestroyBranchDecals(BranchRecord);
	M_DecalBranches.RemoveAt(BranchIndex);
}

void URadixiteGrowthComponent::DestroyAllGrowthContentImmediately()
{
	for (int32 BranchIndex = M_DecalBranches.Num() - 1; BranchIndex >= 0; --BranchIndex)
	{
		RemoveBranchByIndex(BranchIndex);
	}

	for (FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		if (NodeRecord.bM_IsRootNode)
		{
			continue;
		}

		AActor* NodeActor = NodeRecord.SpawnedNodeActor.Get();
		if (not IsValid(NodeActor))
		{
			continue;
		}

		NodeActor->Destroy();
	}
}

void URadixiteGrowthComponent::StartOwnerDestructionSequence()
{
	if (bM_IsOwnerDestructionSequenceActive)
	{
		return;
	}

	if (not GetIsValidWorld())
	{
		DestroyAllGrowthContentImmediately();
		return;
	}

	bM_IsOwnerDestructionSequenceActive = true;
	QueueDestructionBranchesInDistanceOrder();
	if (M_OwnerDestructionBranchQueue.Num() == 0)
	{
		StopOwnerDestructionSequence();
		return;
	}

	const TWeakObjectPtr<URadixiteGrowthComponent> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(
		M_TimerHandleOwnerDestruction,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->TickOwnerDestructionSequence();
		}),
		M_IntervalDestruction,
		true);
}

void URadixiteGrowthComponent::StopOwnerDestructionSequence()
{
	bM_IsOwnerDestructionSequenceActive = false;
	M_OwnerDestructionBranchQueue.Empty();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleOwnerDestruction);
	}

	if (M_ActiveDecalShrinkTimers.Num() > 0)
	{
		return;
	}

	TryDestroyRuntimeFxCache(M_DecalDestructionFxCacheRuntime);
	TryDestroyRuntimeFxCache(M_NodeDestructionFxCacheRuntime);
}

bool URadixiteGrowthComponent::TryPopNextDestructionBranch(FRadixiteGrowthBranchRecord& OutBranchRecord)
{
	if (M_OwnerDestructionBranchQueue.Num() == 0)
	{
		return false;
	}

	const int32 BranchIndex = M_OwnerDestructionBranchQueue[0];
	M_OwnerDestructionBranchQueue.RemoveAt(0);
	if (not M_DecalBranches.IsValidIndex(BranchIndex))
	{
		return false;
	}

	OutBranchRecord = M_DecalBranches[BranchIndex];
	ReleaseInitialYawBucket(M_DecalBranches[BranchIndex]);
	M_DecalBranches.RemoveAt(BranchIndex);

	for (int32 QueueIndex = 0; QueueIndex < M_OwnerDestructionBranchQueue.Num(); ++QueueIndex)
	{
		if (M_OwnerDestructionBranchQueue[QueueIndex] > BranchIndex)
		{
			--M_OwnerDestructionBranchQueue[QueueIndex];
		}
	}

	return true;
}

void URadixiteGrowthComponent::QueueDestructionBranchesInDistanceOrder()
{
	M_OwnerDestructionBranchQueue.Reset();
	const FVector OwnerLocation = M_OwnerActor.IsValid() ? M_OwnerActor->GetActorLocation() : M_LastKnownOwnerLocation;
	TArray<TPair<int32, float>> QueueWithDistances;
	QueueWithDistances.Reserve(M_DecalBranches.Num());
	for (int32 BranchIndex = 0; BranchIndex < M_DecalBranches.Num(); ++BranchIndex)
	{
		const FRadixiteGrowthBranchRecord& BranchRecord = M_DecalBranches[BranchIndex];
		const float BranchDistanceToOwner = FVector::DistSquared(OwnerLocation, BranchRecord.PendingNodeLocation);
		QueueWithDistances.Add(TPair<int32, float>(BranchIndex, BranchDistanceToOwner));
	}

	QueueWithDistances.Sort([](const TPair<int32, float>& Left, const TPair<int32, float>& Right)
	{
		return Left.Value < Right.Value;
	});

	for (const TPair<int32, float>& QueueEntry : QueueWithDistances)
	{
		M_OwnerDestructionBranchQueue.Add(QueueEntry.Key);
	}
}

void URadixiteGrowthComponent::DestroyRemainingGrowthNodesForOwnerDestruction()
{
	for (const FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		if (NodeRecord.bM_IsRootNode)
		{
			continue;
		}

		AActor* NodeActorToDestroy = NodeRecord.SpawnedNodeActor.Get();
		if (not IsValid(NodeActorToDestroy))
		{
			continue;
		}

		HandleNodeDestructionOverTime(NodeActorToDestroy);
	}
}

void URadixiteGrowthComponent::StartDecalShrinkAndDestroy(ADecalActor* DecalToDestroy)
{
	if (not IsValid(DecalToDestroy))
	{
		return;
	}

	PlayDestructionFxAtLocation(DecalToDestroy->GetActorLocation(), false);
	const FVector InitialScale = DecalToDestroy->GetActorScale3D();
	const TWeakObjectPtr<ADecalActor> WeakDecal(DecalToDestroy);
	float ElapsedTime = 0.f;

	FTimerHandle ShrinkTimerHandle;
	const TWeakObjectPtr<URadixiteGrowthComponent> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(
		ShrinkTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis, WeakDecal, InitialScale, ElapsedTime]() mutable
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			if (not WeakDecal.IsValid())
			{
				return;
			}

			const float ShrinkTick = 0.03f;
			ElapsedTime += ShrinkTick;
			const float Alpha = FMath::Clamp(ElapsedTime / WeakThis->M_DecalShrinkTime, 0.f, 1.f);
			const FVector NewScale = FMath::Lerp(InitialScale, FVector::ZeroVector, Alpha);
			WeakDecal->SetActorScale3D(NewScale);

			if (Alpha < 1.f)
			{
				return;
			}

			WeakDecal->Destroy();
			if (not WeakThis->M_ActiveDecalShrinkTimers.Contains(WeakDecal.Get()))
			{
				return;
			}

			if (UWorld* World = WeakThis->GetWorld())
			{
				World->GetTimerManager().ClearTimer(WeakThis->M_ActiveDecalShrinkTimers[WeakDecal.Get()]);
			}
			WeakThis->M_ActiveDecalShrinkTimers.Remove(WeakDecal.Get());
			if (WeakThis->M_ActiveDecalShrinkTimers.Num() > 0)
			{
				return;
			}

			if (WeakThis->bM_IsOwnerDestructionSequenceActive)
			{
				return;
			}

			WeakThis->TryDestroyRuntimeFxCache(WeakThis->M_DecalDestructionFxCacheRuntime);
		}),
		0.03f,
		true);

	M_ActiveDecalShrinkTimers.Add(DecalToDestroy, ShrinkTimerHandle);
}

void URadixiteGrowthComponent::HandleNodeDestructionOverTime(AActor* NodeActorToDestroy)
{
	if (not IsValid(NodeActorToDestroy))
	{
		return;
	}

	PlayDestructionFxAtLocation(NodeActorToDestroy->GetActorLocation(), true);
	NodeActorToDestroy->SetLifeSpan(1.f);
}

void URadixiteGrowthComponent::StartNodeSpawnVerticalAnimation(AActor* SpawnedNodeActor, const FVector& FinalSpawnLocation)
{
	if (not IsValid(SpawnedNodeActor))
	{
		return;
	}

	if (M_NodeSpawnVerticalAnimationTime <= 0.f)
	{
		SpawnedNodeActor->SetActorLocation(FinalSpawnLocation);
		return;
	}

	if (not GetIsValidWorld())
	{
		SpawnedNodeActor->SetActorLocation(FinalSpawnLocation);
		return;
	}

	const float AnimationTickInterval = 0.03f;
	const FVector SpawnStartLocation = SpawnedNodeActor->GetActorLocation();
	const TWeakObjectPtr<URadixiteGrowthComponent> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakNodeActor(SpawnedNodeActor);
	float ElapsedAnimationTime = 0.f;

	FTimerHandle NodeSpawnAnimationTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		NodeSpawnAnimationTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis, WeakNodeActor, SpawnStartLocation, FinalSpawnLocation, ElapsedAnimationTime, AnimationTickInterval]() mutable
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			if (not WeakNodeActor.IsValid())
			{
				return;
			}

			ElapsedAnimationTime += AnimationTickInterval;
			const float AnimationAlpha = FMath::Clamp(
				ElapsedAnimationTime / WeakThis->M_NodeSpawnVerticalAnimationTime,
				0.f,
				1.f);
			const FVector CurrentAnimationLocation = FMath::Lerp(SpawnStartLocation, FinalSpawnLocation, AnimationAlpha);
			WeakNodeActor->SetActorLocation(CurrentAnimationLocation);
			if (AnimationAlpha < 1.f)
			{
				return;
			}

			if (not WeakThis->M_ActiveNodeSpawnAnimationTimers.Contains(WeakNodeActor.Get()))
			{
				return;
			}

			if (UWorld* World = WeakThis->GetWorld())
			{
				World->GetTimerManager().ClearTimer(WeakThis->M_ActiveNodeSpawnAnimationTimers[WeakNodeActor.Get()]);
			}
			WeakThis->M_ActiveNodeSpawnAnimationTimers.Remove(WeakNodeActor.Get());
		}),
		AnimationTickInterval,
		true);

	M_ActiveNodeSpawnAnimationTimers.Add(SpawnedNodeActor, NodeSpawnAnimationTimerHandle);
}

void URadixiteGrowthComponent::TryDestroyRuntimeFxCache(FRadixiteGrowthFxCacheRuntime& FxCacheRuntime)
{
	if (IsValid(FxCacheRuntime.AudioComponent))
	{
		FxCacheRuntime.AudioComponent->Stop();
		FxCacheRuntime.AudioComponent->DestroyComponent();
	}

	if (IsValid(FxCacheRuntime.NiagaraComponent))
	{
		FxCacheRuntime.NiagaraComponent->DeactivateImmediate();
		FxCacheRuntime.NiagaraComponent->DestroyComponent();
	}

	FxCacheRuntime.AudioComponent = nullptr;
	FxCacheRuntime.NiagaraComponent = nullptr;
}

void URadixiteGrowthComponent::RemoveNodeConnections(const int32 DestroyedNodeId)
{
	for (FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		NodeRecord.ConnectedNodeIds.Remove(DestroyedNodeId);
	}
}

void URadixiteGrowthComponent::ReleaseInitialYawBucket(const FRadixiteGrowthBranchRecord& BranchRecord)
{
	if (BranchRecord.InitialYawBucket == INDEX_NONE)
	{
		return;
	}

	const int32 SourceNodeIndex = M_GrowthNodes.IndexOfByPredicate([&BranchRecord](const FRadixiteGrowthNodeRecord& NodeRecord)
	{
		return NodeRecord.NodeId == BranchRecord.StartNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(SourceNodeIndex))
	{
		return;
	}

	M_GrowthNodes[SourceNodeIndex].UsedInitialYawOffsets.Remove(BranchRecord.InitialYawBucket);
}

bool URadixiteGrowthComponent::GetIsValidOwnerActor() const
{
	if (M_OwnerActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerActor",
		"GetIsValidOwnerActor",
		this);
	return false;
}

bool URadixiteGrowthComponent::GetIsValidNavigationSystem() const
{
	if (M_NavigationSystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_NavigationSystem",
		"GetIsValidNavigationSystem",
		this);
	return false;
}

bool URadixiteGrowthComponent::GetIsValidWorld() const
{
	if (IsValid(GetWorld()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"World",
		"GetIsValidWorld",
		this);
	return false;
}

int32 URadixiteGrowthComponent::GetCurrentSpawnedDecalCount() const
{
	int32 SpawnedDecalCount = 0;
	for (const FRadixiteGrowthBranchRecord& BranchRecord : M_DecalBranches)
	{
		for (const TObjectPtr<ADecalActor> SpawnedDecal : BranchRecord.SpawnedDecals)
		{
			if (not IsValid(SpawnedDecal))
			{
				continue;
			}

			++SpawnedDecalCount;
		}
	}

	return SpawnedDecalCount;
}

int32 URadixiteGrowthComponent::GetCurrentSpawnedNodeCount() const
{
	int32 SpawnedNodeCount = 0;
	for (const FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		if (NodeRecord.bM_IsRootNode)
		{
			continue;
		}

		if (not NodeRecord.SpawnedNodeActor.IsValid())
		{
			continue;
		}

		++SpawnedNodeCount;
	}

	return SpawnedNodeCount;
}

int32 URadixiteGrowthComponent::GetNextNodeId()
{
	return M_NextNodeId++;
}

int32 URadixiteGrowthComponent::GetNextBranchId()
{
	return M_NextBranchId++;
}

int32 URadixiteGrowthComponent::GetNodeIdByActor(const AActor* NodeActor) const
{
	for (const FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		if (NodeRecord.SpawnedNodeActor.Get() == NodeActor)
		{
			return NodeRecord.NodeId;
		}
	}

	return INDEX_NONE;
}

int32 URadixiteGrowthComponent::GetYawBucketFromYaw(const float YawToBucket) const
{
	const float RotationOffsetAbs = FMath::Abs(M_DecalRotationOffset);
	if (RotationOffsetAbs < RadixiteGrowthConstants::MinProjectionDistance)
	{
		return INDEX_NONE;
	}

	const float NormalizedYaw = FMath::UnwindDegrees(YawToBucket);
	const float PositiveYaw = NormalizedYaw < 0.f ? NormalizedYaw + 360.f : NormalizedYaw;
	const int32 BucketCount = FMath::Max(1, FMath::FloorToInt(360.f / RotationOffsetAbs));
	const int32 Bucket = FMath::FloorToInt(PositiveYaw / RotationOffsetAbs) % BucketCount;
	return Bucket;
}

bool URadixiteGrowthComponent::TryGetUnsaturatedNodeIds(TArray<int32>& OutNodeIds) const
{
	OutNodeIds.Reset();
	for (const FRadixiteGrowthNodeRecord& NodeRecord : M_GrowthNodes)
	{
		if (not NodeRecord.SpawnedNodeActor.IsValid())
		{
			continue;
		}

		int32 ActiveBranchCount = 0;
		for (const FRadixiteGrowthBranchRecord& BranchRecord : M_DecalBranches)
		{
			if (BranchRecord.StartNodeId != NodeRecord.NodeId)
			{
				continue;
			}

			if (GetHasAtLeastOneValidDecal(BranchRecord))
			{
				++ActiveBranchCount;
			}
		}

		if (ActiveBranchCount < M_GrowthNodeOptions.MaxDecalGrowthPerNode)
		{
			OutNodeIds.Add(NodeRecord.NodeId);
		}
	}

	return OutNodeIds.Num() > 0;
}

bool URadixiteGrowthComponent::TryChooseInitialYaw(
	const FRadixiteGrowthNodeRecord& NodeRecord,
	float& OutYaw,
	int32& OutYawBucket) const
{
	if (GetShouldUseDirectionalBias())
	{
		OutYaw = GetBiasedYaw();
		OutYawBucket = GetYawBucketFromYaw(OutYaw);
		if (OutYawBucket == INDEX_NONE)
		{
			return false;
		}

		if (NodeRecord.UsedInitialYawOffsets.Contains(OutYawBucket))
		{
			return false;
		}
		return true;
	}

	const float RandomOffsetYaw = GetRandomOffsetYaw();
	OutYawBucket = GetYawBucketFromYaw(RandomOffsetYaw);
	if (OutYawBucket == INDEX_NONE)
	{
		return false;
	}

	if (NodeRecord.UsedInitialYawOffsets.Contains(OutYawBucket))
	{
		return false;
	}

	OutYaw = RandomOffsetYaw;
	return true;
}

bool URadixiteGrowthComponent::GetShouldUseDirectionalBias() const
{
	if (GetShouldIgnoreDirectionalBias())
	{
		return false;
	}

	const int32 AxisMask =
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosX) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegX) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosY) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegY);
	if ((M_GrowthDirectionBias & AxisMask) == 0)
	{
		return false;
	}

	return FMath::FRandRange(0.f, 1.f) <= M_GrowthDirectionBiasProbability;
}

bool URadixiteGrowthComponent::GetShouldIgnoreDirectionalBias() const
{
	return (M_GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::NoPreference)) != 0;
}

float URadixiteGrowthComponent::GetBiasedYaw() const
{
	TArray<float> YawOptions;
	if ((M_GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosX)) != 0)
	{
		YawOptions.Add(0.f);
	}
	if ((M_GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegX)) != 0)
	{
		YawOptions.Add(180.f);
	}
	if ((M_GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosY)) != 0)
	{
		YawOptions.Add(90.f);
	}
	if ((M_GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegY)) != 0)
	{
		YawOptions.Add(270.f);
	}
	if (YawOptions.Num() == 0)
	{
		return GetRandomOffsetYaw();
	}

	const int32 RandomYawIndex = FMath::RandRange(0, YawOptions.Num() - 1);
	return YawOptions[RandomYawIndex];
}

float URadixiteGrowthComponent::GetRandomOffsetYaw() const
{
	const float RotationOffsetAbs = FMath::Abs(M_DecalRotationOffset);
	const int32 BucketCount = FMath::Max(1, FMath::FloorToInt(360.f / RotationOffsetAbs));
	const int32 RandomBucket = FMath::RandRange(0, BucketCount - 1);
	return static_cast<float>(RandomBucket) * RotationOffsetAbs;
}

TObjectPtr<ADecalActor> URadixiteGrowthComponent::SpawnGrowthDecal(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const float SegmentDistance,
	const float SegmentYaw)
{
	if (not GetIsValidWorld())
	{
		return nullptr;
	}

	if (M_GrowthDecalOptions.Num() == 0)
	{
		return nullptr;
	}

	const int32 RandomDecalOptionIndex = FMath::RandRange(0, M_GrowthDecalOptions.Num() - 1);
	const FGrowthDecalOptions& DecalOptions = M_GrowthDecalOptions[RandomDecalOptionIndex];
	if (not IsValid(DecalOptions.DecalMaterial))
	{
		return nullptr;
	}

	const FVector SpawnLocation = (SegmentStart + SegmentEnd) * 0.5f;
	const float ReachScale = SegmentDistance / FMath::Max(1.f, DecalOptions.DistanceCoveredAtScale1);
	const float HalfReachAtScale1 = DecalOptions.DistanceCoveredAtScale1 * 0.5f;
	const float HalfReachScaled = HalfReachAtScale1 * ReachScale;
	const FRotator DecalProjectionRotation(
		RadixiteGrowthConstants::DecalSpawnPitch,
		RadixiteGrowthConstants::DecalSpawnYaw,
		RadixiteGrowthConstants::DecalSpawnRoll);
	const FRotator GrowthDirectionRotation(0.f, SegmentYaw + DecalOptions.AimAtRotationYaw, 0.f);
	const FQuat SpawnRotationQuat = GrowthDirectionRotation.Quaternion() * DecalProjectionRotation.Quaternion();
	const FRotator SpawnRotation = SpawnRotationQuat.Rotator();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = M_OwnerActor.Get();
	ADecalActor* SpawnedDecalActor = GetWorld()->SpawnActor<ADecalActor>(SpawnLocation, SpawnRotation, SpawnParameters);
	if (not IsValid(SpawnedDecalActor))
	{
		return nullptr;
	}

	SpawnedDecalActor->SetDecalMaterial(DecalOptions.DecalMaterial);
	SpawnedDecalActor->SetActorScale3D(FVector(
		RadixiteGrowthConstants::DecalThickness,
		HalfReachScaled,
		HalfReachScaled));
	SpawnedDecalActor->SetActorScale3D(FVector::OneVector);
	SpawnedDecalActor->SetActorRotation( SpawnedDecalActor->GetActorRotation() + FRotator(-90,0,0));
	return SpawnedDecalActor;
}

void URadixiteGrowthComponent::PlayCreationFxAtLocation(const FVector& EffectLocation, const bool bIsNodeEffect)
{
	if (bIsNodeEffect)
	{
		PlayFxAtLocation(EffectLocation, M_NodeCreationFxSettings, M_NodeCreationFxCacheRuntime);
		return;
	}

	PlayFxAtLocation(EffectLocation, M_DecalCreationFxSettings, M_DecalCreationFxCacheRuntime);
}

void URadixiteGrowthComponent::PlayDestructionFxAtLocation(const FVector& EffectLocation, const bool bIsNodeEffect)
{
	if (bIsNodeEffect)
	{
		PlayFxAtLocation(EffectLocation, M_NodeDestructionFxSettings, M_NodeDestructionFxCacheRuntime);
		return;
	}

	PlayFxAtLocation(EffectLocation, M_DecalDestructionFxSettings, M_DecalDestructionFxCacheRuntime);
}

void URadixiteGrowthComponent::PlayFxAtLocation(
	const FVector& EffectLocation,
	const FGrowthFxSettings& GrowthFxSettings,
	FRadixiteGrowthFxCacheRuntime& FxCacheRuntime)
{
	if (not GetIsValidWorld())
	{
		return;
	}

	UpdateCachedNiagaraFx(EffectLocation, GrowthFxSettings, FxCacheRuntime);
	UpdateCachedAudioFx(EffectLocation, GrowthFxSettings, FxCacheRuntime);
}

void URadixiteGrowthComponent::UpdateCachedNiagaraFx(
	const FVector& EffectLocation,
	const FGrowthFxSettings& GrowthFxSettings,
	FRadixiteGrowthFxCacheRuntime& FxCacheRuntime)
{
	if (not IsValid(GrowthFxSettings.NiagaraSystem))
	{
		return;
	}

	if (not IsValid(FxCacheRuntime.NiagaraComponent))
	{
		FxCacheRuntime.NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			GrowthFxSettings.NiagaraSystem,
			EffectLocation,
			FRotator::ZeroRotator,
			FVector::OneVector,
			false,
			false,
			ENCPoolMethod::None,
			true);
		if (not IsValid(FxCacheRuntime.NiagaraComponent))
		{
			return;
		}
	}
	else
	{
		FxCacheRuntime.NiagaraComponent->SetAsset(GrowthFxSettings.NiagaraSystem);
		FxCacheRuntime.NiagaraComponent->SetWorldLocation(EffectLocation);
		FxCacheRuntime.NiagaraComponent->ResetSystem();
		FxCacheRuntime.NiagaraComponent->Activate(true);
	}
}

void URadixiteGrowthComponent::UpdateCachedAudioFx(
	const FVector& EffectLocation,
	const FGrowthFxSettings& GrowthFxSettings,
	FRadixiteGrowthFxCacheRuntime& FxCacheRuntime)
{
	if (not IsValid(GrowthFxSettings.Sound))
	{
		return;
	}

	if (not IsValid(FxCacheRuntime.AudioComponent))
	{
		FxCacheRuntime.AudioComponent = NewObject<UAudioComponent>(this);
		if (not IsValid(FxCacheRuntime.AudioComponent))
		{
			return;
		}

		FxCacheRuntime.AudioComponent->RegisterComponent();
		FxCacheRuntime.AudioComponent->bAutoActivate = false;
	}

	FxCacheRuntime.AudioComponent->SetWorldLocation(EffectLocation);
	FxCacheRuntime.AudioComponent->SetSound(GrowthFxSettings.Sound);
	FxCacheRuntime.AudioComponent->AttenuationSettings = GrowthFxSettings.SoundAttenuation;
	FxCacheRuntime.AudioComponent->ConcurrencySet.Reset();
	if (IsValid(GrowthFxSettings.SoundConcurrency))
	{
		FxCacheRuntime.AudioComponent->ConcurrencySet.Add(GrowthFxSettings.SoundConcurrency);
	}
	FxCacheRuntime.AudioComponent->Play();
}

TSubclassOf<AActor> URadixiteGrowthComponent::GetRandomGrowthNodeClass() const
{
	if (M_GrowthNodeOptions.GrowthNodeClasses.Num() == 0)
	{
		return nullptr;
	}

	const int32 RandomNodeClassIndex = FMath::RandRange(0, M_GrowthNodeOptions.GrowthNodeClasses.Num() - 1);
	return M_GrowthNodeOptions.GrowthNodeClasses[RandomNodeClassIndex];
}

void URadixiteGrowthComponent::HandleSpawnedGrowthNodeDestroyed(AActor* DestroyedActor)
{
	const int32 DestroyedNodeId = GetNodeIdByActor(DestroyedActor);
	if (DestroyedNodeId == INDEX_NONE)
	{
		return;
	}

	const int32 NodeIndex = M_GrowthNodes.IndexOfByPredicate([DestroyedNodeId](const FRadixiteGrowthNodeRecord& NodeRecord)
	{
		return NodeRecord.NodeId == DestroyedNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(NodeIndex))
	{
		return;
	}

	const int32 ConnectionsCount = M_GrowthNodes[NodeIndex].ConnectedNodeIds.Num();
	RemoveBranchesForNodeDestruction(DestroyedNodeId, ConnectionsCount);
	RemoveNodeConnections(DestroyedNodeId);
	M_GrowthNodes.RemoveAt(NodeIndex);

	if (GetCurrentSpawnedNodeCount() > 0 || bM_IsOwnerDestructionSequenceActive)
	{
		return;
	}

	TryDestroyRuntimeFxCache(M_NodeDestructionFxCacheRuntime);
}

void URadixiteGrowthComponent::HandleOwnerDestroyed(AActor* DestroyedActor)
{
	if (IsValid(DestroyedActor))
	{
		M_LastKnownOwnerLocation = DestroyedActor->GetActorLocation();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleDecalGrowth);
		World->GetTimerManager().ClearTimer(M_TimerHandleNodeGrowth);
	}

	StartOwnerDestructionSequence();
}
