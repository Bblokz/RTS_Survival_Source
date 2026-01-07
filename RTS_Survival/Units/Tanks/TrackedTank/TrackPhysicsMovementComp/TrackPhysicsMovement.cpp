// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackPhysicsMovement.h"

#include "AsyncTickFunctions.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedAnimationInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "PhysicsEngine/BodyInstance.h"


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
		TankAnimationBP = NewTankAnimBp;
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

	if (!M_TankMesh)
	{
		return;
	}
	const Chaos::FRigidBodyHandle_Internal* RigidBody = UAsyncTickFunctions::GetInternalHandle(M_TankMesh, NAME_None);
	if (!RigidBody)
	{
		return;
	}
	FVector BodyLocation = RigidBody->X();

	float CurrentSpeed = RigidBody->V().Size();
	// Read the variables atomically
	const float CurrentThrottle = M_CurrentThrottle.Load();
	const float CurrentSteeringInDeg = M_CurrentSteeringInDeg.Load();


	// Apply angular velocity for rotation
	UAsyncTickFunctions::ATP_SetAngularVelocityInDegrees(M_TankMesh, FVector(-1, 0, CurrentSteeringInDeg), false, NAME_None);
	const FQuat CurrentRotation = RigidBody->R();

	// Calculate desired forward force based on throttle input
	const float TargetSpeed = M_TrackForceMultiplier.Load() * CurrentThrottle;

	// Make sure that for reversing the interp uses two negative values!
	CurrentSpeed *= FMath::Sign(CurrentThrottle);
	// Best to use the same interp for all tanks and tweak variables in the controller instead.
	const float BlendedSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, 5.0f);
	FVector GroundNormal;
	float Incline;
	if (PerformGroundTrace(GroundNormal, Incline, RigidBody->X()))
	{
		const FVector DesiredVelocity = CurrentRotation.GetForwardVector() * BlendedSpeed;
		// Adjust velocity vector for landscape normal; ensures movement is strictly parallel to the landscape's incline.
		const FVector CorrectedVelocity = FVector::VectorPlaneProject(DesiredVelocity, GroundNormal);

		UAsyncTickFunctions::ATP_SetLinearVelocity(M_TankMesh, CorrectedVelocity, false, NAME_None);
	}
}

void UTrackPhysicsMovement::UpdateTankMovement(
	const float DeltaSeconds,
	float CurrentSpeed,
	const float Throttle,
	const float Steering)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VehiclePathFollowing_PhysicsMovement);
	if (not IsValid(M_TankMesh) || not IsValid(TankAnimationBP))
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

	if(Throttle != 0)
	{
		LastNoneZeroThrottle = Throttle;
	}
	TankAnimationBP->SetMovementParameters(CurrentSpeed, DesiredYawRate, Throttle >= 0);
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
                                               const FVector& StartLocation) const
{
	FVector Start = M_TankMesh->GetComponentLocation() + FVector(0, 0, M_MeshTraceZOffset) - M_TankMesh->
		GetForwardVector() * 200;
	FVector End = Start - FVector(0, 0, 100);
	FHitResult Hit;

	FVector Start2 = M_TankMesh->GetComponentLocation() + FVector(0, 0, M_MeshTraceZOffset) + M_TankMesh->
		GetForwardVector() * 200;
	FVector End2 = Start2 - FVector(0, 0, 100);
	FHitResult Hit2;


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, Params);
	bool bHit2 = GetWorld()->LineTraceSingleByChannel(Hit2, Start2, End2, COLLISION_TRACE_LANDSCAPE, Params);

	if (bHit && bHit2)
	{
		// compute the normal of the landscape.
		OutGroundNormal = (Hit.Normal + Hit2.Normal) * 0.5;
		OutGroundNormal.Normalize();

		// Calculate the incline angle between the two points
		FVector InclineVector = Hit2.ImpactPoint - Hit.ImpactPoint;
		FVector HorizontalVector = FVector(InclineVector.X, InclineVector.Y, 0).GetSafeNormal();
		OutIncline = FMath::RadiansToDegrees(
			FMath::Acos(FVector::DotProduct(HorizontalVector, InclineVector.GetSafeNormal())));
	}

	return bHit && bHit2;
}
