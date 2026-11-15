// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IntroCinematicTypes.h"
#include "RTS_Survival/GameUI/CinematicOverlay/W_CinematicOverlay.h"

#include "W_IntroCinematicHud.generated.h"

class UBorder;
class UCanvasPanel;
class UWidgetTree;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;

namespace IntroCinematicHudLayout
{
	// Size and place of the narrator portrait (the in-game portrait is 176 x 232).
	constexpr float PortraitWidth = 176.0f;
	constexpr float PortraitHeight = 232.0f;
	constexpr float PortraitTopMargin = 22.0f;
	constexpr float SubtitleGap = 8.0f;
	constexpr float SubtitleStripWidth = 640.0f;
	// The layout is drawn for a 1920 x 1080 frame and scaled to whatever is rendered (Movie Render Queue draws
	// widgets at scale 1 at any output size, so without this a half-size preview shows the UI twice as large).
	constexpr float DesignWidth = 1920.0f;
	constexpr float DesignHeight = 1080.0f;
}

namespace IntroCinematicHudLayout
{
	/** Puts the widget tree's root inside a scale-to-fit box of the design size. */
	void WrapRootInDesignFrame(UWidgetTree* WidgetTree);
}

/**
 * @brief Subtitle strip of the intro cinematic: the cinematic overlay placed under the narrator portrait, without the
 * countdown. UIntroCinematicSubsystem shows it with UW_CinematicOverlay::ShowForSequence.
 */
UCLASS()
class RTS_SURVIVAL_API UW_IntroCinematicSubtitles : public UW_CinematicOverlay
{
	GENERATED_BODY()

public:
	UW_IntroCinematicSubtitles(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
};

/**
 * @brief Screen layer of the 1976 intro cinematic: narrator portrait (top middle), date and place cards (lower left),
 * intel stamps, typed labels and the archival notice. Everything shown is a pure function of the master sequence time,
 * so a scrub, a pause or a Movie Render Queue render all give the same picture.
 * UIntroCinematicSubsystem creates it and calls ShowCuesAtTime every frame.
 */
UCLASS()
class RTS_SURVIVAL_API UW_IntroCinematicHud : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Draws the cues that are active at this moment of the master sequence.
	 * @param Cues All cue rows of the film.
	 * @param SequenceSeconds Master sequence time.
	 */
	void ShowCuesAtTime(const TArray<FIntroCinematicCueRow>& Cues, const float SequenceSeconds);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	UPROPERTY()
	TObjectPtr<UImage> M_NoticeBackground;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_NoticeBurnMaterial;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NoticeText;

	UPROPERTY()
	TObjectPtr<UImage> M_PortraitImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CardLine1;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CardLine2;

	UPROPERTY()
	TObjectPtr<UBorder> M_StampBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_StampText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LabelText;

	bool GetIsValidCanvas() const;

	void BuildWidgetTree();
	void BuildNotice();
	void BuildPortrait();
	void BuildCard();
	void BuildStamp();
	void BuildLabel();

	void ShowPortrait(const FIntroCinematicCueRow* Cue, const float SequenceSeconds);
	void ShowCard(const FIntroCinematicCueRow* Cue, const float SequenceSeconds);
	void ShowStamp(const FIntroCinematicCueRow* Cue, const float SequenceSeconds);
	void ShowNotice(const FIntroCinematicCueRow* Cue, const float SequenceSeconds);
	void ShowLabel(const FIntroCinematicCueRow* Cue, const float SequenceSeconds);

	/** @return The first Count characters of Text, so text appears as if typed. */
	static FText GetTypedText(const FString& Text, const float SecondsSinceStart, const float CharactersPerSecond);

	/** @return 0..1: fades in after the cue starts and out before it ends. */
	static float GetCueFade(const FIntroCinematicCueRow& Cue, const float SequenceSeconds, const float FadeSeconds);
};
