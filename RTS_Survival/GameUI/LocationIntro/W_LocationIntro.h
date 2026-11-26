// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "LocationIntroTypes.h"

#include "W_LocationIntro.generated.h"

class UCanvasPanelSlot;
class UImage;
class UPanelWidget;
class USizeBox;
class UTextBlock;

/** Fonts, colours and sizes of the location intro; sizes in Slate units at 1080p. */
USTRUCT(BlueprintType)
struct FLocationIntroStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FSlateFontInfo LocationFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FSlateFontInfo SubtitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Date")
	FSlateFontInfo DateFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Date")
	FSlateFontInfo TimeFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor TextColor = FLinearColor(FColor::FromHex(TEXT("ECE8DCFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor SubtitleColor = FLinearColor(FColor::FromHex(TEXT("C9C2B0FF")));

	// Rules beside the location, the bar beside the date and the time line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor AccentColor = FLinearColor(FColor::FromHex(TEXT("E2B65CFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.75f);

	// Letter spacing (1/1000 em) the location settles on; it starts wide and closes in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	int32 LocationLetterSpacingEnd = 320;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	int32 LocationLetterSpacingStart = 1100;

	// Width of each rule beside the location once fully drawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	float RuleWidth = 150.f;

	// Gap between the top bar (or the screen top without bars) and the location.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	float LocationTopGap = 34.f;

	// Distance of the date from the left screen edge.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Date")
	float DateLeftMargin = 64.f;

	// Gap between the bottom bar (or the screen bottom without bars) and the date.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Date")
	float DateBottomGap = 30.f;
};

/**
 * @brief Overlay of a mission location intro: cinema bars, the location top centre and the date typed bottom left.
 * Has no clock of its own: UMissionLocationIntroSession creates it with ShowIntro and drives it every frame with
 * SetElapsedSeconds, so the text stays in step with the camera move.
 */
UCLASS()
class RTS_SURVIVAL_API UW_LocationIntro : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_LocationIntro(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Creates the overlay above the HUD and fills in the texts; it shows nothing until SetElapsedSeconds.
	 * @param OwningPlayer Player whose screen shows the intro.
	 * @param Texts Location, date and the optional lines.
	 * @param Presentation Timing, bars and sounds.
	 * @param DurationSeconds Total length; the texts fade out in its last Presentation.FadeOutSeconds.
	 * @return The overlay on screen, or nullptr when it could not be created.
	 */
	static UW_LocationIntro* ShowIntro(APlayerController* OwningPlayer,
	                                   const FLocationIntroTexts& Texts,
	                                   const FLocationIntroPresentation& Presentation,
	                                   const float DurationSeconds);

	/**
	 * @brief Shared by every caller, so a too-short duration never cuts the typewriter off mid-word.
	 * @param Texts The texts that will show.
	 * @param Presentation Their timing.
	 * @return The shortest intro in which the date and time type out and stay readable before the fade.
	 */
	static float GetMinDurationSeconds(const FLocationIntroTexts& Texts, const FLocationIntroPresentation& Presentation);

	void SetElapsedSeconds(const float ElapsedSeconds);

	void CloseIntro();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Location Intro")
	FLocationIntroStyle Style;

private:
	UPROPERTY()
	TObjectPtr<UImage> M_FadeBlack;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_TopBarSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_BottomBarSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_LocationBlockSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> M_DateBlockSlot;

	UPROPERTY()
	TObjectPtr<UPanelWidget> M_LocationBlock;

	UPROPERTY()
	TObjectPtr<UPanelWidget> M_DateBlock;

	UPROPERTY()
	TObjectPtr<USizeBox> M_LeftRuleBox;

	UPROPERTY()
	TObjectPtr<USizeBox> M_RightRuleBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LocationText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SubtitleText;

	UPROPERTY()
	TObjectPtr<UImage> M_DateAccentBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DateText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TimeText;

	FLocationIntroPresentation M_Presentation;
	float M_DurationSeconds = 0.f;
	FString M_FullDate;
	FString M_FullTime;

	// Letters already shown, so a typewriter click plays once per new letter and text blocks change only when needed.
	int32 M_ShownTypedLetters = INDEX_NONE;
	bool bM_ShownCursor = false;
	int32 M_ShownLetterSpacing = INDEX_NONE;

	void BuildWidgetTree();
	void BuildLetterboxAndFade(class UCanvasPanel* Canvas);
	void BuildLocationBlock(class UCanvasPanel* Canvas);
	void BuildDateBlock(class UCanvasPanel* Canvas);
	UTextBlock* MakeText(const FSlateFontInfo& Font, const FLinearColor& Color) const;
	USizeBox* MakeRule(const FLinearColor& Color) const;
	void ApplyTexts(const FLocationIntroTexts& Texts, const FLocationIntroPresentation& Presentation,
	                const float DurationSeconds);

	float GetLayoutBarFraction() const;
	// 1 while the intro shows, falling to 0 over the fade-out at the end.
	float GetFadeOutVisibility(const float ElapsedSeconds) const;

	void UpdateLetterbox(const float ElapsedSeconds, const float FadeOutVisibility) const;
	void UpdateFadeFromBlack(const float ElapsedSeconds) const;
	void UpdateLocation(const float ElapsedSeconds, const float FadeOutVisibility);
	void UpdateDate(const float ElapsedSeconds, const float FadeOutVisibility);
	void UpdateTypedText(const float ElapsedSeconds);
	void PlayTypewriterClick() const;
};
