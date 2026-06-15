// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_GlobalAbilityPanel.generated.h"

class UW_GA_Item;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GlobalAbilityPanel : public UUserWidget
{
	GENERATED_BODY()
	
	protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY(EditDefaultsOnly, blueprintReadOnly)
	TSubclassOf<UW_GA_Item> AbilityWidgetClass;
};
