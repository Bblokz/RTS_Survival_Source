#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/AdjacencyPolicy/Enum_AdjacencyPolicy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"

#include "MapObjectAdjacencyRequirement.generated.h"

/**
 * @brief Adjacency constraint used during MissionsPlaced (after NeutralObjectsPlaced and
 *        before Finished) to ensure a mission anchor has nearby companion objects.
 */
USTRUCT(BlueprintType)
struct FMapObjectAdjacencyRequirement
{
	GENERATED_BODY()

	/**
	 * Enables or disables the adjacency requirement for this rule.
	 * Example: Disable this when a mission should stand alone without nearby companion objects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	bool bEnabled = false;

	/**
	 * The item type that must be present within the hop radius.
	 * Example: Require a NeutralItem type to make sure a mission spawns next to a resource node.
	 */
	// What must be nearby (one connection away):
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	EMapItemType RequiredItemType = EMapItemType::NeutralItem;

	/**
	 * Specific subtype (encoded as a byte) for the required item type.
	 * Example: Set this to a specific neutral object subtype so a mission only appears near a
	 * RuinedCity when crafting an urban-themed mid-campaign objective.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	uint8 RawByteSpecificItemSubtype = 0; // use static cast to set EMapNeutralObjectType, EMapMission, etc.

	/**
	 * Minimum number of matching nearby items required.
	 * Example: Raise this when you want a mission to appear only in areas with multiple supports.
	 */
	// How many of these nearby?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	int32 MinMatchingCount = 1;

	/**
	 * Maximum hop distance used to define “nearby.”
	 * Example: Increase this to allow a mission to count a companion object two links away.
	 */
	// Define “nearby”: the max connections away
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adjacency", meta = (EditCondition = "bEnabled"))
	int32 MaxHops = 1;

	/**
	 * Policy for what happens if the required adjacency is not met.
	 * Example: Use TryAutoPlaceCompanion to allow the generator to spawn a companion neutral
	 * object so the mission can still be placed.
	 */
	// What to do if missing:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	EAdjacencyPolicy Policy = EAdjacencyPolicy::RejectIfMissing;
};
