// Copyright 2020 313 Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VehicleAIInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVehicleAIInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IVehicleAIInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	* Updates the vehicle with the algorihm state.
	* @param TargetAngle - Angle towards the target move location
	* @param DestinationDistance - Distance to the current move destination
	* @param DesiredSpeed - The adjusted desired speed
	* @param CalculatedThrottle - The throttle thats been calculated, this is whats normally used by the system internally
	* @param CurrentSpeed - The current speed of the vehicle.
	* @param DeltaTime - The time since the last frame
	* @param CalculatedBreak - The break thats been calculated.
	* @param CalculatedSteering - The steering thats been calculated.
	* 
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Vehicle AI")
	void UpdateVehicle(
		float TargetAngle,
		float DestinationDistance,
		float DesiredSpeed,
		float CalculatedThrottle,
		float CurrentSpeed,
		float DeltaTime,
		float CalculatedBreak,
		float CalculatedSteering);

	virtual void OnFinishedPathFollowing();

};
