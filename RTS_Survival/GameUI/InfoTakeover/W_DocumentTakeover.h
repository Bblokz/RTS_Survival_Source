// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverTypes.h"
#include "W_InfoTakeover.h"

#include "W_DocumentTakeover.generated.h"

class UVerticalBox;
class UWidget;

/**
 * @brief A captured paper document shown as a takeover: the typed title and intro on the panel, then a paper sheet
 * with a heading, lines (some marked in red ink), small print and an optional red rubber stamp. The caller writes all
 * text (FDocumentTakeoverContent), so missions reuse it for any found document; the Ore Mountains convoy manifest is
 * the first. Opened by UInfoTakeoverSubsystem::QueueDocumentTakeover from the Show Document Takeover and Show Ore
 * Manifest Takeover nodes.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_DocumentTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_DocumentTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	/** Sets the texts and builds the sheet; call before OpenTakeover. */
	void SetupDocument(const FDocumentTakeoverContent& Content);

	/** @return How many lines the sheet shows, for the headless validation. */
	int32 GetPaperLineCount() const { return M_PaperLineCount; }

	/** @return True when the sheet carries a stamp, for the headless validation. */
	bool GetHasStamp() const { return bM_HasStamp; }

private:
	int32 M_PaperLineCount = 0;
	bool bM_HasStamp = false;

	void BuildBody(const FDocumentTakeoverContent& Content);
	UWidget* BuildPaperSheet(const FDocumentTakeoverContent& Content);
	void AddPaperHeader(UVerticalBox& PaperColumn, const FDocumentTakeoverContent& Content) const;
	void AddPaperLine(UVerticalBox& PaperColumn, const FDocumentTakeoverLine& Line) const;
	void AddPaperFooter(UVerticalBox& PaperColumn, const FText& Footer) const;
	UWidget* BuildStamp(const FText& StampText) const;
};
