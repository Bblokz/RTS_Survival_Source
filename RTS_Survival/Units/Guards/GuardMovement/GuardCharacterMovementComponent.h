// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GuardCharacterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGuardMoveFinished, bool);

/**
 * @brief Movement component used by guard units while they strafe along authored world-space guard points.
 * Switches to a trace-driven custom movement mode so guards can follow wall tops without navmesh support.
 */
UCLASS()
class RTS_SURVIVAL_API UGuardCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGuardCharacterMovementComponent();

	/**
	 * @brief Starts custom guard movement towards a world-space target using support traces for height.
	 * Keeps inherited walking/path-following behavior untouched whenever guard movement is inactive.
	 *
	 * @param TargetLocation World-space destination for the current guard strafe segment.
	 * @return True when the component could enter guard movement mode.
	 */
	bool StartGuardMove(const FVector& TargetLocation);

	void StopGuardMove();
	bool GetIsGuardMoveActive() const { return bM_IsGuardMoveActive; }

	FOnGuardMoveFinished OnGuardMoveFinished;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	static constexpr uint8 GuardStrafeCustomMode = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardMoveAcceptanceRadiusCm = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardTraceStartHeightCm = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardTraceLengthCm = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardSurfaceClearanceCm = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardMaxStrafeStepCm = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardMinStrafeStepCm = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardMinProgressDistanceCm = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardStepReductionFactor = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	int32 M_MaxGuardStepSearchIterations = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardSurfaceSampleRadiusScale = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardBodySweepRadiusScale = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_GuardBlockingBackoffCm = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard Movement")
	float M_MaxTimeWithoutProgressSeconds = 0.45f;

	bool bM_IsGuardMoveActive = false;
	FVector M_GuardTargetLocation = FVector::ZeroVector;
	float M_TimeWithoutProgressSeconds = 0.0f;

	void PhysGuardMove(float DeltaTime);
	void FinishGuardMove(const bool bReachedDestination);
	void HandleBlockedGuardMove(float DeltaTime);
	void RegisterGuardMoveProgress(const FVector& PreviousLocation, const FVector& NewLocation);

	/**
	 * @brief Advances one guarded strafe step while staying on the authored planar line.
	 * Shrinks the requested step when support or blocking geometry would invalidate the full move.
	 *
	 * @param CurrentLocation Current character center location.
	 * @param MoveDirection2D Normalized planar movement direction.
	 * @param DesiredMoveDistance Requested planar step length for this tick.
	 * @param DeltaTime Frame delta used to update velocity from the applied move.
	 * @return True when the component made measurable forward progress.
	 */
	bool TryMoveTowardsGuardTarget(
		const FVector& CurrentLocation,
		const FVector& MoveDirection2D,
		float DesiredMoveDistance,
		float DeltaTime);

	/**
	 * @brief Finds a supported character center location for the requested planar guard step.
	 * Samples nearby wall-top support points so small cracks and uneven collision do not break the move.
	 *
	 * @param CurrentLocation Current character center location.
	 * @param MoveDirection2D Normalized planar movement direction.
	 * @param DesiredMoveDistance Requested planar step length for this tick.
	 * @param OutSupportedLocation Supported character center location to move towards.
	 * @return True when a walkable support surface was found.
	 */
	bool TryGetSupportedLocationForGuardMove(
		const FVector& CurrentLocation,
		const FVector& MoveDirection2D,
		float DesiredMoveDistance,
		FVector& OutSupportedLocation) const;

	/**
	 * @brief Queries the best walkable support hit below the candidate guard step.
	 * Prefers stable wall-top support that keeps the unit on a straight authored line.
	 *
	 * @param CandidateCenterLocation Candidate character center location before height adjustment.
	 * @param MoveDirection2D Normalized planar movement direction.
	 * @param OutSurfaceHit Walkable support hit chosen for the move.
	 * @return True when a valid support hit was found.
	 */
	bool TryGetWalkableGuardSurfaceHit(
		const FVector& CandidateCenterLocation,
		const FVector& MoveDirection2D,
		FHitResult& OutSurfaceHit) const;

	void BuildGuardSurfaceSampleOffsets(
		const FVector& MoveDirection2D,
		float SampleOffsetDistance,
		TArray<FVector, TInlineAllocator<5>>& OutSampleOffsets) const;

	bool ShouldReplaceBestGuardSurfaceHit(
		const FHitResult& CandidateSurfaceHit,
		float CandidateSampleDistanceSquared,
		float BestSupportHeight,
		float BestSampleDistanceSquared) const;

	bool TryGetWalkableGuardSurfaceHitForSample(const FVector& SampleLocation, FHitResult& OutSurfaceHit) const;

	/**
	 * @brief Shrinks the planar guard step when straight-line body sweeps detect blocking geometry ahead.
	 * Keeps the unit from clipping through walls while preserving the authored strafe direction.
	 *
	 * @param CurrentLocation Current character center location.
	 * @param SupportedLocation Supported character center location for the current step candidate.
	 * @param InOutMoveDistance Requested planar step distance that may be shortened.
	 * @return True when the remaining step is still large enough to attempt.
	 */
	bool TryAdjustMoveDistanceForBlockingGuardGeometry(
		const FVector& CurrentLocation,
		const FVector& SupportedLocation,
		float& InOutMoveDistance) const;

	/**
	 * @brief Sweeps the guard body through the proposed step against support geometry object types.
	 * Detects wall and obstacle blockers even when their collision responses do not block the unit capsule directly.
	 *
	 * @param StartLocation Current character center location.
	 * @param EndLocation Proposed character center location after support adjustment.
	 * @param OutBlockingHit Blocking hit encountered along the straight guard step.
	 * @return True when blocking geometry was found on the path.
	 */
	bool TrySweepForBlockingGuardGeometry(
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& OutBlockingHit) const;

	bool GetIsValidCharacterOwner() const;
};
