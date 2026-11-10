// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CinematicSubtitleTimeline.h"

#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sound/SoundBase.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

namespace CinematicSubtitleTimelinePrivate
{
	// Sub-sequences inside sub-sequences; deeper nesting is not used for cinematics.
	constexpr int32 MaxSubSequenceDepth = 4;

	/** A window of an outer clock and where the inner sequence's time 0 sits on it. */
	struct FClockWindow
	{
		// Outer seconds = inner seconds + InnerToOuterSeconds.
		double InnerToOuterSeconds = 0.0;
		double ClipStartSeconds = -TNumericLimits<double>::Max();
		double ClipEndSeconds = TNumericLimits<double>::Max();
	};

	struct FBuildContext
	{
		TFunctionRef<const FCinematicTranscriptRow*(const USoundBase*)> FindTranscript;
		TFunctionRef<FText(const USoundBase*, const FCinematicTranscriptRow&)> GetSpeaker;
		TArray<FCinematicSubtitleCue> Cues;
	};

	void CollectFromMovieScene(const UMovieScene& MovieScene, const FClockWindow& Window, const int32 Depth,
	                           FBuildContext& Context);

	/** Words of the transcript that the section actually plays, moved onto the outer clock. */
	TArray<FCinematicTranscriptSentence> PlaceSentences(const FCinematicTranscriptRow& Transcript,
	                                                     const double AudioToOuterSeconds,
	                                                     const double VisibleStart, const double VisibleEnd)
	{
		TArray<FCinematicTranscriptSentence> Placed;
		for (const FCinematicTranscriptSentence& Sentence : Transcript.Sentences)
		{
			FCinematicTranscriptSentence Moved;
			for (const FCinematicTranscriptWord& Word : Sentence.Words)
			{
				const double Start = Word.StartSeconds + AudioToOuterSeconds;
				if (Start < VisibleStart || Start >= VisibleEnd)
				{
					continue;            // trimmed off in Sequencer
				}
				FCinematicTranscriptWord MovedWord = Word;
				MovedWord.StartSeconds = static_cast<float>(Start);
				MovedWord.EndSeconds = static_cast<float>(FMath::Min(Word.EndSeconds + AudioToOuterSeconds, VisibleEnd));
				Moved.Words.Add(MoveTemp(MovedWord));
			}
			if (Moved.Words.Num() > 0)
			{
				Placed.Add(MoveTemp(Moved));
			}
		}
		return Placed;
	}

	void AddAudioSection(const UMovieSceneAudioSection& Section, const FFrameRate TickResolution,
	                     const FClockWindow& Window, FBuildContext& Context)
	{
		USoundBase* const Sound = Section.GetSound();
		if (not Section.IsActive() || Sound == nullptr || not Section.HasStartFrame())
		{
			return;
		}
		const FCinematicTranscriptRow* const Transcript = Context.FindTranscript(Sound);
		if (Transcript == nullptr)
		{
			return;
		}

		const double SectionStart = TickResolution.AsSeconds(Section.GetInclusiveStartFrame());
		const double FrontTrim = TickResolution.AsSeconds(Section.GetStartOffset());
		const double SoundEnd = SectionStart + FMath::Max(0.0, Sound->GetDuration() - FrontTrim);
		const double SectionEnd = Section.HasEndFrame()
			                          ? TickResolution.AsSeconds(Section.GetExclusiveEndFrame())
			                          : SoundEnd;

		const double VisibleStart = FMath::Max(SectionStart + Window.InnerToOuterSeconds, Window.ClipStartSeconds);
		const double VisibleEnd = FMath::Min(FMath::Min(SectionEnd, SoundEnd) + Window.InnerToOuterSeconds,
		                                     Window.ClipEndSeconds);
		if (VisibleEnd <= VisibleStart)
		{
			return;
		}

		// A word at Time in the sound plays at SectionStart + Time - FrontTrim on this sequence's clock.
		const double AudioToOuterSeconds = SectionStart - FrontTrim + Window.InnerToOuterSeconds;
		FCinematicSubtitleCue Cue;
		Cue.Sentences = PlaceSentences(*Transcript, AudioToOuterSeconds, VisibleStart, VisibleEnd);
		if (Cue.Sentences.Num() == 0)
		{
			return;
		}
		Cue.Speaker = Context.GetSpeaker(Sound, *Transcript);
		Cue.StartSeconds = static_cast<float>(VisibleStart);
		Cue.EndSeconds = static_cast<float>(VisibleEnd);
		Context.Cues.Add(MoveTemp(Cue));
	}

	void AddSubSection(const UMovieSceneSubSection& Section, const FFrameRate OuterTickResolution,
	                   const FClockWindow& Window, const int32 Depth, FBuildContext& Context)
	{
		const UMovieSceneSequence* const InnerSequence = Section.GetSequence();
		const UMovieScene* const InnerScene = IsValid(InnerSequence) ? InnerSequence->GetMovieScene() : nullptr;
		if (not Section.IsActive() || InnerScene == nullptr || not Section.HasStartFrame()
			|| Depth >= MaxSubSequenceDepth)
		{
			return;
		}

		const FFrameRate InnerTickResolution = InnerScene->GetTickResolution();
		const double SectionStart = OuterTickResolution.AsSeconds(Section.GetInclusiveStartFrame());
		const double SectionEnd = Section.HasEndFrame()
			                          ? OuterTickResolution.AsSeconds(Section.GetExclusiveEndFrame())
			                          : TNumericLimits<double>::Max();
		// At the section start the inner sequence is at its playback start + the section's start offset.
		const double InnerAtSectionStart =
			InnerTickResolution.AsSeconds(InnerScene->GetPlaybackRange().GetLowerBoundValue())
			+ InnerTickResolution.AsSeconds(Section.Parameters.StartFrameOffset);

		FClockWindow InnerWindow;
		InnerWindow.InnerToOuterSeconds = SectionStart - InnerAtSectionStart + Window.InnerToOuterSeconds;
		InnerWindow.ClipStartSeconds = FMath::Max(SectionStart + Window.InnerToOuterSeconds, Window.ClipStartSeconds);
		InnerWindow.ClipEndSeconds = FMath::Min(SectionEnd + Window.InnerToOuterSeconds, Window.ClipEndSeconds);
		CollectFromMovieScene(*InnerScene, InnerWindow, Depth + 1, Context);
	}

	void CollectFromTrack(const UMovieSceneTrack& Track, const FFrameRate TickResolution, const FClockWindow& Window,
	                      const int32 Depth, FBuildContext& Context)
	{
		if (Track.IsEvalDisabled())
		{
			return;                       // muted track in Sequencer
		}
		if (Track.IsA<UMovieSceneAudioTrack>())
		{
			for (const UMovieSceneSection* const Section : Track.GetAllSections())
			{
				if (const UMovieSceneAudioSection* const Audio = Cast<UMovieSceneAudioSection>(Section))
				{
					AddAudioSection(*Audio, TickResolution, Window, Context);
				}
			}
			return;
		}
		if (Track.IsA<UMovieSceneSubTrack>())   // also cinematic shot tracks
		{
			for (const UMovieSceneSection* const Section : Track.GetAllSections())
			{
				if (const UMovieSceneSubSection* const Sub = Cast<UMovieSceneSubSection>(Section))
				{
					AddSubSection(*Sub, TickResolution, Window, Depth, Context);
				}
			}
		}
	}

	void CollectFromMovieScene(const UMovieScene& MovieScene, const FClockWindow& Window, const int32 Depth,
	                           FBuildContext& Context)
	{
		const FFrameRate TickResolution = MovieScene.GetTickResolution();
		for (const UMovieSceneTrack* const Track : MovieScene.GetTracks())
		{
			if (Track != nullptr)
			{
				CollectFromTrack(*Track, TickResolution, Window, Depth, Context);
			}
		}
		for (const FMovieSceneBinding& Binding : MovieScene.GetBindings())
		{
			for (const UMovieSceneTrack* const Track : Binding.GetTracks())
			{
				if (Track != nullptr)
				{
					CollectFromTrack(*Track, TickResolution, Window, Depth, Context);
				}
			}
		}
	}
}

TArray<FCinematicSubtitleCue> CinematicSubtitleTimeline::BuildCues(
	const UMovieSceneSequence* Sequence,
	TFunctionRef<const FCinematicTranscriptRow*(const USoundBase*)> FindTranscript,
	TFunctionRef<FText(const USoundBase*, const FCinematicTranscriptRow&)> GetSpeaker)
{
	using namespace CinematicSubtitleTimelinePrivate;
	FBuildContext Context{FindTranscript, GetSpeaker, {}};
	const UMovieScene* const MovieScene = IsValid(Sequence) ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene == nullptr)
	{
		return {};
	}
	CollectFromMovieScene(*MovieScene, FClockWindow(), 0, Context);
	Context.Cues.Sort([](const FCinematicSubtitleCue& A, const FCinematicSubtitleCue& B)
	{
		return A.StartSeconds < B.StartSeconds;
	});
	return MoveTemp(Context.Cues);
}
