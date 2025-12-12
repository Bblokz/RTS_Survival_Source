#pragma once

#include "CoreMinimal.h"


#include "HealthBarIcons.generated.h"

UENUM(BlueprintType, Blueprintable)
enum class ETargetTypeIcon :uint8
{
	None,
	BasicInfantryArmor,
	HazmatInfantryArmor,
	HeavyInfantryArmor,
	CommandoInfantryArmor,
	ArmoredCarArmor,
	LightArmor,
	MediumArmor,
	HeavilyArmored,
	SuperHeavyArmor,
	Reinforced,
	BasicBuildingArmor
};

static FString GetTargetTypeDisplayName(const ETargetTypeIcon TargetTypeIcon)
{
	switch (TargetTypeIcon) {
	case ETargetTypeIcon::None:
		return "Vulnerable";
	case ETargetTypeIcon::BasicInfantryArmor:
		return "Infantry Gear";
	case ETargetTypeIcon::HazmatInfantryArmor:
		return "Hazmat Armor";
	case ETargetTypeIcon::HeavyInfantryArmor:
		return "Reinforced Suit";
	case ETargetTypeIcon::CommandoInfantryArmor:
		return "Commando Suit";
	case ETargetTypeIcon::ArmoredCarArmor:
		return "Vehicle Plating";
	case ETargetTypeIcon::LightArmor:
		return "Light Armor";
	case ETargetTypeIcon::MediumArmor:
		return "Medium Armor";
	case ETargetTypeIcon::HeavilyArmored:
		return "Heavy Armor";
	case ETargetTypeIcon::SuperHeavyArmor:
		return "Super Heavy Armor";
	case ETargetTypeIcon::Reinforced:
		return "Reinforced";
	case ETargetTypeIcon::BasicBuildingArmor:
		return "Building Armor";
	}
	return "Unknown Target Type Icon";
}
