// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "EngineUtils.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "WorldCameraController/WorldCameraController.h"

void AWorldPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_WorldCameraController = FindComponentByClass<UWorldCameraController>();
}

void AWorldPlayerController::BeginPlay()
{
	Super::BeginPlay();
	Beginplay_HandleWorldGeneration();
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

void AWorldPlayerController::Beginplay_HandleWorldGeneration()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	AGeneratorWorldCampaign* WorldGenerator = nullptr;
	// Find the first instance of AGeneratorWorldCampaign in the world
	for (TActorIterator<AGeneratorWorldCampaign> It(World); It; ++It)
	{
		WorldGenerator = *It;
		if (WorldGenerator)
		{
			break;
		}
	}
	if(not IsValid(WorldGenerator))
	{
		RTSFunctionLibrary::ReportError("did not find a valid world generator."
								  "for the Beginplay_HandleWorldGeneration in WorldPlayerController");
		return;
	}
	M_WorldGenerator = WorldGenerator;
}

bool AWorldPlayerController::GetIsValidWorldGenerator() const
{
	if(not M_WorldGenerator.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_WorldGenerator"),
			TEXT("GetIsValidWorldGenerator"),
			this
		);
		return false;
	}
	return true;
}
