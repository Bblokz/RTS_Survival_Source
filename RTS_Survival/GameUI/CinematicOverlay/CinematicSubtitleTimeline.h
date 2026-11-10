// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/PortraitSubtitleTypes.h"

#include "CinematicSubtitleTimeline.generated.h"

class UMovieSceneSequence;
class USoundBase;

/** One voice line of a cinematic, its words placed on the sequence clock (seconds, same as the player's time). */
USTRUCT()
struct FCinematicSubtitleCue
{
	GENERATED_BODY()

	UPROPERTY()
	FText Speaker;

	UPROPERTY()
	TArray<FCinematicTranscriptSentence> Sentences;

	// Where the audio section plays on the sequence clock (trimmed parts excluded).
	float StartSeconds = 0.0f;
	float EndSeconds = 0.0f;
};

/**
 * Reads the audio tracks a cinematic already has (dragged-in voice lines, moved and trimmed in Sequencer) and turns
 * every section whose sound has a transcript into a subtitle cue. Master tracks, tracks bound to objects and
 * sub-sequences / shots (time scale ignored) are all read, so no per-cinematic setup is needed.
 */
namespace CinematicSubtitleTimeline
{
	/**
	 * @brief Builds the subtitle cues of a sequence, sorted by start.
	 * @param Sequence The sequence the takeover plays.
	 * @param FindTranscript Transcript row of a sound, or nullptr (no subtitle for that section).
	 * @param GetSpeaker Name shown before the words of a sound.
	 * @return Cues on the sequence clock; empty when no audio section has a transcript.
	 */
	RTS_SURVIVAL_API TArray<FCinematicSubtitleCue> BuildCues(
		const UMovieSceneSequence* Sequence,
		TFunctionRef<const FCinematicTranscriptRow*(const USoundBase*)> FindTranscript,
		TFunctionRef<FText(const USoundBase*, const FCinematicTranscriptRow&)> GetSpeaker);
}
