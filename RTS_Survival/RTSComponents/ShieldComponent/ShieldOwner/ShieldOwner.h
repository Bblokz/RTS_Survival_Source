// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ShieldOwner.generated.h"

class UShieldComponent;

/**
 * @brief Native interface marker for actors that expose a cached optional shield component.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UShieldOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Lets weapon and damage systems obtain an actor's cached shield without component searches.
 */
class RTS_SURVIVAL_API IShieldOwner
{
	GENERATED_BODY()

public:
	/** @return The cached optional shield component owned by this actor. */
	virtual UShieldComponent* GetShield() const = 0;
};
