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
	if (not IsValid(CardMenu))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"CardMenu",
			"GetIsValidCardMenu",
			this
		);
		return false;
	}
	return true;
}

