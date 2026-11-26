// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_ClipInfoTakeover.generated.h"

class UImage;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

/** One fact of the right column: a small red label, a heading and a line of text. */
USTRUCT()
struct FClipTakeoverFact
{
	GENERATED_BODY()

	// "VOLLEY", drawn in the skin's label colour.
	UPROPERTY()
	FText Label;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

/** The flipbook of one clip page: ClipColumns x ClipRows frames, left to right, top to bottom. */
USTRUCT()
struct FClipTakeoverClip
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Clip")
	TSoftObjectPtr<UTexture2D> Atlas;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 Columns = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 Rows = 10;

	// Frames used; the last cells of the atlas may be empty.
	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 FrameCount = 80;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1.0"))
	float FramesPerSecond = 12.f;

	// The last frame is held this long before the clip starts over.
	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "0.0"))
	float EndHoldSeconds = 1.f;
};

/** What one built clip page plays: its loaded atlas, the image that shows it and the grid. */
USTRUCT()
struct FClipTakeoverPagePlayback
{
	GENERATED_BODY()

	// Kept loaded while the widget lives.
	UPROPERTY()
	TObjectPtr<UTexture2D> LoadedAtlas = nullptr;

	UPROPERTY()
	TObjectPtr<UImage> Image = nullptr;

	UPROPERTY()
	FClipTakeoverClip Clip;
};

/**
 * @brief Base of the field manuals that show a short looping in-game clip on the left and a few facts on the right.
 * Child classes set the default atlas in their constructor, fill MakeFacts / GetClipCaption and call BuildClipBody
 * from their setup. The clip is a flipbook atlas played from NativeTick on real time, so it runs while paused.
 * Multi-page field manuals override GetPageCount, GetPageClip, MakePageFacts, GetPageClipCaption and
 * ApplyClipPageTexts: every page has its own clip and facts, CONTINUE and PREVIOUS switch between them and the shown
 * page's clip starts over from its first frame.
 * @note Fill M_Style and the first page's clip grid in the Blueprint child's class defaults.
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UW_ClipInfoTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_ClipInfoTakeover(const FObjectInitializer& ObjectInitializer);

	/** @return True when the shown page's clip atlas loaded, for the headless validations. */
	bool GetHasLoadedClip() const;

	/** @return The clip frame on screen (of the shown page); advances while the game is paused. */
	int32 GetShownClipFrame() const { return M_ShownClipFrame; }

	virtual int32 GetCurrentPageIndex() const override { return M_ActiveClipPage; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool TryShowPage(const int32 PageIndex) override;

	/** Rebuilds the body: description, then per page the clip card and the facts card; shows the first page. */
	void BuildClipBody();

	// The first page's facts and caption; one-page field manuals only fill these.
	virtual TArray<FClipTakeoverFact> MakeFacts() const;
	virtual FText GetClipCaption() const;

	/** @return The page's clip; the first page plays the M_Clip* class defaults. */
	virtual FClipTakeoverClip GetPageClip(const int32 PageIndex) const;

	/** @return The page's facts; the first page's are MakeFacts. */
	virtual TArray<FClipTakeoverFact> MakePageFacts(const int32 PageIndex) const;

	/** @return The caption under the page's clip; the first page's is GetClipCaption. */
	virtual FText GetPageClipCaption(const int32 PageIndex) const;

	/** Sets the page's title, description, kind label and footer button text when it shows; multi-page only. */
	virtual void ApplyClipPageTexts(const int32 PageIndex)
	{
	}

	// Flipbook of the clip: ClipColumns x ClipRows frames, left to right, top to bottom.
	UPROPERTY(EditDefaultsOnly, Category = "Clip")
	TSoftObjectPtr<UTexture2D> M_ClipAtlas;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 M_ClipColumns = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 M_ClipRows = 10;

	// Frames used; the last cells of the atlas may be empty.
	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1"))
	int32 M_ClipFrameCount = 80;

	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "1.0"))
	float M_ClipFramesPerSecond = 12.f;

	// The last frame is held this long before the clip starts over.
	UPROPERTY(EditDefaultsOnly, Category = "Clip", meta = (ClampMin = "0.0"))
	float M_ClipEndHoldSeconds = 1.f;

private:
	// One entry per page, in page order.
	UPROPERTY()
	TArray<FClipTakeoverPagePlayback> M_ClipPages;

	// Child order follows the page index.
	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> M_ClipPageSwitcher;

	int32 M_ActiveClipPage = 0;

	// Real seconds into the shown clip's loop; the takeover pauses the game, so world time would stand still.
	float M_ClipSeconds = 0.f;
	int32 M_ShownClipFrame = INDEX_NONE;

	UWidget* BuildClipPage(const int32 PageIndex);
	UWidget* BuildClipCard(const int32 PageIndex);
	UWidget* BuildFactsCard(const int32 PageIndex);
	UWidget* WrapInGlassPane(UWidget* Content);
	void AddFactText(UVerticalBox& FactsColumn, const FClipTakeoverFact& Fact, const bool bIsFirst) const;

	void AdvanceClip(const float RealDeltaSeconds);
	/** Points the shown page's clip image at one cell of its atlas. */
	void ShowClipFrame(const int32 FrameIndex);
	/** @return The shown page's playback when its clip can play; null otherwise. */
	const FClipTakeoverPagePlayback* GetActiveClipPlayback() const;
	static bool GetIsClipSetUp(const FClipTakeoverPagePlayback& Playback);
};
