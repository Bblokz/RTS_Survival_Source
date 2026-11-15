// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_ClipInfoTakeover.generated.h"

class UImage;
class UTexture2D;
class UVerticalBox;
class UWidget;

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

/**
 * @brief Base of the field manuals that show a short looping in-game clip on the left and a few facts on the right.
 * Child classes set the default atlas in their constructor, fill MakeFacts / GetClipCaption and call BuildClipBody
 * from their setup. The clip is a flipbook atlas played from NativeTick on real time, so it runs while paused.
 * @note Fill M_Style and the clip grid in the Blueprint child's class defaults.
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UW_ClipInfoTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_ClipInfoTakeover(const FObjectInitializer& ObjectInitializer);

	/** @return True when the clip atlas loaded, for the headless validations. */
	bool GetHasLoadedClip() const { return IsValid(M_LoadedClipAtlas); }

	/** @return The clip frame on screen; advances while the game is paused. */
	int32 GetShownClipFrame() const { return M_ShownClipFrame; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Rebuilds the body: description, the clip card and the facts card. */
	void BuildClipBody();

	virtual TArray<FClipTakeoverFact> MakeFacts() const;
	virtual FText GetClipCaption() const;

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
	// Kept loaded while the widget lives.
	UPROPERTY()
	TObjectPtr<UTexture2D> M_LoadedClipAtlas;

	UPROPERTY()
	TObjectPtr<UImage> M_ClipImage;

	// Real seconds into the clip loop; the takeover pauses the game, so world time would stand still.
	float M_ClipSeconds = 0.f;
	int32 M_ShownClipFrame = INDEX_NONE;

	UWidget* BuildClipCard();
	UWidget* BuildFactsCard();
	UWidget* WrapInGlassPane(UWidget* Content);
	void AddFactText(UVerticalBox& FactsColumn, const FClipTakeoverFact& Fact, const bool bIsFirst) const;

	void AdvanceClip(const float RealDeltaSeconds);
	/** Points the clip image's brush at one cell of the atlas. */
	void ShowClipFrame(const int32 FrameIndex);
	bool GetIsClipSetUp() const;
};
