// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourceUnloadStation.h"

#include "BucketElevatorUnloadStation.generated.h"

class UHarvester;
class UStaticMesh;
class UStaticMeshComponent;

/** What the elevator is doing for its bay. */
enum class EBucketElevatorState : uint8
{
	// Apron up, belt stopped (or running out the last bars).
	Idle,
	// A harvester docked: the feed apron swings down onto its rear.
	LoweringApron,
	// Bars pour over the hopper rim, slide down the apron into the boot, ride up in the buckets.
	Feeding,
	// Every bar is on its way (or the unload stopped): the apron swings back up.
	RaisingApron
};

/** One bucket on the belt and the bar it may carry. */
struct FElevatorBucket
{
	TWeakObjectPtr<UStaticMeshComponent> Bucket;
	TWeakObjectPtr<UStaticMeshComponent> Bar;
	bool bIsLoaded = false;
};

enum class EElevatorBarMotion : uint8
{
	Inactive,
	// From inside the hopper over its rear rim onto the apron tip.
	Pouring,
	// Down the apron into the boot.
	Sliding,
	// Thrown off the head into the bin.
	Falling
};

/** A loose bar that is not in a bucket (pooled). */
struct FElevatorBar
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	EElevatorBarMotion Motion = EElevatorBarMotion::Inactive;
	float Seconds = 0.f;
	float Distance = 0.f;
	float Speed = 0.f;
	FVector Velocity = FVector::ZeroVector;
};

/**
 * @brief Inclined bucket elevator over the north sandbags (the Strike Division metal depot's unload station).
 * One bay: the harvester reverses up to the boot, a feed apron swings down onto its rear, and bars pour over the hopper
 * rim, slide into the boot, ride up in the buckets and are thrown off the head into the bin. Every BarsPerUnload /
 * TripsPerUnload bars that land credit one chunk through the drop-off (pop-up per chunk); the bars in flight are visual.
 * @note No trolleys: the bay is driven from TickUnloadStation. The rail is the boom's ground line (it only tells the
 * base class the station reaches the metal bin). Harvesters dock by their rear bumper, not their cargo socket. Rig
 * numbers mirror _AssetSource/MetalDepotUnload/build_bucket_elevator.py; see Docs/Units/MetalDepotElevator.md.
 */
UCLASS()
class RTS_SURVIVAL_API ABucketElevatorUnloadStation : public AResourceUnloadStation
{
	GENERATED_BODY()

public:
	ABucketElevatorUnloadStation();

	virtual EUnloadTrolleyPhase GetTrolleyPhase(int32 BayIndex) const override;

protected:
	virtual bool GetUsesTrolleys() const override { return false; }
	virtual void OnUnloadStationReady() override;
	virtual void TickUnloadStation(float DeltaSeconds) override;
	virtual FVector2D GetHarvesterDockPoint(const UHarvester& Harvester) const override;

	// Boot and head pulley axles (station frame, cm); the boom runs between them in the YZ plane.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Rig")
	FVector BootPulleyCentre = FVector(-22.f, -368.f, 80.f);

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Rig")
	FVector HeadPulleyCentre = FVector(-22.f, -45.f, 410.f);

	// Belt pitch radius round both pulleys (cm); the belt runs this far off the boom axis on both strands.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Rig", meta=(ClampMin="5"))
	float BeltRadius = 30.f;

	// Buckets on the belt, spaced evenly round the loop.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Rig", meta=(ClampMin="4"))
	int32 BucketCount = 25;

	// Belt speed while running (cm/s); one bar per bucket, so this also sets the feed rate.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Timing", meta=(ClampMin="10"))
	float BeltSpeed = 55.f;

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Timing", meta=(ClampMin="0.05"))
	float BeltRampSeconds = 0.8f;

	// Bars carried per unload; every BarsPerUnload / TripsPerUnload landed bars credit one chunk.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Timing", meta=(ClampMin="1"))
	int32 BarsPerUnload = 32;

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Timing", meta=(ClampMin="0.05"))
	float ApronSeconds = 1.4f;

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Timing", meta=(ClampMin="0.05"))
	float PourSeconds = 0.5f;

	// Feed apron hinge on the boot (station frame, cm) and the apron mesh's length as modelled.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Apron")
	FVector ApronHinge = FVector(-22.f, -446.f, 152.f);

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Apron", meta=(ClampMin="10"))
	float ApronMeshLength = 140.f;

	// Raised apron direction from the hinge (station frame): nearly upright, tip leaning towards the bay.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Apron")
	FVector ApronStowedDirection = FVector(0.f, -0.17f, 0.985f);

	// The lowered tip rests this far ahead of the harvester's rear bumper and this far above its cargo socket.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Apron")
	float ApronTipAheadOfRear = 40.f;

	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Apron")
	float ApronTipAboveCargo = 75.f;

	// A bar in a bucket (bucket frame: X travel, Z out from the belt).
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Bars")
	FTransform BucketBarSlot = FTransform(FRotator(0.f, 90.f, 0.f), FVector(-3.f, 0.f, 15.f), FVector(0.42f));

	// Loose bars (pouring, sliding, falling) at once.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Bars", meta=(ClampMin="1"))
	int32 LooseBarPoolSize = 10;

	// Origin on the belt, +X = travel (the mouth leads), +Z = out from the belt.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Meshes")
	TObjectPtr<UStaticMesh> BucketMesh;

	// Origin on the axle, axle along X.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Meshes")
	TObjectPtr<UStaticMesh> PulleyMesh;

	// Origin on the hinge, chute along +X (ApronMeshLength), floor facing +Z.
	UPROPERTY(EditDefaultsOnly, Category="Bucket Elevator|Meshes")
	TObjectPtr<UStaticMesh> ApronMesh;

private:
	EBucketElevatorState M_State = EBucketElevatorState::Idle;
	TArray<FElevatorBucket> M_Buckets;
	TArray<FElevatorBar> M_LooseBars;
	TWeakObjectPtr<UStaticMeshComponent> M_HeadPulley;
	TWeakObjectPtr<UStaticMeshComponent> M_BootPulley;
	TWeakObjectPtr<UStaticMeshComponent> M_Apron;

	// Belt position (cm, wraps at the loop length) and current speed.
	float M_BeltDistance = 0.f;
	float M_BeltCurrentSpeed = 0.f;
	// 0 = apron raised, 1 = resting on the harvester.
	float M_ApronAlpha = 0.f;
	// Lowered apron direction and length for the docked harvester (station frame).
	FVector M_ApronLoweredDirection = FVector::UpVector;
	float M_ApronLoweredLength = 140.f;
	// Pour path from inside the hopper to the apron tip (world).
	FVector M_PourStart = FVector::ZeroVector;
	FVector M_PourControl = FVector::ZeroVector;

	// Whether landed bars still credit the docked harvester's cargo.
	bool bM_IsUnloadActive = false;
	int32 M_BarsFed = 0;
	int32 M_BarsLanded = 0;
	// Bars in the boot waiting for an empty bucket.
	int32 M_BootStock = 0;
	float M_FeedTimer = 0.f;
	float M_BinStopZ = 0.f;
	FVector M_BinWorld = FVector::ZeroVector;

	void BuildMetalDepotDefaults();

	float GetLoopLength() const;
	float GetBoomLength() const;
	FVector GetBoomDirection() const;
	FVector GetBoomNormal() const;
	/** @brief Point, travel direction and out-normal at belt distance S (station frame). */
	void GetBeltFrame(float S, FVector& OutPoint, FVector& OutTravel, FVector& OutNormal) const;
	float GetLoadMark() const;
	float GetDischargeMark() const;

	void StartUnload(const UHarvester& Harvester);
	void StopUnload();
	void HideAllBars();
	bool GetHasBarsInSystem() const;
	bool GetHasBarsOnApron() const;

	void TickState(float DeltaSeconds);
	void TickBelt(float DeltaSeconds);
	void TickLooseBars(float DeltaSeconds);
	void FeedBar();
	void OnBarLanded();
	void DischargeBucket(FElevatorBucket& Bucket, const FVector& TravelLocal);

	FVector GetApronDirection() const;
	float GetApronLength() const;
	FVector GetApronTipWorld() const;
	FVector GetApronFloorNormal(const FVector& Direction) const;
	void UpdateApron() const;
	void UpdateBuckets() const;
};
