// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TranscriptReveal.h"

namespace TranscriptRevealPrivate
{
	int32 GetVisibleSentenceIndex(const TArray<FCinematicTranscriptSentence>& Sentences, const float Seconds,
	                              const float HoldSeconds, const bool bKeepLastSentence)
	{
		int32 SentenceIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Sentences.Num(); ++Index)
		{
			const TArray<FCinematicTranscriptWord>& Words = Sentences[Index].Words;
			if (Words.Num() > 0 && Words[0].StartSeconds <= Seconds)
			{
				SentenceIndex = Index;
			}
		}
		if (SentenceIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		const bool bIsLastSentence = SentenceIndex == Sentences.Num() - 1;
		if (bIsLastSentence && bKeepLastSentence)
		{
			return SentenceIndex;
		}
		const float WipeAtSeconds = Sentences[SentenceIndex].Words.Last().EndSeconds + HoldSeconds;
		return Seconds > WipeAtSeconds ? INDEX_NONE : SentenceIndex;
	}
}

FString TranscriptReveal::BuildVisibleText(const TArray<FCinematicTranscriptSentence>& Sentences,
                                           const float Seconds, const float HoldSeconds,
                                           const bool bKeepLastSentence)
{
	const int32 SentenceIndex = TranscriptRevealPrivate::GetVisibleSentenceIndex(
		Sentences, Seconds, HoldSeconds, bKeepLastSentence);
	if (not Sentences.IsValidIndex(SentenceIndex))
	{
		return FString();
	}

	FString VisibleText;
	for (const FCinematicTranscriptWord& Word : Sentences[SentenceIndex].Words)
	{
		if (Word.StartSeconds > Seconds)
		{
			break;
		}
		if (not VisibleText.IsEmpty())
		{
			VisibleText.AppendChar(TEXT(' '));
		}
		VisibleText.Append(Word.Text);
	}
	return VisibleText;
}
