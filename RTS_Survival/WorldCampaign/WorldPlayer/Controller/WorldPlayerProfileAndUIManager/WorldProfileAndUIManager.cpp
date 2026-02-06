// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldProfileAndUIManager.h"

#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"


// Sets default values for this component's properties
UWorldProfileAndUIManager::UWorldProfileAndUIManager()
{
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UWorldProfileAndUIManager::SetupWorldMenu(AWorldPlayerController* PlayerController)
{
	M_PlayerController = PlayerController;
	(void)GetIsValidPlayerController();
	UW_WorldMenu* WorldMenu = CreateWidget<UW_WorldMenu>(PlayerController, WorldMenuClass);
	if (not IsValid(WorldMenu))
	{
		RTSFunctionLibrary::ReportError("Failed to create world menu widget for the world profile and UI manager!");
		return;
	}
	M_WorldMenu = WorldMenu;
	M_WorldMenu->InitWorldMenu(PlayerController, this);
	M_WorldMenu->AddToViewport();
}

void UWorldProfileAndUIManager::OnSetupUIForNewCampaign(ERTSFaction PlayerFaction)
{
	if (not GetIsValidWorldMenu())
	{
		return;
	}
	
}


// Called when the game starts
void UWorldProfileAndUIManager::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

bool UWorldProfileAndUIManager::GetIsValidWorldMenu() const
{
	if (not M_WorldMenu.IsValid())
	{
		RTSFunctionLibrary::ReportError("world menu is not valid for the world profile and UI manager!");
		return false;
	}
	return true;
}

bool UWorldProfileAndUIManager::GetIsValidPlayerController() const
{
	if(not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportError("player controller is not valid for the world profile and UI manager!");
		return false;
	}
	return true;
}
