// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldCameraController.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UWorldCameraController::UWorldCameraController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldCameraController::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitCameraCarrier();
	BeginPlay_InitSpringArmComponent();
}

void UWorldCameraController::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCameraInputDisabled(DeltaTime);
	UpdateMoveToLocation(DeltaTime);
	UpdateAxisMovement(DeltaTime);
}

void UWorldCameraController::SetIsCameraMovementDisabled(const bool bIsDisabled)
{
	bM_IsCameraMovementDisabled = bIsDisabled;
}

bool UWorldCameraController::GetIsCameraMovementDisabled() const
{
	return bM_IsCameraMovementDisabled || M_CameraInputDisabledRemainingSeconds > 0.0f;
}

void UWorldCameraController::ZoomIn()
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (GetIsValidSpringArmComponent())
	{
		const float NewTargetLength = FMath::Clamp(M_SpringArmComponent->TargetArmLength - M_ZoomSpeed, M_MinZoom, M_MaxZoom);
		M_SpringArmComponent->TargetArmLength = NewTargetLength;
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector CurrentLocation = CameraCarrier->GetActorLocation();
	const FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, -M_ZoomSpeed);
	CameraCarrier->SetActorLocation(NewLocation, true);
}

void UWorldCameraController::ZoomOut()
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (GetIsValidSpringArmComponent())
	{
		const float NewTargetLength = FMath::Clamp(M_SpringArmComponent->TargetArmLength + M_ZoomSpeed, M_MinZoom, M_MaxZoom);
		M_SpringArmComponent->TargetArmLength = NewTargetLength;
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector CurrentLocation = CameraCarrier->GetActorLocation();
	const FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, M_ZoomSpeed);
	CameraCarrier->SetActorLocation(NewLocation, true);
}

void UWorldCameraController::ForwardMovement(const float AxisValue)
{
	M_AxisInputState.M_ForwardAxisValue = AxisValue;
}

void UWorldCameraController::RightMovement(const float AxisValue)
{
	M_AxisInputState.M_RightAxisValue = AxisValue;
}

void UWorldCameraController::MoveCameraTo(const FMovePlayerCamera& MoveRequest)
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	if (IsValid(MoveRequest.MoveSound))
	{
		UGameplayStatics::PlaySound2D(this, MoveRequest.MoveSound);
	}

	M_CameraInputDisabledRemainingSeconds = FMath::Max(
		M_CameraInputDisabledRemainingSeconds,
		MoveRequest.TimeCameraInputDisabled
	);

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	if (MoveRequest.TimeToMove <= 0.0f)
	{
		CameraCarrier->SetActorLocation(MoveRequest.MoveToLocation, true);
		M_MoveState.bM_IsMoveActive = false;
		return;
	}

	M_MoveState.M_StartLocation = CameraCarrier->GetActorLocation();
	M_MoveState.M_TargetLocation = MoveRequest.MoveToLocation;
	M_MoveState.M_ElapsedTime = 0.0f;
	M_MoveState.M_TotalTime = MoveRequest.TimeToMove;
	M_MoveState.bM_IsMoveActive = true;
}

void UWorldCameraController::BeginPlay_InitCameraCarrier()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	if (APlayerController* const PlayerController = Cast<APlayerController>(OwnerActor))
	{
		if (APawn* const PossessedPawn = PlayerController->GetPawn())
		{
			M_CameraCarrierActor = PossessedPawn;
			return;
		}
	}

	M_CameraCarrierActor = OwnerActor;
}

void UWorldCameraController::BeginPlay_InitSpringArmComponent()
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	M_SpringArmComponent = CameraCarrier->FindComponentByClass<USpringArmComponent>();
}

void UWorldCameraController::UpdateCameraInputDisabled(const float DeltaTime)
{
	if (M_CameraInputDisabledRemainingSeconds <= 0.0f)
	{
		M_CameraInputDisabledRemainingSeconds = 0.0f;
		return;
	}

	M_CameraInputDisabledRemainingSeconds = FMath::Max(M_CameraInputDisabledRemainingSeconds - DeltaTime, 0.0f);
}

void UWorldCameraController::UpdateMoveToLocation(const float DeltaTime)
{
	if (not M_MoveState.bM_IsMoveActive)
	{
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		M_MoveState.bM_IsMoveActive = false;
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	if (M_MoveState.M_TotalTime <= 0.0f)
	{
		CameraCarrier->SetActorLocation(M_MoveState.M_TargetLocation, true);
		M_MoveState.bM_IsMoveActive = false;
		return;
	}

	M_MoveState.M_ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(M_MoveState.M_ElapsedTime / M_MoveState.M_TotalTime, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(M_MoveState.M_StartLocation, M_MoveState.M_TargetLocation, Alpha);
	CameraCarrier->SetActorLocation(NewLocation, true);

	if (Alpha >= 1.0f)
	{
		CameraCarrier->SetActorLocation(M_MoveState.M_TargetLocation, true);
		M_MoveState.bM_IsMoveActive = false;
	}
}

void UWorldCameraController::UpdateAxisMovement(const float DeltaTime)
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	constexpr float AxisDeadzone = KINDA_SMALL_NUMBER;
	const float ForwardAxisValue = FMath::Abs(M_AxisInputState.M_ForwardAxisValue) > AxisDeadzone
		? M_AxisInputState.M_ForwardAxisValue
		: 0.0f;
	const float RightAxisValue = FMath::Abs(M_AxisInputState.M_RightAxisValue) > AxisDeadzone
		? M_AxisInputState.M_RightAxisValue
		: 0.0f;
	if (ForwardAxisValue == 0.0f && RightAxisValue == 0.0f)
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector ForwardVector = CameraCarrier->GetActorForwardVector();
	const FVector RightVector = CameraCarrier->GetActorRightVector();
	const FVector WorldDelta = (ForwardVector * ForwardAxisValue + RightVector * RightAxisValue) * M_XYSpeed * DeltaTime;
	CameraCarrier->AddActorWorldOffset(WorldDelta, true);
}

bool UWorldCameraController::GetIsValidCameraCarrierActor() const
{
	if (M_CameraCarrierActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CameraCarrierActor"),
		TEXT("GetIsValidCameraCarrierActor"),
		this
	);
	return false;
}

bool UWorldCameraController::GetIsValidSpringArmComponent() const
{
	if (M_SpringArmComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SpringArmComponent"),
		TEXT("GetIsValidSpringArmComponent"),
		this
	);
	return false;
}
