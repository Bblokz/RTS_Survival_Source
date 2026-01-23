#pragma once

#include "CoreMinimal.h"

namespace EnemyAISettings
{
	// How long between checks for enemy formations; controls logic that will periodically try to unstuck enemy units
	// that are idle while in an active formation.
	// inline constexpr float EnemyFormationCheckInterval = 11.25;
	inline constexpr float EnemyFormationCheckInterval = 3.25;

	// How fast the enemy AI requests new strategic decisions from the async thread that processes them.
	// Makes use of one large request structure and batch processes multiple units at once which are then written back in
	// a large structure.
	inline float EnemyStrategicAIThinkingSpeed = 5.f;

	inline float EnemyRetreatControllerCheckInterval = 8.4f;
}
