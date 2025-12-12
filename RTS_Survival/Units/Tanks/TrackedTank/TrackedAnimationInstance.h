// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/ChassisAnimationBase/ChassisAnimInstance.h"
#include "TrackedAnimationInstance.generated.h"

class RTS_SURVIVAL_API ATrackedTankMaster;

UENUM(Blueprintable)
enum class ETrackAnimation : uint8
{
	TA_Forward,
	TA_Backward,
	TA_Stationary,
	TA_TurnLeft,
	TA_TurnRight,
	TA_StationaryTurnLeft,
	TA_StationaryTurnRight
};

static FString TrackAnimationToString(ETrackAnimation Animation)
{
	switch (Animation)
	{
	case ETrackAnimation::TA_Forward:
		return "Forward";
	case ETrackAnimation::TA_Backward:
		return "Backward";
	case ETrackAnimation::TA_Stationary:
		return "Stationary";
	case ETrackAnimation::TA_TurnLeft:
		return "TurnLeft";
	case ETrackAnimation::TA_TurnRight:
		return "TurnRight";
	case ETrackAnimation::TA_StationaryTurnLeft:
		return "StationaryTurnLeft";
	case ETrackAnimation::TA_StationaryTurnRight:
		return "StationaryTurnRight";
	default:
		return "Unknown";
	}
}

/**
 * An anim instance for tracked tanks, allows to propagate movement parameters from vehicleAI.
 * @note all trackedtank animbp instances need to inherit from this.
 */
UCLASS()
class RTS_SURVIVAL_API UTrackedAnimationInstance : public UChassisAnimInstance
{
	GENERATED_BODY()

public:
	UTrackedAnimationInstance();
	/**
	 * @brief Sets the movement parameters needed to keep the animation up to date with the vehicle movement.
	 * Also updates the track material.
	 * @param NewSpeed The set linear velocity of the tank mesh.
	 * @param NewRotationYaw The set angular velocity of the tank mesh.
	 * @param bIsMovingForward True if the tank is moving forward, false otherwise.
	 */
virtual void SetMovementParameters(
		const float NewSpeed,
		const float NewRotationYaw,
		const bool bIsMovingForward) override final;

	virtual void SetChassisAnimToIdle() override final;

virtual void SetChassisAnimToStationaryRotation(const bool bRotateToRight) override final;

protected:
	/**
	 * Initiates the animation constants.
	 * @param NewAnimationSpeedDivider Divides the speed (in cm / s) to obtain the playrate.
	 * @param TrackBaseMaterial The base material used for the track materials.
	 * @param LeftTrackMaterialIndex The index of the left track material in the mesh.
	 * @param RightTrackMaterialIndex The index of the right track material in the mesh.
	 * @param SpeedChangeToStationaryTurn The speed at which the tank will start to turn in place.
	 * IMPORTANT: This needs to be exactly equal to the speed used in the animation state machine.
	 */
	UFUNCTION(BlueprintCallable, Category="ReferenceCasts")
	void InitTrackedAnimationInstance(
		const float NewAnimationSpeedDivider,
		UMaterialInstance* TrackBaseMaterial,
		UNiagaraComponent* TrackParticleComponent,
		UNiagaraComponent* ExhaustParticleComponent,
		const int LeftTrackMaterialIndex,
		const int RightTrackMaterialIndex,
		const float PlayRateSlowForward,
		const float PlayRateFastForward,
		const float PlayRateSlowBackward,
		const float PlayRateFastBackward,
		const float MaxExhaustPlayrate,
		const float MinExhaustPlayrate,
		const float SpeedChangeToStationaryTurn = 30.f);


	UPROPERTY(BlueprintReadOnly, Category="Tracks")
	UMaterialInstanceDynamic* LeftTrackMaterial;

	UPROPERTY(BlueprintReadOnly, Category="Tracks")
	UMaterialInstanceDynamic* RightTrackMaterial;

	// Enum value that defines the current animation to play on the tracks.
	UPROPERTY(BlueprintReadOnly, Category="Tracks")
	ETrackAnimation CurrentTrackAnimation;

	virtual void NativeBeginPlay() override;
	
private:
	UPROPERTY()
	float M_SpeedChangeToStationaryTurn;

	UPROPERTY()
	float M_PlayRateSlowForward;

	UPROPERTY()
	float M_PlayRateFastForward;

	UPROPERTY()
	float M_PlayRateSlowBackward;

	UPROPERTY()
	float M_PlayRateFastBackward;

	ETrackAnimation GetTrackAnimationFromFlag(
		float& OutLeftTrackSpeed,
		float& OutRightTrackSpeed) const;
	
};
