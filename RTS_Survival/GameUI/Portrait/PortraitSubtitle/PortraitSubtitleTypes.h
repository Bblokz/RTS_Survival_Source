// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Sound/SoundBase.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

#include "PortraitSubtitleTypes.generated.h"

/** One spoken word; times in seconds from the start of the (processed) voice line. */
USTRUCT(BlueprintType)
struct FCinematicTranscriptWord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	float StartSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	float EndSeconds = 0.0f;
};

/** The words shown together in the subtitle box; the box clears between sentences. */
USTRUCT(BlueprintType)
struct FCinematicTranscriptSentence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	TArray<FCinematicTranscriptWord> Words;
};

/**
 * @brief Row of DT_CinematicTranscripts: what a cinematic voice line says, word by word, so the portrait subtitle can
 * reveal it in step with the audio. Generated from VoiceLines/Announcer/AAAGerMissions/_CinematicTranscripts.json by
 * VoiceLines/VoiceChanger/ue_import_transcripts.py; rows can be corrected by hand in the editor.
 */
USTRUCT(BlueprintType)
struct FCinematicTranscriptRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	TSoftObjectPtr<USoundBase> VoiceLine;

	// Empty = the name that belongs to the portrait (UW_Portrait::SpeakerNames).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	FText SpeakerOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transcript")
	TArray<FCinematicTranscriptSentence> Sentences;
};

/** Look of the subtitle box under the cinematic portrait; set on W_BP_Portrait. */
USTRUCT(BlueprintType)
struct FPortraitSubtitleStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateBrush PanelBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FMargin PanelPadding = FMargin(10.0f, 6.0f, 10.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateFontInfo SpeakerFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateColor SpeakerColor = FSlateColor(FColor::FromHex(TEXT("E2B65CFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateFontInfo LineFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateColor LineColor = FSlateColor(FColor::FromHex(TEXT("ECE8DCFF")));

	// Fixed width of the box so it does not jump around while words appear; long sentences wrap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle", meta = (ClampMin = "120"))
	float BoxWidth = 340.0f;

	// The box keeps room for this many lines so its height stays put between sentences.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle", meta = (ClampMin = "1"))
	int32 ReservedLines = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	float GapBelowPortrait = 6.0f;

	// How long a finished sentence stays before it is wiped (if the next one has not started already).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle", meta = (ClampMin = "0"))
	float SentenceHoldSeconds = 0.6f;
};
