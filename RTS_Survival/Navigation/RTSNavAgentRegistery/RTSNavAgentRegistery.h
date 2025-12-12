#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "RTSNavAgentRegistery.generated.h"

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
/**
 * Lightweight accessor around the per-world nav singleton.
 * Lets you:
 *  - List all Supported Agents (Project Settings → Navigation System → Agents)
 *  - Find a specific agent by RTSNavAgents enum
 *  - Resolve the corresponding ANavigationData (e.g., ARecastNavMesh)
 *  - Infer which agent a pawn's FNavAgentProperties most likely maps to
 */
UCLASS()
class RTS_SURVIVAL_API URTSNavAgentRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static URTSNavAgentRegistry* Get(const UObject* WorldContext)
	{
		if (!IsValid(WorldContext))
		{
			RTSFunctionLibrary::ReportError(TEXT("URTSNavAgentRegistry::Get called with invalid WorldContext."));
			return nullptr;
		}
		const UWorld* World = WorldContext->GetWorld();
		if (!World)
		{
			RTSFunctionLibrary::ReportError(TEXT("URTSNavAgentRegistry::Get: WorldContext has no World."));
			return nullptr;
		}
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<URTSNavAgentRegistry>();
		}
		RTSFunctionLibrary::ReportError(TEXT("URTSNavAgentRegistry::Get: No GameInstance."));
		return nullptr;
	}

	/** Project Settings → Navigation System → Agents array */
	const TArray<FNavDataConfig>& GetAllAgentConfigs(UWorld* World) const;

	/** Find config by enum (uses Global_GetRTSNavAgentName to match the FNavDataConfig::Name) */
	const FNavDataConfig* FindAgentConfig(UWorld* World, ERTSNavAgents Agent) const;

	/** Get nav data actor for a given agent config (optionally, resolved near a location) */
	const ANavigationData* GetNavDataForAgent(UWorld* World, const FNavDataConfig& AgentCfg, const FVector* NearLocation = nullptr, const FVector* Extent = nullptr) const;

	/** Try to guess agent name for given properties (match by radius/height/PreferredNavData) */
	FName GetAgentNameForProps(UWorld* World, const FNavAgentProperties& Props, float Tolerance = 0.5f) const;

private:
	static const TArray<FNavDataConfig>& GetEmptyAgents();
};
