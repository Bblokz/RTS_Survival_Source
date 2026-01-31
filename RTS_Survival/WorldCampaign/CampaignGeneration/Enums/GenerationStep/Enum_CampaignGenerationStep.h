#pragma once

#include "CoreMinimal.h"

#include "Enum_CampaignGenerationStep.generated.h"

UENUM(BlueprintType)
enum class ECampaignGenerationStep : uint8
{
	NotSet,
	CreateConnections,
	PlaceHQ,
	PlaceEnemyHQ,
	PlaceEnemyObjects,
	PlaceNeutralObjects,
	PlaceMissions
};
