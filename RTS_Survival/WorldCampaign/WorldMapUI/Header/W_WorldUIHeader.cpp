// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_WorldUIHeader.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/WorldUIFocusState/WorldUIFocusState.h"

void UW_WorldUIHeader::InitWorldUIHeader(UW_WorldMenu* WorldMenu)
{
	M_WorldMenu = WorldMenu;
}

bool UW_WorldUIHeader::EnsureButtonIsValid(const TObjectPtr<UButton>& ButtonToCheck)
{
	if(not IsValid(ButtonToCheck))
	{
		RTSFunctionLibrary::ReportError("Button Is not valid for WorldUIHeader, please check the widget blueprint.");
		return false;
	}
	return true;
}

void UW_WorldUIHeader::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bind button clicks to their respective functions
	if (EnsureButtonIsValid(M_CommandPerksButton))
	{
		M_CommandPerksButton->OnClicked.AddDynamic(this, &UW_WorldUIHeader::OnClickedPerks);
	}

	if (EnsureButtonIsValid(M_ArmyLayoutButton))
	{
		M_ArmyLayoutButton->OnClicked.AddDynamic(this, &UW_WorldUIHeader::OnClickedArmyLayout);
	}

	if (EnsureButtonIsValid(M_WorldMapButton))
	{
		M_WorldMapButton->OnClicked.AddDynamic(this, &UW_WorldUIHeader::OnClickedWorldMap);
	}

	if (EnsureButtonIsValid(M_ArchiveButton))
	{
		M_ArchiveButton->OnClicked.AddDynamic(this, &UW_WorldUIHeader::OnClickedArchive);
	}

	if (EnsureButtonIsValid(M_TechTree))
	{
		M_TechTree->OnClicked.AddDynamic(this, &UW_WorldUIHeader::OnClickedTechTree);
	}
}


void UW_WorldUIHeader::OnClickedPerks()
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::CommandPerks);
}

void UW_WorldUIHeader::OnClickedArmyLayout()
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::ArmyLayout);
}

void UW_WorldUIHeader::OnClickedWorldMap()
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::World);
}

void UW_WorldUIHeader::OnClickedArchive()
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::Archive);
}

void UW_WorldUIHeader::OnClickedTechTree()
{
	if(not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::TechTree);	
}

bool UW_WorldUIHeader::GetIsValidWorldMenu() const
{
	if(not M_WorldMenu.IsValid())
	{
		RTSFunctionLibrary::ReportError("No valid world menu set for world UI header!");
		return false;
	}
	return true;
}
