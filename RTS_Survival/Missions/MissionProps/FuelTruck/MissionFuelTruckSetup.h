// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionProps/PropSinkDeath/MissionPropSinkDeath.h"

class AActor;
class UObject;
class UStaticMesh;

/**
 * @brief Shared dressing for the scripted ZIL fuel trucks (USecureBaseMission, UFuelDeliveryDepotComponent):
 * a hull copy with one padded collision box, so shells hit the open bed, and a sink death for the wreck, so no
 * scavengable wreck is left behind.
 */
struct RTS_SURVIVAL_API FMissionFuelTruckSetup
{
	/** @return SM_Hull_Zil_Hitbox: the ZIL hull mesh with one padded collision box. */
	static TSoftObjectPtr<UStaticMesh> GetDefaultZilHullHitboxMesh();

	/** @return Large smoke at 0.75 scale, an 8 s sink of 4 m, one scorch decal and two small ember decals. */
	static FMissionPropSinkDeathSettings MakeDefaultZilWreckDeath();

	/**
	 * @brief Swaps the truck's "Hull" mesh for the hit box copy and makes its wreck run the sink death.
	 * @param Truck The spawned truck; must be an ATankMaster.
	 * @param HullHitboxMesh Same render mesh and pivot as the hull, padded collision; null keeps the hull.
	 * @param WreckDeath Runs on every actor the truck's death spawns.
	 * @param InOutPreloadedAssets Keeps the smoke and decal assets loaded so the death does not hitch.
	 * @param ErrorContext Who asked, for error messages.
	 * @return False when the truck is no ATankMaster (it keeps its own hit box and death).
	 */
	static bool ApplyHitboxAndWreckDeath(AActor* Truck, const TSoftObjectPtr<UStaticMesh>& HullHitboxMesh,
	                                     const FMissionPropSinkDeathSettings& WreckDeath,
	                                     TArray<TObjectPtr<UObject>>& InOutPreloadedAssets,
	                                     const FString& ErrorContext);
};
