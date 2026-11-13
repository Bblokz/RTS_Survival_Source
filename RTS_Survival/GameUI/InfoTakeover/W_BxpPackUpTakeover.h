// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_BxpPackUpTakeover.generated.h"

class UTexture2D;
class UUserWidget;
class UVerticalBox;
class UWidget;

/** One step of the building expansion pack-up takeover, as the text beside the picture explains it. */
USTRUCT()
struct FBxpPackUpTakeoverStep
{
	GENERATED_BODY()

	// Small accent label over the title, e.g. "AUTOMATIC".
	UPROPERTY()
	FText Label;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

/** One row of the slot legend under the illustration: a picture the expansion slot shows, and what it means. */
USTRUCT()
struct FBxpPackUpSlotLegendEntry
{
	GENERATED_BODY()

	// The picture W_BuildingItem shows for one expansion status.
	UPROPERTY()
	TSoftObjectPtr<UTexture2D> SlotPicture;

	UPROPERTY()
	FText Meaning;
};

/**
 * @brief The field manual on what happens to building expansions when their nomadic building packs up and deploys
 * again: one page with a looping illustration (the HQ packs up with its radar and flak, drives, deploys, the radar
 * rebuilds by itself and the flak is placed again from its slot) and the steps beside it. UMG animations tick on real
 * time, so the loop plays while the takeover pauses the game.
 * Opened by UInfoTakeoverSubsystem::QueueBxpPackUpTakeover from the Show Building Expansion Pack Up Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_BxpPackUpTakeover copies the vehicle takeover).
 */
UCLASS()
class RTS_SURVIVAL_API UW_BxpPackUpTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_BxpPackUpTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupBxpPackUp();

	static FText GetBxpPackUpTitle();

	/** @return Every step in plain paragraphs, for the archive entry. */
	static FText GetBxpPackUpSummary();

	/** @return The looping illustration, for the headless validation; null when its class did not load. */
	UUserWidget* GetIllustration() const { return M_IllustrationWidget; }

	/** @return How many slot pictures of the legend loaded, for the headless validation. */
	int32 GetSlotLegendPictureCount() const { return M_SlotLegendPictureCount; }

protected:
	// Looping picture: the HQ packs up with its expansions, drives, deploys and gets them back.
	UPROPERTY(EditDefaultsOnly, Category = "Building Expansion Pack Up")
	TSoftClassPtr<UUserWidget> M_PackUpIllustration;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> M_IllustrationWidget;

	int32 M_SlotLegendPictureCount = 0;

	TArray<FBxpPackUpTakeoverStep> MakeSteps() const;
	static TArray<FBxpPackUpSlotLegendEntry> MakeSlotLegend();
	void BuildBody();
	UWidget* BuildPage();

	/** The pictures an expansion slot shows while packing, packed and going back up, under the illustration. */
	UWidget* BuildSlotLegend();
	UWidget* BuildSlotLegendRow(const FBxpPackUpSlotLegendEntry& Entry);
	UWidget* BuildGlassPane(UWidget* Content);

	/**
	 * @brief The framed, clipped well the illustration loops in.
	 * @param IllustrationClass The illustration Blueprint.
	 * @param DesignedSize The size the Blueprint was designed at.
	 * @return The well, also when it stays empty; M_IllustrationWidget is set when the class loaded.
	 */
	UWidget* BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass,
	                               const FVector2D& DesignedSize);

	void AddStepText(UVerticalBox& TextColumn, const FBxpPackUpTakeoverStep& Step, const bool bIsFirst) const;

	/** Plays every UMG animation of the illustration on an endless loop, from the start. */
	static void PlayAllAnimationsLooped(UUserWidget& Illustration);
};
