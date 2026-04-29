// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/SquadController.h"
#include "GuardController.generated.h"

class AGuardUnit;

USTRUCT()
struct FGuardUnitRuntimeState
{
	GENERATED_BODY()

	void Reset()
	{
		M_GuardUnit = nullptr;
		M_SpawnPoint = FVector::ZeroVector;
		M_GuardPoint = FVector::ZeroVector;
		bM_IsMovingTowardsGuardPoint = false;
		M_ConsecutiveMoveFailures = 0;
		M_MoveCompletedHandle.Reset();
	}

	UPROPERTY()
	TWeakObjectPtr<AGuardUnit> M_GuardUnit = nullptr;

	UPROPERTY()
	FVector M_SpawnPoint = FVector::ZeroVector;

	UPROPERTY()
	FVector M_GuardPoint = FVector::ZeroVector;

	bool bM_IsMovingTowardsGuardPoint = false;
	int32 M_ConsecutiveMoveFailures = 0;
	FDelegateHandle M_MoveCompletedHandle;
};

/**
 * @brief Squad controller that spawns guard units onto authored world-space points and keeps them strafing between paired guard locations.
 * Uses normal squad command logic when external orders are issued, but starts autonomous guarding as soon as squad data is ready.
 */
UCLASS()
class RTS_SURVIVAL_API AGuardController : public ASquadController
{
	GENERATED_BODY()

public:
	AGuardController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	TArray<FVector> M_SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	TArray<FVector> M_GuardPoints;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ExecuteAttackCommand(AActor* TargetActor) override;
	virtual void ExecuteAttackGroundCommand(const FVector GroundLocation) override;
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation) override;
	virtual void GeneralMoveToForAbility(const FVector& MoveToLocation, EAbilityID AbilityID) override;
	virtual void LoadSquadUnitsAsync() override;
	virtual void OnAllSquadUnitsLoaded() override;
	virtual void OnSquadUnitClassLoaded(TSoftClassPtr<ASquadUnit> LoadedClass) override;
	virtual void OnSquadUnitOutOfRange(const FVector& TargetLocation) override;
	virtual void SetUnitToIdleSpecificLogic() override;
	virtual void StopMovement() override;
	virtual void UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected, ERTSDeathType DeathType) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	float M_GuardPointEqualityToleranceCm = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	int32 M_MaxGuardMoveRetries = 3;

	bool bM_IsAutoGuardActive = false;
	bool bM_AutoGuardPermanentlyStopped = false;

	TMap<FSoftObjectPath, TSubclassOf<AGuardUnit>> M_LoadedGuardClassesByPath;
	TArray<FGuardUnitRuntimeState> M_GuardUnitRuntimeStates;
	TArray<FVector> M_RuntimeGuardPoints;

	void BuildRuntimeGuardPoints();
	void HandleGuardSquadDataLoaded();
	void HandleGuardUnitMoveFinished(AGuardUnit* GuardUnit, bool bReachedDestination);
	void IssueGuardMoveForState(FGuardUnitRuntimeState& GuardState);
	bool TryAddGuardRuntimeState(AGuardUnit* GuardUnit, const FVector& SpawnPoint);
	bool TryGetClosestGuardPoint(const FVector& CurrentLocation, const FVector& ExcludedLocation, FVector& OutGuardPoint) const;
	TArray<TSubclassOf<AGuardUnit>> GetOrderedGuardClasses() const;
	void RemoveGuardDelegateBinding(FGuardUnitRuntimeState& GuardState);
	void RemoveGuardRuntimeStateForUnit(const ASquadUnit* GuardUnit);
	void SpawnLoadedGuardUnitsAtSpawnPoints();
	void StopAutoGuardingPermanently();
	void TryStartAutoGuarding();
};
