// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "W_EnemyMapItem.generated.h"

class UW_MissionReward;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_EnemyMapItem : public UUserWidget
{
	GENERATED_BODY()
	
	protected:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_MissionReward* W_BP_MissionReward = nullptr;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* M_ItemRichTitle
		
	
		UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	URichTextBlock* 
};
