// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackPhysicsMovement.h"

#include "AsyncTickFunctions.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedAnimationInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "PhysicsEngine/BodyInstance.h"

namespace TrackAsyncForceMotor
{
	constexpr float ForwardTraceOffset = 200.0f;
	constexpr float TraceLength = 100.0f;
	constexpr float MinTraceNormalDot = 0.2f;
	constexpr float MinTraceDistance = 5.0f;
	constexpr float GravityCmPerS2 = 980.0f;
	constexpr float MinYawInertiaFallback = 1.0f;

	float CalculateGroundForceAuthority(const FTrackAsyncForceMotorSettings& ForceSettings, const float GroundConfidence)
	{
		return FMath::GetMappedRangeValueClamped(
			FVector2D(0.0f, ForceSettings.M_MinGroundConfidenceForFullAuthority),
			FVector2D(0.35f, 1.0f),
			GroundConfidence);
	}

	float CalculateEstimatedNormalLoad(const float Mass, const FVector& GroundNormal)
	{
		const float GravityProjectedOnNormal = GravityCmPerS2 * FMath::Max(
			0.0f,
			FVector::DotProduct(GroundNormal, FVector::UpVector));
		return Mass * GravityProjectedOnNormal;
	}

	FVector CalculateLongitudinalAndLateralForce(
		const FTrackAsyncForceMotorSettings& ForceSettings,
		const float CurrentForwardSpeed,
		const float TargetForwardSpeed,
		const float LateralSlipSpeed,
		const float Mass,
		const float EstimatedNormalLoad,
		const FVector& PlaneForwardVector,
		const FVector& RightVector,
		const float GroundForceAuthority)
	{
		const float TargetAcceleration = FMath::Clamp(
			(TargetForwardSpeed - CurrentForwardSpeed) * ForceSettings.M_SpeedErrorToAccelerationGain,
			-ForceSettings.M_MaxLongitudinalAcceleration,
			ForceSettings.M_MaxLongitudinalAcceleration);

		const float MaxLongitudinalForce = EstimatedNormalLoad * ForceSettings.M_LongitudinalTractionCoefficient;
		const float LongitudinalForce = FMath::Clamp(Mass * TargetAcceleration, -MaxLongitudinalForce, MaxLongitudinalForce);

		const float MaxLateralForce = MaxLongitudinalForce * ForceSettings.M_MaxLateralForceRatio;
		const float LateralSlipForce = FMath::Clamp(
			-ForceSettings.M_LateralSlipDamping * LateralSlipSpeed * Mass,
			-MaxLateralForce,
			MaxLateralForce);

		const FVector LongitudinalForceVector = PlaneForwardVector * LongitudinalForce * GroundForceAuthority;
		const FVector LateralForceVector = -RightVector * LateralSlipForce * GroundForceAuthority;
		return LongitudinalForceVector + LateralForceVector;
	}

	FVector CalculateYawTorque(
		const FTrackAsyncForceMotorSettings& ForceSettings,
		const float CurrentYawRateRad,
		const float DesiredYawRateRad,
		const FVector& GroundNormal,
		const FVector& InertiaTensor,
		const float EstimatedNormalLoad,
		const float GroundForceAuthority)
	{
		const float TargetYawAcceleration = FMath::Clamp(
			(DesiredYawRateRad - CurrentYawRateRad) * ForceSettings.M_YawRateErrorToAccelerationGain,
			-ForceSettings.M_MaxYawAcceleration,
			ForceSettings.M_MaxYawAcceleration);

		const float EstimatedYawInertia = FMath::Max(
			(InertiaTensor.X + InertiaTensor.Y) * 0.5f * ForceSettings.M_YawInertiaScale,
			MinYawInertiaFallback);
		const float MaxYawTorque = EstimatedNormalLoad * ForceSettings.M_LongitudinalTractionCoefficient *
			ForceSettings.M_MaxYawTorqueRatio;
		const float YawTorqueMagnitude = FMath::Clamp(
			EstimatedYawInertia * TargetYawAcceleration,
			-MaxYawTorque,
			MaxYawTorque);
		return GroundNormal * YawTorqueMagnitude * GroundForceAuthority;
	}
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
		M_IsFollowingPath.Store(false);
		SetAsyncPhysicsTickEnabled(false);
		return;
	}

	const Chaos::FRigidBodyHandle_Internal* RigidBody = UAsyncTickFunctions::GetInternalHandle(M_TankMesh, NAME_None);
	if (not RigidBody)
	{
		return;
	}

	const float CurrentThrottle = M_CurrentThrottle.Load();
	const float CurrentSteeringInDeg = M_CurrentSteeringInDeg.Load();

	ApplyForceMotorForPathFollowing(CurrentThrottle, CurrentSteeringInDeg, RigidBody);
}

void UTrackPhysicsMovement::UpdateTankMovement(
	const float DeltaSeconds,
	float CurrentSpeed,
	const float Throttle,
	const float Steering)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_PhysicsMovement);
	if (not GetIsValidTankMesh() || not GetIsValidTankAnimationBP())
	{
		FName OwnerName = GetOwner() ? GetOwner()->GetFName() : NAME_None;
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
// 	if (IsValid(M_TankMesh) && IsValid(TankAnimationBP))
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
// 			TankAnimationBP->SetMovementParameters(CorrectedVelocity.Length(), DesiredYawRate, Throttle >= 0);
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
                                               float& OutGroundConfidence, const FVector& StartLocation) const
{
	OutGroundConfidence = 0.0f;

	const FVector ForwardVector = M_TankMesh->GetForwardVector();
	const FVector Start = StartLocation + FVector(0.0f, 0.0f, M_MeshTraceZOffset) -
		ForwardVector * TrackAsyncForceMotor::ForwardTraceOffset;
	const FVector End = Start - FVector(0.0f, 0.0f, TrackAsyncForceMotor::TraceLength);
	FHitResult Hit;

	const FVector Start2 = StartLocation + FVector(0.0f, 0.0f, M_MeshTraceZOffset) +
		ForwardVector * TrackAsyncForceMotor::ForwardTraceOffset;
	const FVector End2 = Start2 - FVector(0.0f, 0.0f, TrackAsyncForceMotor::TraceLength);
	FHitResult Hit2;


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, Params);
	const bool bHit2 = GetWorld()->LineTraceSingleByChannel(Hit2, Start2, End2, COLLISION_TRACE_LANDSCAPE, Params);

	if (not bHit || not bHit2)
	{
		return false;
	}

	const float CombinedNormalDot = FVector::DotProduct(Hit.Normal.GetSafeNormal(), Hit2.Normal.GetSafeNormal());
	if (CombinedNormalDot < TrackAsyncForceMotor::MinTraceNormalDot)
	{
		return false;
	}

	const float TraceDistanceA = FMath::Abs(Hit.ImpactPoint.Z - Start.Z);
	const float TraceDistanceB = FMath::Abs(Hit2.ImpactPoint.Z - Start2.Z);
	if (TraceDistanceA < TrackAsyncForceMotor::MinTraceDistance || TraceDistanceB < TrackAsyncForceMotor::MinTraceDistance)
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

	const float NormalAgreementConfidence = FMath::GetMappedRangeValueClamped(
		FVector2D(TrackAsyncForceMotor::MinTraceNormalDot, 1.0f),
		FVector2D(0.0f, 1.0f),
		CombinedNormalDot);
	const float DistanceDifference = FMath::Abs(TraceDistanceA - TraceDistanceB);
	const float DistanceConfidence = 1.0f - FMath::Clamp(
		DistanceDifference / TrackAsyncForceMotor::TraceLength,
		0.0f,
		1.0f);
	OutGroundConfidence = FMath::Clamp((NormalAgreementConfidence + DistanceConfidence) * 0.5f, 0.0f, 1.0f);
	return true;
}

bool UTrackPhysicsMovement::GetIsValidTankMesh() const
{
	if (IsValid(M_TankMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TankMesh", "UTrackPhysicsMovement::GetIsValidTankMesh", GetOwner());
	return false;
}

bool UTrackPhysicsMovement::GetIsValidTankAnimationBP() const
{
	if (IsValid(M_TankAnimationBP))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TankAnimationBP",
		"UTrackPhysicsMovement::GetIsValidTankAnimationBP",
		GetOwner());
	return false;
}

void UTrackPhysicsMovement::ApplyForceMotorForPathFollowing(
	const float CurrentThrottle,
	const float CurrentSteeringInDeg,
	const Chaos::FRigidBodyHandle_Internal* RigidBody)
{
	if (not GetIsValidTankMesh() || not RigidBody)
	{
		return;
	}

	FBodyInstance* const TankBodyInstance = M_TankMesh->GetBodyInstance(NAME_None);
	if (not TankBodyInstance)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"TankBodyInstance",
			"UTrackPhysicsMovement::ApplyForceMotorForPathFollowing",
			GetOwner());
		return;
	}

	FVector GroundNormal;
	float Incline = 0.0f;
	float GroundConfidence = 0.0f;
	if (not PerformGroundTrace(GroundNormal, Incline, GroundConfidence, RigidBody->X()))
	{
		return;
	}

	M_InclineAngle = Incline;

	const FVector CurrentLinearVelocity = RigidBody->V();
	const FQuat CurrentRotation = RigidBody->R();
	const FVector ForwardVector = CurrentRotation.GetForwardVector();
	const FVector RightVector = CurrentRotation.GetRightVector();
	const FVector PlaneForwardVector = FVector::VectorPlaneProject(ForwardVector, GroundNormal).GetSafeNormal();
	const float CurrentForwardSpeed = FVector::DotProduct(CurrentLinearVelocity, PlaneForwardVector);
	const float TargetForwardSpeed = M_TrackForceMultiplier.Load() * CurrentThrottle;
	const float Mass = TankBodyInstance->GetBodyMass();
	const float EstimatedNormalLoad = TrackAsyncForceMotor::CalculateEstimatedNormalLoad(Mass, GroundNormal);
	const float LateralSlipSpeed = FVector::DotProduct(CurrentLinearVelocity, RightVector);
	const float GroundForceAuthority = TrackAsyncForceMotor::CalculateGroundForceAuthority(
		M_ForceMotorSettings,
		GroundConfidence);
	const FVector CombinedDriveAndSlipForce = TrackAsyncForceMotor::CalculateLongitudinalAndLateralForce(
		M_ForceMotorSettings,
		CurrentForwardSpeed,
		TargetForwardSpeed,
		LateralSlipSpeed,
		Mass,
		EstimatedNormalLoad,
		PlaneForwardVector,
		RightVector,
		GroundForceAuthority);
	TankBodyInstance->AddForce(CombinedDriveAndSlipForce);

	const FVector CurrentAngularVelocityRad = RigidBody->W();
	const float CurrentYawRateRad = FVector::DotProduct(CurrentAngularVelocityRad, GroundNormal);
	const float DesiredYawRateRad = FMath::DegreesToRadians(CurrentSteeringInDeg);
	const FVector InertiaTensor = TankBodyInstance->GetBodyInertiaTensor();
	const FVector YawTorqueVector = TrackAsyncForceMotor::CalculateYawTorque(
		M_ForceMotorSettings,
		CurrentYawRateRad,
		DesiredYawRateRad,
		GroundNormal,
		InertiaTensor,
		EstimatedNormalLoad,
		GroundForceAuthority);
	TankBodyInstance->AddTorqueInRadians(YawTorqueVector);
}
