// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TankDigInTestSubsystem.generated.h"

class ACPPController;
class ATrackedTankMaster;

enum class ETankDigInTestStep : uint8
{
	WaitingForGameStart,
	SettlingBeforeDigIn,
	WaitingForDigIn,
	Ramming,
	ClearingRam,
	SettlingAttackTargets,
	WatchingAttack,
	WaitingForBreakCover,
	DrivingOff,
	Done
};

/** Where a rammed tank stood and how far it was moved off that spot. */
struct FTankDigInRamTarget
{
	TWeakObjectPtr<ATrackedTankMaster> Tank;
	TWeakObjectPtr<ATrackedTankMaster> Pusher;
	FVector StartLocation = FVector::ZeroVector;
	float StartYaw = 0.f;
	FVector PusherStartLocation = FVector::ZeroVector;
	float MaxShoveCm = 0.f;
	float MaxYawChangeDegrees = 0.f;
};

/**
 * @brief Map test for dug-in tanks, started with -TankDigInValidate (spot: -TankDigInAt=X,Y or the player start).
 * Digs a tank in, rams it and a parked control tank with enemy hulls at full throttle, orders it onto an enemy out
 * of range with a closer one in range, then breaks cover and drives off. Logs RTS_TANKDIG lines and quits.
 */
UCLASS()
class RTS_SURVIVAL_API UTankDigInTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the test has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	bool bM_IsActive = false;
	bool bM_IsFinished = false;
	bool bM_IsSuppressingErrorDialogs = false;
	ETankDigInTestStep M_Step = ETankDigInTestStep::WaitingForGameStart;
	double M_StartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	FVector M_Centre = FVector::ZeroVector;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ATrackedTankMaster> M_DugInTank;

	// The dug-in tank and a parked one that is not dug in; the second shows the ram is hard enough to shove a hull.
	FTankDigInRamTarget M_DugInRam;
	FTankDigInRamTarget M_ControlRam;

	TWeakObjectPtr<ATrackedTankMaster> M_FarEnemy;
	TWeakObjectPtr<ATrackedTankMaster> M_NearEnemy;
	FVector M_AttackStartLocation = FVector::ZeroVector;
	float M_AttackEndedAfterSeconds = -1.f;
	bool bM_DidDriveForTarget = false;
	bool bM_DidEngageNearEnemy = false;
	FVector M_DriveOffGoal = FVector::ZeroVector;

	float GetGameSeconds() const;
	float GetSecondsInStep() const;
	void EnterStep(ETankDigInTestStep NewStep);
	void Check(bool bCondition, const TCHAR* Description, const FString& Details);
	void Finish(const TCHAR* Reason);

	ATrackedTankMaster* SpawnTank(const TCHAR* ClassPath, const FVector& Location, float Yaw, uint8 OwningPlayer,
	                              const TCHAR* Label);
	// Keeps a test enemy from fighting back and in sight of the player, so it can be ordered onto.
	void HoldEnemyQuiet(ATrackedTankMaster* Enemy, bool bRevealToPlayer) const;

	void TickWaitingForGameStart(double RealSeconds);
	void TickSettlingBeforeDigIn();
	void TickWaitingForDigIn();
	void StartRam();
	void TickRamming(float DeltaTime);
	void TickRamTarget(FTankDigInRamTarget& RamTarget, float DeltaTime) const;
	void FinishRam();
	void TickClearingRam();
	void TickSettlingAttackTargets();
	void TickWatchingAttack();
	void TickWaitingForBreakCover();
	void TickDrivingOff();
};
