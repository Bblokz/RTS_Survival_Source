// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "WorldCampaignRulesSettings.generated.h"

/**
 * @brief The tunable numbers behind rank, the global ability tier cap, alert, the enemy budget and the freeze.
 * Plain data so the rules in WorldCampaignRules can be tested without a world.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignRuleNumbers
{
	GENERATED_BODY()

	/** Missions completed needed for Tier II, III and IV, in that order. Tier I needs none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank And Tier")
	TArray<int32> MissionsNeededPerTierAboveOne = {2, 5, 9};

	/** Missions completed per rank step; 16 ranks exist. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rank And Tier", meta = (ClampMin = "1"))
	int32 MissionsPerRank = 1;

	/** Points the enemy may spend in one enemy turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	int32 EnemyBudgetPerTurn = 10;

	/** Alert added when the player wins a mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0.0"))
	float AlertGainMissionWon = 0.25f;

	/** Alert added when the player destroys an enemy item, a division or a convoy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0.0"))
	float AlertGainEnemyItemDestroyed = 0.15f;

	/** Extra alert for a strike right at the wall, scaled down to zero at the player HQ. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0.0"))
	float AlertGainMaxExtraNearWall = 0.15f;

	/** Alert removed in every enemy turn that follows a turn without a successful strike. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0.0"))
	float AlertDecayPerQuietTurn = 0.1f;

	/** Freeze points needed for each stage; the last entry repeats for later stages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TArray<int32> FreezePointsPerStage = {30, 40, 50, 60, 70};

	/** The freeze is complete at this stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "1"))
	int32 FreezeFinalStage = 5;

	/** Reaction points one convoy costs the enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "1"))
	int32 ConvoyReactionCost = 4;

	/** Reaction points one division costs the enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "1"))
	int32 DivisionReactionCost = 8;

	/** A story mission counts this many times toward a country's liberation value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countries", meta = (ClampMin = "1"))
	int32 StoryMissionLiberationWeight = 2;

	/** Liberation value at which the second and later build nodes of a country unlock, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Countries")
	TArray<float> BuildNodeUnlockLiberation = {0.5f, 1.f};

	/** What the player starts a new campaign with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	FWorldMapResources StartingResources;

	/** What one supply depot pays at the start of each player turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	FWorldMapResources SupplyDepotIncomePerTurn;

	/** Price of each building. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	TMap<EWorldCampaignBuilding, FWorldMapResources> BuildingCosts;

	FWorldCampaignRuleNumbers();
};

/**
 * @brief Project settings page for the campaign rule numbers (Config/DefaultGame.ini).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Rules"))
class RTS_SURVIVAL_API UWorldCampaignRulesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "World Campaign Rules")
	FWorldCampaignRuleNumbers Numbers;

	static const FWorldCampaignRuleNumbers& GetNumbers();
};
