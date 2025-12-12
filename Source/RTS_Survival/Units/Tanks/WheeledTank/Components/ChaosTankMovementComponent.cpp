// Fill out your copyright notice in the Description page of Project Settings.


#include "ChaosTankMovementComponent.h"

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

