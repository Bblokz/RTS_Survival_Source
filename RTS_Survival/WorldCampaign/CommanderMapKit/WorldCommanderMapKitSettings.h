// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitTypes.h"
#include "WorldCommanderMapKitSettings.generated.h"

/**
 * @brief Which world-map number a commander's passive scales. Generic so several commanders can share one rule with
 * their own building filter and magnitude. Append only: saved in config by value.
 */
UENUM(BlueprintType)
enum class EWorldCommanderPassive : uint8
{
	None,
	/** Building prices are multiplied by Magnitude. */
	BuildingCostMultiplier,
	/** Building range (airfield, battery, watch post reveal) is multiplied by Magnitude. */
	BuildingRangeMultiplier,
	/** The strength a support building removes from missions in range is multiplied by Magnitude. */
	BuildingStrengthReductionMultiplier,
	/** Per-turn building income is multiplied by Magnitude for the resources in ResourceScope. */
	BuildingIncomeMultiplier,
	/** The building also lowers enemy strength for missions within Magnitude world units. */
	BuildingSupportsMissionsInRange,
	/** Operation rewards are multiplied by Magnitude for the resources in ResourceScope. */
	OperationRewardMultiplier,
	/** The strength an enemy item adds to targets near it is multiplied by Magnitude. */
	EnemyItemStrengthMultiplier,
	/** Operations ignore Magnitude fortifications of their target. */
	IgnoreFortifications,
	/** Every country gets Magnitude extra build nodes. */
	ExtraBuildNodesPerCountry,
	/** Buildings finish in the turn they are ordered. */
	InstantBuildings
};

/**
 * @brief Which of the two map currencies a resource passive scales.
 */
UENUM(BlueprintType)
enum class EWorldMapResourceScope : uint8
{
	Both,
	RadixiteOnly,
	BlueprintsOnly
};

/**
 * @brief One commander's passive. Only the filters its type reads are used.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderPassiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EWorldCommanderPassive Type = EWorldCommanderPassive::None;

	/** Multiplier, range, or count, depending on Type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	float Magnitude = 1.f;

	/** Building the passive applies to; None means every building. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EWorldCampaignBuilding Building = EWorldCampaignBuilding::None;

	/** Operation kind a reward passive applies to; None means every operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EWorldOperationTarget OperationTarget = EWorldOperationTarget::None;

	/** A reward passive with this set skips story (pinned) missions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	bool bExcludeStoryMissions = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EWorldMapResourceScope ResourceScope = EWorldMapResourceScope::Both;

	/** Enemy item an EnemyItemStrengthMultiplier applies to; None means every enemy item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EMapEnemyItem EnemyItem = EMapEnemyItem::None;
};

/**
 * @brief One commander's map ability. Only the fields its type reads are used.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderMapAbilityDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	EWorldCommanderMapAbility Type = EWorldCommanderMapAbility::None;

	/** Percent, damage, count, steps or points, depending on Type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	float Magnitude = 0.f;

	/** World units; for region effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander", meta = (ClampMin = "0.0"))
	float Radius = 0.f;

	/** Player turns the effect lasts, counting the turn of use; 0 = instant or permanent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander", meta = (ClampMin = "0"))
	int32 DurationTurns = 0;

	/** Player turns before the ability can be used again; used on turn N, ready again on turn N + CooldownTurns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander", meta = (ClampMin = "0"))
	int32 CooldownTurns = 0;

	/** Granted by GrantResources; Blueprints is the per-use cap of ConvertRadixiteToBlueprints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FWorldMapResources Resources;
};

/**
 * @brief Everything one commander brings to the world map.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderMapKitDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FText PassiveDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FText PassiveDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FWorldCommanderPassiveDefinition Passive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FText AbilityDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FText AbilityDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	FWorldCommanderMapAbilityDefinition Ability;
};

/**
 * @brief Every commander's world-map kit as plain data, so the rules in WorldCommanderMapKitRules can be tested
 * without a world. The constructor holds the unreviewed proposal of the design doc (section 6).
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderMapKitTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commander")
	TMap<ERTSCommander, FWorldCommanderMapKitDefinition> Commanders;

	FWorldCommanderMapKitTable();

	const FWorldCommanderMapKitDefinition* FindDefinition(const ERTSCommander Commander) const
	{
		return Commanders.Find(Commander);
	}

private:
	void AddFieldCommanderKits();
	void AddSupportCommanderKits();
	void AddAssaultCommanderKits();
};

/**
 * @brief Project settings page for the commanders' world-map kits (Config/DefaultGame.ini). Read through GetKitTable.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Commander Map Kits"))
class RTS_SURVIVAL_API UWorldCommanderMapKitSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "World Commander Map Kits")
	FWorldCommanderMapKitTable KitTable;

	static const FWorldCommanderMapKitTable& GetKitTable();
};
