// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PlayerCameraController.generated.h"

class USpringArmComponent;
class ACameraPawn;

USTRUCT(BlueprintType, Blueprintable)
struct FMovePlayerCamera
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MoveToLocation = FVector::ZeroVector;

	// Time it takes to get from the current location to the new location.
	// If zero the camera will instantly move to the location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeToMove = 0.0f;

	// Time during which camera input is disabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeCameraInputDisabled = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* MoveSound = nullptr;
};

/**
 * Player Camera Controller component.
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

	UFUNCTION(BlueprintCallable, Category = "ReferencesCasts")
	void InitPlayerCameraController(ACameraPawn* NewCameraRef, USpringArmComponent* NewSpringarmRef);

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

	bool bM_IsCameraLocked = false;
};
