#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/MissionTier/Enum_MissionTier.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MapObjectAdjacencyRequirement.h"

#include "MissionPlacementRules.generated.h"

/**
 * @brief Tier-level placement rules applied during MissionsPlaced, which runs after
 *        NeutralObjectsPlaced and before Finished in campaign generation.
 */
USTRUCT(BlueprintType)
struct FMissionTierRules
{
	GENERATED_BODY()

	/**
	 * If true, apply hop-based distance rules from the player HQ.
	 * Example: Enable this to guarantee Tier1 missions stay within a short hop radius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseHopsDistanceFromHQ = true;

	/**
	 * Minimum hop distance from the player HQ.
	 * Example: Increase this for late-tier missions so they do not appear next to the start.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ.
	 * Example: Lower this to keep early missions in a tight, reachable ring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MaxHopsFromHQ = 0;

	/**
	 * Preference for selecting hop distance within Min/Max bounds.
	 * Example: PreferMax to bias mission locations deeper into the graph for higher tiers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	ETopologySearchStrategy HopsDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY (world-space) distance rules from the player HQ.
	 * Example: Enable this when you want missions to spread across the map visually, not just by hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseXYDistanceFromHQ = false;

	/**
	 * Minimum XY distance from the player HQ.
	 * Example: Raise this to stop a mission from spawning visually adjacent to the HQ even if hops are high.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MinXYDistanceFromHQ = 0.f;

	/**
	 * Maximum XY distance from the player HQ.
	 * Example: Lower this to keep missions within a mid-map band for a tighter campaign flow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MaxXYDistanceFromHQ = 0.f;

	/**
	 * Preference for selecting XY distance within Min/Max bounds.
	 * Example: PreferMin to pull early missions closer in the world map layout.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ", meta = (EditCondition = "bUseXYDistanceFromHQ"))
	ETopologySearchStrategy XYDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply hop-based spacing rules between missions of the same tier.
	 * Example: Enable this to prevent two Tier2 missions from appearing adjacent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingHops = false;

	/**
	 * Minimum hop spacing between missions.
	 * Example: Increase this if you want players to travel further between key missions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MinMissionSpacingHops = 0;

	/**
	 * Maximum hop spacing between missions.
	 * Example: Lower this to allow missions to cluster as a “chapter” on the map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MaxMissionSpacingHops = 0;

	/**
	 * Preference for selecting hop spacing within Min/Max bounds.
	 * Example: PreferMax to spread missions evenly across the network.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingHops"))
	ETopologySearchStrategy MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY spacing rules between missions.
	 * Example: Enable this when visual spacing on the world map is more important than graph hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingXY = false;

	/**
	 * Minimum XY spacing between missions.
	 * Example: Increase this to avoid overlapping mission icons in a dense area.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	float MinMissionSpacingXY = 0.f;

	/**
	 * Maximum XY spacing between missions.
	 * Example: Lower this to keep missions visually clustered for a focused story arc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	float MaxMissionSpacingXY = 0.f;

	/**
	 * Preference for selecting XY spacing within Min/Max bounds.
	 * Example: PreferMin to keep missions grouped around a region even if hops are large.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions", meta = (EditCondition = "bUseMissionSpacingXY"))
	ETopologySearchStrategy MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;

	/**
	 * Minimum connection degree required for a candidate mission anchor.
	 * Example: Raise this so missions appear on well-connected nodes to create branching routes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MinConnections = 0;

	/**
	 * Maximum connection degree allowed for a candidate mission anchor.
	 * Example: Lower this if you want missions to appear on quieter side nodes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MaxConnections = 0;

	/**
	 * Preference for selecting anchors based on connection degree.
	 * Example: PreferMax to bias missions toward high-traffic hubs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, require a neutral item to be present at the mission anchor.
	 * Example: Enable this for scavenging missions that must sit on top of a resource node.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item")
	bool bNeutralItemRequired = false;

	/**
	 * Specific neutral item type required when bNeutralItemRequired is true.
	 * Example: Set to RuinedCity to ensure urban missions only appear on city anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item", meta = (EditCondition = "bNeutralItemRequired"))
	EMapNeutralObjectType RequiredNeutralItemType = EMapNeutralObjectType::None;

	/**
	 * Additional adjacency requirement for companion objects.
	 * Example: Configure this to demand a nearby NeutralItem before allowing mission placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adjacency")
	FMapObjectAdjacencyRequirement AdjacencyRequirement;
};

/**
 * @brief Per-mission overrides applied during MissionsPlaced, which follows NeutralObjectsPlaced
 *        and leads into the Finished state.
 */
USTRUCT(BlueprintType)
struct FPerMissionRules
{
	GENERATED_BODY()

	/**
	 * Mission tier used to select the baseline FMissionTierRules.
	 * Example: Assign Tier3 to a late-game mission so it inherits deeper placement distances.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	EMissionTier Tier = EMissionTier::NotSet;

	/**
	 * If true, use OverrideRules instead of the tier rules for this mission.
	 * Example: Enable this to place a story-critical mission closer than its tier would allow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOverrideTierRules = false;

	/**
	 * Override rules when bOverrideTierRules is enabled.
	 * Example: Override MinHopsFromHQ to force a special mission to appear within two hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (EditCondition = "bOverrideTierRules"))
	FMissionTierRules OverrideRules;
};

/**
 * @brief Root mission placement configuration used during MissionsPlaced, which occurs
 *        after NeutralObjectsPlaced and before Finished in the generation pipeline.
 */
USTRUCT(BlueprintType)
struct FMissionPlacement
{
	GENERATED_BODY()

	/**
	 * Default rules per mission tier.
	 * Example: Tighten Tier1 distances while expanding Tier4 to push late missions to the outskirts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMissionTier, FMissionTierRules> RulesByTier;

	/**
	 * Per-mission overrides layered on top of the tier rules.
	 * Example: Override a single mission to require a nearby neutral item even if the tier does not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMapMission, FPerMissionRules> RulesByMission;
};
