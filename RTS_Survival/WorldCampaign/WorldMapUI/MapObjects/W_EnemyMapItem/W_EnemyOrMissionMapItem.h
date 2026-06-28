// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "W_EnemyOrMissionMapItem.generated.h"

class UW_StrengthEstimation;
class UW_RewardCardsViewer;
struct FEnemyOrMissionMapItemUIData;
class UW_MissionReward;
/**
 * @brief Used by world menu to show clicked mission or enemy map item data.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EnemyOrMissionMapItem : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitAuxiliaryWidgets(TWeakObjectPtr<UW_RewardCardsViewer> RewardCardsViewer, TWeakObjectPtr<UW_StrengthEstimation> StrengthEstimator);

	/**
	 * @brief Applies map-object data after selection so Blueprint widgets stay data-only.
	 * @param UIData Text and strength data authored on the clicked world map object.
	 * @param PrimaryReward Rewards shown immediately and used by the card preview.
	 * @param SecondaryReward Optional follow-up reward data shown in the reward panel.
	 */
	void SetupEnemyWidget(const FEnemyOrMissionMapItemUIData& UIData,
	                      const FPrimaryReward& PrimaryReward,
	                      const FSecondaryReward& SecondaryReward);
	void ShowVisibleAnimation();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Map Item")
	void BP_ShowVisibleAnimation();

	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ItemRichTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> M_StrengthButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> M_LaunchButton = nullptr;

	// By default contains <Text_NewBad>20%</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_RichStrengthText;

	// by default <Text_Armor>This CRD Base periodically produces 
	// light vehicles which reinforce nearby CRD elements.</><Text_Armor> Destroy 
	// it to construct a resistance factory in its place.</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ItemRichDescription;

	// by default <Text_Armor>Destroy the factory.</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_PrimaryObjDesc;

	// by default <Text_Armor>Get Radixite Info.</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_SecondaryObjDesc;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UW_MissionReward> W_BP_MissionReward = nullptr;

	// Important note: needs to make use of line breaks.
	// by default contains: <Text_BadTitle>-</> <Text_Bad14>Enemy strength increases by 20%</>
	//<Text_BadTitle>-</> <Text_Bad14>CRD will attempt to reinforce the position with extra divisions</>
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_OnDefeatRichText;
	
private:
	UFUNCTION()
	void OnStrengthButtonHovered();
	UFUNCTION()
	void OnStrengthButtonUnhovered();

	void OnMissionRewardHovered();
	void OnMissionRewardUnhovered();
	void SetTextIfValid(URichTextBlock* RichTextBlock, const FText& Text) const;
	[[nodiscard]] bool GetIsValidStrengthButton() const;
	[[nodiscard]] bool GetIsValidMissionReward() const;

	UPROPERTY()
	TWeakObjectPtr<UW_RewardCardsViewer> M_RewardCardsViewer;
	[[nodiscard]] bool EnsureIsValidRewardCardsViewer() const;
	
	UPROPERTY()
	TWeakObjectPtr<UW_StrengthEstimation> M_StrengthEstimation;
	[[nodiscard]] bool EnsureIsValidStrengthEstimation() const;
};
