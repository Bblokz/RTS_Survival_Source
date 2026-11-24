#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "RTS_Survival/Units/Aircraft/AircraftMovement/AircraftFlightModel.h"
#include "RTS_Survival/Units/Aircraft/AircraftPath/FAircraftPath.h"

#include "AircraftMovement.generated.h"

struct FAircraftFlightModelSettings;

/**
 * @brief Pawn movement of AAircraftMaster. Vertical take-off, landing and straight runs drive the floating pawn
 * movement directly; move, attack and idle flights call TickFlight every frame, whose velocity then replaces the
 * floating pawn's own input handling (which loses most of its speed in sharp turns).
 */
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

	/**
	 * @brief Flies toward the next path point.
	 * @param TargetPoint Point to home in on; its roll is applied.
	 * @param DeltaTime Frame time.
	 * @param ClimbPitch Extra nose pitch (degrees, up positive) while terrain following moves the whole path up or down.
	 */
	void TickMovement_Homing(
		const FAircraftPathPoint& TargetPoint,
		const float DeltaTime,
		const float ClimbPitch = 0.f);

	/**
	 * @brief Bank-to-turn flight toward an aim point: sets the owner's rotation now and the velocity the next movement
	 * update uses, so the speed is kept through turns and the nose follows the flight direction.
	 * @param Input Aim point, wanted speed, climb pitch and floor; location, velocity and rotation are filled in here.
	 * @param Settings Turn, pitch, bank and speed limits of this aircraft.
	 */
	void TickFlight(FAircraftFlightInput Input, const FAircraftFlightModelSettings& Settings);

	/** @return Current heading change of the bank-to-turn flight (deg/s, positive turns right); 0 when not flying it. */
	float GetFlightYawRate() const;

	/**
	 * Smoothly rotates PawnOwner toward InTargetRot.
	 * Interp speed will be faster for small angle‐deltas, slower for large ones.
	 * @param InTargetRot  Desired rotation.
	 * @param InDeltaTime  World delta seconds.
	 */
	void UpdateOwnerRotation(const FRotator& InTargetRot, float InDeltaTime) const;

	void KillMovement();

protected:
	/** @brief Uses the bank-to-turn velocity when TickFlight set one this frame, the floating pawn's input otherwise. */
	virtual void ApplyControlInputToVelocity(float DeltaTime) override;

private:
	FAircraftFlightModel M_FlightModel;

	// Velocity TickFlight wants for the next movement update (the component moves before its owner ticks); applied by
	// ApplyControlInputToVelocity only when set on this or the last frame.
	FVector M_FlightVelocity = FVector::ZeroVector;
	uint64 M_FlightVelocityFrame = 0;
	bool bM_HasFlightVelocity = false;

	// Frame of the last TickFlight; after a pause in it (take-off, straight run) the turn rates start afresh.
	uint64 M_LastFlightFrame = 0;

	/** @return Whether TickFlight ran on one of the last frames (a single long frame does not count as a pause). */
	bool GetIsFlightActiveRecently() const;
};
