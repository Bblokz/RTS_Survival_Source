// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackPhysicsMovement.h"

#include "AsyncTickFunctions.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedAnimationInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TrackPhysicsMovementConstants
{
	/**
	 * @brief Shared Strategy-B constants that remain global for all tracked vehicles.
	 */

	/** @brief Set to -1 only if steering input direction is empirically reversed for this vehicle rig. */
	constexpr float SteeringToYawDirectionSign = 1.0f;
	/** @brief Raise to reject noisy normals on broken terrain; lower if movement drops out too often on uneven ground. */
	constexpr float GroundTraceConfidenceDotThreshold = 0.9f;
	/** @brief Blend weight for the current single-hit normal when one of the two terrain traces misses. */
	constexpr float GroundTraceSingleHitCurrentNormalBlendAlpha = 0.7f;
	/** @brief Number of consecutive failed trace frames that may still reuse the last valid ground sample. */
	constexpr int32 GroundTraceFailureHoldFrames = 6;
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


// Called when the game starts
void UTrackPhysicsMovement::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitRuntimeTrackPhysicsTuningSnapshot();
	SetActive(true);
}

void UTrackPhysicsMovement::BeginPlay_InitRuntimeTrackPhysicsTuningSnapshot()
{
	M_RuntimeTrackPhysicsMovementTuningSnapshot = M_TrackPhysicsMovementTuning;
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
	const FVector ForwardOffset = M_TankMesh->GetForwardVector() *
		M_RuntimeTrackPhysicsMovementTuningSnapshot.GroundTraceForwardDistance;

	const FVector Start = TraceOrigin - ForwardOffset;
	const FVector End = Start - FVector(0, 0, M_RuntimeTrackPhysicsMovementTuningSnapshot.GroundTraceDownDistance);
	FHitResult Hit;

	const FVector Start2 = TraceOrigin + ForwardOffset;
	const FVector End2 = Start2 - FVector(0, 0, M_RuntimeTrackPhysicsMovementTuningSnapshot.GroundTraceDownDistance);
	FHitResult Hit2;


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, Params);
	const bool bHit2 = World->LineTraceSingleByChannel(Hit2, Start2, End2, COLLISION_TRACE_LANDSCAPE, Params);

	if (bHit and bHit2)
	{
		if (FVector::DotProduct(Hit.Normal, Hit2.Normal) <
			TrackPhysicsMovementConstants::GroundTraceConfidenceDotThreshold)
		{
			return TryUseCachedGroundTraceSample(OutGroundNormal, OutIncline);
		}

		// compute the normal of the landscape.
		OutGroundNormal = (Hit.Normal + Hit2.Normal) * 0.5;
		OutGroundNormal.Normalize();

		// Calculate the incline angle between the two points
		const FVector InclineVector = Hit2.ImpactPoint - Hit.ImpactPoint;
		const FVector HorizontalVector = FVector(InclineVector.X, InclineVector.Y, 0).GetSafeNormal();
		OutIncline = FMath::RadiansToDegrees(
			FMath::Acos(FVector::DotProduct(HorizontalVector, InclineVector.GetSafeNormal())));
		CacheGroundTraceSample(OutGroundNormal, OutIncline);
		return true;
	}

	if (bHit or bHit2)
	{
		return TryGetGroundTraceFromSingleHit(bHit, Hit, Hit2, OutGroundNormal, OutIncline);
	}

	return TryUseCachedGroundTraceSample(OutGroundNormal, OutIncline);
}

void UTrackPhysicsMovement::CacheGroundTraceSample(const FVector& GroundNormal, const float Incline) const
{
	M_CachedGroundNormalX.Store(GroundNormal.X);
	M_CachedGroundNormalY.Store(GroundNormal.Y);
	M_CachedGroundNormalZ.Store(GroundNormal.Z);
	M_CachedInclineAngle.Store(Incline);
	M_ConsecutiveGroundTraceFailureCount.Store(0);
	bM_HasCachedGroundTraceSample.Store(true);
}

bool UTrackPhysicsMovement::TryUseCachedGroundTraceSample(FVector& OutGroundNormal, float& OutIncline) const
{
	const int32 ConsecutiveFailureCount = M_ConsecutiveGroundTraceFailureCount.Load() + 1;
	M_ConsecutiveGroundTraceFailureCount.Store(ConsecutiveFailureCount);

	if (not bM_HasCachedGroundTraceSample.Load())
	{
		return false;
	}

	if (ConsecutiveFailureCount > TrackPhysicsMovementConstants::GroundTraceFailureHoldFrames)
	{
		return false;
	}

	OutGroundNormal = FVector(
		M_CachedGroundNormalX.Load(),
		M_CachedGroundNormalY.Load(),
		M_CachedGroundNormalZ.Load()).GetSafeNormal();
	OutIncline = M_CachedInclineAngle.Load();
	return true;
}

bool UTrackPhysicsMovement::TryGetGroundTraceFromSingleHit(
	const bool bFrontTraceHit,
	const FHitResult& FrontHit,
	const FHitResult& RearHit,
	FVector& OutGroundNormal,
	float& OutIncline) const
{
	const FVector SingleHitNormal = bFrontTraceHit ? FrontHit.Normal : RearHit.Normal;
	FVector BlendedGroundNormal = SingleHitNormal;
	if (bM_HasCachedGroundTraceSample.Load())
	{
		const FVector CachedNormal = FVector(
			M_CachedGroundNormalX.Load(),
			M_CachedGroundNormalY.Load(),
			M_CachedGroundNormalZ.Load()).GetSafeNormal();
		BlendedGroundNormal = (SingleHitNormal *
				TrackPhysicsMovementConstants::GroundTraceSingleHitCurrentNormalBlendAlpha)
			+ (CachedNormal * (1.0f - TrackPhysicsMovementConstants::GroundTraceSingleHitCurrentNormalBlendAlpha));
	}

	OutGroundNormal = BlendedGroundNormal.GetSafeNormal();
	if (bM_HasCachedGroundTraceSample.Load())
	{
		OutIncline = M_CachedInclineAngle.Load();
	}
	else
	{
		OutIncline = 0.0f;
	}
	CacheGroundTraceSample(OutGroundNormal, OutIncline);
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
	const float CurrentSignedForwardSpeed = FVector::DotProduct(CurrentLinearVelocity,
	                                                            CurrentRotation.GetForwardVector());
	const float TargetForwardSpeed = M_TrackForceMultiplier.Load() * CurrentThrottle;
	const float BlendedTargetSpeed = FMath::FInterpTo(
		CurrentSignedForwardSpeed,
		TargetForwardSpeed,
		DeltaTime,
		M_RuntimeTrackPhysicsMovementTuningSnapshot.DesiredSpeedInterpRate);

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
	const FVector CurrentLinearVelocity = RigidBody->V();
	const float TankMass = RigidBody->M();
	const FVector VelocityError = DesiredPlanarVelocity - CurrentLinearVelocity;
	const FVector VelocityCorrectionAcceleration =
		VelocityError * M_RuntimeTrackPhysicsMovementTuningSnapshot.VelocityCorrectionGain;
	const FVector ClampedVelocityCorrectionAcceleration = VelocityCorrectionAcceleration.GetClampedToMaxSize(
		M_RuntimeTrackPhysicsMovementTuningSnapshot.MaxCorrectionAcceleration);

	const FVector RightDirection = RigidBody->R().GetRightVector();
	const FVector PlanarRightDirection = FVector::VectorPlaneProject(RightDirection, GroundNormal).GetSafeNormal();
	const float LateralSpeed = FVector::DotProduct(CurrentLinearVelocity, PlanarRightDirection);
	const FVector LateralDampingAcceleration = -PlanarRightDirection * LateralSpeed *
		M_RuntimeTrackPhysicsMovementTuningSnapshot.LateralDampingFactor;
	const FVector TotalAcceleration = ClampedVelocityCorrectionAcceleration + LateralDampingAcceleration;
	const FVector DriveForce = TotalAcceleration * TankMass;
	UAsyncTickFunctions::ATP_AddForce(M_TankMesh, DriveForce, false, NAME_None);
}

void UTrackPhysicsMovement::ApplyYawTorqueStrategyB(const Chaos::FRigidBodyHandle_Internal* RigidBody) const
{
	const float CurrentSteeringDeg = M_CurrentSteeringInDeg.Load();
	const float CurrentThrottle = M_CurrentThrottle.Load();

	if (FMath::IsNearlyZero(CurrentThrottle, KINDA_SMALL_NUMBER))
	{
		// Deadzone logic.
		UAsyncTickFunctions::ATP_SetAngularVelocityInDegrees(M_TankMesh, FVector(-1, 0, CurrentSteeringDeg),
		                                                     false, NAME_None);
		return;
	}
	const FVector UpDirection = RigidBody->R().GetUpVector();
	const float CurrentYawRateRadians = FVector::DotProduct(RigidBody->W(), UpDirection);
	const float TargetYawRateRadians = FMath::DegreesToRadians(
		M_CurrentSteeringInDeg.Load() * TrackPhysicsMovementConstants::SteeringToYawDirectionSign);
	const float YawRateError = TargetYawRateRadians - CurrentYawRateRadians;
	const float DesiredYawAngularAcceleration = FMath::Clamp(
		YawRateError * M_RuntimeTrackPhysicsMovementTuningSnapshot.YawRateCorrectionGain,
		-M_RuntimeTrackPhysicsMovementTuningSnapshot.MaxYawAngularAcceleration,
		M_RuntimeTrackPhysicsMovementTuningSnapshot.MaxYawAngularAcceleration);

	const Chaos::FMatrix33 WorldInertiaTensor = Chaos::FParticleUtilitiesXR::GetWorldInertia(RigidBody);
	const float YawInertia = FMath::Max(
		FVector::DotProduct(UpDirection, WorldInertiaTensor * UpDirection),
		M_RuntimeTrackPhysicsMovementTuningSnapshot.MinimumYawInertia);
	const FVector YawTorque = UpDirection * DesiredYawAngularAcceleration * YawInertia;
	UAsyncTickFunctions::ATP_AddTorque(M_TankMesh, YawTorque, false, NAME_None);
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
