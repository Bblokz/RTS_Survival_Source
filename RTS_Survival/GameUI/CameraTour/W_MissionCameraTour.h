// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"

#include "W_MissionCameraTour.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UPanelWidget;
class USizeBox;
class UTextBlock;

/**
 * @brief Overlay of a mission camera tour: cinema bars and one caption per shot (title between two brass rules, a
 * smaller line under it) bottom centre. Has no clock of its own: UMissionCameraTourSession creates it with ShowTour
 * and drives it every frame, so the captions stay in step with the camera.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MissionCameraTour : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_MissionCameraTour(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Creates the overlay above the HUD with the bars hidden; SetBars and SetCaption show it.
	 * @param OwningPlayer Player whose screen shows the tour.
	 * @param bShowLetterbox Whether SetBars draws cinema bars at all.
	 * @param LetterboxHeightFraction Height of each bar once fully in.
	 * @return The overlay on screen, or nullptr when it could not be created.
	 */
	static UW_MissionCameraTour* ShowTour(APlayerController* OwningPlayer, const bool bShowLetterbox,
	                                      const float LetterboxHeightFraction);

	/** @param BarsShown 0 = bars out of the screen, 1 = fully in. */
	void SetBars(const float BarsShown) const;

	/** Replaces the caption texts; an empty title hides the caption. */
	void SetCaption(const FText& Title, const FText& Subtitle);

	/** @param CaptionShown 0..1 fade of the caption (the session eases it in and out per shot). */
	void SetCaptionVisibility(const float CaptionShown) const;

	void CloseTour();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FSlateFontInfo SubtitleFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FLinearColor TextColor = FLinearColor(FColor::FromHex(TEXT("ECE8DCFF")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FLinearColor SubtitleColor = FLinearColor(FColor::FromHex(TEXT("C9C2B0FF")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FLinearColor AccentColor = FLinearColor(FColor::FromHex(TEXT("E2B65CFF")));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.75f);

	// Gap between the bottom bar (or the screen bottom without bars) and the caption.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	float CaptionBottomGap = 26.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Tour")
	float RuleWidth = 90.f;

private:
	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_TopBarSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_BottomBarSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_CaptionSlot;

	UPROPERTY()
	TObjectPtr<UPanelWidget> M_CaptionBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SubtitleText;

	bool bM_ShowLetterbox = true;
	float M_LetterboxHeightFraction = 0.1f;

	void BuildWidgetTree();
	void BuildBars(UCanvasPanel* Canvas);
	void BuildCaption(UCanvasPanel* Canvas);
	UTextBlock* MakeText(const FSlateFontInfo& Font, const FLinearColor& Color) const;
	USizeBox* MakeRule() const;
	void PlaceCaption() const;
	bool GetIsTreeBuilt() const;
};
