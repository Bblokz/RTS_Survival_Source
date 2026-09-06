// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "EnemyDirectorUnitOverrideSettings.generated.h"

class UEnemyDirectorUnitOverridesDataAsset;

/**
 * @brief Project settings point enemy-director spawning to its centrally configured override Data Asset.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Enemy Director Unit Overrides"))
class RTS_SURVIVAL_API UEnemyDirectorUnitOverrideSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEnemyDirectorUnitOverrideSettings();

	const UEnemyDirectorUnitOverridesDataAsset* GetUnitOverridesDataAsset() const;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unit Overrides")
	TSoftObjectPtr<UEnemyDirectorUnitOverridesDataAsset> M_UnitOverridesDataAsset;
};
