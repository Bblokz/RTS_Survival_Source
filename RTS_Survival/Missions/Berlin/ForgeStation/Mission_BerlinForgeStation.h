// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/BerlinForgeStation.h"
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
};

/** Intercepted radio of the station's hazmat crew (Russian, English subtitles); each plays once, empty slots are skipped. */
USTRUCT(BlueprintType)
struct FBerlinForgeStationCrewLines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Crew Lines")
	ERTSPortraitTypes Portrait = ERTSPortraitTypes::SovietHazmat;

	// Berlin_ForgeCrew_Sighted: calm work chatter when the plate appears, before the advisor's sighted line.
	// Skipped when the garrison is already fighting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew Lines")
	TObjectPtr<USoundBase> SightedLine = nullptr;

	// Berlin_ForgeCrew_Engaged: the first guard or crew comes into combat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew Lines")
	TObjectPtr<USoundBase> EngagedLine = nullptr;

	// Berlin_ForgeCrew_TargetLost: the first counted target (rig, cryo block or command bunker) is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew Lines")
	TObjectPtr<USoundBase> FirstTargetLostLine = nullptr;
};

/**
 * @brief C++ parent of the BONUS stage Mission_Berlin_ForgeStation (BerlinForgeStation.md, BerlinMissionScripting.md
 * section 5). Natively it sets up the station garrison before the trigger (from the Berlin forces data asset of the
 * running difficulty), plays the sighted line and tracks the three targets once the plate is up, and fires
 * BP_OnAllStationTargetsDestroyed once when they are gone. The Blueprint's event graph runs the reward: the waits
 * (ScheduleSingleCallback), GiveIntel, LaunchReinforcements and OnMissionComplete. No effect on the Zoo waves.
 * @note Use the Blueprint child in the manager's Missions; place the station, its three targets and the strongpoint.
 * @note BP_OnAllStationTargetsDestroyed: implement the reward sequence and end it with OnMissionComplete.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMission_BerlinForgeStation : public UMissionBase
{
	GENERATED_BODY()

public:
	UMission_BerlinForgeStation();

	virtual void TickMission(float DeltaTime) override;

	/** @brief The crater intel: archive entry (CraterIntel) and the intel line. A synchronous step. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Forge Station")
	void GiveIntel();

	/** @brief The reinforcements line and HQ's drop at the station (payload from the forces data asset). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Forge Station")
	void LaunchReinforcements();

	bool GetHasFiredAllTargetsDestroyed() const { return bM_HasFiredAllTargetsDestroyed; }

	bool GetHasSetUpGarrison() const { return bM_HasSetUpGarrison; }
	bool GetHasLaunchedReinforcements() const { return bM_HasLaunchedReinforcements; }
	bool GetHasAddedArchiveEntry() const { return bM_HasAddedArchiveEntry; }
	// The trigger fired and the plate is up.
	bool GetIsPlateShown() const { return MissionState.bIsMissionStarted && MissionState.bIsTriggered; }
	bool GetIsBonusComplete() const { return MissionState.bIsMissionComplete; }

protected:
	virtual void OnMissionStart() override;
	virtual void OnMissionStartedNative() override;
	// Tracking does not complete this stage (bCompleteMissionWhenTrackingDone is off); the full count starts the reward.
	virtual void OnTrackingReachedMaxCountNative() override;

	// All three station targets are gone (fired once, after the plate is up). Run the reward from here.
	UFUNCTION(BlueprintImplementableEvent, Category = "Forge Station")
	void BP_OnAllStationTargetsDestroyed();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FBerlinForgeStationCrewLines CrewLines;

	// The crater intel; a Lore entry with its own text (CustomContent).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FArchiveCustomContent CraterIntel;

private:
	UPROPERTY()
	TWeakObjectPtr<ABerlinForgeStation> M_Station = nullptr;

	UPROPERTY()
	TObjectPtr<UBerlinMissionForcesData> M_Forces = nullptr;

	bool bM_HasSetUpGarrison = false;
	bool bM_HasFiredAllTargetsDestroyed = false;
	bool bM_HasStartedTracking = false;
	bool bM_HasLaunchedReinforcements = false;
	bool bM_HasAddedArchiveEntry = false;
	bool bM_HasPlayedCrewEngagedLine = false;
	bool bM_HasPlayedCrewTargetLostLine = false;

	UFUNCTION()
	void OnGarrisonEngaged(AEnemyStrongpoint* Strongpoint);

	UFUNCTION()
	void OnStationTargetDestroyed(ABerlinForgeStation* Station, EForgeStationPartRole Role);

	void PlayCrewLine(USoundBase* Line) const;

	bool GetIsValidStation() const;
	bool TrySetUpGarrison();
	UBerlinMissionForcesData* LoadForcesForDifficulty(const ERTSGameDifficulty Difficulty) const;
	void StartTargetTracking();
	void PlayLine(USoundBase* Line) const;
	bool GetAreAllStationTargetsDestroyed() const;
	void FireAllStationTargetsDestroyed();
	FVector GetGroundedLandingPoint() const;
};
