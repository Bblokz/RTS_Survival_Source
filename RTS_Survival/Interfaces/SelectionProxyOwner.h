// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SelectionProxyOwner.generated.h"

UINTERFACE(MinimalAPI)
class URTSSelectionProxyOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief For actors the player clicks on that are not selectable themselves but stand in for one that is, such as
 * a neutral building that is selected through the garrison actor it spawns while the player has soldiers inside.
 * The player controller swaps a primary click on the implementer for the actor returned here.
 */
class RTS_SURVIVAL_API IRTSSelectionProxyOwner
{
	GENERATED_BODY()

public:
	/** @return The selectable actor to select instead of this one, or nullptr when there is none right now. */
	virtual AActor* GetSelectionProxyActor() const = 0;
};
