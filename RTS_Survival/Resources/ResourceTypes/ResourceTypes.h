// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class ERTSResourceType: uint8
{
	Resource_None UMETA(DisplayName = "None"),
	Resource_Radixite UMETA(DisplayName = "Radixite"),
	Resource_Metal UMETA(DisplayName = "Metal"),
	Resource_VehicleParts UMETA(DisplayName = "VehicleParts"),
	Resource_Fuel UMETA(DisplayName = "Fuel"),
	Resource_Ammo UMETA(DisplayName = "Ammo"),
	Blueprint_Weapon UMETA(DisplayName = "WeaponBlueprint"),
	Blueprint_Vehicle UMETA(DisplayName = "VehicleBlueprint"),
	Blueprint_Energy UMETA(DisplayName = "EnergyBlueprint"),
	Blueprint_Construction UMETA(DisplayName = "ConstructionBlueprint"),
};

static bool GetIsResourceOfBlueprintType(const ERTSResourceType ResourceType)
{
	switch (ResourceType) {
	case ERTSResourceType::Resource_None:
	case ERTSResourceType::Resource_Radixite:
	case ERTSResourceType::Resource_Metal:
	case ERTSResourceType::Resource_VehicleParts:
	case ERTSResourceType::Resource_Fuel:
	case ERTSResourceType::Resource_Ammo:
		return false;
		break;
	case ERTSResourceType::Blueprint_Weapon:
	case ERTSResourceType::Blueprint_Vehicle:
	case ERTSResourceType::Blueprint_Energy:
	case ERTSResourceType::Blueprint_Construction:
		return true;
	}
	return false;
}

static FString Global_GetResourceTypeAsString(const ERTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case ERTSResourceType::Resource_Radixite:
		return "Resource_Radixite";
	case ERTSResourceType::Resource_Metal:
		return "Resource_Metal";
	case ERTSResourceType::Resource_VehicleParts:
		return "Resource_VehicleParts";
	case ERTSResourceType::Resource_Fuel:
		return "Resource_Fuel";
	case ERTSResourceType::Resource_Ammo:
		return "Resource_Ammo";
	case ERTSResourceType::Blueprint_Weapon:
		return "Blueprint_Weapon";
	case ERTSResourceType::Blueprint_Vehicle:
		return "Blueprint_Vehicle";
		case ERTSResourceType::Blueprint_Energy:
			return "Blueprint_Energy";
		case ERTSResourceType::Blueprint_Construction:
			return "Blueprint_Construction";
	default:
		return "INVALID";
	}
}

static FString Global_GetResourceTypeDisplayString(const ERTSResourceType ResourceType)
{
	switch (ResourceType) {
	case ERTSResourceType::Resource_None:
		return "None";
	case ERTSResourceType::Resource_Radixite:
		return "Radixite";
	case ERTSResourceType::Resource_Metal:
		return "Metal";
	case ERTSResourceType::Resource_VehicleParts:
		return "Vehicle Parts";
	case ERTSResourceType::Resource_Fuel:
		return "Fuel";
	case ERTSResourceType::Resource_Ammo:
		return "Ammo";
	case ERTSResourceType::Blueprint_Weapon:
		return "Weapon Blueprint";
	case ERTSResourceType::Blueprint_Vehicle:
		return "Vehicle Blueprint";
	case ERTSResourceType::Blueprint_Construction:
			return "Construction Blueprint";
	case ERTSResourceType::Blueprint_Energy:
		return "Energy Blueprint";
	}
	return "INVALID";
}
