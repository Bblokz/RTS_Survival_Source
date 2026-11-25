// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleFootprint.h"
#include "RTSVehicleCrushSubsystem.generated.h"

class ASquadUnit;
class ATankMaster;
class URTSVehicleCrushSettings;

/** What one soldier decided about the one vehicle bearing down on him. */
struct FRTSVehicleCrushEncounter
{
	TWeakObjectPtr<ATankMaster> Vehicle;
	float LastSeenSeconds = 0.f;
	float DodgeAtSeconds = 0.f;
	float ClearanceCm = 0.f;
	bool bWillDodge = false;
	bool bPicksWrongSide = false;
	bool bHasDodged = false;
};

/** A moving vehicle as seen in one check. */
struct FRTSVehicleCrushHull
{
	ATankMaster* Vehicle = nullptr;
	int32 OwningPlayer = 0;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D::ZeroVector;
	FVector2D DriveDirection = FVector2D::ZeroVector;
	FRTSVehicleFootprint Footprint;
	float VehicleZ = 0.f;
	float LookAheadCm = 0.f;
};

/**
 * @brief Moving vehicles kill hostile soldiers that end up under their hull; soldiers in front may get out of the way.
 * Works only on footprints and owners: no collision response, overlap evasion or path following is changed, so
 * vehicle avoidance and the allied overlap evasion stay exactly as they were.
 * @note Settings: URTSVehicleCrushSettings (Project Settings > Game > Vehicle Crush).
 */
UCLASS()
class RTS_SURVIVAL_API URTSVehicleCrushSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Runs one check now; tests call it to not depend on the timer.
	 */
	void RunCrushCheck();

	/** @return How many soldiers vehicles ran over in this world. */
	int32 GetCrushCount() const { return M_CrushCount; }

	/** @return How many soldiers started to get out of a vehicle's way in this world. */
	int32 GetDodgeCount() const { return M_DodgeCount; }

	/** @return Per crush: X = owner of the vehicle, Y = owner of the soldier it ran over. For tests. */
	const TArray<FIntPoint>& GetCrushOwnerPairs() const { return M_CrushOwnerPairs; }

private:
	FTimerHandle M_CheckTimer;

	// Hulls are measured once per vehicle from the chassis bodies.
	TMap<TWeakObjectPtr<ATankMaster>, FRTSVehicleFootprint> M_FootprintByVehicle;

	TMap<TWeakObjectPtr<ASquadUnit>, FRTSVehicleCrushEncounter> M_EncounterBySoldier;

	// Per vehicle: world second its scream window opened and how many screams it already let out in it.
	TMap<TWeakObjectPtr<ATankMaster>, TPair<float, int32>> M_ScreamWindowByVehicle;

	float M_NextPruneSeconds = 0.f;
	int32 M_CrushCount = 0;
	int32 M_DodgeCount = 0;
	TArray<FIntPoint> M_CrushOwnerPairs;

	bool TryBuildHull(ATankMaster* Vehicle, const URTSVehicleCrushSettings& Settings, FRTSVehicleCrushHull& OutHull);

	/**
	 * @brief Tests one soldier against one hull.
	 * @param OutIsUnderHull Set when the soldier's centre is inside the hull rectangle.
	 * @param OutIsInPath Set when he stands in the corridor the hull drives through in the look-ahead time.
	 */
	void ClassifySoldier(
		const FRTSVehicleCrushHull& Hull,
		const ASquadUnit& Soldier,
		const URTSVehicleCrushSettings& Settings,
		bool& OutIsUnderHull,
		bool& OutIsInPath) const;

	void CrushSoldier(ASquadUnit* Soldier, const FRTSVehicleCrushHull& Hull, float NowSeconds);

	void UpdateDodge(
		ASquadUnit* Soldier,
		const FRTSVehicleCrushHull& Hull,
		const URTSVehicleCrushSettings& Settings,
		float NowSeconds);

	FVector GetDodgeLocation(const ASquadUnit& Soldier, const FRTSVehicleCrushHull& Hull,
	                         const FRTSVehicleCrushEncounter& Encounter) const;

	bool TryConsumeScream(ATankMaster* Vehicle, const URTSVehicleCrushSettings& Settings, float NowSeconds);

	void PruneStaleEntries(const URTSVehicleCrushSettings& Settings, float NowSeconds);
};
