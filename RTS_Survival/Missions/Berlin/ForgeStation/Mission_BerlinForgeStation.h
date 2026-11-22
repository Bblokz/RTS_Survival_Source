// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"

#include "Mission_BerlinForgeStation.generated.h"

class ABerlinForgeStation;
class AEnemyStrongpoint;
class UBerlinMissionForcesData;
class USoundBase;

/** The advisor lines of the bonus; empty slots are skipped (the owner writes and records them). */
USTRUCT(BlueprintType)
struct FBerlinForgeStationLines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lines")
	ERTSPortraitTypes Portrait = ERTSPortraitTypes::GermanAnnouncer;

	// Berlin_ForgeStationSighted: plays when the plate appears.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines")
	TObjectPtr<USoundBase> SightedLine = nullptr;

	// Berlin_ForgeStationIntel: plays when the last target died.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines")
	TObjectPtr<USoundBase> IntelLine = nullptr;

	// Berlin_ReinforcementsInbound: plays when the drop starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines")
	TObjectPtr<USoundBase> ReinforcementsLine = nullptr;

	// Seconds from the last target's death to the intel line and the archive entry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines", meta = (ClampMin = "0.0"))
	float IntelDelaySeconds = 1.f;

	// Seconds from the intel line to the reinforcement line and the drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lines", meta = (ClampMin = "0.0"))
	float ReinforcementsDelaySeconds = 8.f;
};

/**
 * @brief BONUS objective of Ger_Berlin: destroy the Forge survey station (BerlinForgeStation.md). Sets up the station
 * garrison from the Berlin forces data asset of the running difficulty, shows its plate when a player unit comes near
 * (UForgeStationSightedTrigger), counts the three targets of ABerlinForgeStation and, when they are gone, plays the
 * crater intel, adds the archive entry and flies in HQ's reinforcements at the station. No effect on the Zoo waves.
 * @note Add it to the map's MissionManager Missions; place the station, its three targets and the strongpoint.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMission_BerlinForgeStation : public UMissionBase
{
	GENERATED_BODY()

public:
	UMission_BerlinForgeStation();

	virtual void TickMission(float DeltaTime) override;
	virtual void OnMissionComplete() override;

	bool GetHasSetUpGarrison() const { return bM_HasSetUpGarrison; }
	bool GetHasLaunchedReinforcements() const { return bM_HasLaunchedReinforcements; }
	bool GetHasAddedArchiveEntry() const { return bM_HasAddedArchiveEntry; }
	// The trigger fired and the plate is up.
	bool GetIsPlateShown() const { return MissionState.bIsMissionStarted && MissionState.bIsTriggered; }
	bool GetIsBonusComplete() const { return MissionState.bIsMissionComplete; }

protected:
	virtual void OnMissionStart() override;
	virtual void OnMissionStartedNative() override;

	// One asset per difficulty; Ironman falls back to Brutal, missing ones to Normal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	TMap<ERTSGameDifficulty, TSoftObjectPtr<UBerlinMissionForcesData>> ForcesPerDifficulty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FName StrongpointId = TEXT("ForgeStation");

	// Where HQ's reinforcements come down: the open lane square on the player's side of the station.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FVector ReinforcementLandingPoint = FVector(1600.f, 3550.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FBerlinForgeStationLines Lines;

	// The crater intel; a Lore entry with its own text (CustomContent).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FArchiveCustomContent CraterIntel;

private:
	UPROPERTY()
	TWeakObjectPtr<ABerlinForgeStation> M_Station = nullptr;

	UPROPERTY()
	TObjectPtr<UBerlinMissionForcesData> M_Forces = nullptr;

	FTimerHandle M_IntelTimerHandle;
	FTimerHandle M_ReinforcementsTimerHandle;
	// Seconds the targets have all been gone; the plate gets a moment on screen before it completes.
	float M_SecondsAllTargetsDead = 0.f;
	bool bM_HasSetUpGarrison = false;
	bool bM_HasStartedTracking = false;
	bool bM_HasLaunchedReinforcements = false;
	bool bM_HasAddedArchiveEntry = false;

	bool GetIsValidStation() const;
	bool TrySetUpGarrison();
	UBerlinMissionForcesData* LoadForcesForDifficulty(const ERTSGameDifficulty Difficulty) const;
	void StartTargetTracking();
	void PlayLine(USoundBase* Line) const;
	void ScheduleReward();
	void GiveIntel();
	void LaunchReinforcements();
	FVector GetGroundedLandingPoint() const;
};
