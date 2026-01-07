// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_LightTanksTracksImprovement.h"

#include "AIController.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UTE_LightTanksTracksImprovement::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("apply light tanks tracks improvement effect");
	}
	Super::ApplyTechnologyEffect(WorldContextObject);
}

void UTE_LightTanksTracksImprovement::OnApplyEffectToActor(AActor* ValidActor)
{
	ATankMaster* Tank = Cast<ATankMaster>(ValidActor);
	if (!Tank)
	{
		RTSFunctionLibrary::PrintString(
			"ValidActor is not an ATankMaster: " + (ValidActor ? ValidActor->GetName() : "Null ValidActor"));
		return;
	}
	if (const AAITankMaster* Controller = Tank->GetAIController(); IsValid(Controller))
	{
		if (UTrackPathFollowingComponent* PathFollowingComponent = Cast<UTrackPathFollowingComponent>(
			Controller->GetPathFollowingComponent()); IsValid(PathFollowingComponent))
		{
			const float CurrentSpeed = PathFollowingComponent->BP_GetDesiredSpeed();
			PathFollowingComponent->SetDesiredSpeed(CurrentSpeed * MaxSpeedMlt, ESpeedUnits::KilometersPerHour);
			if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString(
					"Upgraded " + Tank->GetName() + " speed to " + FString::SanitizeFloat(CurrentSpeed * MaxSpeedMlt) +
					" km/h" + "from :" + FString::SanitizeFloat(CurrentSpeed) + " km/h", FColor::Blue);
			}
			Tank->UpgradeTurnRate(Tank->GetTurnRate() * TurnRateMlt);
			UpgradeGameStateOfTank(Tank);
		}
	}
}

void UTE_LightTanksTracksImprovement::UpgradeGameStateOfTank(const ATankMaster* ValidTank) const
{
	if (ACPPGameState* GameState = FRTS_Statics::GetGameState(ValidTank))
	{
		if (const URTSComponent* TankRTSComponent = ValidTank->GetRTSComponent(); IsValid(TankRTSComponent))
		{
			const ETankSubtype TankSubtype = TankRTSComponent->GetSubtypeAsTankSubtype();
			const int32 OwningPlayer = TankRTSComponent->GetOwningPlayer();
			FTankData Data = GameState->GetTankDataOfPlayer(OwningPlayer, TankSubtype);
			Data.VehicleRotationSpeed *= TurnRateMlt;
			Data.VehicleMaxSpeedKmh *= MaxSpeedMlt;
			GameState->UpgradeTankDataForPlayer(OwningPlayer, TankSubtype, Data);
			return;
		}
		RTSFunctionLibrary::PrintString(
			"Invalid RTSComponent in UTE_LightTanksTracksImprovement::UpdateGameStateOfTank");
		return;
	}
	RTSFunctionLibrary::PrintString("Invalid GameState in UTE_LightTanksTracksImprovement::UpdateGameStateOfTank");
}

TSet<TSubclassOf<AActor>> UTE_LightTanksTracksImprovement::GetTargetActorClasses() const
{
	TSet<TSubclassOf<AActor>> ClassesToUpgrade;
	for(auto Class : AffectedUnits)
	{
		if(Class)
		{
			ClassesToUpgrade.Add(Class);
		}
	}
	return ClassesToUpgrade;
}
