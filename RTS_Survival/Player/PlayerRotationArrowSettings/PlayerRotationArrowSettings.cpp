#include "PlayerRotationArrowSettings.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Formation/FormationMovement.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

void FPlayerRotationArrowSettings::InitRotationArrowAction(const FVector2D& InitialMouseScreenLocation,
	const FVector& InitialMouseProjectedLocation,
	const FRotationArrowArcSettings& ArcSettings)
{
	if (not EnsureRotationActorIsValid())
	{
		return;
	}
	RotationArrowActor->SetActorLocation(InitialMouseProjectedLocation + ArrowOffset);
	bM_RotationArrowInitialized = true;
	RotationArrowActor->SetActorHiddenInGame(true);
	M_OriginalMouseScreenLocation = InitialMouseScreenLocation;
	InitRotationArrowAction_ArcRadius(InitialMouseProjectedLocation, ArcSettings);
	if constexpr (DeveloperSettings::Debugging::GPlayerRotationArrow_Compile_DebugSymbols)
	{
		const FString StartLocationAsString = RotationArrowActor->GetActorLocation().ToString();
		DebugArrow("Rotation arrow initialized at: " + StartLocationAsString, FColor::Green);
	}
}


void FPlayerRotationArrowSettings::InitRotationArrowAction_ArcRadius(
	const FVector& InitialMouseProjectedLocation,
	const FRotationArrowArcSettings& ArcSettings)
{
	constexpr float MinimumVisibleArcRange = 100.0f;
	const bool bShouldShowArcRadius = ArcSettings.ArcAngle > 0.0f
		&& ArcSettings.WeaponRange > MinimumVisibleArcRange;

	if (not bShouldShowArcRadius)
	{
		return;
	}

	M_ArcRadiusId = URTSBlueprintFunctionLibrary::CreateRTSRadius(
		RotationArrowActor,
		InitialMouseProjectedLocation,
		ArcSettings.WeaponRange,
		ERTSRadiusType::FullCircle_TeamWeaponArc
	);

	if (M_ArcRadiusId < 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("FPlayerRotationArrowSettings::InitRotationArrowAction failed to create a weapon arc radius."));
		return;
	}

	URTSBlueprintFunctionLibrary::AttachRTSRadiusToActor(
		RotationArrowActor,
		M_ArcRadiusId,
		RotationArrowActor,
		ArrowOffset);
	URTSBlueprintFunctionLibrary::UpdateRTSRadiusArc(RotationArrowActor, M_ArcRadiusId, ArcSettings.ArcAngle);
}

void FPlayerRotationArrowSettings::HideArcRadius()
{
	if (M_ArcRadiusId < 0)
	{
		return;
	}

	if (EnsureRotationActorIsValid())
	{
		URTSBlueprintFunctionLibrary::HideRTSRadiusById(RotationArrowActor, M_ArcRadiusId);
	}

	M_ArcRadiusId = -1;
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
	if (not EnsureFormationControllerIsValid())
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


void FPlayerRotationArrowSettings::CancelRotationArrow()
{
	ResetForNextRotation();
}

bool FPlayerRotationArrowSettings::GetIsRotationArrowActive() const
{
	return bM_RotationArrowInitialized;
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
	HideArcRadius();
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
