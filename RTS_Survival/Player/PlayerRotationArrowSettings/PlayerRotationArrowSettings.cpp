#include "PlayerRotationArrowSettings.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Formation/FormationMovement.h"

void FPlayerRotationArrowSettings::InitRotationArrowAction(const FVector2D& InitialMouseScreenLocation,
                                                           const FVector& InitialMouseProjectedLocation)
{
	if (not EnsureRotationActorIsValid())
	{
		return;
	}
	RotationArrowActor->SetActorLocation(InitialMouseProjectedLocation + ArrowOffset);
	bM_RotationArrowInitialized = true;
	RotationArrowActor->SetActorHiddenInGame(true);
	M_OriginalMouseScreenLocation = InitialMouseScreenLocation;
	if constexpr (DeveloperSettings::Debugging::GPlayerRotationArrow_Compile_DebugSymbols)
	{
		const FString StartLocationAsString = RotationArrowActor->GetActorLocation().ToString();
		DebugArrow("Rotation arrow initialized at: " + StartLocationAsString, FColor::Green);
	}
}

void FPlayerRotationArrowSettings::TickArrowRotation(const FVector2D& MouseScreenLocation,
                                                     const FVector& MouseProjectedLocation)
{
	if (not bM_RotationArrowInitialized)
	{
		return;
	}
	if (bM_MouseMovedEnough)
	{
		RotateArrowToProjection(MouseProjectedLocation);
		return;
	}
	constexpr float MoveTolerance = DeveloperSettings::UIUX::MouseDragThreshold;
	bM_MouseMovedEnough = M_OriginalMouseScreenLocation.Equals(MouseScreenLocation, MoveTolerance) == false;
}

void FPlayerRotationArrowSettings::StopRotationArrow()
{
	if (!EnsureFormationControllerIsValid())
	{
		return;
	}
	if (bM_MouseMovedEnough)
	{
		// Grab the arrow’s final yaw and hand it off to the formation controller
		const FRotator FinalRotation = RotationArrowActor->GetActorRotation();
		PlayerFormationController->SetPlayerRotationOverride(FinalRotation);
	}
	// Clean up for the next drag
	ResetForNextRotation();
}


bool FPlayerRotationArrowSettings::EnsureRotationActorIsValid() const
{
	if (not IsValid(RotationArrowActor))
	{
		RTSFunctionLibrary::ReportError("Rotation arrow not valid for player");
		return false;
	}
	return true;
}

bool FPlayerRotationArrowSettings::EnsureFormationControllerIsValid() const
{
	if (not PlayerFormationController.IsValid())
	{
		RTSFunctionLibrary::ReportError("Player rotation arrow struct FPlayerRotationArrowSettings has"
			"no access to a Valid FormationController. ");
		return false;
	}
	return true;
}

void FPlayerRotationArrowSettings::ResetForNextRotation()
{
	bM_RotationArrowInitialized = false;
	bM_MouseMovedEnough = false;
	if (EnsureRotationActorIsValid())
	{
		RotationArrowActor->SetActorHiddenInGame(true);
	}
}

void FPlayerRotationArrowSettings::RotateArrowToProjection(const FVector& MouseProjectedLocation) const
{
	if (not EnsureRotationActorIsValid() || MouseProjectedLocation.IsNearlyZero())
	{
		return;
	}
	RotationArrowActor->SetActorHiddenInGame(false);
	const FVector ArrowLocation = RotationArrowActor->GetActorLocation();
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(ArrowLocation, MouseProjectedLocation);
	// We only want to affect the yaw.
	LookAtRotation.Pitch = 0.f;
	LookAtRotation.Roll = 0.f;
	RotationArrowActor->SetActorRotation(LookAtRotation);
	DebugArrow("Rotated arrow with yaw: " + FString::SanitizeFloat(LookAtRotation.Yaw), FColor::Green);
}

void FPlayerRotationArrowSettings::DebugArrow(const FString& Message, const FColor Color) const
{
	if constexpr (DeveloperSettings::Debugging::GPlayerRotationArrow_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color);
	}
}
