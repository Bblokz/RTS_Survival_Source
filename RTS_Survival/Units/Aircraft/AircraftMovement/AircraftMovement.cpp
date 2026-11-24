#include "AircraftMovement.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

namespace AircraftMovementConstants
{
	// More frames than this between two bank-to-turn frames means another flight mode ran in between.
	constexpr uint64 MaxFlightFrameGap = 2;
	// Long frames (hitches) are flown in steps of at most this, so turn and bank limits hold (seconds).
	constexpr float MaxFlightStepSeconds = 1.f / 30.f;
	constexpr int32 MaxFlightSteps = 30;
}

UAircraftMovement::UAircraftMovement()
{
}

bool UAircraftMovement::TickVerticalTakeOff(
	float DeltaTime,
	float CurrentZ,
	float TargetZ,
	float VTOAccel,
	float VTOMaxSpeed)
{
	const float DeltaZ = TargetZ - CurrentZ;
	FRTSAircraftHelpers::AircraftDebug(
		FString::Printf(TEXT("AircraftMovement: DeltaZ: %f, CurrentZ: %f, TargetZ: %f"), DeltaZ, CurrentZ, TargetZ),
		FColor::Red);
	if (DeltaZ <= 0.f)
	{
		// Already above or at target height
		return true;
	}

	const float VelocityZ = FMath::Min(VTOMaxSpeed, Velocity.Z + VTOAccel * DeltaTime);
	Velocity.Z = VelocityZ;

	AddInputVector(FVector::UpVector);
	return false;
}

bool UAircraftMovement::TickVerticalLanding(
	float DeltaTime,
	float CurrentZ,
	float TargetZ,
	float LandingAccel,
	float LandingMaxSpeed)
{
	const float DeltaZ = CurrentZ - TargetZ; // positive if we are above target
	FRTSAircraftHelpers::AircraftDebug(
		FString::Printf(TEXT("AircraftMovement (Landing): DeltaZ: %f, CurrentZ: %f, TargetZ: %f"), DeltaZ, CurrentZ, TargetZ),
		FColor::Green);

	// Already at/below target height
	if (DeltaZ <= 0.f)
	{
		Velocity.Z = 0.f;
		return true;
	}

	// Accelerate downward up to max speed
	const float SpeedMag = FMath::Min(LandingMaxSpeed, FMath::Abs(Velocity.Z) + LandingAccel * DeltaTime);
	Velocity.Z = -SpeedMag;

	// Drive the integrator downward (mirrors VTO's UpVector usage)
	AddInputVector(-FVector::UpVector);

	return false;
}


void UAircraftMovement::OnVerticalTakeOffCompleted()
{
	// Reset vertical speed to zero
	Velocity.Z = 0.f;
}

void UAircraftMovement::TickMovement_Homing(const FAircraftPathPoint& TargetPoint, const float DeltaTime,
                                           const float ClimbPitch)
{
	if(not PawnOwner)
	{
		return;
	}
	FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(PawnOwner->GetActorLocation(), TargetPoint.Location);
	TargetRot.Roll = TargetPoint.Roll;
    const FVector ForwardDir = TargetRot.Vector().GetSafeNormal();
    AddInputVector(ForwardDir);
	// Only the nose follows the terrain climb: the height itself moves with the path, not through the input vector.
	TargetRot.Pitch += ClimbPitch;
	UpdateOwnerRotation(TargetRot, DeltaTime);
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		
        DrawDebugLine(
            PawnOwner->GetWorld(),
            PawnOwner->GetActorLocation(),
            TargetPoint.Location,
            FColor::Purple,
            false,        // persistent
            DeltaTime*3,// duration
            1,            // depth priority
            10.f          // thickness
        );
	}
}

void UAircraftMovement::UpdateOwnerRotation(const FRotator& InTargetRot, const float InDeltaTime) const
{
    // get current rotation
    const FRotator CurrentRot = PawnOwner->GetActorRotation();

    // 1) compute angle difference (in degrees)
    const float AngleDiffRad = FQuat(CurrentRot).AngularDistance(FQuat(InTargetRot));
    const float AngleDiffDeg = FMath::RadiansToDegrees(AngleDiffRad);

    // 2) remap [0°..180°] → [FastSpeed..SlowSpeed]
    constexpr float FastSpeed = 6.f;
    constexpr float SlowSpeed = 2.f;
    const float DynamicSpeed = FMath::GetMappedRangeValueClamped(
        FVector2D(0.f, 180.f),
        FVector2D(FastSpeed, SlowSpeed),
        AngleDiffDeg
    );

    // 3) interpolate with exponential easing
    const FRotator NewRot = FMath::RInterpTo(
        CurrentRot,
        InTargetRot,
        InDeltaTime,
        DynamicSpeed
    );

    PawnOwner->SetActorRotation(NewRot);
}

void UAircraftMovement::TickFlight(FAircraftFlightInput Input, const FAircraftFlightModelSettings& Settings)
{
	using namespace AircraftMovementConstants;
	if (not PawnOwner || not GetWorld())
	{
		return;
	}
	if (not GetIsFlightActiveRecently())
	{
		M_FlightModel.Reset();
	}
	M_LastFlightFrame = GFrameCounter;

	// A flight velocity not yet consumed by the movement update is the newer one (the update may tick after us).
	Input.Location = PawnOwner->GetActorLocation();
	Input.Velocity = bM_HasFlightVelocity ? M_FlightVelocity : Velocity;
	Input.CurrentRotation = PawnOwner->GetActorRotation();
	const float FrameSeconds = FMath::Max(Input.DeltaTime, 0.f);
	const int32 StepCount = FMath::Clamp(FMath::CeilToInt(FrameSeconds / MaxFlightStepSeconds), 1, MaxFlightSteps);
	Input.DeltaTime = FrameSeconds / StepCount;
	FAircraftFlightOutput Output{Input.Velocity, Input.CurrentRotation};
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		Output = M_FlightModel.Step(Input, Settings);
		Input.Location += Output.Velocity * Input.DeltaTime;
		Input.Velocity = Output.Velocity;
		Input.CurrentRotation = Output.Rotation;
	}
	M_FlightVelocity = Output.Velocity;
	M_FlightVelocityFrame = GFrameCounter;
	bM_HasFlightVelocity = true;
	// The rotation is applied now, with this frame's time step: applying it with the velocity in the next movement
	// update used the previous frame's step and made the turn judder with uneven frame times. The nose then leads
	// the flight direction by one frame of turn (about 1 degree at 90 deg/s).
	PawnOwner->SetActorRotation(Output.Rotation);
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		DrawDebugLine(PawnOwner->GetWorld(), PawnOwner->GetActorLocation(), Input.AimPoint, FColor::Purple, false,
		              FrameSeconds * 3, 1, 10.f);
	}
}

bool UAircraftMovement::GetIsFlightActiveRecently() const
{
	return M_LastFlightFrame > 0 && GFrameCounter - M_LastFlightFrame <= AircraftMovementConstants::MaxFlightFrameGap;
}

float UAircraftMovement::GetFlightYawRate() const
{
	return GetIsFlightActiveRecently() ? M_FlightModel.GetYawRate() : 0.f;
}

void UAircraftMovement::ApplyControlInputToVelocity(const float DeltaTime)
{
	// Only a velocity from this or the previous frame: a stale one would bend the first frame of a straight run.
	const bool bHasFreshFlightVelocity = bM_HasFlightVelocity
		&& GFrameCounter - M_FlightVelocityFrame <= 1;
	bM_HasFlightVelocity = false;
	if (not bHasFreshFlightVelocity)
	{
		Super::ApplyControlInputToVelocity(DeltaTime);
		return;
	}
	ConsumeInputVector();
	Velocity = M_FlightVelocity;
}

void UAircraftMovement::KillMovement()
{
	Velocity = FVector::ZeroVector;
	bM_HasFlightVelocity = false;
	M_FlightModel.Reset();
	StopMovementImmediately();
}
