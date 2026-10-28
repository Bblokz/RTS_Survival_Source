// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "UObject/Object.h"
#include "Animation/AnimInstance.h"
#include "ChassisAnimInstance.generated.h"

class UNiagaraComponent;
class UGroundSurfaceSubsystem;

// The ground look the track smoke shows now; it eases towards the look of the ground under the vehicle.
USTRUCT()
struct FChassisSmokeSurfaceState
{
	GENERATED_BODY()

	FLinearColor M_CurrentTint = FLinearColor::White;

	float M_CurrentDensity = 1.f;

	// False while the smoke is off: the next activation takes the ground's look at once instead of easing to it.
	bool bM_HasAppliedLook = false;
};

/**
 *
 */
UCLASS()
class RTS_SURVIVAL_API UChassisAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void SetMovementParameters(
		const float NewSpeed,
		const float NewRotationYaw,
		const bool bIsMovingForward);

	virtual void SetChassisAnimToStationaryRotation(const bool bRotateToRight);

	virtual void SetChassisAnimToIdle();

protected:

	virtual void NativeBeginPlay() override;
	virtual void BeginDestroy() override;
	
	UPROPERTY()
	bool bM_IsMovingForward;

	/** @brief Set on the tank driving update, the linear velocity of the tank mesh. */
	UPROPERTY(BlueprintReadOnly, Category="TankMesh")
	FVector Velocity;

	/** @brief Set on the tank driving update, lenght of the velocity vector. */
	UPROPERTY(BlueprintReadOnly, Category="TankMesh")
	float Speed;

	/** @brief Set on the tank driving update, the target angular velocity of the tank mesh. */
	UPROPERTY(BlueprintReadOnly, Category="TankMesh")
	float RotationYaw;

	/** @brief The speed at which the animations should play. */
	UPROPERTY(BlueprintReadOnly, Category="Animation")
	float PlayRate;

	/** @brief The value by which the speed is divided to obtain the animation speed. */
	UPROPERTY(BlueprintReadOnly, Category="Animation")
	float AnimationSpeedDivider;

	void SetupParticleVFX(
		UNiagaraComponent* ChassisSmokeComponent,
		UNiagaraComponent* ExhaustParticleComponent,
		float MaxExhaustPlayRate,
		float MinExhaustPlayRate);

private:
	UPROPERTY()
	UNiagaraComponent* M_TrackParticleComponent;

	UPROPERTY()
	UNiagaraComponent* M_ExhaustParticleComponent;

	UPROPERTY()
	float M_MaxExhaustPlayRate;

	UPROPERTY()
	float M_MinExhaustPlayRate;

	FTimerHandle M_VfxHandle;
	FTimerDelegate M_VfxDel;

	UFUNCTION()
	void UpdateTrackVFX();

	void DisableTrackSmoke();

	// Tints the track smoke and scales its amount by the ground under the vehicle (baked surface map).
	void UpdateTrackSmokeSurfaceLook();

	bool GetIsValidGroundSurfaceSubsystem() const;

	UPROPERTY()
	TWeakObjectPtr<UGroundSurfaceSubsystem> M_GroundSurfaceSubsystem;

	UPROPERTY()
	FChassisSmokeSurfaceState M_SmokeSurfaceState;
	
	
};
