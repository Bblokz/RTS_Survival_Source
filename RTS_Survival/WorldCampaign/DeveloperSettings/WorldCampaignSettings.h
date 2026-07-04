// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapPlayerItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "WorldCampaignSettings.generated.h"

class AWorldEnemyObject;
class AWorldDivisionBase;
class AWorldMissionObject;
class AWorldNeutralObject;
class AWorldPlayerObject;

/**
 * @brief Config entry used to spawn mobile divisions when a fresh world campaign is created.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDivisionInitialSpawnData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions")
	EWorldFieldDivisions DivisionType = EWorldFieldDivisions::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions")
	int32 OwningPlayer = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions")
	TArray<ETankSubtype> TankSubtypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions")
	TArray<ESquadSubtype> SquadSubtypes;
};

/**
 * @brief Config-driven map for campaign item types to spawned world object actors.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign"))
class RTS_SURVIVAL_API UWorldCampaignSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldCampaignSettings();

	/** Map of player item types to actor classes used when promoting anchors. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|Objects",
		meta=(ToolTip="Mapping used by the campaign generator to spawn player-side objects when an anchor is promoted."))
	TMap<EMapPlayerItem, TSubclassOf<AWorldPlayerObject>> PlayerObjectClassByType;

	/** Map of enemy item types to actor classes used when promoting anchors. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|Objects",
		meta=(ToolTip="Mapping used by the campaign generator to spawn enemy objects when an anchor is promoted."))
	TMap<EMapEnemyItem, TSubclassOf<AWorldEnemyObject>> EnemyObjectClassByType;

	/** Map of neutral item types to actor classes used when promoting anchors. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|Objects",
		meta=(ToolTip="Mapping used by the campaign generator to spawn neutral objects when an anchor is promoted."))
	TMap<EMapNeutralObjectType, TSubclassOf<AWorldNeutralObject>> NeutralObjectClassByType;

	/** Map of mission types to actor classes used when promoting anchors. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|Objects",
		meta=(ToolTip="Mapping used by the campaign generator to spawn mission objects when an anchor is promoted."))
	TMap<EMapMission, TSubclassOf<AWorldMissionObject>> MissionObjectClassByType;

	/** Map of field division types to actor classes used by the runtime division manager. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|World Divisions",
		meta=(ToolTip="Mapping used by the world division manager to spawn mobile division actors."))
	TMap<EWorldFieldDivisions, TSubclassOf<AWorldDivisionBase>> WorldDivisionClassByType;

	/** Divisions spawned once when a new campaign is generated before save data exists. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|World Divisions")
	TArray<FWorldDivisionInitialSpawnData> InitialWorldDivisions;

	/** Extra clearance used when division paths route around anchor points. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|World Divisions|Pathfinding",
		meta=(ClampMin="0"))
	float WorldDivisionAnchorAvoidanceRadius = 1200.f;

	/** Padding used when projected division path points are pulled back inside the world boundary. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|World Divisions|Pathfinding",
		meta=(ClampMin="0"))
	float WorldDivisionBoundaryProjectionPadding = 200.f;

	/** Maximum repair passes allowed when a division path leaves the boundary or intersects anchors. */
	UPROPERTY(EditAnywhere, Config, Category="World Campaign|World Divisions|Pathfinding",
		meta=(ClampMin="1"))
	int32 WorldDivisionPathRepairMaxAttempts = 8;

	static const UWorldCampaignSettings* Get();
};
