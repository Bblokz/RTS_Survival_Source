// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerAirDropSubsystem.h"

#include "Engine/World.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace PlayerAirDropConstants
{
	// The local player's number in the global ability system (UGlobalAbilitiesManager::IsPlayerAbilityManager).
	constexpr int32 LocalPlayerNumber = 1;
}

namespace
{
	UGlobalAbilitySetDataAsset* GetAbilitySet()
	{
		const URTSGlobalAbilitySettings* Settings = GetDefault<URTSGlobalAbilitySettings>();
		return IsValid(Settings) ? Settings->GlobalAbilityDataAsset.LoadSynchronous() : nullptr;
	}
}

UPlayerAirDropSubsystem* UPlayerAirDropSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UPlayerAirDropSubsystem>() : nullptr;
}

bool UPlayerAirDropSubsystem::DeliverPlayerUnitsByAir(const FVector& LandingPoint, const FPlayerAirDropPayload& Payload)
{
	if (Payload.GetIsEmpty())
	{
		RTSFunctionLibrary::ReportError("DeliverPlayerUnitsByAir got an empty payload.");
		return false;
	}
	UGlobalAbilitiesManager* Manager = GetPlayerGlobalAbilitiesManager();
	if (not IsValid(Manager))
	{
		RTSFunctionLibrary::ReportError("DeliverPlayerUnitsByAir: the player has no global ability manager.");
		return false;
	}
	TArray<UGlobalAbility*> Abilities;
	const bool bCargoPlaneCarriesSquads = not Payload.bParachuteSquads;
	if (not Payload.Vehicles.IsEmpty() || (bCargoPlaneCarriesSquads && not Payload.Squads.IsEmpty()))
	{
		if (UGlobalAbility* CargoPlane = CreateCargoPlaneCopy(Payload))
		{
			Abilities.Add(CargoPlane);
		}
	}
	if (Payload.bParachuteSquads && not Payload.Squads.IsEmpty())
	{
		if (UGlobalAbility* Parachute = CreateParachuteCopy(Payload))
		{
			Abilities.Add(Parachute);
		}
	}
	ACPPController* PlayerController = Cast<ACPPController>(Manager->GetOwner());
	for (UGlobalAbility* Ability : Abilities)
	{
		M_RunningAbilities.Add(Ability);
		Ability->OnGlobalAbilityEnded.AddUObject(this, &UPlayerAirDropSubsystem::HandleAbilityEnded);
		Ability->InitGlobalAbility(PlayerAirDropConstants::LocalPlayerNumber, Manager, PlayerController);
		Ability->ExecuteAbilityAtLocation(LandingPoint);
	}
	return not Abilities.IsEmpty();
}

UGlobalAbility* UPlayerAirDropSubsystem::CreateCargoPlaneCopy(const FPlayerAirDropPayload& Payload) const
{
	UGlobalAbilitySetDataAsset* AbilitySet = GetAbilitySet();
	UGA_AircraftDrop* Copy = IsValid(AbilitySet)
		                         ? Cast<UGA_AircraftDrop>(
			                         AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Payload.CargoPlaneAbility))
		                         : nullptr;
	if (not IsValid(Copy))
	{
		RTSFunctionLibrary::ReportError("Player air drop: " + UEnum::GetValueAsString(Payload.CargoPlaneAbility) +
			" is not an aircraft drop ability in the global ability data asset.");
		return nullptr;
	}
	const TArray<ESquadSubtype> CargoSquads = Payload.bParachuteSquads ? TArray<ESquadSubtype>() : Payload.Squads;
	Copy->SetDropPayload(Payload.Vehicles, CargoSquads);
	return Copy;
}

UGlobalAbility* UPlayerAirDropSubsystem::CreateParachuteCopy(const FPlayerAirDropPayload& Payload) const
{
	UGlobalAbilitySetDataAsset* AbilitySet = GetAbilitySet();
	UGA_Paratroopers* Copy = IsValid(AbilitySet)
		                         ? Cast<UGA_Paratroopers>(
			                         AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Payload.ParachuteAbility))
		                         : nullptr;
	if (not IsValid(Copy))
	{
		RTSFunctionLibrary::ReportError("Player air drop: " + UEnum::GetValueAsString(Payload.ParachuteAbility) +
			" is not a paratrooper ability in the global ability data asset.");
		return nullptr;
	}
	Copy->SetCinematicSquadSubtypes(Payload.Squads);
	return Copy;
}

UGlobalAbilitiesManager* UPlayerAirDropSubsystem::GetPlayerGlobalAbilitiesManager() const
{
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	return IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
}

void UPlayerAirDropSubsystem::HandleAbilityEnded(UGlobalAbility* Ability)
{
	M_RunningAbilities.Remove(Ability);
}
