// AssaultTank.h
// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbededTurretInterface.h"
#include "AssaultTank.generated.h"

UCLASS()
class RTS_SURVIVAL_API AAssaultTank : public ATrackedTankMaster, public IEmbeddedTurretInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AAssaultTank(const FObjectInitializer& ObjectInitializer);

	virtual bool TurnBase_Implementation(const float Degrees) override;

	virtual float GetCurrentTurretAngle_Implementation() const override;
    virtual void SetTurretAngle_Implementation(const float NewAngle) override;
    virtual void UpdateTargetPitch_Implementation(const float NewPitch) override;
    virtual void PlaySingleFireAnimation_Implementation(int32 WeaponIndex) override;
    virtual void PlayBurstFireAnimation_Implementation(int32 WeaponIndex) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Dig-in unit interface overrides so we can lock/unlock hull rotation.
	virtual void OnStartDigIn() override;
	virtual void OnBreakCoverCompleted() override;

private:
	// When true, embedded assault turrets are not allowed to rotate the hull.
	bool bM_IsHullRotationLocked = false;

	void LockHullRotation();
	void UnlockHullRotation();
};
