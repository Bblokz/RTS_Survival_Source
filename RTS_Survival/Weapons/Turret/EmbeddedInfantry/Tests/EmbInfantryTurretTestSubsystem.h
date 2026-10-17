// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"

#include "EmbInfantryTurretTestSubsystem.generated.h"

class AEmbeddedInfantryTurret;

UENUM()
enum class EEmbInfantryTurretTestPhase : uint8
{
	WaitingForStart,
	// The host tanks are out; their mount components spawn the infantry turrets a tick later.
	WaitingForMounts,
	// An enemy tank stands in range; every infantry turret must aim at it and fire.
	Engaging,
	// Each host gets an attack-ground order behind it; its gunner must turn away from the enemy and fire there.
	AttackingGround,
	// An enemy host far outside the player's vision: fog of war must hide its gunner with it.
	CheckingFogOfWar,
	Finished
};

/** One host tank and the infantry turret its mount component must have put in its cupola. */
USTRUCT()
struct FEmbInfantryTurretUnderTest
{
	GENERATED_BODY()

	FString Label;
	FString TankClassPath;
	FString ExpectedTurretClassName;

	UPROPERTY()
	TWeakObjectPtr<AActor> Tank;

	UPROPERTY()
	TWeakObjectPtr<AEmbeddedInfantryTurret> Turret;

	// Magazine of every weapon at the last count; a drop is a shot.
	TArray<int32> LastMagazineCounts;
	int32 Shots = 0;
	bool bHasTargetedEnemy = false;
	bool bSawFireMontage = false;
	// Pitch the soldier's pose reached and the pitch the turret asked for.
	float MaxAbsAimPitch = 0.f;
	float MaxAbsTargetPitch = 0.f;
	float SmallestYawErrorToEnemy = 360.f;

	// Attack-ground phase.
	int32 ShotsBeforeGroundOrder = 0;
	float SmallestYawErrorToGround = 360.f;
};

/**
 * @brief Map test for infantry turrets mounted in tank cupolas, started with -EmbInfantryTurretValidate on a map with
 * open ground (RTS_ToCopySmall with -EITAt=16796,-15808): spawns the T-34E and KV-1 Soviet, checks where and how
 * their gunners are mounted (socket, no collision, navigation or decals) and that they aim at and fire on an enemy
 * tank. -EITCapture=<folder> saves scene-capture pictures. Logs RTS_EIT_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UEmbInfantryTurretTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EEmbInfantryTurretTestPhase M_Phase = EEmbInfantryTurretTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	float M_NextStatusSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	int32 M_CaptureCount = 0;

	UPROPERTY()
	TArray<FEmbInfantryTurretUnderTest> M_Units;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Enemy;

	FVector M_GroundTarget = FVector::ZeroVector;

	// Enemy-owned host far from every player unit, for the fog-of-war check.
	UPROPERTY()
	TWeakObjectPtr<AActor> M_HiddenEnemyHost;

	FB4HowitzerCaptureRig M_CaptureRig;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterPhase(EEmbInfantryTurretTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	void TickWaitingForMounts();
	void TickEngaging();
	void StartAttackGround();
	void TickAttackingGround();
	void StartFogOfWarCheck();
	void TickCheckingFogOfWar();

	bool GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const;
	AActor* SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform, uint8 OwningPlayer) const;

	void CheckMountedTurret(FEmbInfantryTurretUnderTest& Unit);
	void CheckTurretHasNoCollisionNavigationOrDecals(const FEmbInfantryTurretUnderTest& Unit);
	void SpawnEnemyInRange();
	void CountShotsAndAim(FEmbInfantryTurretUnderTest& Unit);
	void KeepUnitsAlive() const;
	void CaptureUnits(const FString& Moment);
};
