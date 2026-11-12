// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"
#include "Styling/SlateTypes.h"
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
