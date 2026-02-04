// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"

#include "Components/Button.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_FactionPopup::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetIsValidAcceptButton())
	{
		return;
	}

	M_AcceptButton->OnClicked.AddUniqueDynamic(this, &UW_FactionPopup::HandleAcceptClicked);
}

void UW_FactionPopup::HandleAcceptClicked()
{
	OnPopupAccepted.Broadcast();
}

bool UW_FactionPopup::GetIsValidAcceptButton() const
{
	if (not IsValid(M_AcceptButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_AcceptButton",
			"GetIsValidAcceptButton",
			this
		);
		return false;
	}

	return true;
}
