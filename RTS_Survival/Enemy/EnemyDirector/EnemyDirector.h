// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "EnemyDirector.generated.h"

/** Identifies the Soviet director whose forces oppose the player in a mission. */
UENUM(BlueprintType)
enum class EEnemyDirector : uint8
{
	DirectorOfShield UMETA(DisplayName = "Director of Shield"),
	DirectorOfForge UMETA(DisplayName = "Director of Forge"),
	DirectorOfHarvest UMETA(DisplayName = "Director of Harvest")
};
