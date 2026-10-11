// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "HealAbilityTypes.generated.h"

// Stored in FUnitAbilityEntry::CustomType for EAbilityID::IdHeal; decides what the card button does.
UENUM(BlueprintType)
enum class EHealAbilityType : uint8
{
	// The button asks for an injured allied squad unit to heal.
	HealingActive,
	// The button cancels the heal order that is running.
	HealingDisabled
};
