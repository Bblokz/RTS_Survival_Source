// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "WorldCameraController.generated.h"

class USpringArmComponent;
class AActor;

/**
 * @brief Component used by the world campaign controller to route Blueprint input
 * into camera movement and zoom logic.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCameraController : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCameraController();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void SetIsCameraMovementDisabled(bool bIsDisabled);
	bool GetIsCameraMovementDisabled() const;

	void ZoomIn();
	void ZoomOut();

	void ForwardMovement(float AxisValue);
	void RightMovement(float AxisValue);

	/**
	 * @brief Allows Blueprint-driven campaign logic to move the camera to a target location.
	 * @param MoveRequest The parameters that describe the move and temporary input lockout.
	 */
	void MoveCameraTo(const FMovePlayerCamera& MoveRequest);

	/**
	 * @brief Registers an additional cached boundary evaluated by every world-camera XY move.
	 * @param BoundaryRegistrationParams Defines the boundary actor, ID and blocked-side settings.
	 * @return True if cached boundary constraints were generated and stored successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	bool RegisterAdditionalCameraBoundary(const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams);

	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	bool RemoveAdditionalCameraBoundaryById(FName BoundaryId);

	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	void RemoveAllAdditionalCameraBounds();

private:
	struct FWorldCameraAxisInputState
	{
		float M_ForwardAxisValue = 0.0f;
		float M_RightAxisValue = 0.0f;
	};

	struct FWorldCameraMoveState
	{
		FVector M_StartLocation = FVector::ZeroVector;
		FVector M_TargetLocation = FVector::ZeroVector;
		float M_ElapsedTime = 0.0f;
		float M_TotalTime = 0.0f;
		bool bM_IsMoveActive = false;
	};

	void BeginPlay_InitCameraCarrier();
	void BeginPlay_InitSpringArmComponent();
	void UpdateCameraInputDisabled(const float DeltaTime);
	void UpdateMoveToLocation(const float DeltaTime);
	void UpdateAxisMovement(const float DeltaTime);
	void UpdateEdgeScroll(const float DeltaTime);

	bool GetIsValidCameraCarrierActor() const;
	bool GetIsValidSpringArmComponent() const;

	void GetViewportSizeAndMouse(
		float& OutScreenX,
		float& OutScreenY,
		float& OutMouseX,
		float& OutMouseY,
		bool& bOutIsMouseInViewport) const;
	FVector GetWorldDeltaFromLocalXY(float LocalForwardValue, float LocalRightValue) const;
	bool GetCanMoveCameraToLocation(const FVector& TargetCameraLocation) const;
	bool GetAreAllPlaneConstraintsSatisfied(
		const FVector& TargetCameraLocation,
		const TArray<FPlayerCameraBoundaryPlaneConstraint>& PlaneConstraints) const;
	bool GetIsBoundaryIdValidForRegistration(const FName& BoundaryId, const FString& CallingFunctionName) const;
	bool GetCanBuildBoundaryFromActor(const AActor* BoundaryActor, const FString& CallingFunctionName) const;
	bool GetHasSideFlag(const int32 BlockedSideFlags, const ECameraBoundaryBlockedSides SideFlag) const;
	bool GetHasAnyBlockedSideFlags(const int32 BlockedSideFlags) const;
	void AddPlaneConstraint(
		const FVector& PlaneOrigin,
		const FVector& PlaneNormal,
		const bool bAllowPositiveSide,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	void AddDirectionalSideConstraintWithSpan(
		const float PlaneAxisValue,
		const FVector& PlaneNormal,
		const bool bAllowPositiveSide,
		const FVector& SpanAxis,
		const float SpanAxisMin,
		const float SpanAxisMax,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	void AddBoxBoundaryConstraints(const FBox& WorldSpaceBox, TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	void AddDirectionalSideConstraints(
		const AActor* BoundaryActor,
		const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
		const int32 BlockedSideFlags,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	bool TryGetBoundaryAxisExtents(
		const AActor* BoundaryActor,
		const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
		FVector& OutAxisX,
		FVector& OutAxisY,
		float& OutMinX,
		float& OutMaxX,
		float& OutMinY,
		float& OutMaxY) const;
	void RebuildCachedAdditionalPlaneConstraints();
	void TryMoveCameraByWorldDelta(const FVector& WorldDelta) const;
	void TryMoveCameraToLocation(const FVector& TargetCameraLocation) const;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CameraCarrierActor;

	UPROPERTY()
	TWeakObjectPtr<USpringArmComponent> M_SpringArmComponent;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float M_MinZoom = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float M_MaxZoom = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float M_ZoomSpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float M_XYSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollSoftZone = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollHardZone = 0.025f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollAccelerationRate = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollMaxAccelerationMultiplier = 250.0f;

	float M_EdgeScrollAccelX = 1.0f;
	float M_EdgeScrollAccelY = 1.0f;

	FWorldCameraAxisInputState M_AxisInputState;
	FWorldCameraMoveState M_MoveState;

	float M_CameraInputDisabledRemainingSeconds = 0.0f;
	bool bM_IsCameraMovementDisabled = false;

	// Flattened constraints from all additional submissions, rebuilt only when registrations change.
	UPROPERTY()
	TArray<FPlayerCameraBoundaryPlaneConstraint> M_CachedAdditionalPlaneConstraints = {};

	UPROPERTY()
	TArray<FPlayerCameraBoundarySubmissionCache> M_AdditionalBoundarySubmissionCaches = {};
};
