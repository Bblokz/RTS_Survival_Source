// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MissionTimelineExportCommandlet.generated.h"

/**
 * @brief Writes one JSON model of a mission map for the timeline simulator (Source/Tools/MissionTimeline), so mission
 * pacing can be estimated without a play test. Run headless:
 * UnrealEditor-Cmd RTS_Survival.uproject -run=MissionTimelineExport -Map=/Game/.../Ger_X -Out=D:/.../model.json
 * [-PropClasses=EnemyConvoyRoute,EnemyArtilleryBattery,...]
 * It only reads: the mission manager's stages (with their triggers), every stage Blueprint's graphs (each node with
 * its reflected properties, pins, pin defaults and links, parent Blueprints and macro libraries included), the placed
 * actors (properties for the PropClasses), and the data assets and sounds those reference (sound durations included).
 * It interprets nothing; the meaning of nodes lives in the simulator.
 */
UCLASS()
class RTS_SURVIVAL_API UMissionTimelineExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UMissionTimelineExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
