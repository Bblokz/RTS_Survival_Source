// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionButton.h"
#include "TechWheelNodeButton.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTechWheelNodeClicked, FName /*NodeId*/);

/**
 * @brief The clickable area of one tech wheel node: a button without art that tells the wheel which node was
 * clicked, since UButton's own click carries no parameter. Never takes keyboard focus.
 */
UCLASS()
class RTS_SURVIVAL_API UTechWheelNodeButton : public UUnitVariantSelectionButton
{
	GENERATED_BODY()

public:
	/** Remembers the node, removes the button art and starts passing clicks on through OnNodeClicked. */
	void InitNodeButton(FName NodeId);

	FName GetNodeId() const { return M_NodeId; }

	FOnTechWheelNodeClicked OnNodeClicked;

private:
	UFUNCTION()
	void HandleClicked();

	FName M_NodeId;
};
