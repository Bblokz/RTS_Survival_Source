// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

// Only the RTS_SurvivalShippingGlobalAbilityTests target defines this (see its Target.cs). With it on, the player
// gets every global ability that has a template, on every map, and no requirement blocks one: commander kit, mission
// tier cap, mission panel hiding, technology, required unit, costs and cooldown. Every other target keeps all gates.
#ifndef RTS_WITH_ALL_GLOBAL_ABILITIES
#define RTS_WITH_ALL_GLOBAL_ABILITIES 0
#endif

#if RTS_WITH_ALL_GLOBAL_ABILITIES

enum class EGlobalAbility : uint8;

namespace RTS::AllGlobalAbilities
{
	/** @return Every EGlobalAbility (GA_None excluded) that has a valid template in the global ability data asset. */
	TArray<EGlobalAbility> GetAllTemplatedAbilities();

	// The global ability panel has 8 slots; the abilities are shown a page at a time (RTS.GlobalAbilities.Page).
	constexpr int32 PanelPageSize = 8;
}

#endif
