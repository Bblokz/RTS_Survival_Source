// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_CoverTakeover.generated.h"

class UTexture2D;
class UVerticalBox;
class UWidget;

/** One card of the cover takeover: a screenshot of the move preview in one colour and what that colour means. */
USTRUCT()
struct FCoverTakeoverCard
{
	GENERATED_BODY()

	// "GREEN", drawn in the preview's own colour.
	UPROPERTY()
	FText ColourLabel;

	UPROPERTY()
	FLinearColor ColourLabelColor = FLinearColor::White;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> Screenshot;
};

/**
 * @brief The field manual on cover: three cards with screenshots of the squad move preview in green, yellow and red
 * (the stance meshes' MI_StrongCover, MI_WeakCover and MI_NoCover) and what each means. Opened by
 * UInfoTakeoverSubsystem::QueueCoverTakeover from the Show Cover Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_CoverTakeover copies the vehicle takeover).
 */
UCLASS()
class RTS_SURVIVAL_API UW_CoverTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_CoverTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the cards; call before OpenTakeover. */
	void SetupCover();

	static FText GetCoverTitle();

	/** @return The three colours and how to take cover in one plain paragraph, for the archive entry. */
	static FText GetCoverSummary();

	/** @return How many cards show a loaded screenshot, for the headless validation. */
	int32 GetLoadedScreenshotCount() const { return M_Screenshots.Num(); }

protected:
	// In-game screenshot of the preview on prone or trench cover (green).
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	TSoftObjectPtr<UTexture2D> M_StrongCoverScreenshot;

	// In-game screenshot of the preview on crouch or standing cover (yellow).
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	TSoftObjectPtr<UTexture2D> M_WeakCoverScreenshot;

	// In-game screenshot of the preview on open ground (red).
	UPROPERTY(EditDefaultsOnly, Category = "Cover")
	TSoftObjectPtr<UTexture2D> M_NoCoverScreenshot;

private:
	// Kept loaded while the widget lives.
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_Screenshots;

	TArray<FCoverTakeoverCard> MakeCards() const;
	void BuildBody();
	UWidget* BuildCard(const FCoverTakeoverCard& Card);
	UWidget* BuildScreenshotWell(const TSoftObjectPtr<UTexture2D>& Screenshot);
	void AddCardText(UVerticalBox& CardColumn, const FCoverTakeoverCard& Card) const;
};
