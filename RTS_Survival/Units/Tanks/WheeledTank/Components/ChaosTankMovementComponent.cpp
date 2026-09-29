// Fill out your copyright notice in the Description page of Project Settings.


#include "ChaosTankMovementComponent.h"

namespace ChaosTankMovementConstants
{
	constexpr float CentimetersPerKilometer = 100000.f;
	constexpr float SecondsPerHour = 3600.f;
}

void UChaosTankMovementComponent::SetModuleMobilityLimits(const float TravelSpeedMultiplier,
	const float TurnRateMultiplier, const float AccelerationMultiplier, const float UnrestrictedMaxSpeedKmh)
{
	M_ModuleTravelSpeedMultiplier = FMath::Clamp(TravelSpeedMultiplier, 0.f, 1.f);
	M_ModuleTurnRateMultiplier = FMath::Clamp(TurnRateMultiplier, 0.f, 1.f);
	M_ModuleAccelerationMultiplier = FMath::Clamp(AccelerationMultiplier, 0.f, 1.f);
	M_UnrestrictedMaxSpeedKmh = FMath::Max(UnrestrictedMaxSpeedKmh, 0.f);
}

void UChaosTankMovementComponent::PreTickGT(const float DeltaTime)
{
	const float UnrestrictedThrottleInput = RawThrottleInput;
	const float UnrestrictedSteeringInput = RawSteeringInput;
	const float MaxSpeedCmPerSecond = M_UnrestrictedMaxSpeedKmh * M_ModuleTravelSpeedMultiplier
		* ChaosTankMovementConstants::CentimetersPerKilometer / ChaosTankMovementConstants::SecondsPerHour;
	const bool bHasReachedModuleSpeedLimit = M_UnrestrictedMaxSpeedKmh > 0.f
		&& FMath::Abs(GetForwardSpeed()) >= MaxSpeedCmPerSecond;
	RawThrottleInput = bHasReachedModuleSpeedLimit || FMath::IsNearlyZero(M_ModuleTravelSpeedMultiplier)
		? 0.f : UnrestrictedThrottleInput * M_ModuleAccelerationMultiplier;
	RawSteeringInput = UnrestrictedSteeringInput * M_ModuleTurnRateMultiplier;
	Super::PreTickGT(DeltaTime);
	RawThrottleInput = UnrestrictedThrottleInput;
	RawSteeringInput = UnrestrictedSteeringInput;
}

void UChaosTankMovementComponent::KillMomentum()
{
	FBodyInstance* TargetInstance = GetBodyInstance();
	if (TargetInstance)
	{
		// if start awake is false then setting the velocity (even to zero) causes particle to wake up.
		if (TargetInstance->IsInstanceAwake())
		{
			//TargetInstance->SetLinearVelocity(FVector::ZeroVector, false);
			TargetInstance->SetAngularVelocityInRadians(FVector::ZeroVector, false);
			TargetInstance->ClearForces();
			//TargetInstance->ClearTorques();
		}
	}
}

void UChaosTankMovementComponent::ApplyForce(FVector const Force)
{
	FBodyInstance* TargetInstance = GetBodyInstance();
	if (TargetInstance)
	{
		TargetInstance->AddForce(Force);
	}
}

