// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EliteScavengersTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class ASquadUnit;
class ATankMaster;
class UGlobalAbility;
enum class EWeaponName : uint32;

UENUM()
enum class EEliteScavengersTestPhase : uint8
{
	WaitingForStart,
	WaitingForSpawnedSquad,
	// The Panzerfaust soldiers fire at a tank, then switch to their Mauser backup and back.
	BackupWeaponFiring,
	BackupWeaponSwitching,
	WaitingForParadrop,
	Finished
};

/**
 * @brief -EliteScavengersValidate: spawns one Squad_Ger_EliteScavengers through the async spawner and checks its
 * soldiers (5, 3 STG 44 + 2 Panzerfaust with a Mauser backup, health = scavenger health x 1.33, subtype, counted as
 * anti-tank by the AI), lets the Panzerfaust soldiers fire at a held-fire enemy T-34 and switches them to the Mauser and
 * back to the Panzerfaust, then fires the GA_GerEliteScavengersParadrop template (500 radixite) on the anchor (where the camera looks, or -EliteScavAnchorX= and
 * -EliteScavAnchorY=) and checks both dropped squads land complete. Screenshots into Saved/EliteScavengersTest.
 * Logs RTS_ELITESCAV_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UEliteScavengersTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EEliteScavengersTestPhase M_Phase = EEliteScavengersTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_DirectSpawnLocation = FVector::ZeroVector;
	bool bM_SawActiveDrop = false;
	bool bM_TookDescentShot = false;
	float M_FirstSquadSeenSeconds = -1.f;
	float M_DropFinishedSeconds = -1.f;

	// Nothing else holds the running ability; its load callbacks and timers outlive the start of the drop.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SpawnedSquad;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> M_DroppedSquads;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	TWeakObjectPtr<ATankMaster> M_EnemyTank;
	// The spawned squad's Panzerfaust soldiers, followed through the weapon switches.
	TArray<TWeakObjectPtr<ASquadUnit>> M_FaustSoldiers;
	// 0: switch to the backup issued, 1: switch back issued.
	int32 M_SwitchStep = 0;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartDirectSpawn();
	void OnDirectSquadSpawned(AActor* SpawnedActor);
	void TickWaitingForSpawnedSquad();
	/** @brief Squad-level checks shared by the spawned and the dropped squads. */
	void CheckSquad(ASquadController* Squad, const FString& Label);
	void StartBackupWeaponTest();
	void TickBackupWeaponFiring();
	void TickBackupWeaponSwitching();
	/** The test is about the soldiers' weapons; a firing tank would kill them first. */
	void HoldEnemyTankFire() const;
	int32 CountFaustSoldiersThatFired() const;
	/**
	 * @brief Checks every followed Panzerfaust soldier carries the expected primary and backup weapon.
	 * @param CheckName Name of the check.
	 * @param ExpectedPrimary Weapon the soldier must hold.
	 * @param ExpectedSecondary Weapon the soldier must carry as backup.
	 */
	void CheckFaustSoldierWeapons(const FString& CheckName, EWeaponName ExpectedPrimary, EWeaponName ExpectedSecondary);
	void StartParadrop();
	void TickWaitingForParadrop();
	void FinishParadrop();

	void ViewFrom(const FVector& Focus, float Distance, float Pitch) const;
	void RequestScreenshot(const FString& Label);
};
