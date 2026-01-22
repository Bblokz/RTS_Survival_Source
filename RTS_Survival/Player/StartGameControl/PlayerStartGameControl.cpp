// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "PlayerStartGameControl.h"

#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/GameUI/StartGameWidget/W_StartGameWidget.h"

namespace PlayerStartGameControlDefaults
{
	constexpr int32 StartGameWidgetZOrder = 1000;
}

UPlayerStartGameControl::UPlayerStartGameControl()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerStartGameControl::InitPlayerStartGameControl(ACPPController* PlayerController)
{
	M_PlayerController = PlayerController;
}

void UPlayerStartGameControl::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitStartGameFlow();
}

void UPlayerStartGameControl::PlayerStartedGame()
{
	if (not bM_HasPausedGame)
	{
		return;
	}

	UnpauseGameAndEnableCamera();
	bM_HasPausedGame = false;

	if (not GetIsValidStartGameWidget())
	{
		return;
	}

	M_StartGameWidget->RemoveFromParent();
	M_StartGameWidget->ConditionalBeginDestroy();
	M_StartGameWidget = nullptr;
}

void UPlayerStartGameControl::BeginPlay_InitStartGameFlow()
{
	if (not GetIsValidPlayerController() || not GetIsValidStartGameWidgetClass())
	{
		return;
	}

	PauseGameAndDisableCamera();
	bM_HasPausedGame = true;

	M_StartGameWidget = CreateWidget<UW_StartGameWidget>(M_PlayerController, M_StartGameWidgetClass);
	if (not GetIsValidStartGameWidget())
	{
		return;
	}

	M_StartGameWidget->InitStartGameWidget(this);
	M_StartGameWidget->AddToViewport(PlayerStartGameControlDefaults::StartGameWidgetZOrder);
}

void UPlayerStartGameControl::PauseGameAndDisableCamera() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->PauseGame(ERTSPauseGameOptions::ForcePause);
	M_PlayerController->SetCameraMovementDisabled(true);
}

void UPlayerStartGameControl::UnpauseGameAndEnableCamera() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	M_PlayerController->SetCameraMovementDisabled(false);
}

bool UPlayerStartGameControl::GetIsValidPlayerController() const
{
	if (IsValid(M_PlayerController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PlayerController",
		"GetIsValidPlayerController",
		this
	);
	return false;
}

bool UPlayerStartGameControl::GetIsValidStartGameWidgetClass() const
{
	if (IsValid(M_StartGameWidgetClass))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StartGameWidgetClass",
		"GetIsValidStartGameWidgetClass",
		this
	);
	return false;
}

bool UPlayerStartGameControl::GetIsValidStartGameWidget() const
{
	if (IsValid(M_StartGameWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StartGameWidget",
		"GetIsValidStartGameWidget",
		this
	);
	return false;
}
