// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "PlayerCardController.h"

#include "RTS_Survival/CardSystem/CardUI/CardMenu/W_CardMenu.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

class USavePlayerProfile;

APlayerCardController::APlayerCardController(const FObjectInitializer& ObjectInitializer)
{
}

bool APlayerCardController::GetIsValidCardMenu() const
{
	if(IsValid(CardMenu))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Card menu is not valid in APlayerCardController::GetIsValidCardMenu.");
	return false;
}


