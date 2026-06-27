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

FPlayerProfileSaveData UWorldProfileAndUIManager::OnSetupUIForNewCampaign(ERTSFaction PlayerFaction)
{
	FPlayerProfileSaveData PlayerProfileSaveData;
	PlayerProfileSaveData.PlayerFaction = PlayerFaction;
	PlayerProfileSaveData.PlayerData = GetDefaultPlayerDataForFaction(PlayerFaction);
	if (not GetIsValidWorldMenu())
	{
		return PlayerProfileSaveData;
	}

	M_WorldMenu->SetupUIForPlayerProfile(PlayerProfileSaveData.PlayerData);
	return PlayerProfileSaveData;
}

void UWorldProfileAndUIManager::SetupUIForLoadedCampaign(const FPlayerProfileSaveData& PlayerProfileSaveData)
{
	if (not GetIsValidWorldMenu())
	{
		return;
	}

	M_WorldMenu->SetupUIForPlayerProfile(PlayerProfileSaveData.PlayerData);
}


// Called when the game starts
void UWorldProfileAndUIManager::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

bool UWorldProfileAndUIManager::GetIsValidWorldMenu() const
{
	if (not IsValid(M_WorldMenu))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_WorldMenu"),
			TEXT("GetIsValidWorldMenu"),
			this
		);
		return false;
	}
	return true;
}

bool UWorldProfileAndUIManager::GetIsValidPlayerController() const
{
	if (not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_PlayerController"),
			TEXT("GetIsValidPlayerController"),
			this
		);
		return false;
	}
	return true;
}

FPlayerData UWorldProfileAndUIManager::GetDefaultPlayerDataForFaction(const ERTSFaction PlayerFaction) const
{
	switch (PlayerFaction)
	{
		case ERTSFaction::GerBreakthroughDoctrine:
			return M_DefaultPlayerProfiles.GerBreakthroughDoctrine;
		case ERTSFaction::GerStrikeDivision:
			return M_DefaultPlayerProfiles.GerStrikeDivision;
		case ERTSFaction::GerItalianFaction:
			return M_DefaultPlayerProfiles.GerItalianFaction;
		default:
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Unsupported faction in GetDefaultPlayerDataForFaction: %d"),
				static_cast<int32>(PlayerFaction)
			));
			return M_DefaultPlayerProfiles.GerBreakthroughDoctrine;
	}
}
