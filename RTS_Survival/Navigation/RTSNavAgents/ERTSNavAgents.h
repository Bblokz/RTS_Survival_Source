// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/NoExportTypes.h"
#include "ERTSNavAgents.generated.h"


/**
 * @brief Difference between FNavDataConfig and ANavigationData
 *
 * @section Overview
 * FNavDataConfig and ANavigationData play distinct roles in Unreal’s navigation system:
 * - **FNavDataConfig** is a configuration descriptor (a struct) defining how navigation
 *   should be generated or used for a particular agent type (radius, height, properties, etc.).
 * - **ANavigationData** is a runtime navigation actor (or UObject) that holds the actual
 *   navigation geometry / connectivity (navmesh, graph, etc.) used for pathfinding queries.
 *
 * @section Roles
 * - FNavDataConfig describes *what* navigation settings are required for an agent type.
 *   It does not contain actual navigation data, only parameters and constraints.
 * - ANavigationData implements *how* the navigation system provides real navigable data
 *   in the world (mesh, graph, tiles, connectivity), and supports queries (pathfinding,
 *   projection, raycasts, etc.).
 *
 * @section Usage
 * - The navigation / pathfinding subsystem uses a set of FNavDataConfig entries to
 *   know which kinds of navigation data should be built or available for the various
 *   agents in the world.
 * - At runtime, for a given agent (or its properties), the system picks the appropriate
 *   ANavigationData instance (subclass) corresponding to a config, and runs pathfinding
 *   / spatial queries on it.
 * - FNavDataConfig is static metadata / settings. ANavigationData is dynamic, mutable,
 *   and live in the world (and may be rebuilt or updated as world geometry changes).
 *
 * @section Analogy
 * Think of FNavDataConfig as a *blueprint / spec* (describing the constraints and intended
 * behavior), whereas ANavigationData is the *constructed object / runtime data* (the actual
 * navigation mesh, graph, or spatial structure you operate on).
 */
UENUM(BlueprintType)
enum class ERTSNavAgents : uint8
{
	NONE       UMETA(DisplayName = "NONE"),
	Character  UMETA(DisplayName = "Character"),
	LightTank  UMETA(DisplayName = "LightTank"),
	MediumTank UMETA(DisplayName = "MediumTank"),
	HeavyTank UMETA(DisplayName = "HeavyTank"),
};

/**
 * @brief Get the "exact" enum name string (e.g. "Character", "LightTank", "NONE")
 * Uses UE's StaticEnum helper. On failure, reports and returns "NONE".
 */
static inline FString Global_GetRTSNavAgentName(const ERTSNavAgents Value)
{
	const UEnum* Enum = StaticEnum<ERTSNavAgents>();
	if (!Enum)
	{
		RTSFunctionLibrary::ReportError(TEXT("StaticEnum<RTSNavAgents>() returned null."));
		return TEXT("NONE");
	}
	// Returns the bare enumerator name (without scope): e.g. "Character"
	const FString Name = Enum->GetNameStringByValue(static_cast<int64>(Value));
	if (Name.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("GetNameStringByValue failed for RTSNavAgents value."));
		return TEXT("NONE");
	}
	return Name;
}
