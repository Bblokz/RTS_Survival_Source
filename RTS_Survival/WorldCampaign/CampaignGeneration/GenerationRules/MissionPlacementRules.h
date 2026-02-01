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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Enables hop-distance filtering from the player HQ for this tier.\n"
		                  "Technical: Gates Min/Max hop filters and preference checks.\n"
		                  "Notes: Drives EditCondition for hop distance fields; disabling ignores hop rules."))
	bool bUseHopsDistanceFromHQ = true;

	/**
	 * Minimum hop distance from the player HQ.
	 * Example: Increase this for late-tier missions so they do not appear next to the start.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.\n"
		                  "Why: Keeps missions away from the start by enforcing a minimum hop distance.\n"
		                  "Technical: Hard filter against PlayerHQHopDistancesByAnchorKey.\n"
		                  "Notes: Must be <= MaxHopsFromHQ; ignored when hop distance is disabled."))
	int32 MinHopsFromHQ = 0;

	/**
	 * Maximum hop distance from the player HQ.
	 * Example: Lower this to keep early missions in a tight, reachable ring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.\n"
		                  "Why: Caps how far missions can be from the player HQ in hop distance.\n"
		                  "Technical: Hard filter against PlayerHQHopDistancesByAnchorKey.\n"
		                  "Notes: Must be >= MinHopsFromHQ; ignored when hop distance is disabled."))
	int32 MaxHopsFromHQ = 0;

	/**
	 * Preference for selecting hop distance within Min/Max bounds.
	 * Example: PreferMax to bias mission locations deeper into the graph for higher tiers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseHopsDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseHopsDistanceFromHQ is true.\n"
		                  "Why: Biases mission anchors toward the near or far edge of the hop window.\n"
		                  "Technical: Soft preference in candidate ordering; does not bypass Min/Max hop limits.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
	ETopologySearchStrategy HopsDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY (world-space) distance rules from the player HQ.
	 * Example: Enable this when you want missions to spread across the map visually, not just by hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Enables world-space (XY) distance filtering from the player HQ.\n"
		                  "Technical: Gates Min/Max XY checks and preference ordering.\n"
		                  "Notes: Drives EditCondition for XY distance fields; disabling ignores XY rules."))
	bool bUseXYDistanceFromHQ = false;

	/**
	 * Minimum XY distance from the player HQ.
	 * Example: Raise this to stop a mission from spawning visually adjacent to the HQ even if hops are high.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.\n"
		                  "Why: Enforces a minimum world-space radius from the HQ.\n"
		                  "Technical: Hard filter against XY distance from PlayerHQAnchor.\n"
		                  "Notes: Must be <= MaxXYDistanceFromHQ; ignored when XY distance is disabled."))
	float MinXYDistanceFromHQ = 0.f;

	/**
	 * Maximum XY distance from the player HQ.
	 * Example: Lower this to keep missions within a mid-map band for a tighter campaign flow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.\n"
		                  "Why: Caps world-space distance so missions stay within a desired ring.\n"
		                  "Technical: Hard filter against XY distance from PlayerHQAnchor.\n"
		                  "Notes: Must be >= MinXYDistanceFromHQ; ignored when XY distance is disabled."))
	float MaxXYDistanceFromHQ = 0.f;

	/**
	 * Preference for selecting XY distance within Min/Max bounds.
	 * Example: PreferMin to pull early missions closer in the world map layout.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distance|HQ",
		meta = (EditCondition = "bUseXYDistanceFromHQ",
		        ToolTip = "Used in: MissionsPlaced when bUseXYDistanceFromHQ is true.\n"
		                  "Why: Biases mission anchors toward near or far XY distances from the HQ.\n"
		                  "Technical: Soft preference applied in candidate ordering; does not bypass Min/Max XY bounds.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
	ETopologySearchStrategy XYDistancePreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply hop-based spacing rules between missions of the same tier.
	 * Example: Enable this to prevent two Tier2 missions from appearing adjacent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Enables hop-based spacing constraints between missions of this tier.\n"
		                  "Technical: Gates Min/Max mission hop spacing filters.\n"
		                  "Notes: Drives EditCondition for hop spacing fields; disabling ignores hop spacing."))
	bool bUseMissionSpacingHops = false;

	/**
	 * Minimum hop spacing between missions.
	 * Example: Increase this if you want players to travel further between key missions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingHops is true.\n"
		                  "Why: Enforces a minimum hop separation between missions of this tier.\n"
		                  "Technical: Hard filter against hop distances between already placed missions.\n"
		                  "Notes: Must be <= MaxMissionSpacingHops; ignored when hop spacing is disabled."))
	int32 MinMissionSpacingHops = 0;

	/**
	 * Maximum hop spacing between missions.
	 * Example: Lower this to allow missions to cluster as a “chapter” on the map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingHops is true.\n"
		                  "Why: Caps how far apart missions of this tier are allowed to be.\n"
		                  "Technical: Hard maximum hop spacing filter against placed missions.\n"
		                  "Notes: Must be >= MinMissionSpacingHops; ignored when hop spacing is disabled."))
	int32 MaxMissionSpacingHops = 0;

	/**
	 * Preference for selecting hop spacing within Min/Max bounds.
	 * Example: PreferMax to spread missions evenly across the network.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingHops",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingHops is true.\n"
		                  "Why: Biases candidate selection toward larger or smaller hop spacing.\n"
		                  "Technical: Soft preference applied during candidate ordering; does not bypass Min/Max.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
	ETopologySearchStrategy MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, apply XY spacing rules between missions.
	 * Example: Enable this when visual spacing on the world map is more important than graph hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Enables world-space spacing rules between missions of this tier.\n"
		                  "Technical: Gates Min/Max XY spacing filters.\n"
		                  "Notes: Drives EditCondition for XY spacing fields; disabling ignores XY spacing."))
	bool bUseMissionSpacingXY = false;

	/**
	 * Minimum XY spacing between missions.
	 * Example: Increase this to avoid overlapping mission icons in a dense area.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingXY is true.\n"
		                  "Why: Enforces a minimum world-space separation between missions of this tier.\n"
		                  "Technical: Hard filter against XY distance between missions.\n"
		                  "Notes: Must be <= MaxMissionSpacingXY; ignored when XY spacing is disabled."))
	float MinMissionSpacingXY = 0.f;

	/**
	 * Maximum XY spacing between missions.
	 * Example: Lower this to keep missions visually clustered for a focused story arc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingXY is true.\n"
		                  "Why: Caps how far apart missions of this tier can be in world space.\n"
		                  "Technical: Hard maximum XY spacing filter against placed missions.\n"
		                  "Notes: Must be >= MinMissionSpacingXY; ignored when XY spacing is disabled."))
	float MaxMissionSpacingXY = 0.f;

	/**
	 * Preference for selecting XY spacing within Min/Max bounds.
	 * Example: PreferMin to keep missions grouped around a region even if hops are large.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing|Missions",
		meta = (EditCondition = "bUseMissionSpacingXY",
		        ToolTip = "Used in: MissionsPlaced when bUseMissionSpacingXY is true.\n"
		                  "Why: Biases candidates toward tighter or looser XY spacing.\n"
		                  "Technical: Soft preference applied in candidate ordering; does not bypass Min/Max.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
	ETopologySearchStrategy MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;

	/**
	 * Minimum connection degree required for a candidate mission anchor.
	 * Example: Raise this so missions appear on well-connected nodes to create branching routes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Ensures missions appear on sufficiently connected nodes for routing options.\n"
		                  "Technical: Hard minimum connection degree filter using cached degrees.\n"
		                  "Notes: Must be <= MaxConnections; too high can eliminate all mission candidates."))
	int32 MinConnections = 0;

	/**
	 * Maximum connection degree allowed for a candidate mission anchor.
	 * Example: Lower this if you want missions to appear on quieter side nodes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Prevents missions from clustering exclusively on high-traffic hubs.\n"
		                  "Technical: Hard maximum connection degree filter using cached degrees.\n"
		                  "Notes: Must be >= MinConnections; too low can block mission placement."))
	int32 MaxConnections = 0;

	/**
	 * Preference for selecting anchors based on connection degree.
	 * Example: PreferMax to bias missions toward high-traffic hubs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Topology",
		meta = (ToolTip = "Used in: MissionsPlaced candidate ordering.\n"
		                  "Why: Biases selection toward lower or higher connection degrees.\n"
		                  "Technical: Soft preference used for sorting; does not bypass Min/Max constraints.\n"
		                  "Notes: Deterministic ordering also uses anchor keys and attempt index."))
	ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;

	/**
	 * If true, require a neutral item to be present at the mission anchor.
	 * Example: Enable this for scavenging missions that must sit on top of a resource node.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Allows missions to be bound to existing neutral items for thematic placement.\n"
		                  "Technical: Gates RequiredNeutralItemType checks against NeutralItemsByAnchorKey.\n"
		                  "Notes: Drives EditCondition; disabling ignores neutral item requirements."))
	bool bNeutralItemRequired = false;

	/**
	 * Specific neutral item type required when bNeutralItemRequired is true.
	 * Example: Set to RuinedCity to ensure urban missions only appear on city anchors.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neutral Item",
		meta = (EditCondition = "bNeutralItemRequired",
		        ToolTip = "Used in: MissionsPlaced when bNeutralItemRequired is true.\n"
		                  "Why: Forces mission placement on anchors that already host a specific neutral item.\n"
		                  "Technical: Hard filter against NeutralItemsByAnchorKey values.\n"
		                  "Notes: Ignored when bNeutralItemRequired is false; can heavily restrict candidates."))
	EMapNeutralObjectType RequiredNeutralItemType = EMapNeutralObjectType::None;

	/**
	 * Additional adjacency requirement for companion objects.
	 * Example: Configure this to demand a nearby NeutralItem before allowing mission placement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adjacency",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Enforces adjacency requirements beyond simple distance checks.\n"
		                  "Technical: Applied as a hard filter against nearby object types.\n"
		                  "Notes: Can gate placement entirely if adjacency requirements are unmet."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Maps this mission to a tier so it inherits the tier rules by default.\n"
		                  "Technical: Used to look up FMissionTierRules from RulesByTier.\n"
		                  "Notes: If Tier is NotSet and no override is provided, placement may fail."))
	EMissionTier Tier = EMissionTier::NotSet;

	/**
	 * If true, use OverrideRules instead of the tier rules for this mission.
	 * Example: Enable this to place a story-critical mission closer than its tier would allow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Allows a specific mission to deviate from its tier rules.\n"
		                  "Technical: When true, OverrideRules replace tier rules for this mission.\n"
		                  "Notes: Drives EditCondition for OverrideRules; disabling hides the override field."))
	bool bOverrideTierRules = false;

	/**
	 * Override rules when bOverrideTierRules is enabled.
	 * Example: Override MinHopsFromHQ to force a special mission to appear within two hops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission",
		meta = (EditCondition = "bOverrideTierRules",
		        ToolTip = "Used in: MissionsPlaced when bOverrideTierRules is true.\n"
		                  "Why: Defines the mission-specific rules that replace tier defaults.\n"
		                  "Technical: Applied as hard/soft constraints for this mission only.\n"
		                  "Notes: Ignored when bOverrideTierRules is false."))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Establishes baseline rules for each mission tier.\n"
		                  "Technical: Looked up by EMissionTier and applied unless overridden per mission.\n"
		                  "Notes: Missing tiers can cause placements to fail if referenced by a mission."))
	TMap<EMissionTier, FMissionTierRules> RulesByTier;

	/**
	 * Per-mission overrides layered on top of the tier rules.
	 * Example: Override a single mission to require a nearby neutral item even if the tier does not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Missions",
		meta = (ToolTip = "Used in: MissionsPlaced.\n"
		                  "Why: Allows specific missions to override their tier's baseline rules.\n"
		                  "Technical: Looked up by EMapMission; overrides apply before placement filtering.\n"
		                  "Notes: Only missions present in this map receive overrides; others use tier rules."))
	TMap<EMapMission, FPerMissionRules> RulesByMission;
};
