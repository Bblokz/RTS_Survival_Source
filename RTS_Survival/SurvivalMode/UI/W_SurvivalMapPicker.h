// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"

#include "W_SurvivalMapPicker.generated.h"

class UTextBlock;
class UVerticalBox;
class USurvivalUiStyleAsset;
class UW_SurvivalMapPicker;
struct FSurvivalUiStyle;

/** @brief One survival map as the main menu offers it. */
USTRUCT()
struct FSurvivalMapChoice
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Description;
};

/** @brief A card that tells the map picker which map it stands for. */
UCLASS()
class RTS_SURVIVAL_API USurvivalMapChoiceButton : public UButton
{
	GENERATED_BODY()

public:
	void InitChoiceButton(UW_SurvivalMapPicker* Picker, const int32 ChoiceIndex);

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UW_SurvivalMapPicker> M_Picker;
	int32 M_ChoiceIndex = INDEX_NONE;
};

/**
 * @brief The SURVIVAL button opens this when more than one survival map exists: one card per map, then the normal
 * faction, variant and difficulty screens. Built in code, styled by DA_SurvivalUiStyle.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalMapPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitMapPicker(const TArray<FSurvivalMapChoice>& Choices);

	/** @brief Called by a map card; starts the pre-game flow towards that map. */
	void ChooseMap(const int32 ChoiceIndex);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_CardColumn = nullptr;

	UPROPERTY()
	TArray<FSurvivalMapChoice> M_Choices;

	const FSurvivalUiStyle* GetStyle() const;
	void StyleText(UTextBlock* Text, const bool bHeading, const int32 Size, const FSlateColor& Color) const;
	void BuildTree();
	UWidget* BuildCard(const FSurvivalMapChoice& Choice, const int32 ChoiceIndex);

	UFUNCTION()
	void OnBackClicked();
};
