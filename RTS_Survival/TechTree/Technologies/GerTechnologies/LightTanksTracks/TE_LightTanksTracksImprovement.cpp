// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_LightTanksTracksImprovement.h"

#include "AIController.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTE_LightTanksTracksImprovement::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	const AAITankMaster* Controller = Tank->GetAIController();
	if (not IsValid(Controller))
	{
		return;
	}

	UTrackPathFollowingComponent* PathFollowingComponent = Cast<UTrackPathFollowingComponent>(
		Controller->GetPathFollowingComponent());
	if (not IsValid(PathFollowingComponent))
	{
		return;
	}

	const float CurrentSpeed = PathFollowingComponent->BP_GetDesiredSpeed();
	PathFollowingComponent->SetDesiredSpeed(CurrentSpeed * MaxSpeedMlt, ESpeedUnits::KilometersPerHour);
	Tank->UpgradeTurnRate(Tank->GetTurnRate() * TurnRateMlt);
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Upgraded " + Tank->GetName() + " speed to " +
			FString::SanitizeFloat(CurrentSpeed * MaxSpeedMlt) + " km/h from :" +
			FString::SanitizeFloat(CurrentSpeed) + " km/h", FColor::Blue);
	}
}
