// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "SupplyPointDeveloperSettings.generated.h"

class ATankMaster;
class USoundBase;

/** The portrait and the lines of one announcer moment; an empty list keeps that moment silent. */
USTRUCT(BlueprintType)
struct FSupplyAnnouncementLines
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Supply Points")
	ERTSPortraitTypes PortraitType = ERTSPortraitTypes::GermanAnnouncer;

	// Sound waves (not cues): only waves get word-timed subtitles under the portrait.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Supply Points")
	TArray<TSoftObjectPtr<USoundBase>> VoiceLines;
};

/**
 * @brief Project settings of the supply point feature: which truck Blueprint (any tank master vehicle) each faction's base sends and which portrait
 * lines the announcer speaks. Read by USupplyPointSubsystem; edit under Project Settings > RTS Supply Points.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RTS Supply Points"))
class RTS_SURVIVAL_API USupplyPointDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Truck used when the faction has no entry of its own.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Supply Truck")
	TSoftClassPtr<ATankMaster> DefaultSupplyTruckClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Supply Truck")
	TMap<ERTSFaction, TSoftClassPtr<ATankMaster>> SupplyTruckClassPerFaction;

	// Truck the enemy sends once AI supply runs exist; unused for now.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Supply Truck")
	TSoftClassPtr<ATankMaster> EnemySupplyTruckClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Announcer")
	FSupplyAnnouncementLines SupplyPointCapturedLines;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Announcer")
	FSupplyAnnouncementLines SupplyTruckDispatchedLines;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Announcer")
	FSupplyAnnouncementLines SupplyTruckReturningLines;

	// Seconds the "truck dispatched" line waits after the "captured" line was requested, so the two do not bunch up.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0.0"))
	float DispatchDelayAfterCapture = 6.f;
};
