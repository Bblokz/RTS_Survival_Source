#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "RTS_Survival/Units/Aircraft/AircraftPath/FAircraftPath.h"

#include "AircraftMovement.generated.h"

UCLASS()
class UAircraftMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

	UAircraftMovement();

public:
	/**
	 * @brief Handles upward movement during vertical takeoff.
	 * @param DeltaTime Time between frames
	 * @param CurrentZ Current Z position
	 * @param TargetZ Desired apex Z
	 * @param VTOAccel Acceleration towards apex
	 * @param MaxSpeed Maximum vertical speed
	 * @return bool True when target Z has been reached or exceeded.
	 */
	bool TickVerticalTakeOff(float DeltaTime, float CurrentZ, float TargetZ, float VTOAccel, float VTOMaxSpeed);

	 bool TickVerticalLanding(float DeltaTime, float CurrentZ, float TargetZ, float LandingAccel, float LandingMaxSpeed);

	void OnVerticalTakeOffCompleted();

	void TickMovement_Homing(
		const FAircraftPathPoint& TargetPoint,
		const float DeltaTime);

	/** 
	 * Smoothly rotates PawnOwner toward InTargetRot.
	 * Interp speed will be faster for small angle‐deltas, slower for large ones.
	 * @param InTargetRot  Desired rotation.
	 * @param InDeltaTime  World delta seconds.
	 */
	void UpdateOwnerRotation(const FRotator& InTargetRot, float InDeltaTime) const;

	void KillMovement();
};
