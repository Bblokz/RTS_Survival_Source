// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RewardStructs/FMissionRewardStructs.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "W_MissionReward.generated.h"

DECLARE_MULTICAST_DELEGATE(FMissionRewardHoverDelegate);

/**
 * @brief Used by map item descriptions to display mission reward values.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MissionReward : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/**
	 * @brief Refreshes all reward rows from clicked map-object reward data.
	 * @param PrimaryReward Rewards shown immediately and used by card slots.
	 * @param SecondaryReward Optional follow-up reward data shown separately.
	 */
	void SetupReward(const FPrimaryReward& PrimaryReward, const FSecondaryReward& SecondaryReward);
	FMissionRewardHoverDelegate& OnMissionRewardHovered() { return M_OnMissionRewardHovered; }
	FMissionRewardHoverDelegate& OnMissionRewardUnhovered() { return M_OnMissionRewardUnhovered; }
	
protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_1 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_2 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_3 = nullptr;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_RTSCard> BP_RTS_Card_4 = nullptr;
	
	// By default has <DisplayAmount>+200</><img id="Radixite"/><Radixite>Radixite</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_RadixiteBonusRichText;
	
	// By default has <DisplayAmount>+200</><img id="Radixite"/><Exp>Exp</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_CommandExpBonusRichText;
	
	// by default has <DisplayAmount>+2</><img id="WeaponBlueprint"/> <DisplayAmount>+2</><img id="VehicleBlueprint"/> 
	// <DisplayAmount>+2</><img id="ConstructionBlueprint"/> <DisplayAmount>+2</><img id="EnergyBlueprint"/>  <Armor>Blueprints</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_BlueprintBonuses;

	// By default has: <Armor>Secondary:</> <DisplayAmount>+200</><img id="Radixite"/><Radixite>Radixite</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_SecondaryBonuses;

private:
	FMissionRewardHoverDelegate M_OnMissionRewardHovered;
	FMissionRewardHoverDelegate M_OnMissionRewardUnhovered;

	TArray<UW_RTSCard*> GetRewardCardWidgets() const;
	void SetupRewardCards(const TArray<ERTSCard>& CardRewards) const;
};
