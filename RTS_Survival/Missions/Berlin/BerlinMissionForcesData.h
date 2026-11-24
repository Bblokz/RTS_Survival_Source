// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerAirDropSubsystem.h"

#include "BerlinMissionForcesData.generated.h"

/** The Forge survey station part of the Berlin forces (Source/Docs/Campaign/BerlinForgeStation.md, sections 4 and 7). */
USTRUCT(BlueprintType)
struct FBerlinForgeStationForces
{
	GENERATED_BODY()

	// Units per named post of the station strongpoint (Arch, RimCrewA, Console, RigFoot, RimCrewB, CrewHut,
	// CrewEscort); posts left out stay unmanned on this difficulty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	TArray<FEnemyStrongpointPostUnits> Garrison;

	// HQ flies these in when the station is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FPlayerAirDropPayload Reinforcements;
};

/**
 * @brief The tunable enemy side of the Berlin mission, one asset per difficulty (BerlinMissionFlow.md 4.5). Missions
 * pick the asset of the running difficulty; for now it holds the Forge survey station, later patrols, fleeing and
 * the Reichstag factory.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UBerlinMissionForcesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station")
	FBerlinForgeStationForces ForgeStation;
};
