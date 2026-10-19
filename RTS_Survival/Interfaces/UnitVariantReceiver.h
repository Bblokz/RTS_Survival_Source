// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"

#include "UnitVariantReceiver.generated.h"

UINTERFACE(BlueprintType)
class UUnitVariantReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Implemented in C++ by every unit base class that supports variants; the unit's Blueprint implements
 * the event and switches on the variant to apply its modifications. Fired once, after the subtype is known.
 */
class RTS_SURVIVAL_API IUnitVariantReceiver
{
	GENERATED_BODY()

public:
	/**
	 * @brief Apply the modifications of this variant. Base is also delivered so a Blueprint can run shared setup.
	 * @param Variant The variant the player selected for this unit type; Base for non-player units.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit Variant")
	void BP_ApplyUnitVariant(EUnitVariant Variant);
};
