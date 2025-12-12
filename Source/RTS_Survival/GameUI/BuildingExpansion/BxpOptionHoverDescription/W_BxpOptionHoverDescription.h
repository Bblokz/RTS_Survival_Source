// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_BxpOptionHoverDescription.generated.h"

class URichTextBlock;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_BxpOptionHoverDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetText(const FString& NewText);

protected:
	UPROPERTY(meta = (BindWidget))
	URichTextBlock* HoverDescription;
};
