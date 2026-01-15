// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"
#include "TeamWeaponController.generated.h"

class ATeamWeapon;
class UTeamWeaponMover;

UENUM(BlueprintType)
enum class ETeamWeaponState : uint8
{
	Uninitialized,
	Spawning,
	Ready_Packed,
	Packing,
	Moving,
	Deploying,
	Ready_Deployed,
	Abandoned,
};

USTRUCT()
struct FTeamWeaponCrewAssignment
{
	GENERATED_BODY()

	void Reset()
	{
		M_Operators.Reset();
		M_Guards.Reset();
		M_RequiredOperators = 0;
	}

	bool GetHasEnoughOperators() const
	{
		return M_RequiredOperators > 0 && M_Operators.Num() >= M_RequiredOperators;
	}

	int32 M_RequiredOperators = 0;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadUnit>> M_Operators;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadUnit>> M_Guards;
};

/**
 * @brief Squad controller that coordinates team weapon crews and delegates movement execution to the weapon mover.
 */
UCLASS()
class RTS_SURVIVAL_API ATeamWeaponController : public ASquadController, public ITurretOwner
{
	GENERATED_BODY()

public:
	ATeamWeaponController();

protected:
	virtual void BeginPlay() override;
	virtual void OnAllSquadUnitsLoaded() override;
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation) override;
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
	virtual void UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected, ERTSDeathType DeathType) override;
	virtual void OnSquadUnitCommandComplete(EAbilityID CompletedAbilityID) override;

private:
	void SpawnTeamWeapon();
	void AssignCrewToTeamWeapon();
	void HandleMoverArrived();
	void HandleMoverFailed(const FString& FailureReason);
	void SetTeamWeaponState(const ETeamWeaponState NewState);
	bool GetIsValidTeamWeapon() const;
	bool GetIsValidTeamWeaponMover() const;

	// ---- ITurretOwner ----
	virtual int GetOwningPlayer() override;
	virtual void OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret) override;
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnMountedWeaponTargetDestroyed(
		ACPPTurretsMaster* CallingTurret,
		UHullWeaponComponent* CallingHullWeapon,
		AActor* DestroyedActor,
		const bool bWasDestroyedByOwnWeapons) override;
	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual FRotator GetOwnerRotation() const override;
	// ---- End ITurretOwner ----

	UPROPERTY(EditDefaultsOnly, Category = "TeamWeapon")
	TSubclassOf<ATeamWeapon> TeamWeaponClass;

	UPROPERTY()
	TObjectPtr<ATeamWeapon> M_TeamWeapon;

	UPROPERTY()
	TWeakObjectPtr<UTeamWeaponMover> M_TeamWeaponMover;

	UPROPERTY()
	FTeamWeaponCrewAssignment M_CrewAssignment;

	UPROPERTY()
	ETeamWeaponState M_TeamWeaponState = ETeamWeaponState::Uninitialized;
};
