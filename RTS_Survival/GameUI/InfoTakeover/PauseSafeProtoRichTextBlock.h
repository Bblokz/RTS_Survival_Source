// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProtoRichTextBlock.h"

#include "PauseSafeProtoRichTextBlock.generated.h"

/**
 * @brief The Proto typewriter for widgets shown while the game is paused: Proto times its letters with world timers,
 * which stop under pause. The owning widget calls TickLetterPrint from NativeTick (Slate keeps ticking while paused)
 * and this block plays Proto's own next-letter step on real time instead.
 */
UCLASS()
class RTS_SURVIVAL_API UPauseSafeProtoRichTextBlock : public UProtoRichTextBlock
{
	GENERATED_BODY()

public:
	UPauseSafeProtoRichTextBlock(const FObjectInitializer& ObjectInitializer);

	virtual void PrintLetterByLetter() override;

	/** @param RealDeltaSeconds Undilated frame time; letters keep their style's intervals. */
	void TickLetterPrint(const float RealDeltaSeconds);

	/** Shows the whole text at once and broadcasts the finish event, as a skip. */
	void FinishPrintNow();

	/**
	 * @brief Proto types only runs tagged <text> (or <t>); anything else appears at once. Wraps every untagged piece
	 * of Text in <text>, line by line since a rich text tag cannot span lines, and keeps the designer's own tags.
	 * @param Text Plain or rich text as written in a library row or on a mission node.
	 * @return The text to set on this block.
	 */
	static FText MakeTypewriterText(const FText& Text);

private:
	// Real seconds since the last printed letter; compared with Proto's interval for the next one.
	float M_SecondsSinceLastLetter = 0.f;

	void ClearWorldLetterTimer();
};
