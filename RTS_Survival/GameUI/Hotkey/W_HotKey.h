// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_HotKey.generated.h"

class URichTextBlock;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_HotKey : public UUserWidget
{
	GENERATED_BODY()

protected:
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_KeyText;
};
