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
	inline float EnemyStrategicAIThinkingSpeed = 6.f;

	namespace ThinkingTimers
	{
		// How often the strategic Ai requests for finding its base locations. 
		inline float UpdateAIBaseLocations_Interval = 35.f;
		// How often the AI requests the counts of player light tanks, squads, nomadic buildings, resource building locations and hq location.
		inline float UpdatePlayerCountsBaseLocations_Interval = 25.f;
		// How often the AI checks whether certain locations are at risk of player attacks.
		inline float UpdateLocationsUnderPlayerAttack_Interval = 15.f;
	}

	namespace Debugging
	{
		inline constexpr bool BaseLocationDebugging = true;
		inline constexpr float BaseLocationDebugDuration = 15.f;
		inline constexpr float BaseLocationDebuggingRadius = 5000.f;
		inline constexpr bool PlayerCountsDebugging = true;
		inline constexpr float PlayerCountsDuration = 15.f;
		inline constexpr bool StochasticDecisionTreeDebugging = true;
		inline constexpr float StochasticActionsDebugTime = 5.f;
		
	}

	inline float EnemyRetreatControllerCheckInterval = 8.4f;
}
