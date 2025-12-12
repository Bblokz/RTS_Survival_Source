// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "ECrushDestructionType.generated.h"

/**
 * @brief Selects which crush rule applies when a component overlaps another actor.
 * Use from Blueprint to configure bindings; logic runs in C++.
 */
UENUM(BlueprintType)
enum class ECrushDestructionType : uint8
{
	AnyTank UMETA(DisplayName = "AnyTank"),
	MediumOrHeavyTank UMETA(DisplayName = "MediumOrHeavyTank"),
	HeavyTank UMETA(DisplayName = "HeavyTank")
};

// Determines whether we use an impulse on (expected) geometry component or fall down vertically like trees do.
UENUM(BlueprintType)
enum class ECrushDeathType: uint8
{
	// Requires configure impulse on crushed with a geo component that has physics sim enabled.
	Impulse,
	// Requires a configure vertical collapse with usually the main static mesh component.
	VerticalCollapse,
	// Fires the OnUnitDies event with the scavenging death type; lets bp specific logic decide what to do.
	RegularDeath,
	
};
