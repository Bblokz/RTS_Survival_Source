// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "W_RewardCardsViewer.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_RewardCardsViewer : public UUserWidget
{
	GENERATED_BODY()
	
	public:
	void SetupCardViewer(const TArray<ERTSCard> CardRewards);
	
	protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_1  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_2  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_3  = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_RTSCard* BP_RTS_Card_4  = nullptr;
	
};
