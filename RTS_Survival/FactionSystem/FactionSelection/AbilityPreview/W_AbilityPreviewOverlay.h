// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/MenuLayout/MenuDesignFrame.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "W_AbilityPreviewOverlay.generated.h"

class UBorder;
class UButton;
class UDataTable;
class UImage;
class URichTextBlock;
class UTextBlock;

/**
 * @brief The only UI while an ability preview runs: the ability with its explanation along the top of the screen and
 * a BACK button at the bottom, so the rest of the view shows the desert. Builds its own widget tree with the commander
 * screen's skin. Escape, Backspace and the gamepad B button go back too.
 */
UCLASS()
class RTS_SURVIVAL_API UW_AbilityPreviewOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @param Style The commander screen's skin (fonts, colours, the BACK button art).
	 * @param RichTextStyle The <Style> tags of the ability descriptions; may be null.
	 * @param AbilitySlot The previewed ability as the commander screen lists it.
	 */
	void SetupOverlay(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
	                  const FCommanderSelectionAbilitySlot& AbilitySlot);

	/** Shown under the explanation once the ability is done: the preview returns by itself in that many seconds. */
	void ShowAutoReturnCountdown(const float SecondsLeft);

	/** Same as the BACK button. */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void RequestBack();

	UButton* GetBackButton() const { return M_BackButton; }

	FSimpleDelegate OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> M_AbilityPlate;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_HeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UBorder> M_TierBadge;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TierText;

	UPROPERTY()
	TObjectPtr<URichTextBlock> M_DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountdownText;

	UPROPERTY()
	TObjectPtr<UButton> M_BackButton;

	// Holds the 1920x1080 layout, so the plate and BACK keep their sizes and edge distances at any aspect ratio.
	UPROPERTY()
	FMenuDesignFrame M_DesignFrame;

	// The design frame's canvas; the Back button is added in SetupOverlay, because its art comes with the style.
	UPROPERTY()
	TObjectPtr<class UCanvasPanel> M_LayoutCanvas;

	void BuildWidgetTree();
	void BuildAbilityPlate();
	void BuildBackButton(const FUnitVariantSelectionStyle& Style);

	UFUNCTION()
	void OnBackClicked();
};
