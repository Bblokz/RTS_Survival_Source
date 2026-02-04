// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
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

	bool GetIsValidCameraCarrierActor() const;
	bool GetIsValidSpringArmComponent() const;

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

	FWorldCameraAxisInputState M_AxisInputState;
	FWorldCameraMoveState M_MoveState;

	float M_CameraInputDisabledRemainingSeconds = 0.0f;
	bool bM_IsCameraMovementDisabled = false;
};
