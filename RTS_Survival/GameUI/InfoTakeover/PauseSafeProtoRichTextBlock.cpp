// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PauseSafeProtoRichTextBlock.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace PauseSafeProtoRichTextBlockPrivate
{
	// Caps the letters printed in one frame after a hitch, so a stall never dumps a page in one go.
	constexpr int32 MaxLettersPerTick = 64;
	const TCHAR* const TypewriterTagOpen = TEXT("<text>");
	const TCHAR* const TagClose = TEXT("</>");

	/** Wraps the untagged pieces of one line; tagged runs (<Name>...</>) stay as written. */
	FString WrapLineForTypewriter(const FString& Line)
	{
		FString Result;
		int32 ReadIndex = 0;
		while (ReadIndex < Line.Len())
		{
			const int32 TagStart = Line.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ReadIndex);
			const int32 TagEnd = TagStart == INDEX_NONE
				                     ? INDEX_NONE
				                     : Line.Find(TagClose, ESearchCase::CaseSensitive, ESearchDir::FromStart, TagStart);
			const int32 PlainEnd = TagEnd == INDEX_NONE ? Line.Len() : TagStart;
			if (PlainEnd > ReadIndex)
			{
				Result += TypewriterTagOpen + Line.Mid(ReadIndex, PlainEnd - ReadIndex) + TagClose;
			}
			if (TagEnd == INDEX_NONE)
			{
				break;
			}
			const int32 RunEnd = TagEnd + FCString::Strlen(TagClose);
			Result += Line.Mid(TagStart, RunEnd - TagStart);
			ReadIndex = RunEnd;
		}
		return Result;
	}
}

UPauseSafeProtoRichTextBlock::UPauseSafeProtoRichTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPauseSafeProtoRichTextBlock::PrintLetterByLetter()
{
	M_SecondsSinceLastLetter = 0.f;
	Super::PrintLetterByLetter();
	// TickLetterPrint drives every next letter; the world timer would fire a second time once the game resumes.
	ClearWorldLetterTimer();
}

void UPauseSafeProtoRichTextBlock::TickLetterPrint(const float RealDeltaSeconds)
{
	using namespace PauseSafeProtoRichTextBlockPrivate;
	if (not LetterByLetterPrintInProgress || GetWorld() == nullptr)
	{
		return;
	}
	M_SecondsSinceLastLetter += RealDeltaSeconds;
	int32 LettersThisTick = 0;
	// With world time frozen Proto's step prints exactly one letter per call, so call it once per elapsed interval.
	while (LetterByLetterPrintInProgress && M_SecondsSinceLastLetter >= LastCharacterPrintInterval
		&& LettersThisTick < MaxLettersPerTick)
	{
		M_SecondsSinceLastLetter -= FMath::Max(LastCharacterPrintInterval, 0.f);
		PlayNextLetter();
		ClearWorldLetterTimer();
		++LettersThisTick;
	}
}

void UPauseSafeProtoRichTextBlock::FinishPrintNow()
{
	if (not LetterByLetterPrintInProgress)
	{
		return;
	}
	ShowText();
	// ShowText stops the print with the "stopped early" event; the owner treats it like a normal finish.
}

void UPauseSafeProtoRichTextBlock::ClearWorldLetterTimer()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(LetterTimer);
}

FText UPauseSafeProtoRichTextBlock::MakeTypewriterText(const FText& Text)
{
	using namespace PauseSafeProtoRichTextBlockPrivate;
	TArray<FString> Lines;
	Text.ToString().ParseIntoArrayLines(Lines, false);
	TArray<FString> WrappedLines;
	WrappedLines.Reserve(Lines.Num());
	for (const FString& Line : Lines)
	{
		WrappedLines.Add(WrapLineForTypewriter(Line));
	}
	const TCHAR* const LineBreak = TEXT("\n");
	return FText::FromString(FString::Join(WrappedLines, LineBreak));
}
