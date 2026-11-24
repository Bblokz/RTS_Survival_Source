// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"

#include "SovietAirSupplyPadTypes.generated.h"

class AAircraftMaster;
class UAnimSequenceBase;

/** Where the pad's supply loop is. */
UENUM(BlueprintType)
enum class ESovietAirSupplyState : uint8
{
	// Off (Settings.bEnabled false, SetDeliveriesEnabled(false), or no aircraft class).
	Disabled,
	// Counting down to the next cargo plane.
	WaitingForNextPlane,
	// The plane flies in and lands on the pad.
	PlaneApproaching,
	// Landed: the cargo is on the pad and the workers carry it into storage.
	Unloading,
	// Delivered (points granted): the plane takes off vertically and flies away.
	PlaneDeparting
};

UENUM(BlueprintType)
enum class ESovietAirSupplyCargo : uint8
{
	Fuel,
	Munition
};

/**
 * Everything a level designer tunes on a placed supply pad: how often the plane comes and what a completed delivery
 * gives the AI player. All fields edit on the placed instance.
 */
USTRUCT(BlueprintType)
struct FSovietAirSupplySettings
{
	GENERATED_BODY()

	// Off: no plane comes until SetDeliveriesEnabled(true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply")
	bool bEnabled = true;

	// The AI player that owns the cargo plane and receives the points (2 in a map with one enemy controller).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply", meta = (ClampMin = "2", ClampMax = "4"))
	int32 EnemyPlayer = RTSPlayers::FirstAIPlayer;

	// Seconds from BeginPlay (or from enabling) to the first plane taking off towards the pad.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Timing", meta = (ClampMin = "0.0"))
	float FirstPlaneDelaySeconds = 60.f;

	// Seconds between one plane leaving the map (or being shot down) and the next plane taking off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Timing", meta = (ClampMin = "5.0"))
	float SecondsBetweenPlanes = 240.f;

	// Training points the AI player gets once all cargo of a delivery is in storage. A plane shot down before that
	// gives nothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Reward", meta = (ClampMin = "0"))
	int32 TrainingPointsPerDelivery = 3;

	// Building points the AI player gets once all cargo of a delivery is in storage (the pool the enemy base builder
	// will spend).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Reward", meta = (ClampMin = "0"))
	int32 BuildingPointsPerDelivery = 3;

	// Fuel barrels unloaded per plane; the workers carry them to the fuel storage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Cargo", meta = (ClampMin = "0", ClampMax = "24"))
	int32 FuelBarrelsPerPlane = 4;

	// Munition crates unloaded per plane; the workers carry them to the munition storage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Cargo", meta = (ClampMin = "0", ClampMax = "24"))
	int32 MunitionCratesPerPlane = 4;

	// Cargo plane that lands on the pad (a regular aircraft: it can be shot down). Set on the Blueprint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane")
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	// The plane starts this far before the pad, against the landing arrow, and flies in along it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "2000.0"))
	float ApproachDistance = 20000.f;

	// After take-off the plane flies on along the landing arrow for this far, then leaves the map (is removed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "2000.0"))
	float DepartureDistance = 25000.f;

	// Flight height above the pad on the way in and out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "500.0"))
	float CruiseHeightAbovePad = 2500.f;

	// Seconds the plane waits on the pad after the last crate is stored, before it lifts off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "0.0"))
	float TakeOffDelaySeconds = 3.f;

	// Safety: a plane that has not landed after this long is removed (no points) and the next one is scheduled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "30.0"))
	float MaxApproachSeconds = 300.f;

	// Safety: if the cargo is not all stored this long after landing, the plane leaves without points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Supply|Plane", meta = (ClampMin = "30.0"))
	float MaxUnloadSeconds = 240.f;
};

/** How the two unarmed workers and the cargo look and move; set once on the Blueprint. */
USTRUCT(BlueprintType)
struct FSovietAirSupplyWorkerVisuals
{
	GENERATED_BODY()

	// Looped while walking empty-handed (to the cargo, back to the post).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	TObjectPtr<UAnimSequenceBase> WalkAnim = nullptr;

	// Looped while carrying a barrel or crate; the walk anim when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	TObjectPtr<UAnimSequenceBase> CarryAnim = nullptr;

	// Looped while standing at the post or waiting for cargo.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	TObjectPtr<UAnimSequenceBase> IdleAnim = nullptr;

	// Played once while bending to pick up an item, sped up or slowed to last HandleCargoSeconds; idle when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	TObjectPtr<UAnimSequenceBase> PickUpAnim = nullptr;

	// Played once while putting an item down at its storage, fitted to HandleCargoSeconds; idle when empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	TObjectPtr<UAnimSequenceBase> PutDownAnim = nullptr;

	// The carried item appears in the arms (pick-up) or leaves them (put-down) at this fraction of HandleCargoSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandOverFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers", meta = (ClampMin = "10.0"))
	float WalkSpeed = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers", meta = (ClampMin = "10.0"))
	float CarrySpeed = 120.f;

	// Seconds a worker bends over the cargo to pick it up, and again to put it down at the storage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers", meta = (ClampMin = "0.0"))
	float HandleCargoSeconds = 1.2f;

	// Added to the walking direction's yaw: skeletal meshes imported from Mannequin-style rigs face +Y (-90).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers")
	float MeshYawOffset = -90.f;

	// Each worker walks this far to the side of the straight line, so the two do not walk through each other.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Workers", meta = (ClampMin = "0.0"))
	float LaneOffset = 70.f;

	// Socket on the worker mesh the carried barrel or crate hangs on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	FName CarrySocketName = TEXT("spine_03");

	// The carried barrel or crate relative to CarrySocketName.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	FTransform FuelCarryTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	FTransform MunitionCarryTransform = FTransform::Identity;

	// Where the cargo pile starts, relative to the landed plane's CargoDoor socket in the plane's frame
	// (X forward, Y right); relative to the landing point when the plane has no CargoDoor socket.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	FVector CargoPileOffset = FVector(-300.f, 600.f, 0.f);

	// Spacing of the cargo pile grid (two items per row).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo", meta = (ClampMin = "20.0"))
	float CargoPileSpacing = 110.f;

	// Seconds between two items appearing on the pile while the plane is unloaded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo", meta = (ClampMin = "0.0"))
	float SecondsBetweenUnloadedItems = 0.6f;
};
