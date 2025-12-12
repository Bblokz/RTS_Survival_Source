// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BxpOptionHoverDescription.h"

#include "Components/RichTextBlock.h"

void UW_BxpOptionHoverDescription::SetText(const FString& NewText)
{
	if(HoverDescription)
	{
		HoverDescription->SetText(FText::FromString(NewText));
	}
}
