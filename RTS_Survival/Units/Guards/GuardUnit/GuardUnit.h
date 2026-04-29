// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "GuardUnit.generated.h"

class UGuardCharacterMovementComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGuardUnitMoveFinished, class AGuardUnit*, bool);

/**
 * @brief Infantry squad unit variant used by guard controllers for autonomous wall-following guard movement.
 * Retains the existing squad-unit weapon and animation setup while swapping movement during guard strafes.
 */
UCLASS()
class RTS_SURVIVAL_API AGuardUnit : public ASquadUnit
{
	GENERATED_BODY()

public:
	AGuardUnit(const FObjectInitializer& ObjectInitializer);

	bool StartGuardMove(const FVector& TargetLocation);
	void StopGuardMove();
	void SetAutoGuardingEnabled(const bool bEnableAutoGuarding);
	bool GetIsGuardMoveActive() const;
	bool GetIsAutoGuardingEnabled() const { return bM_IsAutoGuardingEnabled; }

	FOnGuardUnitMoveFinished OnGuardMoveFinished;

	virtual void OnSpecificTargetOutOfRange(const FVector& TargetLocation) override;
	virtual void OnSpecificTargetDestroyedOrInvisible() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void StrafeToLocation(const FVector& StrafeLocation) override;
	virtual void TerminateMovementCommand() override;

private:
	UPROPERTY()
	TObjectPtr<UGuardCharacterMovementComponent> M_GuardMovementComponent;

	bool bM_IsAutoGuardingEnabled = false;
	FDelegateHandle M_GuardMoveFinishedHandle;

	bool GetIsValidGuardMovementComponent() const;
	void HandleGuardMoveFinished(const bool bReachedDestination);
};
