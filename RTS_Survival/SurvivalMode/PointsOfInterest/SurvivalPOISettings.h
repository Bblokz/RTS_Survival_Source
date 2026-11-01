// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOITypes.h"

#include "SurvivalPOISettings.generated.h"

class UPCGGraphInterface;

/** @brief Pool names the compositions draw from; the data asset holds a pool of assets for each. */
namespace SurvivalPOIPools
{
	const FName SovietWreck = TEXT("Battlefield.SovietWreck");
	const FName SovietScavengeWreck = TEXT("Battlefield.SovietScavengeWreck");
	const FName ScavengeTankWreck = TEXT("Battlefield.ScavengeTankWreck");
	const FName GunPit = TEXT("Battlefield.GunPit");
	const FName Sandbags = TEXT("Battlefield.Sandbags");
	const FName ThrownTurret = TEXT("Battlefield.ThrownTurret");
	const FName Crater = TEXT("Common.Crater");
	const FName SmallCrater = TEXT("Common.SmallCrater");
	const FName AmmoDebris = TEXT("Common.AmmoDebris");
	const FName VehicleParts = TEXT("Common.VehicleParts");
	const FName Scorch = TEXT("Common.Scorch");
	const FName Hangar = TEXT("Hangar.Building");
	const FName CaptureVehicle = TEXT("Hangar.CaptureVehicle");
	const FName HangarStores = TEXT("Hangar.Stores");
	const FName CarWreck = TEXT("Hangar.CarWreck");
	const FName ApronTracks = TEXT("Hangar.ApronTracks");
	const FName PlaneWreck = TEXT("Crash.Plane");
	const FName PlaneLargeDebris = TEXT("Crash.PlaneLargeDebris");
	const FName PlaneDebris = TEXT("Crash.PlaneDebris");
	const FName CrashScar = TEXT("Crash.Scar");
	const FName CrashScavenge = TEXT("Crash.Scavenge");
	const FName ConvoyEscort = TEXT("Convoy.Escort");
	const FName ConvoyTruck = TEXT("Convoy.Truck");
	const FName ConvoyVehicle = TEXT("Convoy.Vehicle");
	const FName ConvoyTracks = TEXT("Convoy.Tracks");
	const FName PumpTower = TEXT("Pump.WaterTower");
	const FName PumpMachinery = TEXT("Pump.Machinery");
	const FName PumpPipes = TEXT("Pump.Pipes");
	const FName PumpBarrels = TEXT("Pump.Barrels");
	const FName RelayMast = TEXT("Relay.Mast");
	const FName RelayFence = TEXT("Relay.Fence");
	const FName RelayPole = TEXT("Relay.Pole");
	const FName RelayFallenMast = TEXT("Relay.FallenMast");
}

/**
 * @brief Everything the survival points of interest and the wildlife are built from: where they may go, which landmark
 * each sector carries, the asset pools the compositions draw from and the animal groups.
 * @note The asset lives at /Game/RTS_Survival/SurvivalMode/PointsOfInterest/DA_SurvivalPointsOfInterest; the
 * subsystem loads it by that path. ResetToDefaults writes the code defaults into it (they are serialized, so the cooker
 * follows every referenced Blueprint and mesh).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalPointsOfInterestSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	/** @brief Fills every table with the code defaults (also used when the asset is missing). */
	void FillDefaults();

	UFUNCTION(CallInEditor, Category = "Points Of Interest")
	void ResetToDefaults();

	/** @return The pool with this name, or nullptr. */
	const FSurvivalPOIAssetPool* FindPool(const FName PoolName) const;
	int32 FindPoolIndex(const FName PoolName) const;

	/** @return The asset an AssetKey of a placed object refers to, or nullptr. */
	const FSurvivalPOIAsset* GetAssetByKey(const int32 AssetKey) const;

	const FSurvivalPOITypeRules* FindTypeRules(const ESurvivalPOIType Type) const;

	bool GetIsUsingCodeDefaults() const { return bM_UsingCodeDefaults; }

	// Sites -> Compose -> Spawn and Sites -> Wildlife -> Spawn; filter points between the nodes to drop content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TSoftObjectPtr<UPCGGraphInterface> Graph = TSoftObjectPtr<UPCGGraphInterface>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/SurvivalMode/PointsOfInterest/PCG_SurvivalPointsOfInterest.PCG_SurvivalPointsOfInterest")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	ESurvivalPOISeedMode SeedMode = ESurvivalPOISeedMode::RandomPerMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	int32 FixedSeed = 1117;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	FSurvivalPOIPlacementRules Placement;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TArray<FSurvivalPOITypeRules> TypeRules;

	// One landmark per sector, by the two starts' display names. A sector missing here gets no landmark.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TArray<FSurvivalPOISectorLandmark> SectorLandmarks;

	// Landmark types that have a composition; a sector whose landmark is not built yet gets none (logged).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TArray<ESurvivalPOIType> BuiltLandmarkTypes;

	// Generic POIs per sector, in order: the first stands on the side of the sector's first start (counter-clockwise
	// order around the basin), the second on the other side, and so on. Every start then has the same mix next to it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TArray<ESurvivalPOIType> GenericPerSector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points Of Interest")
	TArray<FSurvivalPOIAssetPool> Pools;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	TArray<FSurvivalWildlifeEntry> Wildlife;

	// Objects spawned per frame once the assets are loaded.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1"))
	int32 ObjectsPerSpawnStep = 8;

	// Spawned props (not animals) that tick do so at most this often (seconds; 0 keeps every frame). Their Blueprint
	// ticks only animate idle details, and two hundred of them every frame cost about a millisecond of game thread.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0"))
	float PropTickInterval = 0.5f;

	// How often the herds look for units around them (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife", meta = (ClampMin = "0.1"))
	float WildlifeAlarmInterval = 0.5f;

private:
	bool bM_UsingCodeDefaults = false;

	void FillDefaults_Placement();
	void FillDefaults_TypesAndSectors();
	void FillDefaults_BattlefieldPools();
	void FillDefaults_OtherPools();
	void FillDefaults_LandmarkPools();
	void FillDefaults_Wildlife();
};
