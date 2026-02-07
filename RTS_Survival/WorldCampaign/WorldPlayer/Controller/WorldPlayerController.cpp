// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "EngineUtils.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "WorldCameraController/WorldCameraController.h"
#include "WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"

void AWorldPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_WorldCameraController = FindComponentByClass<UWorldCameraController>();
	M_WorldProfileAndUIManager = FindComponentByClass<UWorldProfileAndUIManager>();
}

void AWorldPlayerController::BeginPlay()
{
	Super::BeginPlay();
	Beginplay_SetupWorldGenerator();
	BeginPlay_SetupWorldMenu();
	BeginPlay_GenerateOrLoadWorld();
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

void AWorldPlayerController::Beginplay_SetupWorldGenerator()
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
	if (not IsValid(WorldGenerator))
	{
		RTSFunctionLibrary::ReportError("did not find a valid world generator."
			"for the Beginplay_HandleWorldGeneration in WorldPlayerController");
		return;
	}
	M_WorldGenerator = WorldGenerator;
}

void AWorldPlayerController::BeginPlay_SetupWorldMenu()
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}
	M_WorldProfileAndUIManager->SetupWorldMenu(this);
	FInputModeGameAndUI NewInputMode;
	NewInputMode.SetHideCursorDuringCapture(false);
	NewInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(NewInputMode);
	bShowMouseCursor = true;
}

void AWorldPlayerController::BeginPlay_GenerateOrLoadWorld()
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not GameInstance)
	{
		RTSFunctionLibrary::ReportError(
			"GameInstance not valid in WorldPlayerController::BeginPlay_InitWorldGeneratorWithGameInstance");
		return;
	}
	const FCampaignGenerationSettings CampaignSettings = GameInstance->GetCampaignGenerationSettings();
	const FRTSGameDifficulty SelectedDifficulty = GameInstance->GetSelectedGameDifficulty();
	ERTSFaction PlayerFaction = GameInstance->GetPlayerFaction();
	// Starts world generation if needed because of the settings.
	// todo need to handle loading a saved world here later.
	M_WorldGenerator->InitializeWorldGenerator(
		this,
		CampaignSettings,
		SelectedDifficulty
	);
	M_CampaignSettings = CampaignSettings;
	M_SelectedDifficulty = SelectedDifficulty;
	if (CampaignSettings.bNeedsToGenerateCampaign)
	{
		M_WorldProfileAndUIManager->OnSetupUIForNewCampaign(PlayerFaction);
	}
	else
	{
		// todo in case the world was loaded from save we probably want to provide a const ref to the player profile data here later.
	}
}

bool AWorldPlayerController::GetIsValidWorldProfileAndUIManager() const
{
	if (not M_WorldProfileAndUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_WorldProfileAndUIManager"),
			TEXT("GetIsValidWorldProfileAndUIManager"),
			this
		);
		return false;
	}
	return true;
}

bool AWorldPlayerController::GetIsValidWorldGenerator() const
{
	if (not M_WorldGenerator.IsValid())
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
