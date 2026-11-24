// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "WallGateRelationsTestSubsystem.generated.h"

class AWallGate;
class ACPPController;
class UNavArea;

enum class EWallGateRelationsTestStep : uint8
{
	StartGame,
	Spawn,
	// Units settled and the doorway carries player 2's closed-gate area.
	WaitReady,
	FreeForAllChecks,
	OwnerApproach,
	OwnerLeave,
	HostileApproach,
	HumanApproach,
	TeamChecks,
	AllyApproach,
	AllyLeave,
	// Free for all again and the gate captured by player 4: its allowed player follows the owner.
	CaptureWaitReady,
	CaptureChecks,
	FormerOwnerApproach,
	Done
};

/** Where the gate's doorway is, measured once after the gate has spawned (it never moves). */
struct FWallGateRelationsTestDoorway
{
	// Doorway centre on the ground.
	FVector GroundCentre = FVector::ZeroVector;
	// Doorway centre at the leaf's mid height: weapon shots aim here.
	FVector LeafCentre = FVector::ZeroVector;
	// Across the wall, horizontal.
	FVector Normal = FVector::XAxisVector;
	// Along the wall, horizontal.
	FVector Along = FVector::YAxisVector;
	// Half the leaf's width along the wall.
	float DoorwayHalfWidthCm = 0.f;
	// A path crossing the wall line closer than this to the doorway centre went through the doorway: halfway between
	// the doorway's edge and the frame's far end, so a detour round the frame's end never counts.
	float ThroughDoorwayLimitCm = 0.f;
	// From the doorway centre to the far end of the frame's blocked boxes (padded), along the wall.
	float FrameHalfExtentCm = 0.f;
};

/** How a path from one side of the gate to the other went. */
enum class EWallGatePathOutcome : uint8
{
	// The test could not ask (no navmesh, filter or tank, or path ends off the navmesh): always a failed check.
	SetupFailed,
	NoPath,
	AroundGate,
	ThroughGate
};

/**
 * @brief Headless check of the four-player wall gate rules: launch with -game -WallGateRelationsTest on a flat map,
 * optionally -WallGateTestAt=X,Y -WallGateTestDir=<dir>. Spawns a Soviet wall gate of player 2 and a tank of players
 * 1, 2 and 3 far apart, then brings one tank at a time to the doorway: the gate opens for player 2, stays shut for the
 * hostile players 3 and 1, opens for player 3 once players 2 and 3 share a team, and follows a capture by player 4.
 * Also checks path planning per player through the closed doorway and weapon traces at the closed leaf. Logs
 * RTS_WALLGATE lines, writes summary.md and quits with RTS_WALLGATE RESULT PASS|FAIL.
 * Map tests only: the body compiles to nothing without RTS_WITH_MAP_TESTS (every shipping build).
 */
UCLASS()
class RTS_SURVIVAL_API UWallGateRelationsTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

#if RTS_WITH_MAP_TESTS

private:
	void ReadCommandLine();
	void TickStartGame(double RealSeconds);
	bool TryStartGame();
	void KeepGameRunning() const;
	void TickStep();

	// ---- Setup ----
	void SpawnGateAndTanks();
	bool TryReadDoorway();
	float GetFrameHalfExtentAlongWall(float FallbackHalfExtentCm) const;
	void SpawnParkedTank(uint8 Player, float ParkYawDegrees);
	/** Units the map placed near the doorway would open (or hold open) the gate on their own. */
	bool GetIsAreaClearOfOtherUnits(FString& OutBlockingUnit) const;
	void TickWaitReady(TSubclassOf<UNavArea> ExpectedClosedArea, EWallGateRelationsTestStep NextStep);
	/**
	 * @brief Whether a doorway's navmesh polygon already carries the area its gate set (tile rebuilds are async).
	 * @param ExpectedClosedArea Closed-gate area of the gate's allowed player.
	 * @param DoorwayGroundCentre Doorway centre on the ground.
	 * @param OutDetail Area ids found, or why the check could not be made.
	 * @return True when no navigation build runs and the polygon has the expected area.
	 */
	bool GetIsDoorwayNavReady(TSubclassOf<UNavArea> ExpectedClosedArea, const FVector& DoorwayGroundCentre,
	                          FString& OutDetail) const;
	/** A gate spawned for player 3 with the default AllowedPlayer must open for player 3, not player 2. */
	void SpawnSecondGateForPlayer3(UClass* GateClass);
	void CheckSecondGate();

	// ---- Checks ----
	void RunFreeForAllChecks();
	void RunTeamChecks();
	void StartCapture();
	void RunCaptureChecks();
	/**
	 * @brief A weapon trace of the player (its channel and ignore mask) from in front of the closed leaf at it.
	 * @param Player Shooting player.
	 * @param bExpectHit Whether the relations say the shot must stop on the gate.
	 * @param Stage Name of the stage, for the report.
	 */
	void CheckShotOfPlayer(uint8 Player, bool bExpectHit, const TCHAR* Stage);
	bool GetDoesShotOfPlayerHitGate(uint8 Player) const;
	/**
	 * @brief Plans a path for the player's tank from 8 m in front of the doorway to 8 m behind it, with the RTS
	 * infantry filter (the narrowest agent, so the doorway is wide enough) and the tank as querier.
	 * @param Player Player whose tank asks for the path.
	 * @param bExpectThrough Whether the relations say the closed doorway is open to this player's planning.
	 * @param Stage Name of the stage, for the report.
	 */
	void CheckPathOfPlayer(uint8 Player, bool bExpectThrough, const TCHAR* Stage);
	EWallGatePathOutcome FindPathOutcome(uint8 Player, FString& OutDetail) const;
	EWallGatePathOutcome ClassifyPath(const TArray<FVector>& PathPoints, FString& OutDetail) const;
	void RecordCheck(const FString& CheckName, bool bPassed, const FString& Detail);

	// ---- Approaches ----
	void TeleportTankTo(uint8 Player, const FVector& GroundLocation) const;
	FVector GetApproachLocation() const;
	void EnterStep(EWallGateRelationsTestStep NewStep);
	/**
	 * @brief One tank waits in front of the doorway: the gate must start opening in time, or must stay shut.
	 * @param Player Player whose tank stands at the doorway.
	 * @param bExpectOpen Whether the relations say the gate opens for that player.
	 * @param NextStep Step after the verdict; the tank goes back to its park unless the gate opened.
	 */
	void TickApproach(uint8 Player, bool bExpectOpen, EWallGateRelationsTestStep NextStep);
	/** The tank that opened the gate went back: wait for the gate to shut and the doorway area to come back. */
	void TickLeave(EWallGateRelationsTestStep NextStep);
	bool GetIsGateOpeningOrOpen() const;

	bool TryGetGroundZ(const FVector& Location, float& OutGroundZ) const;
	float GetGameSeconds() const;
	float GetStepSeconds() const;
	FString GetStepName() const;
	/**
	 * @brief Writes summary.md and ends the process with the RESULT line the runner looks for; every exit goes here.
	 * @param bPassed Verdict when no check failed; a failed check always fails the run.
	 * @param Reason Why the run stopped early; empty when it ran to the end.
	 */
	void Finish(bool bPassed, const FString& Reason);

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AWallGate> M_Gate;
	// Spawned for player 3 beside the main gate; only its allowed player and doorway area are checked.
	TWeakObjectPtr<AWallGate> M_SecondGate;
	FVector M_SecondGateDoorwayGround = FVector::ZeroVector;
	// Indexed by player; only players 1, 2 and 3 get a tank.
	TWeakObjectPtr<AActor> M_TankByPlayer[RTSPlayers::PlayerArraySize];
	FVector M_ParkByPlayer[RTSPlayers::PlayerArraySize];
	FWallGateRelationsTestDoorway M_Doorway;
	EWallGateRelationsTestStep M_Step = EWallGateRelationsTestStep::StartGame;
	TArray<FString> M_ReportLines;
	TArray<FString> M_FailedChecks;
	FVector M_ArenaCentre = FVector::ZeroVector;
	FString M_OutputDirectory;
	FString M_LastWaitDetail;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	bool bM_IsActive = false;

#endif
};
