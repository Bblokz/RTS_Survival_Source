// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Environment/FrozenLake/FrozenLakeIceSubsystem.h"
#include "FrozenLakeIceTestSubsystem.generated.h"

class ACPPController;
class ANavigationData;
class ASceneCapture2D;
class ATankMaster;
class UTextureRenderTarget2D;

UENUM()
enum class EFrozenLakeIceTestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForNavigation,
	ComparingStateMask,
	WaitingForTanks,
	Observing,
	Done
};

/** One tank of the test and what the ice did to it. */
struct FFrozenLakeIceTestTank
{
	FString Label;
	TWeakObjectPtr<ATankMaster> Tank;
	ERTSNavAgents Agent = ERTSNavAgents::NONE;
	bool bShouldBreak = false;
	EFrozenLakeIceVehicleState LastState = EFrozenLakeIceVehicleState::Safe;
	double WarningSeconds = -1.0;
	double SinkingSeconds = -1.0;
	double SunkSeconds = -1.0;
};

/**
 * @brief -FrozenLakeIceTest on Ger_FrozenLake: puts a heavy tank on grey ice (it must break through and sink), a medium
 * tank on grey ice and a heavy on heavy ice (both must not), captures the sequence and checks the hole in the navmesh.
 * Runner: Source/Tools/FrozenLakeMap/run_ice_break_test.sh. Output: Saved/FrozenLakeIceTest/, log lines
 * "RTS_FL_ICE_TEST ...".
 */
UCLASS()
class RTS_SURVIVAL_API UFrozenLakeIceTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	// The game starts paused (difficulty picker, mission intros); the test has to tick to press start and unpause.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	UPROPERTY()
	TObjectPtr<ASceneCapture2D> M_Capture = nullptr;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_CaptureTarget = nullptr;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TArray<FFrozenLakeIceTestTank> M_Tanks;
	TArray<FColor> M_AuthoredPixels;
	TArray<FString> M_Failures;
	// Pictures still to take: (seconds after the test start, file name).
	TArray<TPair<double, FString>> M_PendingPictures;
	EFrozenLakeIceTestPhase M_Phase = EFrozenLakeIceTestPhase::Inactive;
	FString M_OutputDirectory;
	FVector M_GreySpot = FVector::ZeroVector;
	FVector M_GreySpotControl = FVector::ZeroVector;
	FVector M_HeavySpot = FVector::ZeroVector;
	double M_PhaseStartSeconds = 0.0;
	double M_ObserveStartSeconds = 0.0;
	double M_LastHeartbeatSeconds = 0.0;
	int32 M_FramesInPhase = 0;
	float M_PathBeforeLength = -1.f;
	bool bM_HasCheckedHolePath = false;
	bool bM_IsSuppressingErrorDialogs = false;

	UFrozenLakeIceSubsystem* GetIceSubsystem() const;
	void LogHeartbeat();
	void EnterPhase(EFrozenLakeIceTestPhase NewPhase);
	double GetSecondsInPhase() const;
	double GetObservedSeconds() const;
	void Finish();

	void TickWaitingForGameStart();
	void TickWaitingForNavigation();
	void TickComparingStateMask();
	void TickWaitingForTanks();
	void TickObserving();

	/**
	 * @brief Finds a spot on the ice where the heavy class is held out but the medium class is not (grey ice), or a
	 * spot that holds heavies (heavy ice), with that same ice all round it.
	 * @param bWantGreyIce True for grey ice, false for heavy ice.
	 * @param AwayFrom The spot must be at least MinSpotSeparation from this one (ignored when zero).
	 * @param OutSpot The spot at ice height.
	 * @return False when the lake has no such spot.
	 */
	bool FindIceSpot(bool bWantGreyIce, const FVector& AwayFrom, FVector& OutSpot) const;
	bool GetIsSpotOfKind(const UFrozenLakeIceSubsystem& Ice, const FVector& Spot, bool bWantGreyIce) const;
	bool FindSpots();
	void SpawnTanks();
	void SpawnTestTank(const FString& Label, const TCHAR* ClassPath, const FVector& Spot, bool bShouldBreak);

	void OnVehicleStateChanged(ATankMaster* Tank, EFrozenLakeIceVehicleState NewState);
	void QueuePicture(double AfterSeconds, const FString& FileName);
	void TakeDuePictures();
	void SavePicture(const FString& FileName) const;
	float CompareWithAuthoredPixels() const;

	/**
	 * @brief Path for the medium-tank navmesh straight across the heavy tank's spot, to see the hole in the navmesh.
	 * @param OutClosestToSpot Closest distance of the path to the spot, in cm.
	 * @return Path length in cm; negative when there is no full path.
	 */
	float MeasurePathAcrossSpot(float& OutClosestToSpot) const;
	ANavigationData* FindNavDataForAgent(ERTSNavAgents Agent) const;
	// Logs, per agent, whether a path leads from heavy ice into the grey ice in game (runtime nav, not the editor's).
	void LogPathsAcrossGreyIce() const;
	void CheckHolePath();
	void CheckResults();
};
