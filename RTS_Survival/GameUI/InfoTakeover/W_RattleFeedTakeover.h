// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_RattleFeedTakeover.generated.h"

/**
 * @brief The field manual on Rattle-Feed, the Panzer II's 20 mm burst ability: a short looping in-game clip of a
 * Panzer II F firing at a T-26, first with its normal pauses and 5 s reload and then without them, next to how the
 * ability works. Opened by
 * UInfoTakeoverSubsystem::QueueRattleFeedTakeover from the Show Rattle-Feed Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_RattleFeedTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_RattleFeedTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_RattleFeedTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupRattleFeed();

	static FText GetRattleFeedTitle();

	/** @return What Rattle-Feed does and when to use it, in plain paragraphs, for the archive entry. */
	static FText GetRattleFeedSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
