#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"

#include "BxpItemWidgetState.generated.h"

USTRUCT()
struct FBxpItemWidgetState
{
	GENERATED_BODY()

	EBuildingExpansionStatus M_Status = EBuildingExpansionStatus::BXS_NotUnlocked;
	EBuildingExpansionType M_Type = EBuildingExpansionType::BXT_Invalid;

	// Saves the construction rules for this expansion also saves the socket it was attached to.
	FBxpConstructionRules M_PackedExpansionConstructionRules = FBxpConstructionRules();
};
