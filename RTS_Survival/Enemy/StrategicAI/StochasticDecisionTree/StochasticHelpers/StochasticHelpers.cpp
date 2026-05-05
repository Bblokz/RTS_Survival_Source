#include "StochasticHelpers.h"

#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"

namespace
{
	/**
	 * @brief Generates a random value in range [0, MaxValue].
	 *
	 * If deterministic mode is enabled, combines Seed and Time into a reproducible random stream.
	 */
	float GetRandomValue(
		const bool bUseSeed,
		const float Seed,
		const float Time,
		const float MaxValue)
	{
		if (!bUseSeed)
		{
			return FMath::FRandRange(0.0f, MaxValue);
		}

		const int32 CombinedSeed =
			static_cast<int32>(Seed * 1000.0f) ^
			static_cast<int32>(Time * 1000.0f);

		FRandomStream RandomStream(CombinedSeed);

		return RandomStream.FRandRange(0.0f, MaxValue);
	}

	/**
	 * @brief Generic weighted picker for pointer arrays.
	 *
	 * @tparam T Type of object.
	 * @param Options Array of const pointers.
	 * @param GetScore Lambda returning score for an option.
	 * @param bUseSeed Deterministic toggle.
	 * @param Seed Seed value.
	 * @param Time Time value.
	 * @return Selected pointer or nullptr.
	 */
	template<typename T>
	const T* PickWeightedInternal(
		const TArray<const T*>& Options,
		const TFunctionRef<float(const T&)> GetScore,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		float TotalScore = 0.0f;

		for (const T* Option : Options)
		{
			if (!Option)
			{
				continue;
			}

			const float Score = FMath::Max(0.0f, GetScore(*Option));
			TotalScore += Score;
		}

		if (TotalScore <= 0.0f)
		{
			return nullptr;
		}

		const float RandomValue = GetRandomValue(bUseSeed, Seed, Time, TotalScore);

		float RunningScore = 0.0f;

		for (const T* Option : Options)
		{
			if (!Option)
			{
				continue;
			}

			const float Score = FMath::Max(0.0f, GetScore(*Option));
			RunningScore += Score;

			if (RandomValue <= RunningScore)
			{
				return Option;
			}
		}

		return nullptr;
	}
}

namespace StochasticHelpers
{
	const FStrategicAIAction* PickStrategicAction(
		const TArray<const FStrategicAIAction*>& ActionDefinitions,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		if (ActionDefinitions.IsEmpty())
		{
			return nullptr;
		}

		return PickWeightedInternal<FStrategicAIAction>(
			ActionDefinitions,
			[](const FStrategicAIAction& Action)
			{
				return Action.GetScore();
			},
			bUseSeed,
			Seed,
			Time);
	}

	const UStrategicAISubAction* PickSubAction(
		const TArray<const UStrategicAISubAction*>& SubActions,
		const bool bUseSeed,
		const float Seed,
		const float Time)
	{
		if (SubActions.IsEmpty())
		{
			return nullptr;
		}

		return PickWeightedInternal<UStrategicAISubAction>(
			SubActions,
			[](const UStrategicAISubAction& SubAction)
			{
				return SubAction.GetScore();
			},
			bUseSeed,
			Seed,
			Time);
	}
}
