// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_AdrenalineShotTakeover.generated.h"

/**
 * @brief The field manual on Adrenaline Shot, the Panzer 38(t)'s crew rush: a short looping in-game clip of a
 * Panzer 38(t) firing at a T-26 at its normal reload and then with Adrenaline Shot, next to how the ability works.
 * Opened by UInfoTakeoverSubsystem::QueueAdrenalineShotTakeover from the Show Adrenaline Shot Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_AdrenalineShotTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_AdrenalineShotTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_AdrenalineShotTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupAdrenalineShot();

	static FText GetAdrenalineShotTitle();

	/** @return What the ability does and when to use it, in plain paragraphs, for the archive entry. */
	static FText GetAdrenalineShotSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
