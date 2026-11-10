// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicSubtitleTimeline.h"

#include "W_CinematicOverlay.generated.h"

class UBorder;
class UDataTable;
class UMovieSceneSequencePlayer;
class USizeBox;
class UTextBlock;

/** Look of the cinematic overlay (subtitle strip at the top, countdown at the bottom right). */
USTRUCT(BlueprintType)
struct FCinematicOverlayStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateBrush StripBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FMargin StripPadding = FMargin(14.0f, 7.0f, 14.0f, 8.0f);

	// Fixed width so the strip does not jump while words appear; long sentences wrap to a second line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle", meta = (ClampMin = "200"))
	float StripWidth = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	float StripTopMargin = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateFontInfo SpeakerFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateColor SpeakerColor = FSlateColor(FColor::FromHex(TEXT("E2B65CFF")));

	// Space between the speaker name and the words.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	float SpeakerGap = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateFontInfo LineFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle")
	FSlateColor LineColor = FSlateColor(FColor::FromHex(TEXT("ECE8DCFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Subtitle", meta = (ClampMin = "0"))
	float SentenceHoldSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countdown")
	FSlateBrush CountdownBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countdown")
	FMargin CountdownPadding = FMargin(12.0f, 5.0f, 12.0f, 6.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countdown")
	FSlateFontInfo CountdownFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countdown")
	FSlateColor CountdownColor = FSlateColor(FColor::FromHex(TEXT("ECE8DCFF")));

	// Distance of the countdown from the bottom-right screen corner.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countdown")
	FVector2D CountdownCornerOffset = FVector2D(36.0f, 36.0f);
};

/**
 * @brief Overlay of a cinematic takeover: the voice lines the sequence plays shown as a one-line subtitle strip at the
 * top (speaker, then the words, revealed in step with the sequence) and a countdown of the time left at the bottom
 * right. The lines are found in the sequence's own audio tracks, so every cinematic gets subtitles without setup.
 * UMissionCinematicTakeOverSession shows it for every takeover; ShowForSequence can be used for other cinematics.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CinematicOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_CinematicOverlay(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Creates the overlay for a playing (or about to play) sequence; it removes itself when the sequence ends.
	 * @param OwningPlayer Player whose screen shows the overlay.
	 * @param SequencePlayer The cinematic; its audio tracks give the subtitles and its time the countdown.
	 * @param OverlayClass Optional Blueprint subclass with a different style; null = this class.
	 * @return The overlay on screen, or nullptr when it could not be created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cinematics", meta = (DeterminesOutputType = "OverlayClass"))
	static UW_CinematicOverlay* ShowForSequence(APlayerController* OwningPlayer,
	                                            UMovieSceneSequencePlayer* SequencePlayer,
	                                            TSubclassOf<UW_CinematicOverlay> OverlayClass = nullptr);

	/** Removes the overlay (also done automatically when the sequence finishes or stops). */
	UFUNCTION(BlueprintCallable, Category = "Cinematics")
	void CloseOverlay();

	/** Number of voice lines with a transcript found in the sequence. */
	UFUNCTION(BlueprintPure, Category = "Cinematics")
	int32 GetSubtitleCueCount() const { return M_Cues.Num(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Word-timed transcripts (rows: FCinematicTranscriptRow); the same table the portrait subtitles use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematics")
	TSoftObjectPtr<UDataTable> TranscriptTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Sounds/VoiceLines/DT_CinematicTranscripts.DT_CinematicTranscripts")));

	/** Speaker shown for a voice line whose asset path contains the key (e.g. "/Portraits/RusHazmat/"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematics")
	TMap<FString, FText> SpeakerByAssetPath;

	/** Speaker for voice lines that match no entry of SpeakerByAssetPath (and have no row override). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematics")
	FText DefaultSpeaker;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematics")
	FCinematicOverlayStyle Style;

	// Show the countdown even when the takeover cannot be skipped.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematics")
	bool bShowCountdown = true;

private:
	UPROPERTY()
	TObjectPtr<UBorder> M_Strip;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SpeakerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LineText;

	UPROPERTY()
	TObjectPtr<UBorder> M_CountdownPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountdownText;

	UPROPERTY()
	TArray<FCinematicSubtitleCue> M_Cues;

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequencePlayer> M_SequencePlayer;

	FTimerHandle M_UpdateTimerHandle;

	// Last values written to the widgets, so text blocks are only touched on a change.
	int32 M_ShownCueIndex = INDEX_NONE;
	FString M_ShownLine;
	int32 M_ShownCountdownSeconds = -1;

	void BuildWidgetTree();
	void ApplyStyle();
	void StartForSequencePlayer(UMovieSceneSequencePlayer* SequencePlayer);
	void UpdateOverlay();
	void UpdateSubtitle(const float SequenceSeconds);
	void UpdateCountdown(const float SequenceSeconds);

	/** @return Index of the cue that owns this moment of the sequence, or INDEX_NONE. */
	int32 GetActiveCueIndex(const float SequenceSeconds) const;

	FText GetSpeakerForSound(const USoundBase* Sound, const FCinematicTranscriptRow& Transcript) const;

	UFUNCTION()
	void HandleSequenceEnded();

	void UnbindSequencePlayer();
	bool GetIsValidSequencePlayer(const bool bReportError = true) const;
};
