// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncTickActorComponent.h"
#include "Components/ActorComponent.h"
#include "TrackPhysicsMovement.generated.h"


class RTS_SURVIVAL_API UChassisAnimInstance;
struct FHitResult;
namespace Chaos
{
	class FRigidBodyHandle_Internal;
}

/**
 * @brief Per-vehicle Strategy-B tuning values exposed on the movement component for Blueprint overrides.
 * Keep these values aligned with observed handling symptoms to make balancing predictable.
 */
USTRUCT(BlueprintType)
struct FTrackPhysicsMovementTuning
{
	GENERATED_BODY()

	/**
	 * @brief Raise when throttle response feels sluggish; lower when acceleration overshoots target speed.
	 *
	 * @note Used as the interpolation speed in FMath::FInterpTo when blending from current forward speed to target speed.
	 * @note Higher values reduce command latency, while lower values preserve more inertia and smoother response.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float DesiredSpeedInterpRate = 40.0f;

	/**
	 * @brief Raise when steady-state speed error remains; lower when force corrections oscillate on rough terrain.
	 *
	 * @note Multiplies planar velocity error to compute correction acceleration before clamping.
	 * @note Controls how aggressively force correction closes the gap to desired ground-aligned velocity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float VelocityCorrectionGain = 10.0f;

	/**
	 * @brief Raise for heavier vehicles that cannot climb; lower when vehicles surge or wheel-spin on command changes.
	 *
	 * @note Caps the magnitude of velocity correction acceleration before force is applied to the rigid body.
	 * @note Prevents correction spikes from overpowering traction and keeps tuning stable across different masses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float MaxCorrectionAcceleration = 2500.0f;

	/**
	 * @brief Raise when steering lags behind commanded yaw; lower when heading oscillates during corner entry.
	 *
	 * @note Multiplies yaw-rate error when computing desired yaw angular acceleration.
	 * @note Sets how strongly torque reacts to steering input mismatch against measured angular velocity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float YawRateCorrectionGain = 3.0f;

	/**
	 * @brief Raise when turning authority is too weak; lower if fast steering causes tipping or jitter on slopes.
	 *
	 * @note Clamps positive and negative yaw angular acceleration after yaw-rate gain is applied.
	 * @note Defines the maximum steering torque authority available regardless of instantaneous error size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float MaxYawAngularAcceleration = 20.0f;

	/**
	 * @brief Raise when the vehicle side-slips or “ice skates”; lower if the chassis feels glued and cannot drift naturally.
	 *
	 * @note Scales damping acceleration derived from planar right-direction lateral speed.
	 * @note Balances side-slip rejection versus allowing natural drift during high-load turning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float LateralDampingFactor = 2.0f;

	/**
	 * @brief Increase for longer vehicles to sample terrain further ahead; decrease if crest transitions become delayed.
	 *
	 * @note Sets symmetric forward and backward trace offsets around the chassis trace origin.
	 * @note Determines how much upcoming slope information contributes to the averaged ground normal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float GroundTraceForwardDistance = 200.0f;

	/**
	 * @brief Increase when traces lose contact on sharp dips; decrease if traces incorrectly latch onto lower geometry.
	 *
	 * @note Defines downward ray length for both ground probes used to validate terrain contact.
	 * @note Affects whether the movement step can resolve a reliable ground plane for force projection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float GroundTraceDownDistance = 300.0f;

	/**
	 * @brief Lower bound to avoid zero-inertia torque calculations on malformed or unloaded body data.
	 *
	 * @note Applied as a minimum fallback when reading rigid-body yaw inertia from the body inertia tensor.
	 * @note Prevents degenerate torque scaling that would otherwise flatten steering response or produce instability.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TrackPhysics|StrategyB")
	float MinimumYawInertia = 1.0f;
};

/**
 * @brief Receives throttle/steering commands from path following and applies them on async physics tick.
 * Uses a bounded force and torque correction model so Chaos gravity and terrain contact stay visible.
 */
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
	 * Moves the tank using bounded force and torque corrections aligned to the traced ground plane.
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

	/** @return The last throttle value that was not zero when navigating the previously followed path. */
	inline float GetLastNoneZeroThrottle() const { return LastNoneZeroThrottle; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void AsyncTick(float DeltaTime) override;

private:
	/**
	 * @brief Builds the ground-aligned velocity target used by Strategy B so gravity can remain fully simulated.
	 * @param RigidBody Async rigid body handle used to read pose and velocity.
	 * @param DeltaTime Async simulation delta time used for speed blending.
	 * @param OutDesiredPlanarVelocity Velocity target projected onto the current ground plane.
	 * @param OutGroundNormal Ground normal derived from validated terrain traces.
	 * @return True when a reliable ground sample was found and a planar target can be used.
	 */
	bool GetDesiredPlanarVelocityStrategyB(
		const Chaos::FRigidBodyHandle_Internal* RigidBody,
		const float DeltaTime,
		FVector& OutDesiredPlanarVelocity,
		FVector& OutGroundNormal) const;

	/**
	 * @brief Applies bounded longitudinal correction and lateral damping so steering stays controlled without skating.
	 * @param RigidBody Async rigid body handle used to read current velocity.
	 * @param DesiredPlanarVelocity Ground-aligned velocity intent generated from throttle.
	 * @param GroundNormal Ground normal used to keep damping parallel to terrain.
	 */
	void ApplyDriveForceStrategyB(
		const Chaos::FRigidBodyHandle_Internal* RigidBody,
		const FVector& DesiredPlanarVelocity,
		const FVector& GroundNormal) const;

	/**
	 * @brief Applies bounded yaw torque from yaw-rate error to realize steering intent without direct angular overrides.
	 * @param RigidBody Async rigid body handle used to read current angular velocity and orientation.
	 */
	void ApplyYawTorqueStrategyB(const Chaos::FRigidBodyHandle_Internal* RigidBody) const;

	bool GetIsValidTankMesh() const;
	bool GetIsValidTankAnimationBP() const;
	bool GetIsValidMovementDependencies() const;
	void BeginPlay_InitRuntimeTrackPhysicsTuningSnapshot();

	/**
	 * @brief Stores the latest reliable terrain sample so brief trace gaps can reuse a stable ground frame.
	 * @param GroundNormal Normal used to project movement and damping along terrain.
	 * @param Incline Incline angle associated with the sampled terrain normal.
	 */
	void CacheGroundTraceSample(const FVector& GroundNormal, const float Incline) const;

	/**
	 * @brief Reuses the latest valid terrain sample for a short failure window to bridge narrow contact gaps.
	 * @param OutGroundNormal Cached normal returned when fallback is still valid.
	 * @param OutIncline Cached incline returned when fallback is still valid.
	 * @return True when fallback could provide a valid sample for this frame.
	 */
	bool TryUseCachedGroundTraceSample(FVector& OutGroundNormal, float& OutIncline) const;

	/**
	 * @brief Resolves ground orientation when only one of the two terrain traces hits.
	 * @param bFrontTraceHit Whether the first trace hit terrain.
	 * @param FrontHit Hit data of the first trace.
	 * @param RearHit Hit data of the second trace.
	 * @param OutGroundNormal Blended normal using current hit and previous cached sample.
	 * @param OutIncline Incline reused from cached sample when available.
	 * @return True when a blended single-hit sample was generated.
	 */
	bool TryGetGroundTraceFromSingleHit(
		const bool bFrontTraceHit,
		const FHitResult& FrontHit,
		const FHitResult& RearHit,
		FVector& OutGroundNormal,
		float& OutIncline) const;

	float M_TurnRate;
	float M_InclineAngle = 0.0f;
	float M_MeshTraceZOffset;

	// The skeletal mesh of this tank.
	// Exists on both the tank as well as the physics track movement component for quick access.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_TankMesh;

	// Exists both on the tank as well as the physics track movement component for quick accesss.
	UPROPERTY()
	TObjectPtr<UChassisAnimInstance> M_TankAnimationBP;

	/** Strategy-B force and torque tuning values that can be adjusted per vehicle from Blueprint defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TrackPhysics|StrategyB", meta=(AllowPrivateAccess="true"))
	FTrackPhysicsMovementTuning M_TrackPhysicsMovementTuning;

	// Immutable runtime copy used by async physics thread to avoid reading mutable Blueprint-authored settings.
	FTrackPhysicsMovementTuning M_RuntimeTrackPhysicsMovementTuningSnapshot;

	/**
	 * Trace the landscape to find the incline angle.
	 * @param OutGroundNormal The normal vector averaged over two points on the landscape.
	 * @param OutIncline The calculated incline angle of the landscape
	 * @param StartLocation
	 * @return True if a successful trace was performed.
	 */
	bool PerformGroundTrace(FVector& OutGroundNormal, float& OutIncline, const FVector& StartLocation) const;

	TAtomic<float> M_CurrentThrottle{0.0f};
	TAtomic<float> M_TrackForceMultiplier;
	TAtomic<float> M_CurrentSteeringInDeg{0.0f};
	TAtomic<bool> M_IsFollowingPath{false};

	mutable TAtomic<float> M_CachedGroundNormalX{0.0f};
	mutable TAtomic<float> M_CachedGroundNormalY{0.0f};
	mutable TAtomic<float> M_CachedGroundNormalZ{1.0f};
	mutable TAtomic<float> M_CachedInclineAngle{0.0f};
	mutable TAtomic<int32> M_ConsecutiveGroundTraceFailureCount{0};
	mutable TAtomic<bool> bM_HasCachedGroundTraceSample{false};

	float LastNoneZeroThrottle = -1;
};
