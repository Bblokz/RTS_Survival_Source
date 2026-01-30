// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_HotKey.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_HotKey::SetKeyText(const FText& NewKeyText)
{
	if (not GetIsValidKeyText())
	{
		return;
	}

	M_KeyText->SetText(NewKeyText);
}

bool UW_HotKey::GetIsValidKeyText() const
{
	if (IsValid(M_KeyText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyText"),
		TEXT("UW_HotKey::GetIsValidKeyText"),
		this
	);

	return false;
}
