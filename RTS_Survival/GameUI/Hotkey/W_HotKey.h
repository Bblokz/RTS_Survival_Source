// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_HotKey.generated.h"

class URichTextBlock;
/**
 * @brief Rich text hotkey display widget that receives key strings from UI logic.
 */
UCLASS()
class RTS_SURVIVAL_API UW_HotKey : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetKeyText(const FText& NewKeyText);

protected:
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_KeyText;

private:
	bool GetIsValidKeyText() const;
};
