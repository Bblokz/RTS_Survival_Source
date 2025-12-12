#pragma once


#include "CoreMinimal.h"

#include "ArchiveItemTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSArchiveItem :uint8
{
	None,
	// If the archive item is a unit, then to determine if this item is already in the archive you need to check the training option
	// set on it.
	Unit,
	Radixite,
	Metal,
	VehicleParts,
	ArmorMechanic,
	RotateTowards,
	SwitchingWeapons,
	MovingTheCamera,
	PlatedSovietTanks,
	NomadicBuildings,
	DifferentAmmoTypes,
	UnitExperience,
};

static FName Global_GetArchiveItemName(const ERTSArchiveItem Item)
{
	const FName Name = StaticEnum<ERTSArchiveItem>()->GetNameByValue((int64)Item);
	const FName CleanName = FName(*Name.ToString().RightChop(FString("ERTSArchiveItem::").Len()));
	return CleanName;
}

// Used to sort archive items into categories.
UENUM(BlueprintType)
enum class ERTSArchiveType : uint8
{
	None,
	GameMechanics,
	Lore,
	Units
};
