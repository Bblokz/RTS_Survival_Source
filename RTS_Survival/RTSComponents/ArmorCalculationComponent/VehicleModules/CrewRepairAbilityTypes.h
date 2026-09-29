// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "CrewRepairAbilityTypes.generated.h"

// Stored in FUnitAbilityEntry::CustomType and FQueueCommand::CustomType for EAbilityID::IdCrewRepair.
UENUM(BlueprintType)
enum class ECrewRepairAbilityType : uint8
{
	EnableRepair,
	DisableRepair
};
