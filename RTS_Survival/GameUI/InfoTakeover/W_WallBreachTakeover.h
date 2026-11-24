// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_WallBreachTakeover.generated.h"

/**
 * @brief The field manual on driving tanks through Soviet concrete walls: a short looping in-game clip of a Tiger
 * crushing a Rus concrete wall, next to which tanks can do it and what the gap does afterwards.
 * Opened by UInfoTakeoverSubsystem::QueueWallBreachTakeover from the Show Wall Breach Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_WallBreachTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_WallBreachTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_WallBreachTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupWallBreach();

	static FText GetWallBreachTitle();

	/** @return What breaking through walls does and who can do it, in plain paragraphs, for the archive entry. */
	static FText GetWallBreachSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
