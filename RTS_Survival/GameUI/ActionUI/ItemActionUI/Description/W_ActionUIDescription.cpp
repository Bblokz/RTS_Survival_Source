// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ActionUIDescription.h"

#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_ActionUIDescription::SetCostsForAbility(const TMap<ERTSResourceType, int32>& ResourceCosts) const
{
	if (not EnsureIsValidCostDisplay())
	{
		return;
	}
	M_CostDisplay->SetupCost(ResourceCosts);
}

bool UW_ActionUIDescription::EnsureIsValidCostDisplay() const
{
	if (IsValid(M_CostDisplay))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CostDisplay"),
		TEXT("UW_ActionUIDescription::EnsureIsValidCostDisplay"),
		this
	);
	return false;
}
