// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PortraitSubtitleTypes.h"

#include "W_PortraitSubtitle.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;

/**
 * @brief The small box under the cinematic portrait: the speaker's name and what they say, revealed word by word in
 * step with the voice line; a finished sentence is wiped before the next one starts so the box stays narrow.
 * UW_Portrait creates it, places it under the portrait image and calls StartLine / StopLine; it builds its own tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_PortraitSubtitle : public UUserWidget
{
	GENERATED_BODY()

public:
	void ApplyStyle(const FPortraitSubtitleStyle& Style);

	/**
	 * @brief Shows the speaker and reveals the transcript in step with the voice line's real playback position.
	 * @param SpeakerName Name line of the box.
	 * @param Sentences The words with their times in the voice line; empty = only the name is shown.
	 * @param GetPlaybackSeconds Where the audio is in the line (< 0 while it is not playing yet / any more); the
	 * renderer's position, so a queued line, a frame hitch or a pause cannot put the words out of step.
	 */
	void StartLine(const FText& SpeakerName, const TArray<FCinematicTranscriptSentence>& Sentences,
	               TFunction<float()> GetPlaybackSeconds);

	void StopLine();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<USizeBox> M_OuterBox;

	UPROPERTY()
	TObjectPtr<UBorder> M_Panel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SpeakerText;

	UPROPERTY()
	TObjectPtr<USizeBox> M_LineBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LineText;

	UPROPERTY()
	FPortraitSubtitleStyle M_Style;

	UPROPERTY()
	TArray<FCinematicTranscriptSentence> M_Sentences;

	// Playback position of the voice line in seconds (< 0 when not playing).
	TFunction<float()> M_GetPlaybackSeconds;

	FTimerHandle M_UpdateTimerHandle;

	// What the line text block shows now, so it is only touched when a word is added or the sentence is wiped.
	FString M_ShownText;

	void BuildWidgetTree();
	void UpdateSubtitle();
	void StopUpdateTimer();

	void SetLineText(const FString& NewText);
};
