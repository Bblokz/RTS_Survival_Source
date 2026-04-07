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

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Testing")
	void SetImpulseTorqueMultipliers(const float NewImpulseMultiplier, const float NewTorqueMultiplier);

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

	float M_TurnRate;
	float M_InclineAngle = 0.0f;
	float M_MeshTraceZOffset;

	float ImpulseMultiplier = 1.0f;
	float TorqueMultiplier = 1.0f;

	// The skeletal mesh of this tank.
	// Exists on both the tank as well as the physics track movement component for quick access.
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_TankMesh;

	// Exists both on the tank as well as the physics track movement component for quick accesss.
	UPROPERTY()
	TObjectPtr<UChassisAnimInstance> M_TankAnimationBP;


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

	float LastNoneZeroThrottle = -1;
};
