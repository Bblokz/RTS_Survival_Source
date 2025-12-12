// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_TutorialOvertake.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UW_TutorialOvertake::OvertakeUI(const UObject* WorldContextObject)
{
	if(not IsValid(WorldContextObject))
	{
		RTSFunctionLibrary::ReportError("Invalid WorldContextObject in W_TutorialOvertake::OvertakeUI");
		return;
	}
	M_MainGameUI = FRTS_Statics::GetMainGameUI(WorldContextObject);
	M_PlayerController = FRTS_Statics::GetRTSController(WorldContextObject);
	if(not EnsureMainGameUIIsValid() or not EnsurePlayerControllerIsValid())
	{
		return;
	}
	M_MainGameUI->SetMainMenuVisiblity(false);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(
		M_PlayerController.Get(), this);
	M_PlayerController->PauseAndLockGame(true);
}

void UW_TutorialOvertake::OnClickedContinue()
{
	if(not EnsurePlayerControllerIsValid())
	{
		M_PlayerController = FRTS_Statics::GetRTSController(this);
	}
	if(not EnsurePlayerControllerIsValid())
	{
		return;
	}
	M_PlayerController->PauseAndLockGame(false);
	if(not EnsureMainGameUIIsValid())
	{
		M_MainGameUI = FRTS_Statics::GetMainGameUI(this);
	}
	if(not EnsureMainGameUIIsValid())
	{
		return;
	}
	M_MainGameUI->SetMainMenuVisiblity(true);
	RemoveFromParent();
	UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(M_PlayerController.Get(), nullptr, EMouseLockMode::LockAlways,
		false, false);
}

bool UW_TutorialOvertake::EnsureMainGameUIIsValid() const
{
	if(not M_MainGameUI.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid maingame UI in W_TutorialOvertake::EnsureMainGameUIIsValid");
		return false;
	}
	return true;
}

bool UW_TutorialOvertake::EnsurePlayerControllerIsValid() const
{
	if(not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid player controller in W_TutorialOvertake::EnsurePlayerControllerIsValid");
		return false;
	}
	return true;
}
