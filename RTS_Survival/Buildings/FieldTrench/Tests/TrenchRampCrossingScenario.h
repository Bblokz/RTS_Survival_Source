// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

class ACPPController;
class ARTSFieldTrench;
class ATrackedTankMaster;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ETrenchRampTestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForVehicles,
	Driving,
	Done
};

enum class ETrenchRampLaneKind : uint8
{
	// Drives square across a short built trench.
	Square,
	// Drives across a long built trench at 45 degrees.
	Oblique,
	// Drives the same distance over open ground: tells a vehicle that cannot climb from one that cannot drive.
	Control,
	// Parked where a trench is dug under it after it settled.
	Parked
};

/** One lane of the scenario: a vehicle class, its owner and the trench kit in its way. */
struct FTrenchRampLane
{
	const TCHAR* Label = TEXT("");
	const TCHAR* ClassPath = TEXT("");
	uint8 Owner = 1;
	EFieldTrenchKit Kit = EFieldTrenchKit::German;
	ETrenchRampLaneKind Kind = ETrenchRampLaneKind::Square;
};

/** One vehicle the scenario follows across (or parked on) a trench. */
struct FTrenchRampTestVehicle
{
	TWeakObjectPtr<ATrackedTankMaster> Vehicle;
	FTrenchRampLane Lane;
	FVector SiteCentre = FVector::ZeroVector;
	FVector GoalLocation = FVector::ZeroVector;
	// Actor height once settled on the open ground, before the trench is reached.
	float SettledZ = 0.f;
	float HighestRise = 0.f;
	float LowestUpZ = 1.f;
	float ArrivalSeconds = -1.f;
	float LastTraceLogSeconds = -1.f;
};

/**
 * @brief Checks that vehicles drive over the earth bank of a field trench on its invisible vehicle ramp instead of
 * through the bag walls: every lane sends one vehicle class across its own trench (German kit for the player, Soviet
 * kit for the enemy), one tank crosses at 45 degrees, a control lane drives the light car over open ground, and a
 * trench dug under a parked tank lifts it. Every vehicle's along-lane position and height are logged once a second.
 * Started with -TrenchRampValidate (spot override -TrenchRampAt=X,Y), e.g. on UT_Nomadics; logs RTS_TRENCH_RAMP and
 * quits with RESULT PASS or FAIL. Not part of a shipping build.
 */
struct FTrenchRampCrossingScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& Host, float DeltaTime);

private:
	ETrenchRampTestPhase M_Phase = ETrenchRampTestPhase::Inactive;
	double M_PhaseStartRealSeconds = 0.0;
	float M_PhaseStartGameSeconds = 0.f;
	int32 M_FailedCheckCount = 0;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<UWorld> M_World;
	FVector M_Anchor = FVector::ZeroVector;

	TArray<FTrenchRampLane> M_Lanes;
	TArray<FTrenchRampTestVehicle> M_Vehicles;
	TArray<TWeakObjectPtr<ARTSFieldTrench>> M_Trenches;
	// Dug while the parked tank stands on it.
	TWeakObjectPtr<ARTSFieldTrench> M_DugTrench;
	float M_DugTrenchFinishedSeconds = -1.f;

	void EnterPhase(ETrenchRampTestPhase NewPhase);
	float GetGameSecondsInPhase() const;
	double GetRealSecondsInPhase() const;
	void Check(bool bCondition, const TCHAR* Description, const FString& Details);
	void Finish();

	void TickWaitingForGameStart(UWorld& World);
	void TickWaitingForVehicles(UWorld& World);
	void TickDriving(UWorld& World);

	void MakeLanes();
	FVector GetSiteOffset(int32 LaneIndex) const;
	void SetUpLane(UWorld& World, int32 LaneIndex);

	/**
	 * @brief Spawns a straight trench (end cap, straights, end cap) whose middle lies on Centre.
	 * @param Yaw Direction the trench runs in.
	 * @param Straights Number of 500 cm straights.
	 * @param Kit The faction kit of its pieces.
	 * @param bDigIt Digs it piece by piece instead of building it at once.
	 */
	ARTSFieldTrench* SpawnTrench(UWorld& World, const FVector& Centre, float Yaw, int32 Straights, EFieldTrenchKit Kit,
	                             bool bDigIt);
	void SpawnVehicle(UWorld& World, const FTrenchRampLane& Lane, const FVector& SiteCentre, const FVector& Start,
	                  const FVector& Goal);
	void OrderVehicles();
	void UpdateVehicle(FTrenchRampTestVehicle& TestVehicle, float SecondsSinceOrders) const;
	void CheckVehicles();
	void CheckRamps();
	bool GetIsDugTrenchFinished() const;
	float GetGroundZ(UWorld& World, const FVector& Location) const;
	// Open, even landscape for every lane, nearest the search origin first.
	bool FindOpenArea(UWorld& World, const FVector& SearchOrigin, FVector& OutAnchor) const;
	bool GetIsOpenSite(UWorld& World, const FVector2D& Centre) const;
	bool GetLandscapeZ(UWorld& World, const FVector2D& Location, float& OutZ) const;
};

#endif
