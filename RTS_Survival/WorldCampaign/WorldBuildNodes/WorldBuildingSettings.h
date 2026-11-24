// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "WorldBuildingSettings.generated.h"

/**
 * @brief What one building type does on the world map. Effects are capabilities, so a new building type is an enum
 * value, a price in the campaign rules and one entry here; no new code unless it needs a new kind of effect.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldBuildingEffectDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (MultiLine = true))
	FText Description;

	/** World-map fog revealed around the node; 0 = no reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0"))
	float FowRevealRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog", meta = (ClampMin = "0"))
	float FowRevealFalloff = 0.f;

	/** Pays the campaign rules' SupplyDepotIncomePerTurn at the start of every player turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	bool bPaysSupplyDepotIncome = false;

	/** Enemy items and missions within this XY radius of the node are weakened; 0 = no support. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0"))
	float EnemyStrengthReductionRadius = 0.f;

	/** Strength percentage removed from each enemy item or mission in range; applied as a negative contribution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0"))
	int32 EnemyStrengthReductionPercent = 0;

	/** Reason line shown in the strength estimate of a weakened target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support")
	FText EnemyStrengthReductionReason;

	bool GetRevealsFow() const { return FowRevealRadius > 0.f; }
	bool GetReducesEnemyStrength() const { return EnemyStrengthReductionRadius > 0.f && EnemyStrengthReductionPercent > 0; }
};

/**
 * @brief Project settings page (Config/DefaultGame.ini) for what each world-map building does. Prices and depot
 * income stay in UWorldCampaignRulesSettings; this page holds the effects only.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Buildings"))
class RTS_SURVIVAL_API UWorldBuildingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldBuildingSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Buildings")
	TMap<EWorldCampaignBuilding, FWorldBuildingEffectDefinition> BuildingEffects;

	static const UWorldBuildingSettings* Get()
	{
		return GetDefault<UWorldBuildingSettings>();
	}

	/** @return The building's effects, or nullptr when the building has no entry. */
	static const FWorldBuildingEffectDefinition* FindBuildingEffect(EWorldCampaignBuilding Building);
};
