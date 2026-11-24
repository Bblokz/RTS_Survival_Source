// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "MissionDebriefTypes.h"

#include "W_MissionEnd.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTexture2D;

/** Look of the mission end overlay; sizes are Slate units at 1080p. */
USTRUCT(BlueprintType)
struct FMissionEndStyle
{
	GENERATED_BODY()

	FMissionEndStyle();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle ButtonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo KickerFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo ReasonFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo ButtonFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor SignalColor = FLinearColor(FColor::FromHex(TEXT("C24A3AFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor BoneColor = FLinearColor(FColor::FromHex(TEXT("E6DFCFFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor BoneDimColor = FLinearColor(FColor::FromHex(TEXT("A9A395FF")));
};

/**
 * @brief The mission end overlay over the 3D debrief stage: a fade from black, then (when the stage's timeline says so)
 * the outcome line and its buttons rise in at the bottom. Victory: Continue and optionally Main Menu; defeat: Restart
 * and Quit. Created by AMissionManager; works over the paused game without a stage too (buttons at once).
 */
UCLASS()
class RTS_SURVIVAL_API UW_MissionEnd : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_MissionEnd(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Fills in the outcome's texts and buttons; call before adding to the viewport.
	 * @param Outcome Which buttons show.
	 * @param ReasonText The line under MISSION ACCOMPLISHED / MISSION FAILED.
	 * @param ContinueLevel Victory: level that Continue opens.
	 * @param MenuLevel Victory: level that Main Menu opens.
	 * @param bShowBackToMenu Victory: whether Main Menu shows.
	 */
	void SetupEndScreen(const EMissionDebriefOutcome Outcome, const FText& ReasonText,
	                    const TSoftObjectPtr<UWorld>& ContinueLevel, const TSoftObjectPtr<UWorld>& MenuLevel,
	                    const bool bShowBackToMenu);

	// Starts the rise-in of the outcome line and buttons.
	void RevealButtons();

	// Hides the fade from black at once (used when there is no stage to fade into).
	void SkipFadeIn();

	bool GetAreButtonsRevealed() const { return bM_AreButtonsRevealed; }
	EMissionDebriefOutcome GetOutcome() const { return M_Outcome; }

	// Continue on victory, Restart on defeat; tests press it.
	UButton* GetPrimaryButton() const { return M_PrimaryButton; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission End")
	FMissionEndStyle Style;

private:
	UPROPERTY()
	TObjectPtr<UImage> M_FadeBlack;

	UPROPERTY()
	TObjectPtr<UImage> M_BottomShade;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_EndBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_KickerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_ReasonText;

	UPROPERTY()
	TObjectPtr<UButton> M_PrimaryButton;

	UPROPERTY()
	TObjectPtr<UButton> M_SecondaryButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PrimaryLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SecondaryLabel;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_ShadeGradient;

	UPROPERTY()
	TSoftObjectPtr<UWorld> M_ContinueLevel;

	UPROPERTY()
	TSoftObjectPtr<UWorld> M_MenuLevel;

	EMissionDebriefOutcome M_Outcome = EMissionDebriefOutcome::Victory;
	bool bM_AreButtonsRevealed = false;
	float M_RevealSeconds = 0.f;
	float M_FadeSeconds = 0.f;

	void BuildWidgetTree();
	void BuildWidgetTree_EndBar(class UCanvasPanel* Canvas);
	UButton* BuildButton(UTextBlock*& OutLabel, const bool bPrimary);
	UTexture2D* CreateShadeGradient();
	void ApplyRevealProgress(const float Progress) const;

	UFUNCTION()
	void HandlePrimaryClicked();

	UFUNCTION()
	void HandleSecondaryClicked();

	void OpenLevel(const TSoftObjectPtr<UWorld>& Level, const TCHAR* const ActionName) const;
};
