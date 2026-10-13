// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "W_TrainingQuickNavButton.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTrainingMenuManager;
struct FSlateBrush;

/**
 * @brief A small icon button at the top of the training menu; clicking it selects the player's first
 * expanded building of its type, which loads that building into the training menu.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingQuickNavButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitQuickNavButton(UTrainingMenuManager* TrainingMenuManager);

	/**
	 * @param Building The building type this button selects from now on.
	 * @param IconBrush The icon of that building; null shows the initials of the building instead.
	 */
	void SetupForBuilding(const ENomadicSubtype Building, const FSlateBrush* IconBrush);

	/**
	 * @param bIsBuildingAvailable Whether the player has an expanded building of this type that can train.
	 * The button takes no space when not.
	 * @param bIsBuildingShown Whether the training menu currently shows a building of this type.
	 */
	void UpdateQuickNavButton(const bool bIsBuildingAvailable, const bool bIsBuildingShown);

	ENomadicSubtype GetBuilding() const { return M_Building; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_NavButton;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> M_NavIcon;

	// Shows the initials of a building that has no icon, so that its button is never an empty square.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UTextBlock> M_NavFallbackText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Nav Button")
	FLinearColor ShownBuildingButtonColor = FLinearColor(0.75f, 0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Nav Button")
	FLinearColor OtherBuildingButtonColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.85f);

private:
	UPROPERTY()
	TWeakObjectPtr<UTrainingMenuManager> M_TrainingMenuManager;
	bool GetIsValidTrainingMenuManager() const;

	ENomadicSubtype M_Building = ENomadicSubtype::Nomadic_None;

	void ShowIcon(const FSlateBrush& IconBrush);
	void ShowInitialsInsteadOfIcon(const FString& BuildingDisplayName);

	UFUNCTION()
	void OnNavButtonClicked();
};
