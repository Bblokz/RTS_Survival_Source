#include "AircraftMovement.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"

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

void UAircraftMovement::TickMovement_Homing(const FAircraftPathPoint& TargetPoint, const float DeltaTime)
{
	if(not PawnOwner)
	{
		return;
	}
	FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(PawnOwner->GetActorLocation(), TargetPoint.Location);
	TargetRot.Roll = TargetPoint.Roll;
    const FVector ForwardDir = TargetRot.Vector().GetSafeNormal();
    AddInputVector(ForwardDir);
	UpdateOwnerRotation(TargetRot, DeltaTime);
	if(DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
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

void UAircraftMovement::KillMovement()
{
	Velocity = FVector::ZeroVector;
	StopMovementImmediately();
}
