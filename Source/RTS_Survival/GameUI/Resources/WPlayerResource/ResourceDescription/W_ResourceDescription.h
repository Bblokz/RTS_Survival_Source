// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_ResourceDescription.generated.h"

class URichTextBlock;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_ResourceDescription : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetText(const FText& Text);
protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* RichText;
};
