// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "WorldCameraController/WorldCameraController.h"

void AWorldPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_WorldCameraController = FindComponentByClass<UWorldCameraController>();
}

void AWorldPlayerController::BeginPlay()
{
	Super::BeginPlay();

}

void AWorldPlayerController::SetIsWorldCameraMovementDisabled(const bool bIsDisabled)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->SetIsCameraMovementDisabled(bIsDisabled);
}

bool AWorldPlayerController::GetIsWorldCameraMovementDisabled() const
{
	if (not GetIsValidWorldCameraController())
	{
		return false;
	}

	return M_WorldCameraController->GetIsCameraMovementDisabled();
}

void AWorldPlayerController::WorldCamera_ZoomIn()
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomIn();
}

void AWorldPlayerController::WorldCamera_ZoomOut()
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomOut();
}

void AWorldPlayerController::WorldCamera_ForwardMovement(const float AxisValue)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ForwardMovement(AxisValue);
}

void AWorldPlayerController::WorldCamera_RightMovement(const float AxisValue)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->RightMovement(AxisValue);
}

void AWorldPlayerController::WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->MoveCameraTo(MoveRequest);
}

bool AWorldPlayerController::GetIsValidWorldCameraController() const
{
	if (M_WorldCameraController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCameraController"),
		TEXT("GetIsValidWorldCameraController"),
		this
	);
	return false;
}
