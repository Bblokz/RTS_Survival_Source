#pragma once


#include "CoreMinimal.h"

#include "EGlobalAbilityTier.generated.h"

/**
 * @brief How far into a campaign a global ability belongs. Each mission manager sets the highest tier the player
 * may use, so a mission can keep e.g. the radixite V-weapon out of an early operation.
 * Commanders follow the slot plan: slots 1-2 Tier I, slot 3 Tier II, slots 4-5 Tier III, slot 6 Tier IV.
 */
UENUM(BlueprintType)
enum class EGlobalAbilityTier : uint8
{
	TierI UMETA(DisplayName = "Tier I"),
	TierII UMETA(DisplayName = "Tier II"),
	TierIII UMETA(DisplayName = "Tier III"),
	TierIV UMETA(DisplayName = "Tier IV"),
};

inline FString Global_GetGlobalAbilityTierDisplayName(const EGlobalAbilityTier Tier)
{
	switch (Tier)
	{
	case EGlobalAbilityTier::TierI:
		return TEXT("Tier I");
	case EGlobalAbilityTier::TierII:
		return TEXT("Tier II");
	case EGlobalAbilityTier::TierIII:
		return TEXT("Tier III");
	case EGlobalAbilityTier::TierIV:
		return TEXT("Tier IV");
	default:
		return TEXT("Tier");
	}
}
