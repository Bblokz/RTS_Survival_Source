#pragma once

#include "CoreMinimal.h"

#include "Enum_CampaignGenerationStep.generated.h"

UENUM(BlueprintType)
enum class ECampaignGenerationStep : uint8
{
	NotStarted,
	ConnectionsCreated,
	PlayerHQPlaced,
	EnemyHQPlaced,
	EnemyObjectsPlaced,
	NeutralObjectsPlaced,
	MissionsPlaced,
	Finished
};
