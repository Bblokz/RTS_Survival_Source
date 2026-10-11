#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleFootprint.h"

class AActor;
class ATrackedTankMaster;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class EVehicleAvoidancePhase : uint8
{
	Inactive,
	WarmingUp,
	Observing,
	DrivingSamePoint,
	DrivingFormation,
	// Two groups, each to its own block, far apart.
	DrivingSplit,
	// The two groups trade blocks and meet head-on halfway.
	DrivingSwap,
	// Every vehicle trades places with one on the other side of its own block.
	DrivingShuffle
};

/** One vehicle the scenario follows, with the order the scenario gave it when it drives the vehicles itself. */
struct FVehicleAvoidanceTestVehicle
{
	TWeakObjectPtr<ATrackedTankMaster> Tank;
	FRTSVehicleFootprint Footprint;
	// The box around the chassis collision, in the space of the chassis mesh, at its real size.
	FBox HullBox = FBox(ForceInit);
	int32 OwningPlayer = 0;
	bool bWasIdle = true;
	bool bHasGoal = false;
	FVector GoalLocation = FVector::ZeroVector;
	float GoalYaw = 0.f;
	float ArrivalSeconds = -1.f;
	float FirstSeenSeconds = 0.f;
};

/** What happened between vehicles of one pairing (player with player, enemy with enemy, player with enemy). */
struct FVehicleAvoidancePairStats
{
	double OverlapPairSeconds = 0.0;
	float DeepestOverlap = 0.f;
	int32 OverlappingPairsNow = 0;
};

/** Totals of one measured stretch of time. */
struct FVehicleAvoidanceStats
{
	FVehicleAvoidancePairStats PlayerPairs;
	FVehicleAvoidancePairStats EnemyPairs;
	FVehicleAvoidancePairStats HostilePairs;
	double StalledVehicleSeconds = 0.0;
	double MovingVehicleSeconds = 0.0;
	double DistanceDriven = 0.0;
	// Times a vehicle was somewhere else from one frame to the next: a teleport, not driving.
	int32 Jumps = 0;
	float SteepestTiltDegrees = 0.f;
	float LastReportedTiltDegrees = 0.f;
	int32 Arrivals = 0;
	float StartSeconds = 0.f;
};

/**
 * @brief Measures how well vehicles stay out of each other: how long and how deep their hulls are inside each
 * other, how long they stand still while ordered to move, how far they tip over and whether they end up where
 * and how they were told to. Logged as RTS_VEHICLE_AVOIDANCE.
 * Started with -VehicleAvoidanceMeasure on a map that moves its own vehicles (TestVehicleAvoidance); with
 * -VehicleAvoidanceDrive the scenario orders the player's vehicles around itself (TestHarvest). Not part of a
 * shipping build.
 */
struct FVehicleAvoidanceScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem, float DeltaTime);

private:
	EVehicleAvoidancePhase M_Phase = EVehicleAvoidancePhase::Inactive;
	bool bM_DriveVehicles = false;
	int32 M_ExtraVehiclesToSpawn = 0;
	float M_ObserveSeconds = 120.f;
	float M_PhaseEndSeconds = 0.f;
	float M_NextReportSeconds = 0.f;
	float M_NextCollectSeconds = 0.f;
	float M_NextTraceSeconds = 0.f;
	bool bM_TraceVehicles = false;
	FVector M_DriveCenter = FVector::ZeroVector;

	TArray<FVehicleAvoidanceTestVehicle> M_Vehicles;
	TArray<FVector> M_LastLocations;
	// Pairs of vehicles that were already reported as deep inside each other.
	TSet<uint64> M_ReportedDeepOverlaps;
	FVehicleAvoidanceStats M_Stats;

	// With -VehicleAvoidanceScreenshots a camera hangs above the vehicles and a picture is saved now and then.
	// With -VehicleAvoidancePerf the engine writes its averaged stat tree to the log once the map is busy.
	bool bM_DumpStats = false;
	bool bM_HasDumpedStats = false;
	bool bM_TakeScreenshots = false;
	float M_NextScreenshotSeconds = 0.f;
	int32 M_ScreenshotCount = 0;
	TWeakObjectPtr<AActor> M_OverviewCamera;

	void UpdateOverviewCamera(UWorld& World);
	void CollectVehicles(UWorld& World);
	void SpawnExtraVehicles(UWorld& World);
	void EnterPhase(UWorld& World, EVehicleAvoidancePhase NewPhase, float DurationSeconds);
	void SampleFrame(UWorld& World, float DeltaTime);
	void SamplePairs(const UWorld& World, float DeltaTime);
	void ReportNewDeepOverlap(const UWorld& World, int32 FirstIndex, int32 SecondIndex, float Depth);
	void ReportProgress(const UWorld& World, const TCHAR* Label) const;
	void ReportBusyVehicles(const UWorld& World) const;
	void ReportPhaseResult(const UWorld& World, const TCHAR* PhaseName) const;
	void AdvanceDrivePhase(UWorld& World);
	bool GetAreAllDrivenVehiclesIdle() const;

	void OrderSamePoint();
	void OrderFormation();
	void OrderGroupsToBlocks(bool bSwapBlocks, bool bMirrorSlots);
	void OrderVehicleToSlot(int32 VehicleIndex, const FVector& Slot, float Yaw);
	TArray<int32> GetPlayerVehicleIndices() const;
	TArray<FVector> BuildFormationSlots(const TArray<int32>& VehicleIndices, const FVector& Center, float Yaw) const;
};
