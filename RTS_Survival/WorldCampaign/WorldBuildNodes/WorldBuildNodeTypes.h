// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBuildNodeTypes.generated.h"

/**
 * @brief Where a build node is in its life. A node only moves forward: locked, unlocked, built.
 */
UENUM(BlueprintType)
enum class EWorldBuildNodeState : uint8
{
	/** The country is not free enough yet. */
	Locked,
	/** The player may build here. */
	Unlocked,
	/** A building stands here; it cannot be lost or replaced for now. */
	Built
};

/**
 * @brief Why a build request was accepted or refused, so the UI can explain a refusal.
 */
UENUM(BlueprintType)
enum class EWorldBuildNodeBuildResult : uint8
{
	Success,
	UnknownNode,
	InvalidBuilding,
	NodeLocked,
	NodeOccupied,
	NoPriceConfigured,
	CannotAfford
};
