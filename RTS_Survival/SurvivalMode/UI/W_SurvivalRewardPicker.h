// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalRewardTypes.h"

#include "W_SurvivalRewardPicker.generated.h"

class ASurvivalModeDirector;
class UButton;
class UDataTable;
class UHorizontalBox;
class USurvivalUiStyleAsset;
class UTextBlock;
class UTexture2D;
class UUnitVariantDataAsset;
class UVerticalBox;

/**
 * @brief Between waves: the reinforcement sets the player can pick, one card per option (theme, units with icon
 * and variant, supply worth) plus the intermission countdown. Clicking a card calls
 * ASurvivalModeDirector::ChooseReward. Builds its own widget tree, so it works without a Blueprint child; the
 * game keeps running and only the cards take the mouse.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalRewardPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the cards; called by the director right after the widget is added to the viewport.
	 * @param Director Receives the pick.
	 * @param RoundNumber The wave just survived.
	 * @param Options The sets to offer.
	 */
	void InitRewardPicker(ASurvivalModeDirector* Director, int32 RoundNumber,
	                      const TArray<FSurvivalRewardOption>& Options);

	/** @brief Disables the cards and highlights the chosen one once a pick was made (by click or timeout). */
	void ShowChosen(int32 OptionIndex);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// Same look as the other survival widgets; empty loads DA_SurvivalUiStyle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset = nullptr;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TimerText = nullptr;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_CardRow = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> M_CardButtons;

	// Keeps loaded variant icons alive while the cards show them.
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_LoadedIcons;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable = nullptr;

	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset = nullptr;

	bool bM_HasChosen = false;
	FTimerHandle M_TimerRefreshHandle;

	void BuildTree();
	UWidget* BuildCard(const FSurvivalRewardOption& Option, int32 OptionIndex);
	UWidget* BuildUnitLine(const FSurvivalArmyEntry& Entry, int32 Count);
	FSlateBrush MakeIconBrush(const FTrainingOption& Unit, EUnitVariant Variant);
	void RefreshTimer();
	void StyleText(UTextBlock* Text, bool bHeading, int32 Size, const FSlateColor& Color) const;
	const struct FSurvivalUiStyle* GetStyle() const;

	UFUNCTION()
	void OnCard0Clicked();

	UFUNCTION()
	void OnCard1Clicked();

	UFUNCTION()
	void OnCard2Clicked();

	UFUNCTION()
	void OnCard3Clicked();

	UFUNCTION()
	void OnCard4Clicked();

	UFUNCTION()
	void OnCard5Clicked();

	void OnCardClicked(int32 OptionIndex);
};
