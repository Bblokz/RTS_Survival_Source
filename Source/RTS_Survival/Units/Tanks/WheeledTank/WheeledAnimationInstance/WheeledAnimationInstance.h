// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/ChassisAnimationBase/ChassisAnimInstance.h"
#include "WheeledAnimationInstance.generated.h"

UENUM(Blueprintable)
enum class EWheelAnimation : uint8
{
	WA_Forward,
	WA_Backward,
	WA_Stationary,
	WA_TurnLeft,
	WA_TurnRight,
	WA_StationaryTurnLeft,
	WA_StationaryTurnRight,
	WA_TurnLeftBackwards,
	WA_TurnRightBackwards
};

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UWheeledAnimationInstance : public UChassisAnimInstance
{
	GENERATED_BODY()

public:
	virtual void SetMovementParameters(
		const float NewSpeed,
		const float NewRotationYaw,
		const bool bIsMovingForward) override final;

	virtual void SetChassisAnimToStationaryRotation(const bool bRotateToRight) override final;

	virtual void SetChassisAnimToIdle() override final;

protected:

	virtual void NativeBeginPlay() override;
	
	// Enum value that defines the current animation to play on the tracks.
	UPROPERTY(BlueprintReadOnly, Category="Tracks")
	EWheelAnimation CurrentWheelAnimation;

	/**
	 * 
	 * @param ChassisSmokeComponent Main vfx component for movement smoke.
	 * @param ExhaustParticleComponent Simple vfx component for engine exhaust.
	 * @param MaxExhaustPlayrate Max Playrate for the exhaust particle system.
	 * @param MinExhaustPlayrate Min Playrate for the exhaust particle system.
	 * @param NewAnimationSpeedDivider Determines the playrate, decrease to speed up the animations.
	 * in cm/s; divides the current speed to obtain the playrate.
	 * @param SpeedChangeToStationaryTurn At what speed turning is considered stationary in cm/s 
	 * @param ConsiderRotationYawTurningThreshold At what rotation yaw a turning animation will be played. 
	 */
	UFUNCTION(BlueprintCallable, Category="ReferenceCasts")
	void InitWheeledAnimationInstance(
		UNiagaraComponent* ChassisSmokeComponent,
		UNiagaraComponent* ExhaustParticleComponent,
		const float MaxExhaustPlayrate,
        const float MinExhaustPlayrate,
		float NewAnimationSpeedDivider,
		float SpeedChangeToStationaryTurn = 30.f,
		float ConsiderRotationYawTurningThreshold = 15);

private:
	UPROPERTY()
	float M_SpeedChangeToStationaryTurn;

	UPROPERTY()
	float M_ConsiderRotationYawTurningThreshold;
};
