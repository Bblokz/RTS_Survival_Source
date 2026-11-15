// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirector.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "SupplyPointTypes.generated.h"

namespace SupplyPointConstants
{
	// Owner value of a supply point nobody holds.
	inline constexpr uint8 NeutralOwner = 0;
	inline constexpr uint8 PlayerOwner = 1;
	inline constexpr uint8 EnemyOwner = 2;
}

/** Where a supply point is in its supply cycle; capturing runs beside it and resets it when the owner changes. */
UENUM(BlueprintType)
enum class ESupplyPointState : uint8
{
	// No supplies are being prepared: nobody owns the point, or its single supply run is done.
	Inactive,
	// Owned; the supplies-ready timer runs.
	PreparingSupplies,
	// Owned; supplies wait for a truck in the bay.
	SuppliesReady,
	// The crane is loading the parked truck.
	LoadingTruck
};

UENUM(BlueprintType)
enum class ESupplyTruckState : uint8
{
	// Spawned, no run assigned yet.
	Idle,
	DrivingToApproach,
	DrivingToPark,
	// Parked in the bay; the supplies are not ready yet.
	WaitingForSupplies,
	BeingLoaded,
	DrivingToExit,
	DrivingToBase,
	// Cargo handed over; the truck is about to be removed.
	Delivered
};

/** The three announcer moments of a supply run; each has its own deck of lines. */
UENUM(BlueprintType)
enum class ESupplyAnnouncement : uint8
{
	SupplyPointCaptured,
	SupplyTruckDispatched,
	SupplyTruckReturning
};

/** What one truck carries: paid out only when the truck reaches its base. */
USTRUCT(BlueprintType)
struct FSupplyTruckCargo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point", meta = (ClampMin = "0"))
	int32 Radixite = 150;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point", meta = (ClampMin = "0"))
	int32 Metal = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point", meta = (ClampMin = "0"))
	int32 VehicleParts = 25;

	bool GetIsEmpty() const { return Radixite <= 0 and Metal <= 0 and VehicleParts <= 0; }

	int32 GetAmount(const ERTSResourceType ResourceType) const
	{
		switch (ResourceType)
		{
		case ERTSResourceType::Resource_Radixite: return Radixite;
		case ERTSResourceType::Resource_Metal: return Metal;
		case ERTSResourceType::Resource_VehicleParts: return VehicleParts;
		default: return 0;
		}
	}

	void Reset()
	{
		Radixite = 0;
		Metal = 0;
		VehicleParts = 0;
	}
};

/**
 * @brief Everything a level designer tunes on one placed supply point: capture, the supply timers and what a
 * truck takes home per trip.
 */
USTRUCT(BlueprintType)
struct FSupplyPointSettings
{
	GENERATED_BODY()

	// Who holds the point when the map starts: 2 = the enemy director (default), 1 = the player, 0 = nobody.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture",
		meta = (ClampMin = "0", ClampMax = "2"))
	uint8 InitialOwningPlayer = SupplyPointConstants::EnemyOwner;

	// The enemy director of this map: its flag flies (on the pole and on the minimap) while the player does not
	// hold the point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture")
	EEnemyDirector EnemyDirector = EEnemyDirector::DirectorOfShield;

	// Off (for now): only the player captures; enemy units in the radius just block the player's capture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture")
	bool bEnemyCanCapture = false;

	// Infantry and vehicles inside this radius capture the point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture", meta = (ClampMin = "100.0"))
	float CaptureRadius = 1200.f;

	// Seconds an uncontested capture takes from a lowered flag to a raised one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture", meta = (ClampMin = "0.1"))
	float CaptureDuration = 12.f;

	// Seconds an abandoned, unfinished capture takes to fall back (a held point restores at the same speed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Capture", meta = (ClampMin = "0.1"))
	float CaptureDecayDuration = 20.f;

	// Seconds after capture (and after every loaded truck) until the next load of supplies is ready.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies", meta = (ClampMin = "0.0"))
	float SuppliesReadyTime = 45.f;

	// Vehicles stop short of a move goal by about this much; the park order aims this far past the park point so
	// the truck ends up on it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies", meta = (ClampMin = "0.0"))
	float TruckStopShortDistance = 230.f;

	// Seconds the crane needs to load a parked truck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies", meta = (ClampMin = "0.0"))
	float TruckLoadingTime = 12.f;

	// What a truck gets per trip; a kind set to zero is not loaded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies")
	FSupplyTruckCargo SuppliesPerTrip;

	// Keep sending trucks while the point is held; off = one truck per capture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies")
	bool bRepeatSupplyRuns = true;

	// Seconds between a truck delivering (or being lost) and the next one leaving the base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Supplies", meta = (ClampMin = "0.0"))
	float TimeBetweenTrucks = 10.f;

	// Offset of the progress bar above the point's root.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|UI")
	FVector ProgressBarOffset = FVector(0.f, 0.f, 900.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|UI", meta = (ClampMin = "0.1"))
	float ProgressBarScale = 1.f;

	// Local height the flag travels up the pole from its lowered position during a capture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Supply Point|Flag", meta = (ClampMin = "0.0"))
	float FlagRaiseHeight = 600.f;
};
