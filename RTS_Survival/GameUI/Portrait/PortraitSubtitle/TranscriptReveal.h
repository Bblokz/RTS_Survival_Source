// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PortraitSubtitleTypes.h"

/**
 * Word-by-word reveal shared by the portrait subtitle and the cinematic overlay: one sentence at a time, its words
 * appear at their start times and the sentence is wiped HoldSeconds after its last word (unless the next one starts).
 */
namespace TranscriptReveal
{
	/**
	 * @brief Text to show at a moment of the line, so both subtitle widgets reveal and wipe the same way.
	 * @param Sentences Words with times on the same clock as Seconds.
	 * @param Seconds The moment to show.
	 * @param HoldSeconds How long a finished sentence stays before it is wiped.
	 * @param bKeepLastSentence True = the last sentence is never wiped (the portrait closes on its own).
	 * @return The visible words of the current sentence, or empty before the first word / after a wipe.
	 */
	RTS_SURVIVAL_API FString BuildVisibleText(const TArray<FCinematicTranscriptSentence>& Sentences,
	                                          const float Seconds, const float HoldSeconds,
	                                          const bool bKeepLastSentence);
}
