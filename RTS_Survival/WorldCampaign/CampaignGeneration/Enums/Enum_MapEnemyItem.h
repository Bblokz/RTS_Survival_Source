#pragma once

#include "CoreMinimal.h"
#include "Enum_MapEnemyItem.generated.h"

UENUM(BlueprintType)
enum class EMapEnemyItem : uint8
{
	None,
	EnemyHQ,
	Factory,
	Airfield,
	Barracks,
	SupplyDepo,
	ResearchFacility,
	FortifiedCheckpoint
};
