// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/CameraTour/MissionCameraTourTypes.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionProps/PropSinkDeath/MissionPropSinkDeath.h"
#include "RTS_Survival/Enemy/AirDrop/EnemyAirDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/MissionPlayerGlobalAbilitySettings.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "SecureBaseMission.generated.h"

class AMissionFuelCrane;
class UGlobalAbility;
class USoundBase;
class UStaticMesh;

/** An enemy squad that marches up and down between two points to make the base look manned. */
USTRUCT(BlueprintType)
struct FSecureBaseMarchingSquad
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	TSoftClassPtr<AActor> SquadClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	float SpawnYaw = 0.f;

	// The far end of the march; the squad marches back and forth between its spawn point and this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	FVector MarchToLocation = FVector::ZeroVector;
};

/** A supply truck that drives up to the base and a crane that unloads it. */
USTRUCT(BlueprintType)
struct FSecureBaseFuelDelivery
{
	GENERATED_BODY()

	FSecureBaseFuelDelivery();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	TSoftClassPtr<AActor> TruckClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	FVector TruckSpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	float TruckSpawnYaw = 0.f;

	// Seconds after the camera tour starts before the truck spawns, so it arrives during the tour's crane shot;
	// 0 spawns it with the rest of the base life at the mission start.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float TruckSpawnDelaySeconds = 0.f;

	// Where the truck parks for the crane; it drives there with a move order.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	FVector ParkLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	float ParkYaw = 0.f;

	// The truck counts as parked within this distance (2D) of ParkLocation once it stopped ordering moves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck",
		meta = (EditCondition = "bEnabled", ClampMin = "50.0"))
	float ParkedRadius = 450.f;

	// Copy of the truck's hull mesh with one padded collision box (the original collides per triangle, so shells
	// pass through the open bed). Swapped onto the spawned truck's "Hull" component; it stays its armour mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	TSoftObjectPtr<UStaticMesh> TruckHullHitboxMesh;

	// What happens to the truck's wreck: smoke, sink, scorch + ember decals. Runs on every actor its death spawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Truck", meta = (EditCondition = "bEnabled"))
	FMissionPropSinkDeathSettings TruckWreckDeath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Crane", meta = (EditCondition = "bEnabled"))
	TSubclassOf<AMissionFuelCrane> CraneClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Crane", meta = (EditCondition = "bEnabled"))
	FVector CraneLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuel Delivery|Crane", meta = (EditCondition = "bEnabled"))
	float CraneYaw = 0.f;
};

/** One enemy air drop the base calls in when the player assaults it. */
USTRUCT(BlueprintType)
struct FSecureBaseResponseAirDrop
{
	GENERATED_BODY()

	// World location the units land on (the drop's aircraft fly in from the enemy's aircraft edge).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	FVector LandingPoint = FVector::ZeroVector;

	// Seconds after the assault response starts (the enemy alarm line) before the drop's aircraft take off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop", meta = (ClampMin = "0.0"))
	float DelaySeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	FEnemyAirDropUnits Units;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Drop")
	FEnemyAirDropDeliverySettings Delivery;
};

/**
 * The base answers an assault: when a player unit enters the trigger box (armed once the camera tour has finished),
 * an enemy portrait line plays, enemy air drops reinforce the base and the player's HQ sends an air strike with its
 * own advisor line.
 */
USTRUCT(BlueprintType)
struct FSecureBaseAssaultResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response")
	bool bEnabled = false;

	// Centre and half size of the box a player unit has to enter (Z half size covers slopes).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Trigger",
		meta = (EditCondition = "bEnabled"))
	FVector TriggerCentre = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Trigger",
		meta = (EditCondition = "bEnabled"))
	FVector TriggerHalfExtent = FVector(1000.f, 1000.f, 1000.f);

	// Played on EnemyAlarmPortrait when the trigger fires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Enemy",
		meta = (EditCondition = "bEnabled"))
	TObjectPtr<USoundBase> EnemyAlarmLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Enemy",
		meta = (EditCondition = "bEnabled"))
	ERTSPortraitTypes EnemyAlarmPortrait = ERTSPortraitTypes::SovietMosin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Enemy",
		meta = (EditCondition = "bEnabled"))
	TArray<FSecureBaseResponseAirDrop> AirDrops;

	// Seconds after the trigger before AirSupportLine plays on AirSupportPortrait (queued behind the alarm line).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float AirSupportLineDelaySeconds = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled"))
	TObjectPtr<USoundBase> AirSupportLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled"))
	ERTSPortraitTypes AirSupportPortrait = ERTSPortraitTypes::GermanAnnouncer;

	// Player strafing ability (copied from the global ability data asset) flown at AirSupportTarget; GA_None = no
	// air support.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled"))
	EGlobalAbility AirSupportAbility = EGlobalAbility::GA_Ju87Strafing;

	// Seconds after the trigger before the strike aircraft take off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float AirSupportStrikeDelaySeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled", ClampMin = "1"))
	int32 AirSupportAircraftCount = 2;

	// How long each aircraft keeps attacking the target area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled", ClampMin = "1.0"))
	float AirSupportStrafeSeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled"))
	FVector AirSupportTarget = FVector::ZeroVector;

	// On: every attack run goes over AirSupportTarget. Off: one run, then the aircraft hunt nearby enemies anywhere.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Air Support",
		meta = (EditCondition = "bEnabled"))
	bool bAirSupportStaysOnTarget = true;

	// Squad whose info takeover opens once the strike is in (the enemy unit the player should learn about first);
	// Squad_None shows none. Waits until a living squad of this type stands on the map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Squad Intel",
		meta = (EditCondition = "bEnabled"))
	ESquadSubtype IntelSquadSubtype = ESquadSubtype::Squad_None;

	// Row of DT_SquadInfoTakeovers shown for that squad.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Squad Intel",
		meta = (EditCondition = "bEnabled"))
	FName IntelSquadLibraryEntry = NAME_None;

	// Seconds after the strike aircraft took off before the squad takeover opens.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Squad Intel",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float IntelSquadDelaySeconds = 8.f;

	// Ability the player keeps after the strike (charges, cooldown, no cost); GA_None grants nothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Granted Ability",
		meta = (EditCondition = "bEnabled"))
	FMissionGrantedGlobalAbility GrantedAbility;

	// Seconds after the strike aircraft took off before the ability is granted and GrantedAbilityLine plays.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Granted Ability",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float GrantedAbilityDelaySeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Granted Ability",
		meta = (EditCondition = "bEnabled"))
	TObjectPtr<USoundBase> GrantedAbilityLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assault Response|Granted Ability",
		meta = (EditCondition = "bEnabled"))
	ERTSPortraitTypes GrantedAbilityPortrait = ERTSPortraitTypes::GermanAnnouncer;
};

/**
 * @brief "Secure the line": the player has to destroy a set of enemy base buildings.
 * When the mission starts it lifts the fog over the base for a while, puts a destroy marker (and a minimap icon) on
 * every target building, brings the base to life (marching squads, a fuel truck driving up to a crane that unloads
 * it) and plays a short camera tour over the base. After the tour the objective counts the destroyed targets; the
 * mission completes once all of them are dead (never while its tour still shows).
 * Targets are found at start: every actor of TargetClasses (subclasses count) whose location lies in the target
 * area box (2D). Targets that are already dead are skipped; with none left the mission completes after the tour.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API USecureBaseMission : public UMissionBase
{
	GENERATED_BODY()

public:
	virtual void OnMissionComplete() override;
	virtual void OnMissionFailed() override;
	virtual void OnCleanUpMission() override;

	/** @return Targets that are still alive. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	TArray<AActor*> GetLivingTargets() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	int32 GetTargetCount() const { return M_Targets.Num(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	int32 GetDestroyedTargetCount() const;

	/** @return Squads and the truck this mission spawned to bring the base to life (living ones only). */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	TArray<AActor*> GetBaseLifeActors() const;

	/** @return Living spawned squads whose active order is the march (tests read it). */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	int32 GetMarchingSquadCount() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	AActor* GetFuelTruck() const { return M_FuelTruck.Get(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	AMissionFuelCrane* GetFuelCrane() const;

	/** @return True once the truck parked and the crane was told to unload it. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	bool GetHasFuelTruckParked() const { return bM_HasTruckParked; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	bool GetHasTourFinished() const { return bM_HasTourFinished; }

	/** @return True once TourVoiceLine was handed to the portrait manager. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base")
	bool GetHasTourVoiceLinePlayed() const { return bM_HasTourVoiceLinePlayed; }

	/** @return True once a player unit entered the assault trigger and the base called in its response. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base|Assault Response")
	bool GetHasAssaultResponseStarted() const { return bM_HasAssaultResponseStarted; }

	/** @return The enemy air drops the assault response spawned (living ones). */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base|Assault Response")
	TArray<AEnemyAirDrop*> GetResponseAirDrops() const;

	/** @return True once the air support ability was executed. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base|Assault Response")
	bool GetHasAirSupportLaunched() const { return bM_HasAirSupportLaunched; }

	/** @return True once the intel squad's info takeover was requested. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base|Assault Response")
	bool GetHasShownIntelSquadTakeover() const { return bM_HasShownIntelSquadTakeover; }

	/** @return True once AssaultResponse.GrantedAbility was handed to the player. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Secure Base|Assault Response")
	bool GetHasGrantedAbility() const { return bM_HasGrantedAbility; }

	/** @brief Starts the assault response as if a player unit entered the trigger (tests, scripted events). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Secure Base|Assault Response")
	void StartAssaultResponse();

protected:
	// ---- Targets ----
	// Classes of the buildings to destroy; subclasses match too.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	TArray<TSubclassOf<AActor>> TargetClasses;

	// Centre and half size (only X and Y are used) of the box the targets must stand in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	FVector TargetAreaCentre = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	FVector TargetAreaHalfExtent = FVector(5000.f, 5000.f, 0.f);

	// World marker spawned above every target (for example BP_Hint_DestroyTarget); removed when its target dies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	TSubclassOf<AActor> TargetMarkerClass;

	// Text handed to the marker's "SetText" event, when it has one; empty keeps the marker's own text.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	FText TargetMarkerText;

	// Height of the marker above the top of its target's bounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	float TargetMarkerHeightAboveTarget = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Targets")
	EMinimapIconType TargetMiniMapIcon = EMinimapIconType::AttackTarget;

	// ---- Objective ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Objective")
	FText ObjectiveTitle;

	// {Destroyed} and {Total} are replaced by the counts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Objective")
	FText ObjectiveDescription;

	// Optional line played on ObjectivePortrait once the tour has handed the screen back.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Objective")
	TObjectPtr<USoundBase> ObjectiveVoiceLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Objective")
	ERTSPortraitTypes ObjectivePortrait = ERTSPortraitTypes::GermanAnnouncer;

	// ---- Reveal ----
	// Each point lifts the fog over a circle of RevealRadius for RevealSeconds from the mission start.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Reveal")
	TArray<FVector> RevealCentres;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Reveal", meta = (ClampMin = "100.0"))
	float RevealRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Reveal", meta = (ClampMin = "1.0"))
	float RevealSeconds = 60.f;

	// ---- Base life ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Base Life")
	TArray<FSecureBaseMarchingSquad> MarchingSquads;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Base Life")
	FSecureBaseFuelDelivery FuelDelivery;

	// ---- Tour ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour")
	TArray<FMissionCameraTourShot> TourShots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour")
	FMissionCameraTourSettings TourSettings;

	// The tour waits for the base life to spawn (so nothing pops in on camera), at most this long.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour", meta = (ClampMin = "0.0"))
	float MaxSecondsWaitingForBaseLife = 12.f;

	// Optional line the advisor speaks over the tour: it starts TourVoiceLineDelaySeconds after the first shot shows
	// (the tour's takeover stops any portrait playing when it starts). The portrait box is part of the hidden HUD, so
	// during the tour only the voice is heard. A tour that never showed plays the line once it hands the screen back.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour")
	TObjectPtr<USoundBase> TourVoiceLine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour")
	ERTSPortraitTypes TourVoiceLinePortrait = ERTSPortraitTypes::GermanAnnouncer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Tour", meta = (ClampMin = "0.0"))
	float TourVoiceLineDelaySeconds = 0.8f;

	// ---- Assault response ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Secure Base|Assault Response")
	FSecureBaseAssaultResponse AssaultResponse;

	// A player unit entered the assault trigger; the alarm line, air drops and air support follow.
	UFUNCTION(BlueprintImplementableEvent, Category = "Secure Base|Assault Response")
	void BP_OnAssaultResponseStarted();

	// A target died: its marker is gone and the objective counts it.
	UFUNCTION(BlueprintImplementableEvent, Category = "Secure Base")
	void BP_OnSecureBaseTargetDestroyed(AActor* Target, const int32 DestroyedCount, const int32 TotalCount);

	virtual void OnMissionStartedNative() override;
	virtual void OnCameraTourFinishedNative(const int32 ID, const bool bWasShown) override;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_Targets;

	// Per target: its marker actor, minimap icon ID and whether its death was counted.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_TargetMarkers;

	TArray<FName> M_TargetIconIds;
	TArray<bool> M_TargetCounted;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedSquads;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_FuelTruck;

	UPROPERTY()
	TWeakObjectPtr<AMissionFuelCrane> M_FuelCrane;

	// Truck wreck smoke and decal assets, loaded when the truck spawns.
	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_PreloadedTruckDeathAssets;

	FTimerHandle M_TargetPollTimer;
	FTimerHandle M_TruckPollTimer;
	FTimerHandle M_TruckSpawnTimer;
	FTimerHandle M_TourStartTimer;
	// Short-lived timers that give a freshly spawned squad its march order again while it settles.
	TArray<TSharedRef<FTimerHandle>> M_MarchRetryTimers;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyAirDrop>> M_ResponseAirDrops;

	// Strike copy; only needed until its aircraft spawned (they fly their strafe on their own after that).
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_AirSupportAbility = nullptr;

	FTimerHandle M_AssaultTriggerPollTimer;
	FTimerHandle M_AirSupportLineTimer;
	FTimerHandle M_AirSupportStrikeTimer;
	FTimerHandle M_IntelSquadTimer;
	FTimerHandle M_GrantedAbilityTimer;
	bool bM_HasAssaultResponseStarted = false;
	bool bM_HasAirSupportLaunched = false;
	bool bM_HasShownIntelSquadTakeover = false;
	bool bM_HasGrantedAbility = false;
	// Polls after IntelSquadDelaySeconds until a squad of IntelSquadSubtype stands (its drop may still be landing).
	int32 M_IntelSquadPollsLeft = 0;

	void ArmAssaultTrigger();
	// Starts the response once a living player unit stands in the trigger box.
	void PollAssaultTrigger();
	bool GetIsInAssaultTrigger(const FVector& Location) const;
	void SpawnResponseAirDrop(const int32 DropIndex);
	void PlayAirSupportLine();
	void LaunchAirSupport();
	// After the strike took off: the intel squad takeover and the granted ability follow on their own delays.
	void ScheduleFollowUpsAfterAirSupport();
	void TryShowIntelSquadTakeover();
	AActor* FindLivingIntelSquad() const;
	void GrantAbilityToPlayer();

	int32 M_PendingLifeSpawns = 0;
	float M_SecondsWaitedForBaseLife = 0.f;
	bool bM_HasTourStarted = false;
	bool bM_HasTourFinished = false;

	// Tour voice line: polls for the first shot on screen, then waits TourVoiceLineDelaySeconds.
	FTimerHandle M_TourVoiceLineTimer;
	bool bM_HasTourVoiceLinePlayed = false;
	void ScheduleTourVoiceLine();
	void PollTourVoiceLineShot();
	void PlayTourVoiceLine();
	bool bM_HasTruckParked = false;
	bool bM_HasCompletedTargets = false;

	void Start_CollectTargets();
	void Start_RevealBase();
	void Start_MarkTargets();
	void Start_SpawnBaseLife();
	void SpawnMarchingSquad(const int32 SquadIndex);
	void OnMarchingSquadSpawned(const int32 SquadIndex, AActor* SpawnedSquad);
	void SpawnFuelCrane();
	void ScheduleDelayedFuelTruck();
	void SpawnFuelTruck();
	void OnFuelTruckSpawned(AActor* SpawnedTruck);
	void SetupFuelTruckHitboxAndDeath(AActor* Truck);
	void PollFuelTruck();
	void OnLifeSpawnReported();

	void PollTourStart();
	void StartTour();

	void PollTargets();
	void OnTargetDestroyed(const int32 TargetIndex);
	void RemoveTargetMarker(const int32 TargetIndex);
	void RefreshObjective();
	void TryCompleteMission();

	void ClearRuntime();
	bool GetIsTargetDead(const int32 TargetIndex) const;
	bool GetIsInTargetArea(const FVector& Location) const;
};
