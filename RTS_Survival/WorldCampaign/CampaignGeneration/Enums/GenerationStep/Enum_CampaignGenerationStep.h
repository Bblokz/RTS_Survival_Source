#pragma once

#include "CoreMinimal.h"

#include "Enum_CampaignGenerationStep.generated.h"

/**
 * @brief Ordered campaign generation steps, used to sequence placement so each step
 *        knows what came before and what will follow.
 */
UENUM(BlueprintType)
enum class ECampaignGenerationStep : uint8
{
	/** Initial state before any generation occurs; next step is ConnectionsCreated. */
	NotStarted,
	/** Connection graph created between anchors; next step is PlayerHQPlaced. */
	ConnectionsCreated,
	/** Player HQ placed on an anchor; next step is EnemyHQPlaced. */
	PlayerHQPlaced,
	/** Enemy HQ placed after the player HQ; next step is EnemyWallPlaced. */
	EnemyHQPlaced,
	/** Enemy wall placed after the enemy HQ; next step is EnemyObjectsPlaced. */
	EnemyWallPlaced,
	/** Enemy items placed across the graph; next step is NeutralObjectsPlaced. */
	EnemyObjectsPlaced,
	/** Neutral items placed after enemy items; next step is MissionsPlaced. */
	NeutralObjectsPlaced,
	/** Missions placed after neutral items; next step is Finished. */
	MissionsPlaced,
	/** Terminal state after all placement steps complete. */
	Finished
};
