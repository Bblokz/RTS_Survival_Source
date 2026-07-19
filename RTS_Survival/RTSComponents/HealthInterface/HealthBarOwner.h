// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HealthBarOwner.generated.h"


enum class EHealthLevel : uint8;
DECLARE_MULTICAST_DELEGATE(FOnUnitDies);

/** @brief Marks actors whose health state can be driven through the shared health-owner contract. */
UINTERFACE(Blueprintable)
class UHealthBarOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Receives health-state changes and optionally exposes a native unit-death notification.
 */
class RTS_SURVIVAL_API IHealthBarOwner
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** Returns the native death event when this health owner provides one. */
	virtual FOnUnitDies* GetOnUnitDiesDelegate()
	{
		return nullptr;
	}

	/**
	 * Called by implemented health component.
	 * @param PercentageLeft What percentage of health is left.
	 * @param bIsHealing
	 */
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) =0;
};
