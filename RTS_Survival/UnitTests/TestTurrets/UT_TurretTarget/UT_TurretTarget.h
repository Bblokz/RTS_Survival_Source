// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "UT_TurretTarget.generated.h"

class AUT_TurretManager;

/**
 * A test "Target" pawn which, when destroyed, tells the TurretManager so the manager can 
 * track that the turret has killed one of its associated targets.
 */
UCLASS()
class RTS_SURVIVAL_API AUT_TurretTarget : public AHpPawnMaster
{
	GENERATED_BODY()

public:
	AUT_TurretTarget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Called right after spawn so we can store a pointer to the manager 
	 * that spawned us.
	 */
	void Init(AUT_TurretManager* UtManager);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void UnitDies(const ERTSDeathType DeathType) override;
private:
	/** The manager of this unit test. */
	UPROPERTY()
	AUT_TurretManager* M_UtManager;
};
