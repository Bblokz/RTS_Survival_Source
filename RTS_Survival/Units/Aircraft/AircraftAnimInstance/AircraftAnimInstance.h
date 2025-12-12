// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AircraftAnimationSettings/FPropAnimationSettings.h"
#include "AircraftSoundController/AircraftSoundController.h"
#include "Animation/AnimInstance.h"
#include "AircraftAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void Init(AAircraftMaster* OwningAircraft);
	void UpdateAnim(const FRotator TargetRotator,
		const float MovementSpeed);
	void UpdateLandedState(const EAircraftLandingState NewLandingState);
	void SetMaxMovementSpeed(const float MaxMovementSpeed);
	void OnPrepareVto(const float VtoPrepTime);
	void OnVtoStart(const float TotalVTOTime);
	void OnVtoComplete();
	void OnWaitForLanding();
	void OnStartLanding(const float TotalLandingTime);
	void OnLandingComplete();
	inline float GetAnimPropSpeed() const { return M_PropSpeed; }

	// Last registered airspeed at anim instance; public so aircraft sound controller can access.
	inline float GetLastAirSpeed() const
	{
		return PropellerAnimSettings.MaxMovementSpeed / M_PropSpeed;
	}

	/**
	 * C++-only tuning parameter for stabilizer hysteresis (in degrees).
	 * Larger values = more "sticky" states near thresholds.
	 * Not exposed to Blueprint by design.
	 */
	inline void SetStabilizerHysteresisDegrees(const float InDegrees)
	{
		// No clamping above 0 besides non-negative; caller owns range decisions.
		StabilizerHysteresisDegrees = FMath::Max(0.0f, InDegrees);
	}

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Propeller Animation")
	FPropAnimationSettings PropellerAnimSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stabilizer Animation")
	FStabilizerAnimationSettings StabilizerAnimSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stabilizer Animation")
	FWingRotorsAnimationSettings WingRotorAnimSettings;

	/**
	 * ThreadSafe to allow multiple threads to access the function.
	 * @return Returns the Propeller AO of this aircraft.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAimOffsetBlendSpace1D* GetPropellerAo() const { return PropellerAnimSettings.PropellerBlendSpace; }

	/**
	 * ThreadSafe to allow multiple threads to access the function.
	 * @return Returns the Propeller Base Animation of this aircraft.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAnimSequence* GetPropellerBaseSequence() const { return PropellerAnimSettings.PropellerBaseAnimation; }

	/**
	 * ThreadSafe to allow multiple threads to access the function. 
	 * @return The Stabilizer animation depending on the set stabilizer animation type enum.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAnimSequence* GetStabilizerBaseSequence() const { return StabilizerAnimSettings.GetAnimation(); }

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAnimSequence* GetWingRotorAnimSequence() const { return WingRotorAnimSettings.GetAnimation(); }

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	float GetPropSpeed();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FAircraftSoundController AircraftSoundController;
	
private:
	float M_PropSpeed = 0.f;

	// Hysteresis for deciding stabilizer animation states (degrees). C++ EXPOSED ONLY.
	// Default is small but effective to avoid rapid flipping close to thresholds.
	float StabilizerHysteresisDegrees = 2.5f;

	float GetMontagePlayRateForTime(const float TotalMontageTimeAllowed, const UAnimMontage* Montage) const;
};
