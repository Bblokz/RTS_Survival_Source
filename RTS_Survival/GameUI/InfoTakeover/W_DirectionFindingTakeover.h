// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"

#include "W_DirectionFindingTakeover.generated.h"

/**
 * @brief The Oder-Warthe field manual on radio direction finding: a looping clip of the tactical map (the pump house's
 * bearing wedge, an attack order narrowing it, a second post's wedge crossing it) next to how the hunt for the hidden
 * command post works. Opened by UInfoTakeoverSubsystem::QueueDirectionFindingTakeover from the Show Direction Finding
 * Takeover node (Mission_OW_ListeningPost, when DF 1 is taken).
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_DirectionFindingTakeover copies the Dig In one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_DirectionFindingTakeover : public UW_ClipInfoTakeover
{
	GENERATED_BODY()

public:
	UW_DirectionFindingTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupDirectionFinding();

	static FText GetDirectionFindingTitle();

	/** @return How the listening posts find the command post, in plain paragraphs, for the archive entry. */
	static FText GetDirectionFindingSummary();

protected:
	virtual TArray<FClipTakeoverFact> MakeFacts() const override;
	virtual FText GetClipCaption() const override;
};
