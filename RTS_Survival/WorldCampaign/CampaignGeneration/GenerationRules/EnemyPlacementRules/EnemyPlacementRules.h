#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyRuleVariantSelectionMode/Enum_EnemyRuleVariantSelectionMode.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyTopologySearchStrategy/Enum_EnemyTopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"

#include "EnemyPlacementRules.generated.h"

USTRUCT(BlueprintType)
struct FEnemyItemToItemSpacingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinEnemySeparationHopsOtherType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinEnemySeparationHopsSameType = 0;
};

USTRUCT(BlueprintType)
struct FEnemyItemToEnemyHQSpacingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MinHopsFromEnemyHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	int32 MaxHopsFromEnemyHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing")
	EEnemyTopologySearchStrategy Preference = EEnemyTopologySearchStrategy::None;
};

USTRUCT(BlueprintType)
struct FEnemyItemPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FEnemyItemToItemSpacingRules ItemSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FEnemyItemToEnemyHQSpacingRules EnemyHQSpacing;
};

USTRUCT(BlueprintType)
struct FEnemyItemPlacementVariant
{
	GENERATED_BODY()

	// When false, ignore this variant entirely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bEnabled = true;

	// If true, use these rules instead of base for this placement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant")
	bool bOverrideRules = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant", meta = (EditCondition = "bOverrideRules"))
	FEnemyItemPlacementRules OverrideRules;
};

USTRUCT(BlueprintType)
struct FEnemyItemRuleset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	FEnemyItemPlacementRules BaseRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset")
	EEnemyRuleVariantSelectionMode VariantMode = EEnemyRuleVariantSelectionMode::None;

	// Example: 3 variants -> placements 0,3,6 use Variants[0], 1,4,7 use [1], 2,5,8 use [2].
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ruleset", meta = (EditCondition = "VariantMode!=EEnemyRuleVariantSelectionMode::None"))
	TArray<FEnemyItemPlacementVariant> Variants;
};

USTRUCT(BlueprintType)
struct FEnemyPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TMap<EMapEnemyItem, FEnemyItemRuleset> RulesByEnemyItem;
};
