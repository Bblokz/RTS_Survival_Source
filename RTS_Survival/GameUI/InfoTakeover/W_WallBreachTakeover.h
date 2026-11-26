// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_WallBreachTakeover.generated.h"

/** The pages of the crushing field manual, in the order CONTINUE walks them. */
UENUM()
enum class EWallBreachTakeoverPage : uint8
{
	// A Tiger drives through a Soviet wall.
	CrushingWalls,
	// A tank runs over an enemy rifle squad.
	RunOverInfantry,
	Count UMETA(Hidden)
};

/**
 * @brief The field manual on crushing with heavy vehicles. Page 1: a short looping in-game clip of a Tiger crushing a
 * Rus concrete wall, with which vehicles can do it. Page 2: a tank running over an enemy rifle squad.
 * Opened by UInfoTakeoverSubsystem::QueueWallBreachTakeover from the Show Wall Breach Takeover node and by
 * USecureBaseMission after the squad intel (FSecureBaseAssaultResponse::bShowWallBreachManualAfterIntelSquad).
 * @note Fill M_Style and the first page's clip grid in the Blueprint child's class defaults (WBP_WallBreachTakeover
 * copies the spaced armour one); the run-over clip's defaults come from the constructor.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WallBreachTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_WallBreachTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds both pages; call before OpenTakeover. */
	void SetupWallBreach();

	static FText GetWallBreachTitle();

	/** @return What crushing walls and running over infantry does, in plain paragraphs, for the archive entry. */
	static FText GetWallBreachSummary();

	virtual int32 GetPageCount() const override { return static_cast<int32>(EWallBreachTakeoverPage::Count); }

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
	virtual FClipTakeoverClip GetPageClip(const int32 PageIndex) const override;
	virtual TArray<FClipTakeoverFact> MakePageFacts(const int32 PageIndex) const override;
	virtual FText GetPageClipCaption(const int32 PageIndex) const override;
	virtual void ApplyClipPageTexts(const int32 PageIndex) override;

	// Page 2: a tank running over an enemy rifle squad (Tools/InfoTakeover/RunOver/capture_clip.py).
	UPROPERTY(EditDefaultsOnly, Category = "Clip")
	FClipTakeoverClip M_RunOverClip;
};
