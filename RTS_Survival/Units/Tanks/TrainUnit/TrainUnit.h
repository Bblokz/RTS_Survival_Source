#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "TrainUnit.generated.h"

class ARoadSplineActor;
class UTrainSplineMovementComponent;
class USplineComponent;
class UMeshComponent;

/**
 * @brief Wraps ATankMaster command/combat systems while forcing all locomotion onto one assigned spline.
 * Designers assign a road spline and move commands are projected to deterministic distances.
 */
UCLASS()
class RTS_SURVIVAL_API ATrainUnit : public ATankMaster
{
	GENERATED_BODY()

public:
	ATrainUnit(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Collision")
	void SetupTrainMeshCollision(const TArray<UMeshComponent*>& MeshComponents);

	/**
	 * @brief Captures the authored train mesh layout so cart spacing can remain stable on spline movement.
	 * @param InLocomotiveMesh Mesh that should lead the train.
	 * @param InCartMeshes Ordered trailing cart meshes that should stay behind the locomotive.
	 */
	UFUNCTION(BlueprintCallable, Category="Train")
	void SetupTrainComposition(UMeshComponent* InLocomotiveMesh, const TArray<UMeshComponent*>& InCartMeshes);

	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void TerminateMoveCommand() override;
	virtual void ExecuteReverseCommand(const FVector ReverseToLocation) override;
	virtual void TerminateReverseCommand() override;
	virtual void ExecuteStopCommand() override;
	virtual void ExecuteAttackCommand(AActor* Target) override;

	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
	virtual void TerminateRotateTowardsCommand() override;
	virtual void ApplyRotateTowardsStep(const float TurnAmountDegrees, const float DeltaSeconds) override;

	virtual void OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret) override;
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnUnitIdleAndNoNewCommands() override;

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

private:
	bool TryCacheRoadSplineComponentFromAssignedRoad();
	bool GetIsValidAssignedRoadSpline() const;
	bool GetIsValidTrainSplineMovementComponent() const;
	bool GetIsValidRoadSplineComponent() const;
	bool TryConvertWorldLocationToSplineDistance(const FVector& WorldLocation, float& OutDistanceAlongSpline) const;
	bool TryGetSplineDistanceAtWorldLocation(const FVector& WorldLocation, float& OutDistanceAlongSpline) const;
	void StartSplineMovementToDistance(const float TargetDistanceAlongSpline);
	void OnReachedSplineMoveTarget();
	bool GetIsValidLocomotiveMesh() const;
	bool GetHasValidTrainComposition() const;
	bool TryBuildCartDistanceOffsetsFromCurrentPlacement(const UMeshComponent* InLocomotiveMesh,
	                                                     const TArray<UMeshComponent*>& InCartMeshes,
	                                                     TArray<float>& OutCartDistanceOffsets) const;
	bool TryGetLocomotiveStartDistance(float& OutDistanceAlongSpline) const;
	float GetMaxCartDistanceOffset() const;
	FRotator GetSplineRotationForCurrentMovementDirection(const float DistanceAlongSpline) const;
	void ApplyActorTransformFromSplineDistance(const float DistanceAlongSpline);
	void ApplyTrainTransforms_Forward(const float ClampedLocomotiveDistance, const float SplineLength);
	void ApplyTrainTransforms_Reverse(const float ClampedLeaderDistance, const float SplineLength);
	void ApplyTrainTransforms(float LocomotiveDistance);
	void OnSplineMovementReachedTarget();

	friend class UTrainSplineMovementComponent;

	UPROPERTY(EditAnywhere, Category="Spline Movement")
	TWeakObjectPtr<ARoadSplineActor> AssignedRoadSpline;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTrainSplineMovementComponent> M_TrainSplineMovementComponent;

	UPROPERTY()
	TWeakObjectPtr<USplineComponent> M_RoadSplineComponent;

	UPROPERTY(EditAnywhere, Category="Train")
	TObjectPtr<UMeshComponent> M_LocomotiveMesh;

	UPROPERTY(EditAnywhere, Category="Train")
	TArray<TObjectPtr<UMeshComponent>> M_TrainCartMeshes;

	// Stores trailing spline distance offsets where index matches M_TrainCartMeshes exactly.
	UPROPERTY()
	TArray<float> M_CartDistanceOffsets;

	bool bM_IsReversing = false;
	bool bM_LastMoveCommandWasReverse = false;
};
