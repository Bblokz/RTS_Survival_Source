// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "W_GA_Description.generated.h"

class UImage;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GA_Description : public UUserWidget
{
	GENERATED_BODY()
	
	
	protected:
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr< UW_CostDisplay> CostDisplay;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> Title;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> AbilityIcon;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> Description;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> Cooldown;
	
	
};
