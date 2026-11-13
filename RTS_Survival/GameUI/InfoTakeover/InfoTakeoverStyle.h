// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "RTS_Survival/GameUI/GlassButtons/GlassHoverAnimator.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "InfoTakeoverStyle.generated.h"

/**
 * @brief The look and sound of every info takeover, filled in the class defaults of the takeover's Blueprint child.
 * Brushes, fonts and colours are the unit variant screen's skin so both screens read as one game; sizes and paddings
 * live in code.
 */
USTRUCT(BlueprintType)
struct FInfoTakeoverStyle
{
	GENERATED_BODY()

	FInfoTakeoverStyle();

	// Copied from the unit variant selection menu's Blueprint: plate (Card), icon well, glass pane, Confirm button,
	// fonts, colours, weapon icons and stat pictograms.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FUnitVariantSelectionStyle Skin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FCheckBoxStyle NeverShowAgainCheckBox;

	// Gauge glass laid over every picture (portrait, illustration, screenshot, render), above the picture and under
	// nothing else; normally an instance of the shared M_UI_GlassPane. No resource (the default) draws no glass.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FSlateBrush PictureGlass = FSlateNoResource();

	// One lamp per page in the title placard; the lamp of the page that shows is lit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Pages")
	FSlateBrush PageLampLit = FSlateRoundedBoxBrush(FLinearColor(0.25f, 0.94f, 1.f), 11.f,
	                                                FLinearColor(0.79f, 0.64f, 0.34f), 2.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Pages")
	FSlateBrush PageLampUnlit = FSlateRoundedBoxBrush(FLinearColor(0.03f, 0.03f, 0.025f), 11.f,
	                                                  FLinearColor(0.79f, 0.64f, 0.34f), 2.f);

	// The main UI's glass hover (M_UI_GlassPane Hover / HoverSweep / Pressed) on panes, picture glass and buttons.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Glass Hover")
	FGlassHoverTimings GlassHoverTimings;

	// The body panes carry text, so their hover is quieter than on a button: these replace the pane material's
	// light band strength, scratch boost and edge tint deepening.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Glass Hover", meta = (ClampMin = "0.0"))
	float PaneHoverSweepIntensity = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Glass Hover", meta = (ClampMin = "0.0"))
	float PaneHoverScratchBoost = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin|Glass Hover", meta = (ClampMin = "0.0"))
	float PaneHoverEdgeTint = 0.04f;

	// Darkens the paused game behind the panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	FLinearColor ScreenDimColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	// Proto style set of the typewritten title; its Default row is the title look.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typewriter",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/ProtoAnimatedText.ProtoRichTextStyleRow"))
	TSoftObjectPtr<UDataTable> TitleTextStyleSet;

	// Proto style set of the typewritten description; its Default row is the body look.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typewriter",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/ProtoAnimatedText.ProtoRichTextStyleRow"))
	TSoftObjectPtr<UDataTable> BodyTextStyleSet;

	// The title types in this many real seconds, matching TitleTypingSound.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typewriter", meta = (ClampMin = "0.1"))
	float TitleTypingSeconds = 1.f;

	// Seconds per description letter for normal lengths.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typewriter", meta = (ClampMin = "0.001"))
	float DescriptionLetterInterval = 0.022f;

	// Long descriptions type faster so they never take longer than this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typewriter", meta = (ClampMin = "1.0"))
	float DescriptionMaxTypingSeconds = 9.f;

	// Played once while the title types.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> TitleTypingSound;

	// One is picked at random when the description starts typing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TArray<TSoftObjectPtr<USoundBase>> DescriptionStartSounds;

	// Chained after the start sound for as long as the description is still typing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TSoftObjectPtr<USoundBase> DescriptionOngoingSound;
};
