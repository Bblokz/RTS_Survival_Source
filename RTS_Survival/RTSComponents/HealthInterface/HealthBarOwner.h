// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HealthBarOwner.generated.h"


enum class EHealthLevel : uint8;
// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UHealthBarOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IHealthBarOwner
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	 * Called by implemented health component.
	 * @param PercentageLeft What percentage of health is left.
	 * @param bIsHealing
	 */
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) =0;
};
