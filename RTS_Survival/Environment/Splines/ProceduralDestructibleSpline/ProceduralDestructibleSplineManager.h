// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProceduralDestructibleSplineManager.generated.h"

class ADestructibleSplineActor;
class UProceduralDestructibleSplineSpawner;

/** @brief Complete ownership record for one generated connection and both reserved endpoint sockets. */
USTRUCT()
struct FProceduralDestructibleSplineConnection
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UProceduralDestructibleSplineSpawner> SourceSpawner;

	UPROPERTY()
	TWeakObjectPtr<UProceduralDestructibleSplineSpawner> TargetSpawner;

	UPROPERTY()
	TWeakObjectPtr<ADestructibleSplineActor> SplineActor;

	UPROPERTY()
	FName SourceSocket = NAME_None;

	UPROPERTY()
	FName TargetSocket = NAME_None;
};

/**
 * @brief Game-world coordinator that gives higher-priority endpoints first access to globally vacant
 * sockets and owns the lifecycle of every generated destructible spline connection.
 */
UCLASS()
class RTS_SURVIVAL_API UProceduralDestructibleSplineManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	void RegisterSpawner(UProceduralDestructibleSplineSpawner* Spawner);
	void UnregisterSpawner(UProceduralDestructibleSplineSpawner* Spawner, bool bCollapseConnections);

private:
	struct FConnectionCandidate
	{
		TWeakObjectPtr<UProceduralDestructibleSplineSpawner> TargetSpawner;
		FName SourceSocket = NAME_None;
		FName TargetSocket = NAME_None;
		float DistanceSquared = TNumericLimits<float>::Max();
		float DepartureYaw = TNumericLimits<float>::Max();
	};

	void RequestConnectionEvaluation();
	void EvaluateConnections();
	void CompactInvalidState();
	void SortSpawnersByPriority(TArray<TWeakObjectPtr<UProceduralDestructibleSplineSpawner>>& Spawners) const;
	bool TryCreateNextConnection(UProceduralDestructibleSplineSpawner& SourceSpawner);

	/**
	 * @brief Revalidates both endpoints after Blueprint construction code had an opportunity to destroy them.
	 * @param SourceSpawner Endpoint which created the spline.
	 * @param TargetSpawner Endpoint receiving the spline.
	 * @param Candidate Socket pair which must still be reserved.
	 * @return True when both endpoints remain registered and still own the reservations.
	 */
	bool GetAreCandidateReservationsValid(
		const UProceduralDestructibleSplineSpawner& SourceSpawner,
		const UProceduralDestructibleSplineSpawner& TargetSpawner,
		const FConnectionCandidate& Candidate) const;

	/**
	 * @brief Rolls back an uncommitted socket pair without dereferencing an endpoint destroyed by Blueprint code.
	 * @param SourceSpawner Endpoint whose source socket was reserved.
	 * @param TargetSpawner Endpoint whose target socket was reserved.
	 * @param Candidate Reserved socket names to release.
	 */
	void ReleaseCandidateReservations(
		UProceduralDestructibleSplineSpawner* SourceSpawner,
		UProceduralDestructibleSplineSpawner* TargetSpawner,
		const FConnectionCandidate& Candidate) const;

	/**
	 * @brief Finds the closest vacant pair which the source endpoint can naturally depart toward.
	 * @param SourceSpawner Endpoint which would own and configure the spline.
	 * @param OutCandidate Best compatible pair across every other registered endpoint.
	 * @return True when a vacant pair satisfies the source range and yaw constraints.
	 */
	bool FindBestCandidate(
		const UProceduralDestructibleSplineSpawner& SourceSpawner,
		FConnectionCandidate& OutCandidate) const;

	/**
	 * @brief Evaluates one target without exposing socket-selection details outside the manager.
	 * @param SourceSpawner Endpoint which would own the spline.
	 * @param TargetSpawner Potential endpoint receiving the spline.
	 * @param InOutBestCandidate Closest candidate found across targets so far.
	 */
	void ConsiderTargetCandidates(
		const UProceduralDestructibleSplineSpawner& SourceSpawner,
		UProceduralDestructibleSplineSpawner& TargetSpawner,
		FConnectionCandidate& InOutBestCandidate) const;

	/**
	 * @brief Applies source-owned connection constraints immediately before a pair can be reserved.
	 * @param SourceSpawner Endpoint which would own the spline.
	 * @param SourceSocket Vacant socket on the source.
	 * @param TargetSpawner Endpoint receiving the spline.
	 * @param TargetSocket Vacant socket on the target.
	 * @param OutDistanceSquared Pair distance used for deterministic ranking.
	 * @param OutDepartureYaw Required source-socket yaw traversal used for ranking.
	 * @return True when the pair is distinct, in range, and inside the allowed departure yaw.
	 */
	bool GetIsEligibleSocketPair(
		const UProceduralDestructibleSplineSpawner& SourceSpawner,
		FName SourceSocket,
		const UProceduralDestructibleSplineSpawner& TargetSpawner,
		FName TargetSocket,
		float& OutDistanceSquared,
		float& OutDepartureYaw) const;

	/**
	 * @brief Gives equivalent eligible pairs a stable ordering so results do not depend on registration order.
	 * @param Candidate Pair being considered.
	 * @param CurrentBest Best pair found so far.
	 * @return True when Candidate should replace CurrentBest.
	 */
	bool GetIsBetterCandidate(
		const FConnectionCandidate& Candidate,
		const FConnectionCandidate& CurrentBest) const;
	void RemoveConnectionsForSpawner(
		UProceduralDestructibleSplineSpawner& Spawner,
		bool bCollapseConnections);
	void RemoveConnectionAt(int32 ConnectionIndex, bool bCollapseConnection);

	UPROPERTY()
	TArray<TWeakObjectPtr<UProceduralDestructibleSplineSpawner>> M_RegisteredSpawners;

	UPROPERTY()
	TArray<FProceduralDestructibleSplineConnection> M_Connections;

	FTimerHandle M_ConnectionEvaluationTimer;
	bool bM_IsConnectionEvaluationScheduled = false;
	bool bM_IsEvaluatingConnections = false;
	bool bM_IsDeinitializing = false;
};
