// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncTickActorComponent.h"
#include "Components/ActorComponent.h"
#include "TrackPhysicsMovement.generated.h"


class RTS_SURVIVAL_API UChassisAnimInstance;
namespace Chaos
{
	class FRigidBodyHandle_Internal;
}

USTRUCT(BlueprintType)
struct FTrackAsyncForceMotorSettings
{
	GENERATED_BODY()

	/** @brief Scales speed error into target longitudinal acceleration so throttle intent reaches target speed smoothly.
	 * Example: if throttle response feels lazy, increase by +0.5; if it oscillates around target speed, decrease by -0.5.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_SpeedErrorToAccelerationGain = 4.0f;

	/** @brief Hard cap for requested forward/reverse acceleration in cm/s² to prevent aggressive force spikes.
	 * Example: if uphill starts fail, raise by +200; if launches look arcade-like, lower by -200.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_MaxLongitudinalAcceleration = 2500.0f;

	/** @brief Converts estimated normal load into available drive traction; lower values reduce hill-climb authority and wheel-slip risk.
	 * Example: if tracks spin too much on slopes, lower by -0.1; if vehicle cannot hold grade, increase by +0.1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_LongitudinalTractionCoefficient = 1.4f;

	/** @brief Damps body-right slip velocity to reduce ice-skating while still allowing controlled lateral compliance on uneven terrain.
	 * Example: if sideways skating appears, increase by +1.0; if turns feel too sticky, decrease by -1.0.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_LateralSlipDamping = 8.0f;

	/** @brief Fraction of longitudinal traction budget reserved for lateral slip correction to avoid sideways over-constraining.
	 * Example: if lateral correction is weak on side slopes, increase by +0.05; if jitter appears on rough ground, lower by -0.05.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_MaxLateralForceRatio = 0.9f;

	/** @brief Scales yaw-rate error into yaw angular acceleration so steering behaves like a servo instead of instant angular velocity override.
	 * Example: if steering reacts slowly, increase by +0.5; if it wobbles after turn-in, decrease by -0.5.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_YawRateErrorToAccelerationGain = 7.0f;

	/** @brief Absolute limit for yaw angular acceleration in rad/s² to keep turn response stable during abrupt steering changes.
	 * Example: if pivot turns stall, increase by +0.3; if snap-turns look unrealistic, decrease by -0.3.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_MaxYawAcceleration = 4.0f;

	/** @brief Multiplier applied to inertia-derived yaw moment estimate to tune rotational heaviness without changing vehicle mass setup.
	 * Example: if hull feels too sluggish in yaw, lower by -0.05; if it rotates too freely, increase by +0.05.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_YawInertiaScale = 0.35f;

	/** @brief Fraction of traction budget allowed for yaw torque output, preventing turn-in from overpowering contact constraints.
	 * Example: if steering cannot bite under load, increase by +0.05; if terrain chatter appears while turning, decrease by -0.05.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_MaxYawTorqueRatio = 0.75f;

	/** @brief Ground-trace confidence threshold where full force authority is restored; below this value output is conservatively blended down.
	 * Example: if force drops too often on uneven ground, lower by -0.05; if bad traces still cause jitter, increase by +0.05.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor")
	float M_MinGroundConfidenceForFullAuthority = 0.65f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTrackPhysicsMovement : public UAsyncTickActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTrackPhysicsMovement();

	inline float GetInclineAngle() const { return M_InclineAngle; }

	/**
	 * @brief Required to setup this movement component.
	 * @param TankMesh The mesh part with the tracks and wheels upon which velocities will be applied.
	 * @param NewTrackForceMultiplier Is multiplied with the throttle default 2500
	 * @param NewTurnRate Max angular velocity in degrees/second
	 * @param NewTankAnimBp The animation Bp that controls the tracks.
	 * @param TankMeshZOffset possible positive offset used when ray tracing the ground to determine the
	 * landscape incline.
	 */
	UFUNCTION(BlueprintCallable)
	void InitTrackPhysicsMovement(
		USkeletalMeshComponent* TankMesh,
		const float NewTrackForceMultiplier,
		const float NewTurnRate,
		UChassisAnimInstance* NewTankAnimBp,
		const float TankMeshZOffset);

	/**
	 * Moves the tank using physics and line traces the landscape to adjust the forward vector used.
	 * Overrides physics linear velocity with calculated speed and custom gravity.
	 * 
	 * @param DeltaSeconds Time between previous two frames; used to lerp the change in speed.
	 * @param CurrentSpeed The current speed of the tank.
	 * @param Throttle The calculated throttle value.
	 * @param Steering The calculated steering value.
	 * @note Adjust M_TrackForceMultiplier to adjust the ration of speed/calculated_throttle for the tank.
	 */
	void UpdateTankMovement(
		const float DeltaSeconds,
		float CurrentSpeed,
		const float Throttle,
		const float Steering);

	void OnPathFollowingFinished();


	inline void UpdateTurnRate(const float NewTurnRate) { M_TurnRate = NewTurnRate; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Testing")
	void SetImpulseTorqueMultipliers(const float NewImpulseMultiplier, const float NewTorqueMultiplier);

	/** @return The last throttle value that was not zero when navigating the previously followed path. */
	inline float GetLastNoneZeroThrottle() const { return LastNoneZeroThrottle; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void AsyncTick(float DeltaTime) override;

private:
	bool GetIsValidTankMesh() const;
	bool GetIsValidTankAnimationBP() const;

	void ApplyForceMotorForPathFollowing(
		const float CurrentThrottle,
		const float CurrentSteeringInDeg,
		const Chaos::FRigidBodyHandle_Internal* RigidBody);

	float M_TurnRate;
	float M_InclineAngle = 0.0f;
	float M_MeshTraceZOffset;

	float ImpulseMultiplier = 1.0f;
	float TorqueMultiplier = 1.0f;

	// The skeletal mesh of this tank.
	// Exists on both the tank as well as the physics track movement component for quick access.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_TankMesh = nullptr;

	// Exists both on the tank as well as the physics track movement component for quick accesss.
	UPROPERTY()
	TObjectPtr<UChassisAnimInstance> M_TankAnimationBP = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|ForceMotor", meta=(AllowPrivateAccess=true))
	FTrackAsyncForceMotorSettings M_ForceMotorSettings;

	/**
	 * Trace the landscape to find the incline angle.
	 * @param OutGroundNormal The normal vector averaged over two points on the landscape.
	 * @param OutIncline The calculated incline angle of the landscape
	 * @param OutGroundConfidence Confidence [0-1] used to blend force authority on unreliable traces.
	 * @param StartLocation
	 * @return True if a successful trace was performed.
	 */
	bool PerformGroundTrace(
		FVector& OutGroundNormal,
		float& OutIncline,
		float& OutGroundConfidence,
		const FVector& StartLocation) const;

	TAtomic<float> M_CurrentThrottle{0.0f};
	TAtomic<float> M_TrackForceMultiplier;
	TAtomic<float> M_CurrentSteeringInDeg{0.0f};
	TAtomic<bool> M_IsFollowingPath{false};

	float LastNoneZeroThrottle = -1;
};
