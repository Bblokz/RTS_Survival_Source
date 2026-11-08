// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * @brief Render-cost helpers from the performance audit (Docs/Performance/OptimizationLog.md).
 */
namespace RTSRenderPerf
{
	/**
	 * @brief Takes the small static meshes an infantry actor carries (weapons, equipment) out of the global distance
	 * field and the Lumen scene, and makes them Movable.
	 * A non-movable mesh attached to an animated hand is re-added to the cached global distance field on every move,
	 * which rebuilt the static distance field pages each frame (~0.9 ms GPU on Ger_FirstMission). Objects this small add
	 * nothing visible to distance field shadows/AO or Lumen bounce light; they still render, cast shadows and receive GI.
	 * @param Actor The weapon or squad unit whose static mesh components are changed.
	 */
	RTS_SURVIVAL_API void ExcludeSmallAttachmentsFromDistanceFields(const AActor* Actor);
}
