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

	// todo remove.
	inline constexpr float DEV_AI_ThinkTimers_Scaling = 0.33;
	namespace ThinkingTimers
	{
		// How often the strategic Ai requests for finding its base locations. 
		inline float UpdateAIBaseLocations_Interval = 35.f * DEV_AI_ThinkTimers_Scaling;
		// How often the AI requests the counts of player light tanks, squads, nomadic buildings, resource building locations and hq location.
		inline float UpdatePlayerCountsBaseLocations_Interval = 25.f* DEV_AI_ThinkTimers_Scaling;
		// How often the AI checks whether certain locations are at risk of player attacks.
		inline float UpdateLocationsUnderPlayerAttack_Interval = 15.f* DEV_AI_ThinkTimers_Scaling;
		// How often the AI finds player squad and tank force concentrations.
		inline float UpdatePlayerUnitBulkLocations_Interval = 15.f* DEV_AI_ThinkTimers_Scaling;
		// How often the AI finds player Heavy tanks to flank
		inline float UpdatePlayerHeavyTankFlank_Interval = 3.f* DEV_AI_ThinkTimers_Scaling;
		// How often the AI checks for what tech level it is at given the buildings it has created / are on the map.
		inline float UpdateAITechLevel_Interval = 42.f* DEV_AI_ThinkTimers_Scaling;
		// How often the enemy AI updates training pressure from strategic sub-actions.
		inline float UpdateEnemyTrainingPressure_Interval = 12.f * DEV_AI_ThinkTimers_Scaling;
		// How often the enemy AI gets new training points depending on the setting in the EnemyController
		// Ensure this is a multiple of 60 as the training points are calculated per minute.
		inline float UpdateEnemyTrainingPoints_Interval = 60.f* DEV_AI_ThinkTimers_Scaling;
	}

	namespace TrainingPressure
	{
		inline constexpr float MinimumTrainingBucketPressure = 0.01f;
		inline constexpr float PickedBucketRemainingPressureMultiplier = 0.35f;
		inline constexpr float RecentPickPenaltyMultiplier = 0.75f;
		inline constexpr float TrainingPressureAgeRampSeconds = 300.f;
		inline constexpr float TrainingPressureMaxAgeBonusMultiplier = 2.f;
		inline constexpr int32 RecentTrainingSelectionHistorySize = 5;
		inline constexpr int32 MaxRecentDebugEntries = 12;
	}

	namespace Debugging
	{
		const FColor EnemyLocationColor = FColor::Purple;
		const FColor PlayerLocationColor = FColor::Blue;
		const FColor AttackLocationColor = FColor::Red;
		const FColor PickedActionLocationColor = FColor::Green;
		const FColor FlankLocationColor = FColor::Orange;
		inline constexpr float PickedActionLocationRadius = 300.f;
		inline constexpr bool BaseLocationDebugging = true;
		inline constexpr float BaseLocationDebugDuration = 15.f* DEV_AI_ThinkTimers_Scaling;
		inline constexpr float BaseLocationDebuggingRadius = 5000.f;
		inline constexpr bool PlayerCountsDebugging = true;
		inline constexpr float PlayerCountsDuration = 14.f* DEV_AI_ThinkTimers_Scaling;
		inline constexpr bool LocationsUnderPlayerAttackDebugging = true;
		inline constexpr float LocationsUnderAttackDuration = 14.f* DEV_AI_ThinkTimers_Scaling;
		inline constexpr float LocationUnderAttackDebuggingRadius = 500.f;
		inline constexpr bool FlankPositionsDebugging = true;
		inline constexpr bool PlayerUnitBulkDebugging= true;
		inline constexpr float PlayerUnitBulkLocationDebuggingRadius = 250.f;
		inline constexpr float PlayerUnitBulkLocationDebugDuration = 14.f* DEV_AI_ThinkTimers_Scaling;
		inline constexpr bool StochasticDecisionTreeDebugging = true;
		inline constexpr bool StochasticPathFindingDebugging = true;
		inline constexpr bool TrainingPressureDebugging = true;
		inline constexpr float TrainingPressureDebugDuration =15.f;
		// Should be lower than thinking speed
		inline constexpr float StochasticActionsDebugTime = 5.f;
		inline constexpr float StochasticActionsAttackLocationDebugRadius = 200.f;
		inline constexpr float RegisteringByICommandsOffset = 300.f;
		inline constexpr bool  RegisteringByIcommandsDebugging= true;
	}

	inline float EnemyRetreatControllerCheckInterval = 8.4f;
}
