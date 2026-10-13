// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_TrainingSubSectionTab.generated.h"

class UButton;
class UTextBlock;
class UTrainingMenuManager;

/**
 * @brief One tab above the training items; the training menu places up to three of them in a row and
 * the player clicks one to see the training items of that sub section.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingSubSectionTab : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSubSectionTab(UTrainingMenuManager* TrainingMenuManager, const int32 SubSectionIndex);

	/**
	 * @param DisplayName The name of the sub section.
	 * @param QueuedCount How many units of this sub section are in the training queue; zero hides the count.
	 * @param bIsSelected Whether the training items of this sub section are the ones on screen.
	 */
	void UpdateSubSectionTab(const FText& DisplayName, const int32 QueuedCount, const bool bIsSelected);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_TabButton;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UTextBlock> M_TabText;

	// Shows how many units of the sub section are queued; a tab without it only shows its name.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> M_TabCountText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sub Section Tab")
	FLinearColor SelectedButtonColor = FLinearColor(0.75f, 0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sub Section Tab")
	FLinearColor UnselectedButtonColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sub Section Tab")
	FLinearColor SelectedTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sub Section Tab")
	FLinearColor UnselectedTextColor = FLinearColor(0.65f, 0.65f, 0.65f, 1.0f);

private:
	UPROPERTY()
	TWeakObjectPtr<UTrainingMenuManager> M_TrainingMenuManager;
	bool GetIsValidTrainingMenuManager() const;

	int32 M_SubSectionIndex = INDEX_NONE;

	UFUNCTION()
	void OnTabClicked();
};
