// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapPlayerItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "WorldCampaignSettings.generated.h"

class AWorldEnemyObject;
class AWorldMissionObject;
class AWorldNeutralObject;
class AWorldPlayerObject;

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

	static const UWorldCampaignSettings* Get();
};
