// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_ResourceDescription.h"

#include "Components/RichTextBlock.h"

void UW_ResourceDescription::SetText(const FText& Text)
{
	if (IsValid(RichText))
	{
		RichText->SetText(Text);
	}
}
