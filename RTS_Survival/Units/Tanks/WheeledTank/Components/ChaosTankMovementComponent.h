// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosTankMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UChaosTankMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_BODY()

public:
	/** @brief Sets angular momentum to zero. */
	void KillMomentum();

	UFUNCTION(BlueprintCallable)
	void ApplyForce(FVector const Force);

	inline uint8 GetForwardGears() const { return TransmissionSetup.ForwardGearRatios.Num(); }
	inline uint8 GetReverseGears() const { return TransmissionSetup.ReverseGearRatios.Num(); }
	
};
