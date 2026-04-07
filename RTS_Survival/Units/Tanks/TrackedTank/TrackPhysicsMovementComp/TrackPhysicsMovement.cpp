// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackPhysicsMovement.h"

#include "AsyncTickFunctions.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedAnimationInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "PhysicsEngine/BodyInstance.h"

namespace TrackPhysicsMovementConstants
{
	/**
	 * @brief Shared Strategy-B tuning constants for tracked movement force execution.
	 * Increase only the constant linked to the observed symptom so tuning stays predictable.
	 */

	/** @brief Raise when throttle response feels sluggish; lower when acceleration overshoots target speed. */
	constexpr float DesiredSpeedInterpRate = 40.0f;
	/** @brief Raise when steady-state speed error remains; lower when force corrections oscillate on rough terrain. */
	constexpr float VelocityCorrectionGain = 10.0f; // was 6
	/** @brief Raise for heavier vehicles that cannot climb; lower when vehicles surge or wheel-spin on command changes. */
	constexpr float MaxCorrectionAcceleration = 2500.0f;
	/** @brief Raise when steering lags behind commanded yaw; lower when heading oscillates during corner entry. */
	constexpr float YawRateCorrectionGain = 3.0f;
	/** @brief Raise when turning authority is too weak; lower if fast steering causes tipping or jitter on slopes. */
	constexpr float MaxYawAngularAcceleration = 20.0f; // was 8
	/** @brief Set to -1 only if steering input direction is empirically reversed for this vehicle rig. */
	constexpr float SteeringToYawDirectionSign = 1.0f;
	/** @brief Raise when the vehicle side-slips or “ice skates”; lower if the chassis feels glued and cannot drift naturally. */
	constexpr float LateralDampingFactor = 2.0f;
	/** @brief Increase for longer vehicles to sample terrain further ahead; decrease if crest transitions become delayed. */
	constexpr float GroundTraceForwardDistance = 200.0f;
	/** @brief Increase when traces lose contact on sharp dips; decrease if traces incorrectly latch onto lower geometry. */
	constexpr float GroundTraceDownDistance = 100.0f;
	/** @brief Raise to reject noisy normals on broken terrain; lower if movement drops out too often on uneven ground. */
	constexpr float GroundTraceConfidenceDotThreshold = 0.9f;
	/** @brief Lower bound to avoid zero-inertia torque calculations on malformed or unloaded body data. */
	constexpr float MinimumYawInertia = 1.0f;
}


// Sets default values for this component's properties
UTrackPhysicsMovement::UTrackPhysicsMovement()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetAsyncPhysicsTickEnabled(false);

	// ...
}

void UTrackPhysicsMovement::InitTrackPhysicsMovement(
	USkeletalMeshComponent* TankMesh,
	const float NewTrackForceMultiplier,
	const float NewTurnRate,
	UChassisAnimInstance* NewTankAnimBp,
	const float TankMeshZOffset)
{
	if (TankMesh)
	{
		M_TankMesh = TankMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this, "TankMesh", "UTrackPhysicsMovement::InitTrackPhysicsMovement");
	}

	M_TrackForceMultiplier.Store(NewTrackForceMultiplier);
	M_TurnRate = NewTurnRate;

	if (IsValid(NewTankAnimBp))
	{
		M_TankAnimationBP = NewTankAnimBp;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this, "TankAnimBp", "UTrackPhysicsMovement::InitTrackPhysicsMovement");
	}

	M_MeshTraceZOffset = TankMeshZOffset;
}


void UTrackPhysicsMovement::SetImpulseTorqueMultipliers(const float NewImpulseMultiplier,
                                                        const float NewTorqueMultiplier)
{
	ImpulseMultiplier = NewImpulseMultiplier;
	TorqueMultiplier = NewTorqueMultiplier;
}

// Called when the game starts
void UTrackPhysicsMovement::BeginPlay()
{
	Super::BeginPlay();
	SetActive(true);

	// ...
}

void UTrackPhysicsMovement::AsyncTick(float DeltaTime)
{
	Super::AsyncTick(DeltaTime);
	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_AsyncTick);
	if (not M_IsFollowingPath.Load())
	{
		return;
	}

	if (not GetIsValidTankMesh())
	{
		return;
	}

	const Chaos::FRigidBodyHandle_Internal* RigidBody = UAsyncTickFunctions::GetInternalHandle(M_TankMesh, NAME_None);
	if (not RigidBody)
	{
		return;
	}

	FVector DesiredPlanarVelocity;
	FVector GroundNormal;
	if (not GetDesiredPlanarVelocityStrategyB(RigidBody, DeltaTime, DesiredPlanarVelocity, GroundNormal))
	{
		return;
	}

	ApplyDriveForceStrategyB(RigidBody, DesiredPlanarVelocity, GroundNormal);
	ApplyYawTorqueStrategyB(RigidBody);
}

void UTrackPhysicsMovement::UpdateTankMovement(
	const float DeltaSeconds,
	float CurrentSpeed,
	const float Throttle,
	const float Steering)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_PhysicsMovement);
	if (not GetIsValidMovementDependencies())
	{
		const FName OwnerName = GetOwner() ? GetOwner()->GetFName() : NAME_None;
		RTSFunctionLibrary::PrintString(
			"Stopping physics movement on: " + OwnerName.ToString() + " due to invalid TankMesh or TankAnimBp."
			"\n Will disable async physics tick.");
		SetAsyncPhysicsTickEnabled(false);
		return;
	}

	const float DesiredYawRate = Steering * M_TurnRate;
	M_CurrentThrottle.Store(Throttle);
	M_CurrentSteeringInDeg.Store(DesiredYawRate);
	M_IsFollowingPath.Store(true);

	if (Throttle != 0)
	{
		LastNoneZeroThrottle = Throttle;
	}
	M_TankAnimationBP->SetMovementParameters(CurrentSpeed, DesiredYawRate, Throttle >= 0);
	// if constexpr (DeveloperSettings::Debugging::GVehicle_Track_Movement_Compile_DebugSymbols)
	// {
	// 	FString Debug = "Game Thread: "
	// 		"\n CurrentSpeed: " + FString::SanitizeFloat(CurrentSpeed) +
	// 		"\n Throttle: " + FString::SanitizeFloat(Throttle) +
	// 		"\n Steering: " + FString::SanitizeFloat(Steering);
	// 	const FVector DebugLocation = M_TankMesh->GetComponentLocation() + FVector(0, 0, 400);
	// 	DrawDebugString(GetWorld(), DebugLocation, Debug, nullptr, FColor::Red, DeltaSeconds, false);
	// }
}

void UTrackPhysicsMovement::OnPathFollowingFinished()
{
	M_IsFollowingPath.Store(false);
}

// 	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_PhysicsMovement);
// 	// // .5 ms for 100 vehicles.
// 	if (IsValid(M_TankMesh) && IsValid(M_TankAnimationBP))
// 	{
// 		if (FMath::IsNearlyZero(Throttle, KINDA_SMALL_NUMBER))
// 		{
// 			// Teleport rotation to simulate instant turning without applying physics forces
// 			const FRotator NewRotation = M_TankMesh->GetRelativeRotation() + FRotator(
// 				0, DesiredYawRate * DeltaSeconds, 0);
// 			M_TankMesh->SetRelativeRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
// 		}
// 		else
// 		{
// 			// Apply angular velocity normally
// 			M_TankMesh->SetPhysicsAngularVelocityInDegrees(FVector(0, 0, DesiredYawRate), false);
// 		}
// 		// Now that we've set the angular velocity, the tank's orientation will be updated when physics are next simulated.
// 		// Therefore, we should set the linear velocity based on the tank's new forward vector after turning.
// 		const FVector ForwardVector = M_TankMesh->GetForwardVector().RotateAngleAxis(
// 			DesiredYawRate * DeltaSeconds, FVector::UpVector);
// 	
// 		const float TargetSpeed = M_TrackForceMultiplier * Throttle;
// 		CurrentSpeed *= FMath::Sign(Throttle);
// 		// Make sure the transition from the current velocity to the new velocity vector is smooth.
// 		const float BlendedSpeed = FMath::Lerp(CurrentSpeed, TargetSpeed, DeltaSeconds);
// 	
// 		FVector GroundNormal;
// 		float Incline;
// 		if (PerformGroundTrace(GroundNormal, Incline))
// 		{
// 			const FVector DesiredVelocity = ForwardVector * BlendedSpeed;
// 			// Adjust velocity vector for landscape normal; ensures movement is strictly parallel to the landscapes' incline.
// 			const FVector CorrectedVelocity = FVector::VectorPlaneProject(DesiredVelocity, GroundNormal);
// 	
// 	
// 			M_TankMesh->SetPhysicsLinearVelocity(CorrectedVelocity, false);
// 			const FVector GravityForce = GroundNormal * M_TankMesh->GetMass() * -
// 				DeveloperSettings::GamePlay::Navigation::TankTrackGravityMultiplier;
// 			M_TankMesh->AddForce(GravityForce, NAME_None, true);
// 	
// 			M_TankAnimationBP->SetMovementParameters(CorrectedVelocity.Length(), DesiredYawRate, Throttle >= 0);
// 			M_InclineAngle = Incline;
// 	
// 			// if constexpr (DeveloperSettings::Debugging::GVehicle_Track_Movement_Compile_DebugSymbols)
// 			// {
// 			// 	RTSFunctionLibrary::PrintString("Incline: " + FString::SanitizeFloat(Incline));
// 			// 	RTSFunctionLibrary::PrintString("Throttle: " + FString::SanitizeFloat(Throttle) + " Steering: " + FString::SanitizeFloat(Steering));
// 			// }
// 		}
// 		return;
// 	}
// 	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
// 		this, "TankMesh or TankAnimBp", "UTrackPhysicsMovement::UpdateTankMovement", GetOwner());
// }


bool UTrackPhysicsMovement::PerformGroundTrace(FVector& OutGroundNormal, float& OutIncline,
                                               const FVector& StartLocation) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"World",
			"PerformGroundTrace",
			this);
		return false;
	}

	const FVector TraceOrigin = StartLocation + FVector(0, 0, M_MeshTraceZOffset);
	const FVector ForwardOffset = M_TankMesh->GetForwardVector() * TrackPhysicsMovementConstants::GroundTraceForwardDistance;

	const FVector Start = TraceOrigin - ForwardOffset;
	const FVector End = Start - FVector(0, 0, TrackPhysicsMovementConstants::GroundTraceDownDistance);
	FHitResult Hit;

	const FVector Start2 = TraceOrigin + ForwardOffset;
	const FVector End2 = Start2 - FVector(0, 0, TrackPhysicsMovementConstants::GroundTraceDownDistance);
	FHitResult Hit2;


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, Params);
	const bool bHit2 = World->LineTraceSingleByChannel(Hit2, Start2, End2, COLLISION_TRACE_LANDSCAPE, Params);

	if (not bHit || not bHit2)
	{
		return false;
	}

	if (FVector::DotProduct(Hit.Normal, Hit2.Normal) < TrackPhysicsMovementConstants::GroundTraceConfidenceDotThreshold)
	{
		return false;
	}

	// compute the normal of the landscape.
	OutGroundNormal = (Hit.Normal + Hit2.Normal) * 0.5;
	OutGroundNormal.Normalize();

	// Calculate the incline angle between the two points
	const FVector InclineVector = Hit2.ImpactPoint - Hit.ImpactPoint;
	const FVector HorizontalVector = FVector(InclineVector.X, InclineVector.Y, 0).GetSafeNormal();
	OutIncline = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(HorizontalVector, InclineVector.GetSafeNormal())));

	return true;
}

bool UTrackPhysicsMovement::GetDesiredPlanarVelocityStrategyB(
	const Chaos::FRigidBodyHandle_Internal* RigidBody,
	const float DeltaTime,
	FVector& OutDesiredPlanarVelocity,
	FVector& OutGroundNormal) const
{
	const FVector CurrentLinearVelocity = RigidBody->V();
	const FQuat CurrentRotation = RigidBody->R();
	const float CurrentThrottle = M_CurrentThrottle.Load();
	const float CurrentSignedForwardSpeed = FVector::DotProduct(CurrentLinearVelocity, CurrentRotation.GetForwardVector());
	const float TargetForwardSpeed = M_TrackForceMultiplier.Load() * CurrentThrottle;
	const float BlendedTargetSpeed = FMath::FInterpTo(
		CurrentSignedForwardSpeed,
		TargetForwardSpeed,
		DeltaTime,
		TrackPhysicsMovementConstants::DesiredSpeedInterpRate);

	float Incline;
	if (not PerformGroundTrace(OutGroundNormal, Incline, RigidBody->X()))
	{
		return false;
	}

	const FVector DesiredVelocity = CurrentRotation.GetForwardVector() * BlendedTargetSpeed;
	OutDesiredPlanarVelocity = FVector::VectorPlaneProject(DesiredVelocity, OutGroundNormal);
	return true;
}

void UTrackPhysicsMovement::ApplyDriveForceStrategyB(
	const Chaos::FRigidBodyHandle_Internal* RigidBody,
	const FVector& DesiredPlanarVelocity,
	const FVector& GroundNormal) const
{
	const FBodyInstance* TankBodyInstance = M_TankMesh->GetBodyInstance(NAME_None);
	if (not TankBodyInstance)
	{
		return;
	}

	FBodyInstance* MutableBodyInstance = M_TankMesh->GetBodyInstance(NAME_None);
	if (not MutableBodyInstance)
	{
		return;
	}

	const FVector CurrentLinearVelocity = RigidBody->V();
	const float TankMass = TankBodyInstance->GetBodyMass();
	const FVector VelocityError = DesiredPlanarVelocity - CurrentLinearVelocity;
	const FVector VelocityCorrectionAcceleration = VelocityError * TrackPhysicsMovementConstants::VelocityCorrectionGain;
	const FVector ClampedVelocityCorrectionAcceleration = VelocityCorrectionAcceleration.GetClampedToMaxSize(
		TrackPhysicsMovementConstants::MaxCorrectionAcceleration * ImpulseMultiplier);

	const FVector RightDirection = RigidBody->R().GetRightVector();
	const FVector PlanarRightDirection = FVector::VectorPlaneProject(RightDirection, GroundNormal).GetSafeNormal();
	const float LateralSpeed = FVector::DotProduct(CurrentLinearVelocity, PlanarRightDirection);
	const FVector LateralDampingAcceleration = -PlanarRightDirection * LateralSpeed *
		TrackPhysicsMovementConstants::LateralDampingFactor;
	const FVector TotalAcceleration = ClampedVelocityCorrectionAcceleration + LateralDampingAcceleration;
	const FVector DriveForce = TotalAcceleration * TankMass;
	MutableBodyInstance->AddForce(DriveForce, false, false);
}

void UTrackPhysicsMovement::ApplyYawTorqueStrategyB(const Chaos::FRigidBodyHandle_Internal* RigidBody) const
{
	const FBodyInstance* TankBodyInstance = M_TankMesh->GetBodyInstance(NAME_None);
	if (not TankBodyInstance)
	{
		return;
	}

	FBodyInstance* MutableBodyInstance = M_TankMesh->GetBodyInstance(NAME_None);
	if (not MutableBodyInstance)
	{
		return;
	}

	const FVector UpDirection = RigidBody->R().GetUpVector();
	const float CurrentYawRateRadians = FVector::DotProduct(RigidBody->W(), UpDirection);
	const float TargetYawRateRadians = FMath::DegreesToRadians(
		M_CurrentSteeringInDeg.Load() * TrackPhysicsMovementConstants::SteeringToYawDirectionSign);
	const float YawRateError = TargetYawRateRadians - CurrentYawRateRadians;
	const float DesiredYawAngularAcceleration = FMath::Clamp(
		YawRateError * TrackPhysicsMovementConstants::YawRateCorrectionGain,
		-TrackPhysicsMovementConstants::MaxYawAngularAcceleration * TorqueMultiplier,
		TrackPhysicsMovementConstants::MaxYawAngularAcceleration * TorqueMultiplier);
	const float YawInertia = FMath::Max(
		TankBodyInstance->GetBodyInertiaTensor().Z,
		TrackPhysicsMovementConstants::MinimumYawInertia);
	const FVector YawTorque = UpDirection * DesiredYawAngularAcceleration * YawInertia;
	MutableBodyInstance->AddTorqueInRadians(YawTorque, false, false);
}

bool UTrackPhysicsMovement::GetIsValidTankMesh() const
{
	if (IsValid(M_TankMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TankMesh",
		"GetIsValidTankMesh",
		this);
	return false;
}

bool UTrackPhysicsMovement::GetIsValidTankAnimationBP() const
{
	if (IsValid(M_TankAnimationBP))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TankAnimationBP",
		"GetIsValidTankAnimationBP",
		this);
	return false;
}

bool UTrackPhysicsMovement::GetIsValidMovementDependencies() const
{
	const bool bIsValidTankMesh = GetIsValidTankMesh();
	const bool bIsValidTankAnimationBP = GetIsValidTankAnimationBP();
	return bIsValidTankMesh and bIsValidTankAnimationBP;
}
