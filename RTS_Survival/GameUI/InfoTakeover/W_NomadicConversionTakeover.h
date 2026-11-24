// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_NomadicConversionTakeover.generated.h"

class UUserWidget;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

/** One step of the nomadic conversion takeover, as the text beside the picture explains it. */
USTRUCT()
struct FNomadicConversionTakeoverStep
{
	GENERATED_BODY()

	// Small accent label over the title: the input, e.g. "LEFT CLICK".
	UPROPERTY()
	FText Label;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

/** The pages of the nomadic conversion takeover, in the order CONTINUE shows them. */
UENUM()
enum class ENomadicConversionTakeoverPage : uint8
{
	// Convert, place and pack up a truck.
	Conversion,
	// Where trucks may deploy: the HQ anywhere it fits, every other truck inside a build radius.
	BuildRadius,
	// The number of pages; not a page.
	Count UMETA(Hidden)
};

/**
 * @brief The field manual on nomadic trucks, two pages with a looping illustration and the steps beside it: converting
 * (a truck is selected, converted, placed, drives there and unfolds; the button turns into pack up) ends in CONTINUE,
 * the build radius (the HQ deploys anywhere, other trucks only inside a radius) in UNDERSTOOD. UMG animations tick on
 * real time, so the loops play while the takeover pauses the game.
 * Opened by UInfoTakeoverSubsystem::QueueNomadicConversionTakeover from the Show Nomadic Conversion Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_NomadicConversionTakeover copies the vehicle takeover).
 */
UCLASS()
class RTS_SURVIVAL_API UW_NomadicConversionTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_NomadicConversionTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupNomadicConversion();

	static FText GetNomadicConversionTitle();

	/** @return Every step in plain paragraphs, for the archive entry. */
	static FText GetNomadicConversionSummary();

	/** @return The conversion page's looping illustration, for the headless validation; null when it did not load. */
	UUserWidget* GetIllustration() const { return M_DeployIllustrationWidget; }

	/** @return The build radius page's looping illustration; null when its class did not load. */
	UUserWidget* GetRadiusIllustration() const { return M_RadiusIllustrationWidget; }

	ENomadicConversionTakeoverPage GetActivePage() const { return M_ActivePage; }

	virtual int32 GetPageCount() const override { return static_cast<int32>(ENomadicConversionTakeoverPage::Count); }
	virtual int32 GetCurrentPageIndex() const override { return static_cast<int32>(M_ActivePage); }

protected:
	virtual bool TryShowPage(const int32 PageIndex) override;

	// Looping picture: the HQ truck is selected, converted, placed, drives there and unfolds into the HQ.
	UPROPERTY(EditDefaultsOnly, Category = "Nomadic Conversion")
	TSoftClassPtr<UUserWidget> M_DeployIllustration;

	// Looping picture: the HQ deploys on open ground; then another truck's preview is refused outside the HQ's build
	// radius and placed inside it.
	UPROPERTY(EditDefaultsOnly, Category = "Nomadic Conversion")
	TSoftClassPtr<UUserWidget> M_RadiusIllustration;

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> M_DeployIllustrationWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_RadiusIllustrationWidget;

	// Child order follows ENomadicConversionTakeoverPage.
	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> M_PageSwitcher;

	ENomadicConversionTakeoverPage M_ActivePage = ENomadicConversionTakeoverPage::Conversion;

	TArray<FNomadicConversionTakeoverStep> MakeConversionSteps() const;
	TArray<FNomadicConversionTakeoverStep> MakeBuildRadiusSteps() const;
	void ApplyPageTexts(const ENomadicConversionTakeoverPage Page);
	void BuildBody();

	/**
	 * @brief One page: the looping picture with its steps beside it, on a glass pane.
	 * @param IllustrationClass The page's illustration Blueprint.
	 * @param Steps The texts beside the picture, top to bottom.
	 * @param OutIllustration Set to the created illustration; null when its class did not load.
	 * @return The page's glass pane.
	 */
	UWidget* BuildPage(const TSoftClassPtr<UUserWidget>& IllustrationClass,
	                   const TArray<FNomadicConversionTakeoverStep>& Steps, TObjectPtr<UUserWidget>& OutIllustration);
	UWidget* BuildGlassPane(UWidget* Content);

	/**
	 * @brief The framed, clipped well the illustration loops in.
	 * @param IllustrationClass The illustration Blueprint.
	 * @param DesignedSize The size the Blueprint was designed at.
	 * @param OutIllustration Set to the created illustration; null when its class did not load.
	 * @return The well, also when it stays empty.
	 */
	UWidget* BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass,
	                               const FVector2D& DesignedSize, TObjectPtr<UUserWidget>& OutIllustration);

	void AddStepText(UVerticalBox& TextColumn, const FNomadicConversionTakeoverStep& Step, const bool bIsFirst) const;

	/** Plays every UMG animation of the illustration on an endless loop, from the start. */
	static void PlayAllAnimationsLooped(UUserWidget& Illustration);
};
