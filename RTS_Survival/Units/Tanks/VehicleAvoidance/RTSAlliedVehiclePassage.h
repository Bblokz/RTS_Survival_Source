// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** How a vehicle treats the vehicles of its own player that stand or drive in its way. */
enum class EAlliedVehiclePassage : uint8
{
	// Waits behind, steers around and bumps into allied hulls.
	Avoid,
	// Drives through allied vehicles and they through it; only enemy vehicles stop it. Harvesters.
	PassThrough,
	// Allied vehicles do not hold it up and the idle ones in its way are sent aside. A nomadic truck on its way
	// to deploy; allies still make room for it.
	PushAside
};
