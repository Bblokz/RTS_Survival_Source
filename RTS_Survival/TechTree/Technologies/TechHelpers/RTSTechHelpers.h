// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
enum class EArmorPlate : uint8;

namespace RTSTechHelpers
{
	/**
	 * @brief Shared armour tech path so component validation and error reporting stay consistent.
	 * @param ActorToAdjust Actor expected to own the armour calculation component.
	 * @param ArmorPlatesToAdjust Armour plates that receive the multiplier.
	 * @param ArmorValueMultiplier Positive multiplier applied to matching plates.
	 * @return Whether the multiplier was applied to a valid armour component.
	 */
	bool ApplyArmorValueMultiplierToMatchingPlates(
		AActor* ActorToAdjust,
		const TArray<EArmorPlate>& ArmorPlatesToAdjust,
		float ArmorValueMultiplier);

	/**
	 * @brief Shared health tech path so max-health upgrades use the same validation and reporting.
	 * @param ActorToAdjust Actor expected to own the health component.
	 * @param HealthMultiplier Positive multiplier applied to the current max health.
	 * @return Whether the multiplier was applied to a valid health component.
	 */
	bool ApplyMaxHealthMultiplier(AActor* ActorToAdjust, float HealthMultiplier);
}
