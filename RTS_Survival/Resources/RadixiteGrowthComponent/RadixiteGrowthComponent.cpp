#include "RadixiteGrowthComponent.h"

#include "Engine/DecalActor.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSPathFindingHelpers/FRTSPathFindingHelpers.h"

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
}

void URadixiteGrowthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleDecalGrowth);
		World->GetTimerManager().ClearTimer(M_TimerHandleNodeGrowth);
	}

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
	RootNode.Location = M_OwnerActor->GetActorLocation();
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
		IntervalDecalGrowth,
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
		IntervalNodeGrowth,
		true);
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

void URadixiteGrowthComponent::CleanupInvalidReferences()
{
	for (int32 BranchIndex = M_DecalBranches.Num() - 1; BranchIndex >= 0; --BranchIndex)
	{
		FRadixiteGrowthBranchRecord& Branch = M_DecalBranches[BranchIndex];
		for (int32 DecalIndex = Branch.SpawnedDecals.Num() - 1; DecalIndex >= 0; --DecalIndex)
		{
			if (not IsValid(Branch.SpawnedDecals[DecalIndex]))
			{
				Branch.SpawnedDecals.RemoveAt(DecalIndex);
			}
		}
	}

	for (int32 NodeIndex = M_GrowthNodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		if (M_GrowthNodes[NodeIndex].NodeId == 0)
		{
			continue;
		}

		if (M_GrowthNodes[NodeIndex].SpawnedNodeActor.IsValid())
		{
			continue;
		}

		const int32 DestroyedNodeId = M_GrowthNodes[NodeIndex].NodeId;
		RemoveBranchesForNodeDestruction(DestroyedNodeId, M_GrowthNodes[NodeIndex].ConnectedNodeIds.Num());
		RemoveNodeConnections(DestroyedNodeId);
		M_GrowthNodes.RemoveAt(NodeIndex);
	}
}

bool URadixiteGrowthComponent::TrySpawnDecalBranch()
{
	if (GetCurrentSpawnedDecalCount() >= MaxDecalGrowth)
	{
		return false;
	}

	TArray<int32> UnsaturatedNodeIds;
	if (not TryGetUnsaturatedNodeIds(UnsaturatedNodeIds))
	{
		return false;
	}

	for (int32 AttemptIndex = 0; AttemptIndex < MaxRetries; ++AttemptIndex)
	{
		const int32 PickedNodeId = UnsaturatedNodeIds[FMath::RandRange(0, UnsaturatedNodeIds.Num() - 1)];
		if (TrySpawnDecalBranchForNode(PickedNodeId))
		{
			return true;
		}
	}

	return false;
}

bool URadixiteGrowthComponent::TrySpawnDecalBranchForNode(const int32 StartNodeId)
{
	const int32 NodeIndex = M_GrowthNodes.IndexOfByPredicate([StartNodeId](const FRadixiteGrowthNodeRecord& Node)
	{
		return Node.NodeId == StartNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	FRadixiteGrowthNodeRecord& StartNodeRecord = M_GrowthNodes[NodeIndex];
	float InitialYaw = 0.f;
	int32 YawBucket = INDEX_NONE;
	if (not TryChooseInitialYaw(StartNodeRecord, InitialYaw, YawBucket))
	{
		return false;
	}

	if (GrowthDecalOptions.Num() == 0)
	{
		return false;
	}

	const int32 BranchLength = FMath::RandRange(MinDecalGrowthsUntilNode, MaxDecalGrowthsUntilNode);
	const float SegmentDistance = FMath::FRandRange(MinDecalExpansionDistance, MaxDecalExpansionDistance);
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
			for (TObjectPtr<ADecalActor> DecalActor : SpawnedDecals)
			{
				if (IsValid(DecalActor))
				{
					DecalActor->Destroy();
				}
			}
			return false;
		}

		SpawnedDecals.Add(SpawnedDecal);
		if (PointIndex >= 0)
		{
			SegmentYaw += DirectionSign * DecalRotationOffset;
		}
	}

	RegisterSpawnedBranch(StartNodeId, InitialYaw, BranchPoints.Last(), SpawnedDecals);
	StartNodeRecord.UsedInitialYawOffsets.Add(YawBucket);
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

	float YawToUse = InitialYaw;
	FVector CurrentLocation = StartLocation;
	for (int32 SegmentIndex = 0; SegmentIndex < BranchLength; ++SegmentIndex)
	{
		const FVector Direction = FRotator(0.f, YawToUse, 0.f).Vector();
		const FVector DesiredPoint = CurrentLocation + (Direction * SegmentDistance);

		FVector ValidatedPoint;
		if (not TryGetValidGrowthPoint(DesiredPoint, ValidatedPoint))
		{
			return false;
		}

		OutBranchPoints.Add(ValidatedPoint);
		CurrentLocation = ValidatedPoint;
		YawToUse += DirectionSign * DecalRotationOffset;
	}

	return OutBranchPoints.Num() > 1;
}

bool URadixiteGrowthComponent::TryGetValidGrowthPoint(const FVector& DesiredPoint, FVector& OutValidatedPoint) const
{
	if (not GetIsValidWorld() || not GetIsValidNavigationSystem())
	{
		return false;
	}

	const FVector TraceOffset(0.f, 0.f, ZLandscapeTraceOffset * 0.5f);
	const FVector TraceStart = DesiredPoint + TraceOffset;
	const FVector TraceEnd = DesiredPoint - TraceOffset;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;

	if (not GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	if (not M_NavigationSystem->ProjectPointToNavigation(HitResult.ImpactPoint, ProjectedLocation, ProjectionToNavMeshExtent))
	{
		return false;
	}

	const FPathFindingQuery PathFindingQuery;
	if (FRTSPathFindingHelpers::IsLocationInHighCostArea(GetWorld(), ProjectedLocation.Location, PathFindingQuery))
	{
		return false;
	}

	OutValidatedPoint = ProjectedLocation.Location;
	return true;
}

bool URadixiteGrowthComponent::TrySpawnGrowthNodeFromPendingBranch()
{
	if (GetCurrentSpawnedNodeCount() >= GrowthNodeOptions.MaxSpawnedNodes)
	{
		return false;
	}

	for (int32 AttemptIndex = 0; AttemptIndex < MaxRetries; ++AttemptIndex)
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

		const int32 PickedBranchIndex = PendingBranchIndices[FMath::RandRange(0, PendingBranchIndices.Num() - 1)];
		if (TrySpawnGrowthNodeForBranch(M_DecalBranches[PickedBranchIndex]))
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

	const float RandomYaw = FMath::FRandRange(GrowthNodeOptions.MinRotation, GrowthNodeOptions.MaxRotation);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = M_OwnerActor.Get();
	AActor* SpawnedNodeActor = GetWorld()->SpawnActor<AActor>(
		GrowthNodeClass,
		BranchToGrow.PendingNodeLocation,
		FRotator(0.f, RandomYaw, 0.f),
		SpawnParams);
	if (not IsValid(SpawnedNodeActor))
	{
		return false;
	}

	SpawnedNodeActor->OnDestroyed.AddUniqueDynamic(this, &URadixiteGrowthComponent::HandleSpawnedGrowthNodeDestroyed);
	RegisterSpawnedGrowthNode(BranchToGrow, SpawnedNodeActor, BranchToGrow.PendingNodeLocation);
	return true;
}

void URadixiteGrowthComponent::RegisterSpawnedBranch(
	const int32 StartNodeId,
	const float InitialYaw,
	const FVector& PendingNodeLocation,
	const TArray<TObjectPtr<ADecalActor>>& SpawnedDecals)
{
	FRadixiteGrowthBranchRecord NewBranch;
	NewBranch.BranchId = GetNextBranchId();
	NewBranch.StartNodeId = StartNodeId;
	NewBranch.InitialYaw = InitialYaw;
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

	const int32 ParentNodeIndex = M_GrowthNodes.IndexOfByPredicate([&BranchToGrow](const FRadixiteGrowthNodeRecord& Node)
	{
		return Node.NodeId == BranchToGrow.StartNodeId;
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
		FRadixiteGrowthBranchRecord& Branch = M_DecalBranches[BranchIndex];
		const bool bIsBetweenNodeAndConnection = Branch.StartNodeId == DestroyedNodeId || Branch.EndNodeId == DestroyedNodeId;
		if (ConnectionsCount <= 1 && bIsBetweenNodeAndConnection)
		{
			DestroyBranchDecals(Branch);
			M_DecalBranches.RemoveAt(BranchIndex);
			continue;
		}

		const bool bBranchStartedFromDestroyed = Branch.StartNodeId == DestroyedNodeId;
		if (ConnectionsCount > 1 && bBranchStartedFromDestroyed && Branch.bM_HasPendingGrowthNode)
		{
			DestroyBranchDecals(Branch);
			M_DecalBranches.RemoveAt(BranchIndex);
		}
	}
}

void URadixiteGrowthComponent::RemoveBranchByIndex(const int32 BranchIndex)
{
	if (not M_DecalBranches.IsValidIndex(BranchIndex))
	{
		return;
	}

	DestroyBranchDecals(M_DecalBranches[BranchIndex]);
	M_DecalBranches.RemoveAt(BranchIndex);
}

void URadixiteGrowthComponent::RemoveNodeConnections(const int32 DestroyedNodeId)
{
	for (FRadixiteGrowthNodeRecord& GrowthNode : M_GrowthNodes)
	{
		GrowthNode.ConnectedNodeIds.Remove(DestroyedNodeId);
	}
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
	int32 TotalDecals = 0;
	for (const FRadixiteGrowthBranchRecord& Branch : M_DecalBranches)
	{
		for (const TObjectPtr<ADecalActor> SpawnedDecal : Branch.SpawnedDecals)
		{
			if (IsValid(SpawnedDecal))
			{
				++TotalDecals;
			}
		}
	}
	return TotalDecals;
}

int32 URadixiteGrowthComponent::GetCurrentSpawnedNodeCount() const
{
	int32 SpawnedNodes = 0;
	for (const FRadixiteGrowthNodeRecord& Node : M_GrowthNodes)
	{
		if (Node.NodeId == 0)
		{
			continue;
		}

		if (Node.SpawnedNodeActor.IsValid())
		{
			++SpawnedNodes;
		}
	}
	return SpawnedNodes;
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
	for (const FRadixiteGrowthNodeRecord& Node : M_GrowthNodes)
	{
		if (Node.SpawnedNodeActor.Get() == NodeActor)
		{
			return Node.NodeId;
		}
	}

	return INDEX_NONE;
}

bool URadixiteGrowthComponent::TryGetUnsaturatedNodeIds(TArray<int32>& OutNodeIds) const
{
	OutNodeIds.Reset();

	for (const FRadixiteGrowthNodeRecord& Node : M_GrowthNodes)
	{
		if (not Node.SpawnedNodeActor.IsValid())
		{
			continue;
		}

		int32 BranchCount = 0;
		for (const FRadixiteGrowthBranchRecord& Branch : M_DecalBranches)
		{
			if (Branch.StartNodeId == Node.NodeId)
			{
				++BranchCount;
			}
		}

		if (BranchCount < GrowthNodeOptions.MaxDecalGrowthPerNode)
		{
			OutNodeIds.Add(Node.NodeId);
		}
	}

	return OutNodeIds.Num() > 0;
}

bool URadixiteGrowthComponent::TryChooseInitialYaw(
	const FRadixiteGrowthNodeRecord& NodeRecord,
	float& OutYaw,
	int32& OutYawBucket) const
{
	if (FMath::IsNearlyZero(DecalRotationOffset))
	{
		OutYaw = FMath::FRandRange(0.f, 360.f);
		OutYawBucket = FMath::RoundToInt(OutYaw);
		return true;
	}

	const int32 BucketCount = FMath::FloorToInt(360.f / FMath::Abs(DecalRotationOffset));
	if (BucketCount <= 0)
	{
		return false;
	}

	TArray<int32> AvailableBuckets;
	for (int32 Bucket = 0; Bucket < BucketCount; ++Bucket)
	{
		if (not NodeRecord.UsedInitialYawOffsets.Contains(Bucket))
		{
			AvailableBuckets.Add(Bucket);
		}
	}
	if (AvailableBuckets.Num() == 0)
	{
		return false;
	}

	OutYawBucket = AvailableBuckets[FMath::RandRange(0, AvailableBuckets.Num() - 1)];
	OutYaw = static_cast<float>(OutYawBucket) * FMath::Abs(DecalRotationOffset);
	if (GetShouldUseDirectionalBias())
	{
		OutYaw = GetBiasedYaw();
		OutYawBucket = FMath::RoundToInt(FMath::UnwindDegrees(OutYaw));
	}
	return true;
}

bool URadixiteGrowthComponent::GetShouldUseDirectionalBias() const
{
	const bool bHasNoPreference = (GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::NoPreference)) != 0;
	if (bHasNoPreference)
	{
		return false;
	}

	const int32 AxisMask =
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosX) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegX) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosY) |
		static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegY);
	if ((GrowthDirectionBias & AxisMask) == 0)
	{
		return false;
	}

	return FMath::FRandRange(0.f, 1.f) <= GrowthDirectionBiasProbability;
}

float URadixiteGrowthComponent::GetBiasedYaw() const
{
	TArray<float> YawOptions;
	if ((GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosX)) != 0)
	{
		YawOptions.Add(0.f);
	}
	if ((GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegX)) != 0)
	{
		YawOptions.Add(180.f);
	}
	if ((GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldPosY)) != 0)
	{
		YawOptions.Add(90.f);
	}
	if ((GrowthDirectionBias & static_cast<int32>(ERadixiteGrowthDirectionBias::WorldNegY)) != 0)
	{
		YawOptions.Add(270.f);
	}

	if (YawOptions.Num() == 0)
	{
		return GetRandomOffsetYaw();
	}

	return YawOptions[FMath::RandRange(0, YawOptions.Num() - 1)];
}

float URadixiteGrowthComponent::GetRandomOffsetYaw() const
{
	if (FMath::IsNearlyZero(DecalRotationOffset))
	{
		return FMath::FRandRange(0.f, 360.f);
	}

	const int32 BucketCount = FMath::Max(1, FMath::FloorToInt(360.f / FMath::Abs(DecalRotationOffset)));
	const int32 PickedBucket = FMath::RandRange(0, BucketCount - 1);
	return static_cast<float>(PickedBucket) * FMath::Abs(DecalRotationOffset);
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

	const FGrowthDecalOptions& DecalOption = GrowthDecalOptions[FMath::RandRange(0, GrowthDecalOptions.Num() - 1)];
	if (not IsValid(DecalOption.DecalMaterial))
	{
		return nullptr;
	}

	const FVector SpawnLocation = (SegmentStart + SegmentEnd) * 0.5f;
	const float Scale = SegmentDistance / FMath::Max(1.f, DecalOption.DistanceCoveredAtScale1);
	const FRotator DecalRotation(-90.f, SegmentYaw + DecalOption.AimAtRotationYaw, 0.f);
	AActor* SpawnedActor = GetWorld()->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), SpawnLocation, DecalRotation);
	ADecalActor* SpawnedDecalActor = Cast<ADecalActor>(SpawnedActor);
	if (not IsValid(SpawnedDecalActor))
	{
		return nullptr;
	}

	SpawnedDecalActor->SetDecalMaterial(DecalOption.DecalMaterial);
	SpawnedDecalActor->SetActorScale3D(FVector(Scale, Scale, Scale));
	return SpawnedDecalActor;
}

TSubclassOf<AActor> URadixiteGrowthComponent::GetRandomGrowthNodeClass() const
{
	if (GrowthNodeOptions.GrowthNodeClasses.Num() == 0)
	{
		return nullptr;
	}

	return GrowthNodeOptions.GrowthNodeClasses[FMath::RandRange(0, GrowthNodeOptions.GrowthNodeClasses.Num() - 1)];
}

void URadixiteGrowthComponent::HandleSpawnedGrowthNodeDestroyed(AActor* DestroyedActor)
{
	const int32 DestroyedNodeId = GetNodeIdByActor(DestroyedActor);
	if (DestroyedNodeId == INDEX_NONE)
	{
		return;
	}

	const int32 NodeIndex = M_GrowthNodes.IndexOfByPredicate([DestroyedNodeId](const FRadixiteGrowthNodeRecord& Node)
	{
		return Node.NodeId == DestroyedNodeId;
	});
	if (not M_GrowthNodes.IsValidIndex(NodeIndex))
	{
		return;
	}

	const int32 ConnectionsCount = M_GrowthNodes[NodeIndex].ConnectedNodeIds.Num();
	RemoveBranchesForNodeDestruction(DestroyedNodeId, ConnectionsCount);
	RemoveNodeConnections(DestroyedNodeId);
	M_GrowthNodes.RemoveAt(NodeIndex);
}
