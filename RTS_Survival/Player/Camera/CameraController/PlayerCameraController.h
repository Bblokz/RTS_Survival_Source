// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "PlayerCameraController.generated.h"

class USpringArmComponent;
class ACameraPawn;
class AActor;

UENUM(BlueprintType)
enum class ECameraBoundaryAxisSpace : uint8
{
	World = 0,
	ActorLocal = 1
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ECameraBoundaryBlockedSides : uint8
{
	None = 0 UMETA(Hidden),
	PositiveX = 1 << 0,
	NegativeX = 1 << 1,
	PositiveY = 1 << 2,
	NegativeY = 1 << 3
};
ENUM_CLASS_FLAGS(ECameraBoundaryBlockedSides);

USTRUCT(BlueprintType)
struct FCameraBoundaryRegistrationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PlayableBounds")
	FName M_BoundaryId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PlayableBounds")
	TObjectPtr<AActor> M_BoundaryActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PlayableBounds")
	bool bM_UseActorBoundsAsBoundary = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PlayableBounds")
	ECameraBoundaryAxisSpace M_AxisSpaceForBlockedSides = ECameraBoundaryAxisSpace::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PlayableBounds", meta = (Bitmask, BitmaskEnum = "/Script/RTS_Survival.ECameraBoundaryBlockedSides"))
	int32 M_BlockedSideFlags = 0;
};

USTRUCT()
struct FPlayerCameraBoundaryPlaneConstraint
{
	GENERATED_BODY()

	FVector M_PlaneOrigin = FVector::ZeroVector;
	FVector M_PlaneNormal = FVector::ForwardVector;
	bool bM_AllowPositiveSide = false;
	bool bM_HasSpanLimit = false;
	FVector M_SpanAxis = FVector::RightVector;
	float M_SpanAxisMin = 0.0f;
	float M_SpanAxisMax = 0.0f;
};

USTRUCT()
struct FPlayerCameraBoundarySubmissionCache
{
	GENERATED_BODY()

	FName M_BoundaryId = NAME_None;
	TArray<FPlayerCameraBoundaryPlaneConstraint> M_CachedPlaneConstraints = {};
};

/**
 * @brief Handles runtime camera movement requests and applies movement constraints for the local player.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerCameraController : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCameraController();

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsCameraLocked() const { return bM_IsCameraLocked; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetIsCameraLocked(const bool bIsLocked) { bM_IsCameraLocked = bIsLocked; }
	
	inline void SetIsPlayerInTechTreeOrArchive(const bool bIsInTechTreeOrArchive) { bM_IsPlayerInTechTreeOrArchive = bIsInTechTreeOrArchive; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ZoomIn();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ZoomOut();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCameraMovementDisabled(const bool bIsDisabled) { bM_IsCameraMovementDisabled = bIsDisabled; }

	void SetCameraMovementSpeedMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintCallable, Category = "ReferencesCasts")
	void InitPlayerCameraController(ACameraPawn* NewCameraRef, USpringArmComponent* NewSpringarmRef);

	/**
	 * @brief Sets the playable-area bounds used to block camera movement beyond the map.
	 * @param NewPlayableAreaCenter World-space center of the playable area.
	 * @param NewPlayableAreaExtent Half-size of the playable square in Unreal units.
	 */
	void SetPlayableAreaBounds(const FVector& NewPlayableAreaCenter, float NewPlayableAreaExtent);

	/**
	 * @brief Registers an additional cached boundary that is evaluated together with FOW limits.
	 * @param BoundaryRegistrationParams Defines the boundary actor, ID and blocked-side settings.
	 * @return True if cached boundary constraints were generated and stored successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	bool RegisterAdditionalCameraBoundary(const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams);

	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	bool RemoveAdditionalCameraBoundaryById(FName BoundaryId);

	UFUNCTION(BlueprintCallable, Category = "Camera|PlayableBounds")
	void RemoveAllAdditionalCameraBounds();

	void ResetCameraToBaseZoomLevel() const;
	void SetCustomCameraZoomLevel(const float NewZoomLevel) const;

	/**
	 * @brief Moves the camera when the mouse hits the corners of the viewport.
	 * Uses edgeScrollSpeed x and y to determine how fast the camera moves.
	 * @param DeltaTime
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void EdgeScroll(const float DeltaTime);

	/**
	 * @brief Moves the camera either forward left, forward right, backward left or backward right
	 * depending on the input.
	 * @param bOnForward: if the camera should be moved forward or backward.
	 * @param AxisX: input axis value, used in direction calculation.
	 * @param bOnRight: if the camera should move to the right or to the left.
	 * @param AxisY: input axis value, used in direction calculation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ForwardRightMovement(bool bOnForward, float AxisX, bool bOnRight, float AxisY);

	// Resets the camera to start-off pan position.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void PanReset();

	/**
	 * @brief Moves the camera over time based on the provided FMovePlayerCamera struct.
	 * If a move is already in progress, it is queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveCameraOverTime(const FMovePlayerCamera& NewMove);


	virtual void PostInitProperties() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool GetIsLockedOrDisabled() const;
	// References
	UPROPERTY()
	ACameraPawn* M_PlayerCamera;

	UPROPERTY()
	USpringArmComponent* M_SpringArmComponent;

	// Edge scroll speeds.
	float EdgeScrollSpeedX;
	float EdgeScrollSpeedY;
	float M_EdgeScrollAccelX;
	float M_EdgeScrollAccelY;
	float M_CameraMovementSpeedMultiplier;

	bool bM_IsCameraMovementDisabled;
	bool bM_IsPlayerInTechTreeOrArchive;
	float M_BaseCameraZoomLevel;

	TArray<FMovePlayerCamera> CameraMoveQueue;
	bool bIsCameraMoving = false;
	FMovePlayerCamera CurrentMove;
	FVector MoveStartLocation;
	float MoveStartTime = 0.0f;
	float MoveDuration = 0.0f;
	FTimerHandle MoveCameraTimerHandle;

	// Callback when the current move is complete.
	void OnCameraMoveComplete();

	bool GetIsValidCameraPawn() const;
	bool GetIsValidSpringArmComponent() const;

	void GetViewportSizeAndMouse(
		float& OutScreenX,
		float& OutScreenY,
		float& OutMouseX,
		float& OutMouseY,
		bool& bOutIsMouseInViewport) const;

	bool GetCanMoveCameraByWorldDelta(const FVector& WorldDelta) const;
	bool GetCanMoveCameraToLocation(const FVector& TargetCameraLocation) const;
	bool GetAreAllPlaneConstraintsSatisfied(
		const FVector& TargetCameraLocation,
		const TArray<FPlayerCameraBoundaryPlaneConstraint>& PlaneConstraints) const;
	bool GetIsBoundaryIdValidForRegistration(const FName& BoundaryId, const FString& CallingFunctionName) const;
	bool GetCanBuildBoundaryFromActor(const AActor* BoundaryActor, const FString& CallingFunctionName) const;
	bool GetHasSideFlag(const int32 BlockedSideFlags, const ECameraBoundaryBlockedSides SideFlag) const;
	bool GetHasAnyBlockedSideFlags(const int32 BlockedSideFlags) const;
	void SetFowBoundaryConstraints(const FVector& NewPlayableAreaCenter, const float NewPlayableAreaExtent);
	void AddPlaneConstraint(
		const FVector& PlaneOrigin,
		const FVector& PlaneNormal,
		const bool bAllowPositiveSide,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	/**
	 * @brief Builds a directional boundary from a specific boundary face and limits it to the opposite-axis span.
	 * This keeps blocked movement local to the actor bounds instead of creating a global half-plane.
	 * @param PlaneAxisValue Plane position on the plane-normal axis.
	 * @param PlaneNormal Axis that determines the blocked side.
	 * @param bAllowPositiveSide True when movement is allowed on the positive side of the plane.
	 * @param SpanAxis Axis used to clamp where this boundary is active.
	 * @param SpanAxisMin Lower span limit on SpanAxis.
	 * @param SpanAxisMax Upper span limit on SpanAxis.
	 * @param OutConstraints Output array receiving the generated constraint.
	 */
	void AddDirectionalSideConstraintWithSpan(
		const float PlaneAxisValue,
		const FVector& PlaneNormal,
		const bool bAllowPositiveSide,
		const FVector& SpanAxis,
		const float SpanAxisMin,
		const float SpanAxisMax,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	void AddBoxBoundaryConstraints(const FBox& WorldSpaceBox, TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	/**
	 * @brief Calculates boundary-face positions and span limits from actor bounds in the selected axis space.
	 * This allows side flags to block from the actor boundary face instead of from actor origin.
	 * @param BoundaryActor Actor whose component bounds define the boundary area.
	 * @param AxisSpaceForBlockedSides Chooses world or actor-local axes for side evaluation.
	 * @param OutAxisX Returned X axis used for side checks.
	 * @param OutAxisY Returned Y axis used for span checks.
	 * @param OutMinX Returned minimum projection on OutAxisX.
	 * @param OutMaxX Returned maximum projection on OutAxisX.
	 * @param OutMinY Returned minimum projection on OutAxisY.
	 * @param OutMaxY Returned maximum projection on OutAxisY.
	 * @return True when valid axes and bounds were available to build directional constraints.
	 */
	bool TryGetBoundaryAxisExtents(
		const AActor* BoundaryActor,
		const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
		FVector& OutAxisX,
		FVector& OutAxisY,
		float& OutMinX,
		float& OutMaxX,
		float& OutMinY,
		float& OutMaxY) const;
	/**
	 * @brief Converts blocked-side settings into cached plane constraints so side rules are evaluated cheaply at runtime.
	 * @param BoundaryActor Source actor that provides origin and optional local axis directions.
	 * @param AxisSpaceForBlockedSides Chooses whether blocked side checks use world or actor-local axes.
	 * @param BlockedSideFlags Bitmask of blocked sides that should become plane constraints.
	 * @param OutConstraints Output array receiving generated constraints.
	 */
	void AddDirectionalSideConstraints(
		const AActor* BoundaryActor,
		const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
		const int32 BlockedSideFlags,
		TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const;
	void RebuildCachedAdditionalPlaneConstraints();
	void TryMoveCameraByWorldDelta(const FVector& WorldDelta) const;
	void TryMoveCameraToLocation(const FVector& TargetCameraLocation) const;

	bool bM_IsCameraLocked = false;
	bool bM_HasFowPlayableAreaBounds = false;
	// Cached constraints generated from the FOW map center/extent.
	UPROPERTY()
	TArray<FPlayerCameraBoundaryPlaneConstraint> M_FowPlaneConstraints = {};

	// Flattened constraints from all additional submissions, rebuilt only when registrations change.
	UPROPERTY()
	TArray<FPlayerCameraBoundaryPlaneConstraint> M_CachedAdditionalPlaneConstraints = {};

	UPROPERTY()
	TArray<FPlayerCameraBoundarySubmissionCache> M_AdditionalBoundarySubmissionCaches = {};
};
