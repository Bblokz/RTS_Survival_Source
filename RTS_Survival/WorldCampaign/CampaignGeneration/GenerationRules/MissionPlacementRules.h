#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/MissionTier/Enum_MissionTier.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MapObjectAdjacencyRequirement.h"

#include "MissionPlacementRules.generated.h"

class AAnchorPoint;

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
	 * @note Used in: MissionsPlaced.
	 * @note Why: Enables hop-distance filtering from the player HQ for this tier.
	 * @note Technical: Gates Min/Max hop filters and preference checks.
	 * @note Notes: Drives EditCondition for hop distance fields; disabling ignores hop rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseHopsDistanceFromHQ = true;

	/**
	 * Minimum hop distance from the player HQ.
	 * Example: Increase this for late-tier missions so they do not appear next to the start.
	 * @note Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.
	 * @note Why: Keeps missions away from the start by enforcing a minimum hop distance.
	 * @note Technical: Hard filter against PlayerHQHopDistancesByAnchorKey.
	 * @note Notes: Must be <= MaxHopsFromHQ; ignored when hop distance is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ.
	 * Example: Lower this to keep early missions in a tight, reachable ring.
	 * @note Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.
	 * @note Why: Caps how far missions can be from the player HQ in hop distance.
	 * @note Technical: Hard filter against PlayerHQHopDistancesByAnchorKey.
	 * @note Notes: Must be >= MinHopsFromHQ; ignored when hop distance is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	int32 MaxHopsFromHQ = 0;

	/**
	 * Preference for selecting hop distance within Min/Max bounds.
	 * Example: PreferMax to bias mission locations deeper into the graph for higher tiers.
	 * @note Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.
	 * @note Why: Biases mission anchors toward the near or far edge of the hop window.
	 * @note Technical: Soft preference in candidate ordering; does not bypass Min/Max hop limits.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ"))
	ETopologySearchStrategy HopsDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY (world-space) distance rules from the player HQ.
	 * Example: Enable this when you want missions to spread across the map visually, not just by hops.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Enables world-space (XY) distance filtering from the player HQ.
	 * @note Technical: Gates Min/Max XY checks and preference ordering.
	 * @note Notes: Drives EditCondition for XY distance fields; disabling ignores XY rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ")
	bool bUseXYDistanceFromHQ = false;

	/**
	 * Minimum XY distance from the player HQ.
	 * Example: Raise this to stop a mission from spawning visually adjacent to the HQ even if hops are high.
	 * @note Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.
	 * @note Why: Enforces a minimum world-space radius from the HQ.
	 * @note Technical: Hard filter against XY distance from PlayerHQAnchor.
	 * @note Notes: Must be <= MaxXYDistanceFromHQ; ignored when XY distance is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MinXYDistanceFromHQ = 0.f;

	/**
	 * Maximum XY distance from the player HQ.
	 * Example: Lower this to keep missions within a mid-map band for a tighter campaign flow.
	 * @note Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.
	 * @note Why: Caps world-space distance so missions stay within a desired ring.
	 * @note Technical: Hard filter against XY distance from PlayerHQAnchor.
	 * @note Notes: Must be >= MinXYDistanceFromHQ; ignored when XY distance is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ"))
	float MaxXYDistanceFromHQ = 0.f;

	/**
	 * Preference for selecting XY distance within Min/Max bounds.
	 * Example: PreferMin to pull early missions closer in the world map layout.
	 * @note Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.
	 * @note Why: Biases mission anchors toward near or far XY distances from the HQ.
	 * @note Technical: Soft preference applied in candidate ordering; does not bypass Min/Max XY bounds.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ"))
	ETopologySearchStrategy XYDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply hop-based spacing rules between missions of the same tier.
	 * Example: Enable this to prevent two Tier2 missions from appearing adjacent.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Enables hop-based spacing constraints between missions of this tier.
	 * @note Technical: Gates Min/Max mission hop spacing filters.
	 * @note Notes: Drives EditCondition for hop spacing fields; disabling ignores hop spacing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingHops = false;

	/**
	 * Minimum hop spacing between missions.
	 * Example: Increase this if you want players to travel further between key missions.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingHops is true.
	 * @note Why: Enforces a minimum hop separation between missions of this tier.
	 * @note Technical: Hard filter against hop distances between already placed missions.
	 * @note Notes: Must be <= MaxMissionSpacingHops; ignored when hop spacing is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MinMissionSpacingHops = 0;

	/**
	 * Maximum hop spacing between missions.
	 * Example: Lower this to allow missions to cluster as a “chapter” on the map.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingHops is true.
	 * @note Why: Caps how far apart missions of this tier are allowed to be.
	 * @note Technical: Hard maximum hop spacing filter against placed missions.
	 * @note Notes: Must be >= MinMissionSpacingHops; ignored when hop spacing is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops"))
	int32 MaxMissionSpacingHops = 0;

	/**
	 * Preference for selecting hop spacing within Min/Max bounds.
	 * Example: PreferMax to spread missions evenly across the network.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingHops is true.
	 * @note Why: Biases candidate selection toward larger or smaller hop spacing.
	 * @note Technical: Soft preference applied during candidate ordering; does not bypass Min/Max.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops"))
	ETopologySearchStrategy MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY spacing rules between missions.
	 * Example: Enable this when visual spacing on the world map is more important than graph hops.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Enables world-space spacing rules between missions of this tier.
	 * @note Technical: Gates Min/Max XY spacing filters.
	 * @note Notes: Drives EditCondition for XY spacing fields; disabling ignores XY spacing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions")
	bool bUseMissionSpacingXY = false;

	/**
	 * Minimum XY spacing between missions.
	 * Example: Increase this to avoid overlapping mission icons in a dense area.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingXY is true.
	 * @note Why: Enforces a minimum world-space separation between missions of this tier.
	 * @note Technical: Hard filter against XY distance between missions.
	 * @note Notes: Must be <= MaxMissionSpacingXY; ignored when XY spacing is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY"))
	float MinMissionSpacingXY = 0.f;

	/**
	 * Maximum XY spacing between missions.
	 * Example: Lower this to keep missions visually clustered for a focused story arc.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingXY is true.
	 * @note Why: Caps how far apart missions of this tier can be in world space.
	 * @note Technical: Hard maximum XY spacing filter against placed missions.
	 * @note Notes: Must be >= MinMissionSpacingXY; ignored when XY spacing is disabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY"))
	float MaxMissionSpacingXY = 0.f;

	/**
	 * Preference for selecting XY spacing within Min/Max bounds.
	 * Example: PreferMin to keep missions grouped around a region even if hops are large.
	 * @note Used in: MissionsPlaced when bUseMissionSpacingXY is true.
	 * @note Why: Biases candidates toward tighter or looser XY spacing.
	 * @note Technical: Soft preference applied in candidate ordering; does not bypass Min/Max.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY"))
	ETopologySearchStrategy MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;

	/**
	 * Minimum connection degree required for a candidate mission anchor.
	 * Example: Raise this so missions appear on well-connected nodes to create branching routes.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Ensures missions appear on sufficiently connected nodes for routing options.
	 * @note Technical: Hard minimum connection degree filter using cached degrees.
	 * @note Notes: Must be <= MaxConnections; too high can eliminate all mission candidates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MinConnections = 0;

	/**
	 * Maximum connection degree allowed for a candidate mission anchor.
	 * Example: Lower this if you want missions to appear on quieter side nodes.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Prevents missions from clustering exclusively on high-traffic hubs.
	 * @note Technical: Hard maximum connection degree filter using cached degrees.
	 * @note Notes: Must be >= MinConnections; too low can block mission placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	int32 MaxConnections = 0;

	/**
	 * Preference for selecting anchors based on connection degree.
	 * Example: PreferMax to bias missions toward high-traffic hubs.
	 * @note Used in: MissionsPlaced candidate ordering.
	 * @note Why: Biases selection toward lower or higher connection degrees.
	 * @note Technical: Soft preference used for sorting; does not bypass Min/Max constraints.
	 * @note Notes: Deterministic ordering also uses anchor keys and attempt index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology")
	ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, require a neutral item to be present at the mission anchor.
	 * Example: Enable this for scavenging missions that must sit on top of a resource node.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Allows missions to be bound to existing neutral items for thematic placement.
	 * @note Technical: Gates RequiredNeutralItemType checks against NeutralItemsByAnchorKey.
	 * @note Notes: Drives EditCondition; disabling ignores neutral item requirements.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item")
	bool bNeutralItemRequired = false;

	/**
	 * Specific neutral item type required when bNeutralItemRequired is true.
	 * Example: Set to RuinedCity to ensure urban missions only appear on city anchors.
	 * @note Used in: MissionsPlaced when bNeutralItemRequired is true.
	 * @note Why: Forces mission placement on anchors that already host a specific neutral item.
	 * @note Technical: Hard filter against NeutralItemsByAnchorKey values.
	 * @note Notes: Ignored when bNeutralItemRequired is false; can heavily restrict candidates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item",
		meta = (EditCondition = "bNeutralItemRequired"))
	EMapNeutralObjectType RequiredNeutralItemType = EMapNeutralObjectType::None;

	/**
	 * Additional adjacency requirement for companion objects.
	 * Example: Configure this to demand a nearby NeutralItem before allowing mission placement.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Enforces adjacency requirements beyond simple distance checks.
	 * @note Technical: Applied as a hard filter against nearby object types.
	 * @note Notes: Can gate placement entirely if adjacency requirements are unmet.
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
	 * @note Used in: MissionsPlaced.
	 * @note Why: Maps this mission to a tier so it inherits the tier rules by default.
	 * @note Technical: Used to look up FMissionTierRules from RulesByTier.
	 * @note Notes: If Tier is NotSet and no override is provided, placement may fail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	EMissionTier Tier = EMissionTier::NotSet;

	/**
	 * If true, use OverrideRules instead of the tier rules for this mission.
	 * Example: Enable this to place a story-critical mission closer than its tier would allow.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Allows a specific mission to deviate from its tier rules.
	 * @note Technical: When true, OverrideRules replace tier rules for this mission.
	 * @note Notes: Drives EditCondition for OverrideRules; disabling hides the override field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOverrideTierRules = false;

	/**
	 * If true, only consider the specified anchor list for this mission placement.
	 * Example: Enable this to force a mission to appear on a curated set of anchors.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Allows designers to restrict mission placement to specific anchors.
	 * @note Technical: Candidate selection only considers anchors from OverridePlacementAnchorCandidates.
	 * @note Notes: If no candidates are valid, mission placement fails for this mission.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOverridePlacementWithArray = false;

	/**
	 * Candidate anchors used when bOverridePlacementWithArray is true.
	 * Example: Populate with anchor actors that match the narrative requirements of this mission.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Explicitly constrains mission placement to designer-selected anchors.
	 * @note Technical: Filtered against cached anchors and occupancy before selection.
	 * @note Notes: Invalid or uncached anchors are ignored during filtering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	TArray<TObjectPtr<AAnchorPoint>> OverridePlacementAnchorCandidates;

	/**
	 * Minimum connection degree when bOverridePlacementWithArray is enabled.
	 * Example: Set to 2 to require at least two connections for the override anchors.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier topology bounds for override-driven placement.
	 * @note Technical: Clamped against OverrideMaxConnections before filtering.
	 * @note Notes: Defaults to 0 to avoid restricting when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	int32 OverrideMinConnections = 0;

	/**
	 * Maximum connection degree when bOverridePlacementWithArray is enabled.
	 * Example: Lower this to prevent placement on highly connected anchors.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier topology bounds for override-driven placement.
	 * @note Technical: Clamped to be >= OverrideMinConnections before filtering.
	 * @note Notes: Defaults to a large value so it does not restrict when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	int32 OverrideMaxConnections = TNumericLimits<int32>::Max();

	/**
	 * Connection preference when bOverridePlacementWithArray is enabled.
	 * Example: PreferMax to bias toward highly connected anchors.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier preference to control override candidate scoring.
	 * @note Technical: Applied as a soft preference in candidate ordering.
	 * @note Notes: NotSet disables preference weighting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	ETopologySearchStrategy OverrideConnectionPreference = ETopologySearchStrategy::NotSet;

	/**
	 * Minimum hops from the player HQ when bOverridePlacementWithArray is enabled.
	 * Example: Set to 1 to prevent the mission from spawning directly on the HQ anchor.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier hop bounds for override-driven placement.
	 * @note Technical: Clamped against OverrideMaxHopsFromPlayerHQ before filtering.
	 * @note Notes: Defaults to 0 to avoid restricting when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	int32 OverrideMinHopsFromPlayerHQ = 0;

	/**
	 * Maximum hops from the player HQ when bOverridePlacementWithArray is enabled.
	 * Example: Lower this to keep the override mission close to the start.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier hop bounds for override-driven placement.
	 * @note Technical: Clamped to be >= OverrideMinHopsFromPlayerHQ before filtering.
	 * @note Notes: Defaults to a large value so it does not restrict when unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	int32 OverrideMaxHopsFromPlayerHQ = TNumericLimits<int32>::Max();

	/**
	 * Hop-distance preference when bOverridePlacementWithArray is enabled.
	 * Example: PreferMin to pull the override mission toward the HQ side of the range.
	 * @note Used in: MissionsPlaced when bOverridePlacementWithArray is true.
	 * @note Why: Overrides tier hop preference to control override candidate scoring.
	 * @note Technical: Applied as a soft preference in candidate ordering.
	 * @note Notes: NotSet disables preference weighting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Override",
		meta = (EditCondition = "bOverridePlacementWithArray"))
	ETopologySearchStrategy OverrideHopsPreference = ETopologySearchStrategy::NotSet;

	/**
	 * Override rules when bOverrideTierRules is enabled.
	 * Example: Override MinHopsFromHQ to force a special mission to appear within two hops.
	 * @note Used in: MissionsPlaced when bOverrideTierRules is true.
	 * @note Why: Defines the mission-specific rules that replace tier defaults.
	 * @note Technical: Applied as hard/soft constraints for this mission only.
	 * @note Notes: Ignored when bOverrideTierRules is false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission",
		meta = (EditCondition = "bOverrideTierRules"))
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
	 * @note Used in: MissionsPlaced.
	 * @note Why: Establishes baseline rules for each mission tier.
	 * @note Technical: Looked up by EMissionTier and applied unless overridden per mission.
	 * @note Notes: Missing tiers can cause placements to fail if referenced by a mission.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMissionTier, FMissionTierRules> RulesByTier;

	/**
	 * Per-mission overrides layered on top of the tier rules.
	 * Example: Override a single mission to require a nearby neutral item even if the tier does not.
	 * @note Used in: MissionsPlaced.
	 * @note Why: Allows specific missions to override their tier's baseline rules.
	 * @note Technical: Looked up by EMapMission; overrides apply before placement filtering.
	 * @note Notes: Only missions present in this map receive overrides; others use tier rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions")
	TMap<EMapMission, FPerMissionRules> RulesByMission;
};
