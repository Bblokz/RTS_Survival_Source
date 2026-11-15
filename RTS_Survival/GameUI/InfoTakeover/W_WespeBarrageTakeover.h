// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_WespeBarrageTakeover.generated.h"

/**
 * @brief The field manual on the Wespe's Barrage, an aimed artillery ability: a short looping in-game clip of a
 * Wespe battery firing a barrage and the shells landing on a T-26 and a rifle squad, next to how the ability works.
 * Opened by UInfoTakeoverSubsystem::QueueWespeBarrageTakeover from the Show Wespe Barrage Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_WespeBarrageTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_WespeBarrageTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_WespeBarrageTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupWespeBarrage();

	static FText GetWespeBarrageTitle();

	/** @return What the ability does and when to use it, in plain paragraphs, for the archive entry. */
	static FText GetWespeBarrageSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
