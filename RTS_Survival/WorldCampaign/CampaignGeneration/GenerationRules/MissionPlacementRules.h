#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/MissionTier/Enum_MissionTier.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MapObjectAdjacencyRequirement.h"

#include "MissionPlacementRules.generated.h"

USTRUCT(BlueprintType)
struct FMissionTierRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseHopsDistanceFromHQ = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MinHopsFromHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MaxHopsFromHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	ETopologySearchStrategy HopsDistancePreference = ETopologySearchStrategy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseXYDistanceFromHQ = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MinXYDistanceFromHQ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MaxXYDistanceFromHQ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	ETopologySearchStrategy XYDistancePreference = ETopologySearchStrategy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingHops = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MinMissionSpacingHops = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MaxMissionSpacingHops = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	ETopologySearchStrategy MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingXY = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	float MinMissionSpacingXY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	float MaxMissionSpacingXY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	ETopologySearchStrategy MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MinConnections = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MaxConnections = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item")
	bool bNeutralItemRequired = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item", meta = (EditCondition = "bNeutralItemRequired"))
	EMapNeutralObjectType RequiredNeutralItemType = EMapNeutralObjectType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adjacency")
	FMapObjectAdjacencyRequirement AdjacencyRequirement;
};

USTRUCT(BlueprintType)
struct FPerMissionRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	EMissionTier Tier = EMissionTier::NotSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOverrideTierRules = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (EditCondition = "bOverrideTierRules"))
	FMissionTierRules OverrideRules;
};

USTRUCT(BlueprintType)
struct FMissionPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMissionTier, FMissionTierRules> RulesByTier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMapMission, FPerMissionRules> RulesByMission;
};
