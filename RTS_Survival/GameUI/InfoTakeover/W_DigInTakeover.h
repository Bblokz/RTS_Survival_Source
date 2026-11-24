// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_DigInTakeover.generated.h"

/**
 * @brief The field manual on Dig In, the tank ability that builds a sandbag wall: a short looping in-game clip of a
 * Panzer IV digging in and two T-26s' shells hitting the sandbags, next to how the wall works.
 * Opened by UInfoTakeoverSubsystem::QueueDigInTakeover from the Show Dig In Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_DigInTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_DigInTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_DigInTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupDigIn();

	static FText GetDigInTitle();

	/** @return What the ability does and when to use it, in plain paragraphs, for the archive entry. */
	static FText GetDigInSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
