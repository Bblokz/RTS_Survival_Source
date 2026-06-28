// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "W_RewardCardsViewer.generated.h"

/**
 * @brief Used by the mission map description to preview card rewards on demand.
 */
UCLASS()
class RTS_SURVIVAL_API UW_RewardCardsViewer : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetupCardViewer(const TArray<ERTSCard>& CardRewards);
	void ShowVisibleAnimation();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Map Item")
	void BP_ShowVisibleAnimation();
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_1 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_2 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_3 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_4 = nullptr;

private:
	TArray<UW_RTSCard*> GetRewardCardWidgets() const;
	void SetupRewardCard(UW_RTSCard* RewardCardWidget, ERTSCard CardReward) const;
};
