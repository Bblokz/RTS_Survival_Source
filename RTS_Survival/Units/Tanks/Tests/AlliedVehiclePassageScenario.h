// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleFootprint.h"

class ACPPController;
class ANomadicVehicle;
class ATrackedTankMaster;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class EAlliedPassagePhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForVehicles,
	Driving,
	Done
};

/** One vehicle the scenario follows: what it was told to do and what it did. */
struct FAlliedPassageTestVehicle
{
	TWeakObjectPtr<ATrackedTankMaster> Vehicle;
	FString Label;
	FRTSVehicleFootprint Footprint;
	FVector StartLocation = FVector::ZeroVector;
	FVector GoalLocation = FVector::ZeroVector;
	bool bHasGoal = false;
	float ArrivalSeconds = -1.f;
	float StalledSeconds = 0.f;
	float FarthestFromStart = 0.f;
};

/** Two vehicles whose hulls the scenario watches. */
struct FAlliedPassageTestPair
{
	int32 FirstIndex = INDEX_NONE;
	int32 SecondIndex = INDEX_NONE;
	float DeepestPenetration = 0.f;
};

/**
 * @brief Checks that harvesters drive through allied vehicles and each other while enemy vehicles still stop them,
 * and that a nomadic truck sent to deploy pushes the idle allies on its way and on its building spot aside.
 * Started with -AlliedPassageValidate on UT_Nomadics (lanes are laid out around the map's HQ truck, override with
 * -AlliedPassageAt=X,Y); logs RTS_ALLIED_PASSAGE and quits with RESULT PASS or FAIL. Not part of a shipping build.
 */
struct FAlliedVehiclePassageScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& Host, float DeltaTime);

private:
	EAlliedPassagePhase M_Phase = EAlliedPassagePhase::Inactive;
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;
	int32 M_FailedCheckCount = 0;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UWorld> M_World;
	TWeakObjectPtr<ANomadicVehicle> M_Nomadic;

	// Lanes are laid out from the HQ truck: its lane runs along its facing, the harvester lanes beside it.
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_LaneForward = FVector::ForwardVector;
	FVector M_LaneRight = FVector::RightVector;
	FVector M_NomadicGoal = FVector::ZeroVector;
	// Enemy-owned chassis drop through this map's landscape, so the enemy tank is held here as a kinematic body.
	FVector M_EnemyHoldLocation = FVector::ZeroVector;

	TArray<FAlliedPassageTestVehicle> M_Vehicles;
	TArray<FAlliedPassageTestPair> M_Pairs;

	// Indices into M_Vehicles of the vehicles the checks are about.
	int32 M_HeadOnHarvesterA = INDEX_NONE;
	int32 M_HeadOnHarvesterB = INDEX_NONE;
	int32 M_LaneHarvester = INDEX_NONE;
	int32 M_OncomingTank = INDEX_NONE;
	int32 M_LaneParkedTank = INDEX_NONE;
	int32 M_EnemyLaneHarvester = INDEX_NONE;
	int32 M_EnemyTank = INDEX_NONE;
	int32 M_NomadicIndex = INDEX_NONE;
	TArray<int32> M_NomadicParkedTanks;

	int32 M_PairHeadOnHarvesters = INDEX_NONE;
	int32 M_PairHarvesterOncomingTank = INDEX_NONE;
	int32 M_PairHarvesterParkedTank = INDEX_NONE;
	int32 M_PairHarvesterEnemy = INDEX_NONE;

	// Game time the truck left truck mode, and how far its goal and the allies parked on it were then.
	float M_NomadicDeploySeconds = -1.f;
	float M_NomadicDeployGoalDistance = -1.f;
	float M_NomadicDeployNearestParkedGap = -1.f;

	void EnterPhase(EAlliedPassagePhase NewPhase);
	float GetGameSecondsInPhase() const;
	double GetRealSecondsInPhase() const;
	void Check(bool bCondition, const TCHAR* Description, const FString& Details);
	void Finish();

	void TickWaitingForGameStart(UWorld& World);
	void TickWaitingForVehicles(UWorld& World);
	void TickDriving(UWorld& World, float DeltaTime);

	// Spawns the scenario's HQ truck on open ground; its location is where all lanes are laid out from.
	bool FindAnchor(UWorld& World);

	/**
	 * @brief Looks for flat open landscape on the navmesh that holds every lane, nearest to the test spot first.
	 * @param World Game world.
	 * @param SearchOrigin The test spot.
	 * @param OutAnchor Receives the ground location the lanes are laid out from.
	 * @return False when no such area lies within the search radius.
	 */
	bool FindOpenTestArea(UWorld& World, const FVector& SearchOrigin, FVector& OutAnchor) const;
	bool GetIsOpenTestArea(UWorld& World, const FVector2D& Candidate, FVector& OutAnchor) const;
	bool GetOpenGroundZ(UWorld& World, const FVector2D& Location, float& OutGroundZ) const;
	FVector GetLanePoint(float AlongLane, float AcrossLane) const;

	/**
	 * @brief Spawns a vehicle for the scenario and starts following it.
	 * @param World Game world.
	 * @param ClassPath Blueprint class of the vehicle.
	 * @param Location Where it appears; dropped onto the ground and the navmesh.
	 * @param Yaw Its facing.
	 * @param OwningPlayer Player it belongs to.
	 * @param Label Name in the log.
	 * @return Its index in M_Vehicles, INDEX_NONE when it could not be spawned.
	 */
	int32 SpawnVehicle(UWorld& World, const TCHAR* ClassPath, const FVector& Location, float Yaw, uint8 OwningPlayer,
	                   const TCHAR* Label);
	int32 AddPair(int32 FirstIndex, int32 SecondIndex);
	void SpawnHarvesterLanes(UWorld& World);
	void SpawnNomadicLane(UWorld& World);

	bool GetAreVehiclesReady();
	// Keeps the enemy tank on its spot without physics; see M_EnemyHoldLocation.
	void HoldEnemyTank() const;
	void SilenceVehicles() const;
	void CheckCollisionSetup();
	void OrderVehicles(UWorld& World);
	void OrderMove(int32 VehicleIndex, const FVector& Goal);

	void SampleVehicles(float DeltaTime);
	void SamplePairs();
	void SampleNomadicDeploy();
	bool GetIsDrivingDone() const;
	void EvaluateResults();
	void EvaluateHarvesterResults();
	void EvaluateNomadicResults();
	void ReportVehicles() const;

	float GetSecondsSinceOrders() const;
	FVector2D GetHullCenter(const FAlliedPassageTestVehicle& TestVehicle) const;
	bool GetHasArrived(int32 VehicleIndex) const;
};

#endif
