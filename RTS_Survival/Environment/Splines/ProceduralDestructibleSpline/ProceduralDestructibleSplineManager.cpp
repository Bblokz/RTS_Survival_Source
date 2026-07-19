// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ProceduralDestructibleSplineManager.h"

#include "Engine/World.h"
#include "ProceduralDestructibleSplineSpawner.h"
#include "RTS_Survival/Environment/Splines/DestructibleSpline/DestructibleSplineActor.h"
#include "TimerManager.h"

bool UProceduralDestructibleSplineManager::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void UProceduralDestructibleSplineManager::Deinitialize()
{
	bM_IsDeinitializing = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ConnectionEvaluationTimer);
	}
	bM_IsConnectionEvaluationScheduled = false;

	for (const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& WeakSpawner : M_RegisteredSpawners)
	{
		if (UProceduralDestructibleSplineSpawner* Spawner = WeakSpawner.Get())
		{
			Spawner->M_UsedSocketNames.Empty();
			Spawner->M_NumOwnConnections = 0;
			Spawner->bM_IsRegistered = false;
			Spawner->M_Manager.Reset();
		}
	}
	M_Connections.Empty();
	M_RegisteredSpawners.Empty();

	Super::Deinitialize();
}

void UProceduralDestructibleSplineManager::RegisterSpawner(
	UProceduralDestructibleSplineSpawner* Spawner)
{
	if (bM_IsDeinitializing || not IsValid(Spawner) || not Spawner->GetIsInitialized())
	{
		return;
	}

	M_RegisteredSpawners.AddUnique(Spawner);
	Spawner->bM_IsRegistered = true;
	Spawner->M_Manager = this;
	RequestConnectionEvaluation();
}

void UProceduralDestructibleSplineManager::UnregisterSpawner(
	UProceduralDestructibleSplineSpawner* Spawner,
	const bool bCollapseConnections)
{
	// EndPlay can mark the component as garbage before it asks the manager to unregister. The pointer
	// is still safe for this synchronous teardown, and its weak identity must remain matchable.
	if (not Spawner)
	{
		return;
	}

	RemoveConnectionsForSpawner(*Spawner, bCollapseConnections);
	M_RegisteredSpawners.RemoveAllSwap(
		[Spawner](const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& RegisteredSpawner)
		{
			return RegisteredSpawner.Get(true) == Spawner;
		}, EAllowShrinking::No);
	if (IsValid(Spawner))
	{
		Spawner->M_UsedSocketNames.Empty();
		Spawner->M_NumOwnConnections = 0;
		Spawner->bM_IsRegistered = false;
		Spawner->M_Manager.Reset();
	}

	if (not bM_IsDeinitializing)
	{
		RequestConnectionEvaluation();
	}
}

void UProceduralDestructibleSplineManager::RequestConnectionEvaluation()
{
	if (bM_IsDeinitializing || bM_IsConnectionEvaluationScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	bM_IsConnectionEvaluationScheduled = true;
	M_ConnectionEvaluationTimer = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UProceduralDestructibleSplineManager::EvaluateConnections));
}

void UProceduralDestructibleSplineManager::EvaluateConnections()
{
	bM_IsConnectionEvaluationScheduled = false;
	if (bM_IsDeinitializing || bM_IsEvaluatingConnections)
	{
		return;
	}

	bM_IsEvaluatingConnections = true;
	CompactInvalidState();
	TArray<TWeakObjectPtr<UProceduralDestructibleSplineSpawner>> SortedSpawners = M_RegisteredSpawners;
	SortSpawnersByPriority(SortedSpawners);
	for (const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& WeakSpawner : SortedSpawners)
	{
		UProceduralDestructibleSplineSpawner* Spawner = WeakSpawner.Get();
		if (not IsValid(Spawner) || not Spawner->GetIsInitialized() || not Spawner->bM_IsRegistered)
		{
			continue;
		}

		while (Spawner->M_NumOwnConnections < Spawner->GetMaximumOwnConnections())
		{
			if (not TryCreateNextConnection(*Spawner))
			{
				break;
			}
		}
	}
	bM_IsEvaluatingConnections = false;
}

void UProceduralDestructibleSplineManager::CompactInvalidState()
{
	while (true)
	{
		const int32 InvalidConnectionIndex = M_Connections.IndexOfByPredicate(
			[](const FProceduralDestructibleSplineConnection& Connection)
			{
				return not Connection.SourceSpawner.IsValid()
					|| not Connection.TargetSpawner.IsValid()
					|| not Connection.SplineActor.IsValid();
			});
		if (InvalidConnectionIndex == INDEX_NONE)
		{
			break;
		}
		RemoveConnectionAt(InvalidConnectionIndex, true);
	}

	M_RegisteredSpawners.RemoveAllSwap(
		[](const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& Spawner)
		{
			return not Spawner.IsValid();
		}, EAllowShrinking::No);

	TArray<TWeakObjectPtr<UProceduralDestructibleSplineSpawner>> SpawnersToRemove;
	for (const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& WeakSpawner : M_RegisteredSpawners)
	{
		UProceduralDestructibleSplineSpawner* Spawner = WeakSpawner.Get();
		if (Spawner->GetIsInitialized() && Spawner->bM_IsRegistered)
		{
			continue;
		}
		SpawnersToRemove.Add(WeakSpawner);
	}

	for (const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& WeakSpawner : SpawnersToRemove)
	{
		UProceduralDestructibleSplineSpawner* Spawner = WeakSpawner.Get();
		if (not IsValid(Spawner))
		{
			continue;
		}
		RemoveConnectionsForSpawner(*Spawner, true);
		if (not IsValid(Spawner))
		{
			continue;
		}
		Spawner->M_UsedSocketNames.Empty();
		Spawner->M_NumOwnConnections = 0;
		Spawner->bM_IsRegistered = false;
		Spawner->M_Manager.Reset();
		M_RegisteredSpawners.RemoveAllSwap(
			[Spawner](const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& RegisteredSpawner)
			{
				return RegisteredSpawner.Get() == Spawner;
			}, EAllowShrinking::No);
	}
}

void UProceduralDestructibleSplineManager::SortSpawnersByPriority(
	TArray<TWeakObjectPtr<UProceduralDestructibleSplineSpawner>>& Spawners) const
{
	Spawners.Sort([](
		const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& Left,
		const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& Right)
	{
		const UProceduralDestructibleSplineSpawner* LeftSpawner = Left.Get();
		const UProceduralDestructibleSplineSpawner* RightSpawner = Right.Get();
		if (not IsValid(LeftSpawner) || not IsValid(RightSpawner))
		{
			return IsValid(LeftSpawner);
		}
		if (LeftSpawner->Settings.Priority != RightSpawner->Settings.Priority)
		{
			return LeftSpawner->Settings.Priority > RightSpawner->Settings.Priority;
		}
		return LeftSpawner->GetPathName() < RightSpawner->GetPathName();
	});
}

bool UProceduralDestructibleSplineManager::TryCreateNextConnection(
	UProceduralDestructibleSplineSpawner& SourceSpawner)
{
	FConnectionCandidate Candidate;
	if (not FindBestCandidate(SourceSpawner, Candidate))
	{
		return false;
	}

	UProceduralDestructibleSplineSpawner* TargetSpawner = Candidate.TargetSpawner.Get();
	if (not IsValid(TargetSpawner) || not SourceSpawner.ReserveSocket(Candidate.SourceSocket))
	{
		return false;
	}
	if (not TargetSpawner->ReserveSocket(Candidate.TargetSocket))
	{
		SourceSpawner.ReleaseUncommittedSocket(Candidate.SourceSocket);
		return false;
	}

	ADestructibleSplineActor* SplineActor = SourceSpawner.SpawnConnectionSpline(
		Candidate.SourceSocket, *TargetSpawner, Candidate.TargetSocket);
	if (not IsValid(SplineActor))
	{
		ReleaseCandidateReservations(&SourceSpawner, TargetSpawner, Candidate);
		return false;
	}
	if (not GetAreCandidateReservationsValid(SourceSpawner, *TargetSpawner, Candidate))
	{
		SplineActor->DestroyAllSegments();
		if (IsValid(SplineActor))
		{
			SplineActor->Destroy();
		}
		ReleaseCandidateReservations(&SourceSpawner, TargetSpawner, Candidate);
		return false;
	}

	FProceduralDestructibleSplineConnection& Connection = M_Connections.Emplace_GetRef();
	Connection.SourceSpawner = &SourceSpawner;
	Connection.TargetSpawner = TargetSpawner;
	Connection.SplineActor = SplineActor;
	Connection.SourceSocket = Candidate.SourceSocket;
	Connection.TargetSocket = Candidate.TargetSocket;
	++SourceSpawner.M_NumOwnConnections;
	return true;
}

bool UProceduralDestructibleSplineManager::GetAreCandidateReservationsValid(
	const UProceduralDestructibleSplineSpawner& SourceSpawner,
	const UProceduralDestructibleSplineSpawner& TargetSpawner,
	const FConnectionCandidate& Candidate) const
{
	return IsValid(&SourceSpawner)
		&& IsValid(&TargetSpawner)
		&& SourceSpawner.bM_IsRegistered
		&& TargetSpawner.bM_IsRegistered
		&& SourceSpawner.GetIsInitialized()
		&& TargetSpawner.GetIsInitialized()
		&& SourceSpawner.M_UsedSocketNames.Contains(Candidate.SourceSocket)
		&& TargetSpawner.M_UsedSocketNames.Contains(Candidate.TargetSocket);
}

void UProceduralDestructibleSplineManager::ReleaseCandidateReservations(
	UProceduralDestructibleSplineSpawner* SourceSpawner,
	UProceduralDestructibleSplineSpawner* TargetSpawner,
	const FConnectionCandidate& Candidate) const
{
	if (IsValid(SourceSpawner))
	{
		SourceSpawner->ReleaseUncommittedSocket(Candidate.SourceSocket);
	}
	if (IsValid(TargetSpawner))
	{
		TargetSpawner->ReleaseUncommittedSocket(Candidate.TargetSocket);
	}
}

bool UProceduralDestructibleSplineManager::FindBestCandidate(
	const UProceduralDestructibleSplineSpawner& SourceSpawner,
	FConnectionCandidate& OutCandidate) const
{
	for (const TWeakObjectPtr<UProceduralDestructibleSplineSpawner>& WeakTarget : M_RegisteredSpawners)
	{
		UProceduralDestructibleSplineSpawner* TargetSpawner = WeakTarget.Get();
		if (not IsValid(TargetSpawner)
			|| TargetSpawner == &SourceSpawner
			|| not TargetSpawner->GetIsInitialized()
			|| TargetSpawner->GetOwner() == SourceSpawner.GetOwner()
			|| not TargetSpawner->Settings.SocketFilter.Equals(
				SourceSpawner.Settings.SocketFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		ConsiderTargetCandidates(SourceSpawner, *TargetSpawner, OutCandidate);
	}
	return OutCandidate.TargetSpawner.IsValid();
}

void UProceduralDestructibleSplineManager::ConsiderTargetCandidates(
	const UProceduralDestructibleSplineSpawner& SourceSpawner,
	UProceduralDestructibleSplineSpawner& TargetSpawner,
	FConnectionCandidate& InOutBestCandidate) const
{
	for (const FName SourceSocket : SourceSpawner.M_EligibleSocketNames)
	{
		if (not SourceSpawner.GetIsSocketAvailable(SourceSocket))
		{
			continue;
		}
		for (const FName TargetSocket : TargetSpawner.M_EligibleSocketNames)
		{
			float DistanceSquared = 0.f;
			float DepartureYaw = 0.f;
			if (not GetIsEligibleSocketPair(
				SourceSpawner, SourceSocket, TargetSpawner, TargetSocket,
				DistanceSquared, DepartureYaw))
			{
				continue;
			}

			FConnectionCandidate Candidate;
			Candidate.TargetSpawner = &TargetSpawner;
			Candidate.SourceSocket = SourceSocket;
			Candidate.TargetSocket = TargetSocket;
			Candidate.DistanceSquared = DistanceSquared;
			Candidate.DepartureYaw = DepartureYaw;
			if (GetIsBetterCandidate(Candidate, InOutBestCandidate))
			{
				InOutBestCandidate = Candidate;
			}
		}
	}
}

bool UProceduralDestructibleSplineManager::GetIsEligibleSocketPair(
	const UProceduralDestructibleSplineSpawner& SourceSpawner,
	const FName SourceSocket,
	const UProceduralDestructibleSplineSpawner& TargetSpawner,
	const FName TargetSocket,
	float& OutDistanceSquared,
	float& OutDepartureYaw) const
{
	if (not SourceSpawner.GetIsSocketAvailable(SourceSocket)
		|| not TargetSpawner.GetIsSocketAvailable(TargetSocket))
	{
		return false;
	}

	FTransform SourceTransform;
	FTransform TargetTransform;
	if (not SourceSpawner.TryGetSocketTransform(SourceSocket, SourceTransform)
		|| not TargetSpawner.TryGetSocketTransform(TargetSocket, TargetTransform))
	{
		return false;
	}

	const FVector ConnectionDelta = TargetTransform.GetLocation() - SourceTransform.GetLocation();
	OutDistanceSquared = ConnectionDelta.SizeSquared();
	const float ConnectionRange = SourceSpawner.GetConnectionRange();
	if (OutDistanceSquared <= FMath::Square(UE_KINDA_SMALL_NUMBER)
		|| OutDistanceSquared > FMath::Square(ConnectionRange))
	{
		return false;
	}

	const FVector HorizontalDelta(ConnectionDelta.X, ConnectionDelta.Y, 0.f);
	if (HorizontalDelta.IsNearlyZero())
	{
		OutDepartureYaw = 0.f;
		return true;
	}

	const float DesiredYaw = HorizontalDelta.Rotation().Yaw;
	const float SocketYaw = SourceTransform.Rotator().Yaw;
	OutDepartureYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(SocketYaw, DesiredYaw));
	return OutDepartureYaw <= SourceSpawner.GetMaximumDepartureYaw() + UE_KINDA_SMALL_NUMBER;
}

bool UProceduralDestructibleSplineManager::GetIsBetterCandidate(
	const FConnectionCandidate& Candidate,
	const FConnectionCandidate& CurrentBest) const
{
	if (not CurrentBest.TargetSpawner.IsValid())
	{
		return true;
	}
	if (not FMath::IsNearlyEqual(Candidate.DistanceSquared, CurrentBest.DistanceSquared))
	{
		return Candidate.DistanceSquared < CurrentBest.DistanceSquared;
	}
	if (not FMath::IsNearlyEqual(Candidate.DepartureYaw, CurrentBest.DepartureYaw))
	{
		return Candidate.DepartureYaw < CurrentBest.DepartureYaw;
	}

	const UProceduralDestructibleSplineSpawner* CandidateTarget = Candidate.TargetSpawner.Get();
	const UProceduralDestructibleSplineSpawner* CurrentTarget = CurrentBest.TargetSpawner.Get();
	if (IsValid(CandidateTarget) && IsValid(CurrentTarget)
		&& CandidateTarget->GetPathName() != CurrentTarget->GetPathName())
	{
		return CandidateTarget->GetPathName() < CurrentTarget->GetPathName();
	}
	if (Candidate.SourceSocket != CurrentBest.SourceSocket)
	{
		return Candidate.SourceSocket.LexicalLess(CurrentBest.SourceSocket);
	}
	return Candidate.TargetSocket.LexicalLess(CurrentBest.TargetSocket);
}

void UProceduralDestructibleSplineManager::RemoveConnectionsForSpawner(
	UProceduralDestructibleSplineSpawner& Spawner,
	const bool bCollapseConnections)
{
	while (true)
	{
		const int32 ConnectionIndex = M_Connections.IndexOfByPredicate(
			[&Spawner](const FProceduralDestructibleSplineConnection& Connection)
			{
				return Connection.SourceSpawner.Get(true) == &Spawner
					|| Connection.TargetSpawner.Get(true) == &Spawner;
			});
		if (ConnectionIndex == INDEX_NONE)
		{
			break;
		}
		RemoveConnectionAt(ConnectionIndex, bCollapseConnections);
	}
}

void UProceduralDestructibleSplineManager::RemoveConnectionAt(
	const int32 ConnectionIndex,
	const bool bCollapseConnection)
{
	if (not M_Connections.IsValidIndex(ConnectionIndex))
	{
		return;
	}

	const FProceduralDestructibleSplineConnection Connection = M_Connections[ConnectionIndex];
	M_Connections.RemoveAt(ConnectionIndex, 1, EAllowShrinking::No);

	// A committed connection permanently consumes both endpoint sockets. Only the source's active
	// own-connection count is released so it may still use a different, never-consumed socket.
	if (UProceduralDestructibleSplineSpawner* SourceSpawner = Connection.SourceSpawner.Get())
	{
		SourceSpawner->M_NumOwnConnections = FMath::Max(0, SourceSpawner->M_NumOwnConnections - 1);
	}

	ADestructibleSplineActor* SplineActor = Connection.SplineActor.Get();
	if (not IsValid(SplineActor))
	{
		return;
	}
	if (bCollapseConnection)
	{
		SplineActor->DestroyAllSegments();
	}
	if (IsValid(SplineActor))
	{
		SplineActor->Destroy();
	}
}
