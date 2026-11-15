// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_VulturesAtTakeover.generated.h"

/**
 * @brief The field manual on the Hazmat Vultures' AT stick grenades: a short looping in-game clip of the squad
 * sticking its grenades onto a T-26 next to how the volley works. Opened by
 * UInfoTakeoverSubsystem::QueueVulturesAtTakeover from the Show Vultures AT Grenade Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_VulturesAtTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_VulturesAtTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_VulturesAtTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupVulturesAt();

	static FText GetVulturesAtTitle();

	/** @return What the AT stick grenade volley does and how to use it, in plain paragraphs, for the archive entry. */
	static FText GetVulturesAtSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
