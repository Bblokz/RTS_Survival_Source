// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/PortraitSubtitleTypes.h"
#include "W_Portrait.generated.h"

class UDataTable;
class USlateBrushAsset;
class USoundBase;
class UW_PortraitSubtitle;

/**
 * Simple portrait widget that can asynchronously load and display
 * a portrait image based on the requested portrait type.
 * Under the image it shows a subtitle box (UW_PortraitSubtitle) with the speaker's name and, for cinematic lines that
 * have a row in TranscriptTable, the words revealed in step with the voice line.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Portrait : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_Portrait(const FObjectInitializer& ObjectInitializer);

	/** Asynchronously update the portrait to the given type and show the widget. */
	void UpdatePortrait(ERTSPortraitTypes NewType);

	/** Hide the portrait and clear its brush. */
	void HidePortrait();

	/**
	 * @brief Starts the subtitle box for a voice line that plays with this portrait.
	 * @param PortraitType Picks the speaker name unless the transcript row overrides it.
	 * @param VoiceLine Looked up in TranscriptTable; a line without a row shows only the name.
	 * @param GetPlaybackSeconds Where the audio is in the line (< 0 when not playing); the words follow it.
	 */
	void StartSubtitles(ERTSPortraitTypes PortraitType, const USoundBase* VoiceLine,
	                    TFunction<float()> GetPlaybackSeconds);

protected:
	virtual void NativeOnInitialized() override;

	/** Word-timed transcripts of the cinematic voice lines (rows: FCinematicTranscriptRow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Subtitle")
	TSoftObjectPtr<UDataTable> TranscriptTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Sounds/VoiceLines/DT_CinematicTranscripts.DT_CinematicTranscripts")));

	/** Name shown above the subtitle for each portrait. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Subtitle")
	TMap<ERTSPortraitTypes, FText> SpeakerNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Subtitle")
	FPortraitSubtitleStyle SubtitleStyle;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UImage* PortraitImage = nullptr;

	/** Soft references to the portrait brushes keyed by portrait type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Portrait")
	TMap<ERTSPortraitTypes, TSoftObjectPtr<USlateBrushAsset>> PortraitBrushes;

	/** Duration in seconds for the fade-in animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Fade")
	float M_FadeInDurationSeconds = 2.0f;

	/** Tick interval for the fade-in timer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Fade")
	float M_FadeInTickIntervalSeconds = 0.02f;

private:
	/** Called when an async brush load finishes. */
	void OnBrushLoaded(USlateBrushAsset* LoadedAsset);

	/** Start (or restart) the fade-in animation. */
	void StartFadeIn();

	/** Per-tick fade logic driven by the timer manager. */
	void TickFadeIn();

	/** Handle for the fade-in timer. */
	FTimerHandle M_FadeInTimerHandle;

	/** True while the widget is currently fading in. */
	bool bM_IsFadingIn = false;

	/** Accumulated time in seconds since the fade-in started. */
	float M_CurrentFadeTimeSeconds = 0.0f;

	UPROPERTY()
	TObjectPtr<UW_PortraitSubtitle> M_Subtitle;

	void SetDefaultSubtitleSetup();
	void CreateSubtitleWidget();

	/** Adds the subtitle to the root canvas, centred under the canvas child that holds PortraitImage. */
	void PlaceSubtitleUnderPortrait();

	const FCinematicTranscriptRow* FindTranscript(const USoundBase* VoiceLine) const;
	FText GetSpeakerName(ERTSPortraitTypes PortraitType, const FCinematicTranscriptRow* Transcript) const;
	bool GetIsValidSubtitle() const;
};
