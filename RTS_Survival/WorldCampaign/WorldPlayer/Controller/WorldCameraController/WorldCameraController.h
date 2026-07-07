// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "WorldCameraController.generated.h"

class USpringArmComponent;
class USoundBase;
class AActor;
class AWorldPlayerController;

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FCameraOvertakeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Overtake", meta = (ClampMin = "0.0"))
	float TimeGetToFirstPoint = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Overtake")
	TArray<FVector> Points = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Overtake", meta = (ClampMin = "0.0"))
	float TotalTimeMovementSequentialPoints = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Overtake")
	TObjectPtr<USoundBase> SoundStart = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Overtake")
	TObjectPtr<USoundBase> SoundPerSequentialPoint = nullptr;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FWorldCameraOvertakeFinishedDelegate, FVector, LastCameraPosition);
DECLARE_DELEGATE_TwoParams(FWorldCameraOvertakePointReachedNativeDelegate, int32, const FVector&);
DECLARE_DELEGATE_OneParam(FWorldCameraOvertakeFinishedNativeDelegate, const FVector&);

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
	 * @brief Starts a designer-authored camera overtake path while preserving camera boundary handling.
	 * @param CameraOvertakeSettings Points, timings, and optional audio assets for the overtake sequence.
	 * @param PointReachedDelegate Native callback used by the player controller for per-point side effects.
	 * @param FinishedDelegate Native callback fired with the final camera location when the sequence ends.
	 * @return true when a sequence was accepted or completed immediately.
	 */
	bool StartCameraOvertake(
		const FCameraOvertakeSettings& CameraOvertakeSettings,
		const FWorldCameraOvertakePointReachedNativeDelegate& PointReachedDelegate,
		const FWorldCameraOvertakeFinishedNativeDelegate& FinishedDelegate);

	bool GetIsCameraOvertakeActive() const;
	FVector GetCurrentCameraLocation() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|EdgeScroll", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeScrollingPercentage = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|EdgeScroll", meta = (ClampMin = "0.0"))
	float EdgeScrollingSpeedMlt = 1.0f;

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
		bool bM_IgnoreBoundaryConstraints = false;
	};

	struct FWorldCameraOvertakeState
	{
		TArray<FVector> M_Points = {};
		TArray<float> M_SequentialSegmentDurations = {};
		FWorldCameraOvertakePointReachedNativeDelegate M_PointReachedDelegate;
		FWorldCameraOvertakeFinishedNativeDelegate M_FinishedDelegate;
		FVector M_SegmentStartLocation = FVector::ZeroVector;
		FVector M_SegmentTargetLocation = FVector::ZeroVector;
		FVector M_LastReachedLocation = FVector::ZeroVector;
		float M_TimeGetToFirstPoint = 0.0f;
		float M_SegmentElapsedTime = 0.0f;
		float M_SegmentTotalTime = 0.0f;
		int32 M_TargetPointIndex = INDEX_NONE;
		bool bM_IsActive = false;
	};

	void BeginPlay_InitCameraCarrier();
	void BeginPlay_InitSpringArmComponent();
	void UpdateCameraInputDisabled(const float DeltaTime);
	void UpdateMoveToLocation(const float DeltaTime);
	void UpdateCameraOvertake(const float DeltaTime);
	void UpdateAxisMovement(const float DeltaTime);
	void UpdateEdgeScroll(const float DeltaTime);
	void UpdateBoundarySafety();

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
	FVector GetConstrainedCameraLocation(const FVector& TargetCameraLocation) const;
	bool GetIsCameraOutsideBoundaries() const;
	void StartSafetyMoveToPlayerHQ();
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
	void PlayOptionalMoveSound(USoundBase* MoveSound) const;
	bool GetCanStartCameraOvertake(const FCameraOvertakeSettings& CameraOvertakeSettings) const;
	void CacheCameraOvertakeDurations(const FCameraOvertakeSettings& CameraOvertakeSettings);
	float GetCameraOvertakeSequentialSegmentDuration(const int32 TargetPointIndex) const;
	void StartCameraOvertakeSegment(const int32 TargetPointIndex);
	void CompleteCameraOvertakeSegment();
	void FinishCameraOvertake();
	void CancelActiveCameraMove();

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
	float M_EdgeScrollHardZone = 0.025f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollAccelerationRate = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|EdgeScroll")
	float M_EdgeScrollMaxAccelerationMultiplier = 250.0f;

	float M_EdgeScrollAccelX = 1.0f;
	float M_EdgeScrollAccelY = 1.0f;

	FWorldCameraAxisInputState M_AxisInputState;
	FWorldCameraMoveState M_MoveState;
	FWorldCameraOvertakeState M_CameraOvertakeState;

	float M_CameraInputDisabledRemainingSeconds = 0.0f;
	bool bM_IsCameraMovementDisabled = false;
	bool bM_IsSafetyMoveToPlayerHQActive = false;

	// Flattened constraints from all additional submissions, rebuilt only when registrations change.
	UPROPERTY()
	TArray<FPlayerCameraBoundaryPlaneConstraint> M_CachedAdditionalPlaneConstraints = {};

	UPROPERTY()
	TArray<FPlayerCameraBoundarySubmissionCache> M_AdditionalBoundarySubmissionCaches = {};
};
