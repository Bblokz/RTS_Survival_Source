#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrainSplineMovementComponent.generated.h"

class ARoadSplineActor;
class USplineComponent;
class ATrainUnit;

/**
 * @brief Owns deterministic spline movement for train units.
 * Use this component from a train actor and feed target distances from command execution.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTrainSplineMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrainSplineMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void SetAssignedRoadSpline(ARoadSplineActor* InAssignedRoadSpline);
	void SetCurrentDistanceAlongSpline(const float InCurrentDistanceAlongSpline);
	void SetTargetDistanceAlongSpline(const float InTargetDistanceAlongSpline);
	void StopMovement();

	bool GetIsMoving() const { return bM_IsMoving; }
	float GetCurrentDistanceAlongSpline() const { return M_CurrentDistanceAlongSpline; }
	float GetTargetDistanceAlongSpline() const { return M_TargetDistanceAlongSpline; }
	float GetSplineWorldZOffset() const { return M_SplineWorldZOffset; }

protected:
	virtual void BeginPlay() override;

private:
	bool GetHasValidAssignedRoadSplineActorNoReport() const;
	bool GetHasValidCachedSplineComponentNoReport() const;
	bool GetIsValidAssignedRoadSplineActor() const;
	bool GetIsValidCachedSplineComponent() const;
	bool GetIsValidTrainOwner() const;
	bool TryCacheSplineComponent();
	float ClampDistanceToSplineLength(const float InDistance) const;
	float GetValidatedAcceleration() const;
	bool GetHasReachedTargetDistance(const float DistanceToTarget) const;
	bool GetHasCrossedTargetDistance(const float PreviousDistanceAlongSpline, const float NewDistanceAlongSpline) const;
	void HandleReachedTargetDistance();
	void Tick_UpdateSpeed(const float DeltaTime, const float DistanceToTarget);
	void Tick_ApplyTrainTransformsAtCurrentDistance() const;

	UPROPERTY()
	TWeakObjectPtr<ARoadSplineActor> M_AssignedRoadSplineActor;

	UPROPERTY()
	TWeakObjectPtr<USplineComponent> M_CachedSplineComponent;

	float M_CurrentDistanceAlongSpline = 0.0f;
	float M_TargetDistanceAlongSpline = 0.0f;
	float M_CurrentSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category="Spline Movement")
	float M_MaxSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Spline Movement")
	float M_Acceleration = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category="Spline Movement")
	float M_TargetDistanceTolerance = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Spline Movement")
	float M_MinBrakingDistance = 100.0f;

	// Adds a world-space Z height offset on top of spline-evaluated positions for the whole train.
	UPROPERTY(EditAnywhere, Category="Spline Movement")
	float M_SplineWorldZOffset = 0.0f;

	bool bM_IsMoving = false;
	mutable bool bM_HasLoggedInvalidAssignedRoadSplineActor = false;
	mutable bool bM_HasLoggedInvalidCachedSplineComponent = false;
	mutable bool bM_HasLoggedInvalidAcceleration = false;
	mutable bool bM_HasLoggedInvalidTrainOwner = false;
};
