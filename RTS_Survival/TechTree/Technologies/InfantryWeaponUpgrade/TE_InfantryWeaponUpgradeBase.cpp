// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_InfantryWeaponUpgradeBase.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

void UTE_InfantryWeaponUpgradeBase::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Applying technology effect: " + GetName() + " to infantry weapons.");
	}
	Super::ApplyTechnologyEffect(WorldContextObject);
}

void UTE_InfantryWeaponUpgradeBase::OnApplyEffectToActor(AActor* ValidActor)
{
	// Cast to the Infantry unit class
	ASquadUnit* InfantryUnit = Cast<ASquadUnit>(ValidActor);
	if (IsValid(InfantryUnit))
	{
		// Get the InfantryWeaponMaster component or actor
		UWeaponState* Weapon = InfantryUnit->GetWeaponState();
		if (IsValid(Weapon))
		{
			FWeaponData* WeaponData = Weapon->GetWeaponDataToUpgrade();
			if (WeaponData)
			{
				ApplyEffectToWeapon(WeaponData);
				if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString("Upgraded weapon: " + Weapon->GetName() + " on squad unit.");
				}
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					"Data not found for weapon: " + Weapon->GetName() + "\n in upgrade: " + GetName());
			}
		}

		if (!bHasUpdatedGameState)
		{
			int32 OwningPlayer = InfantryUnit->GetOwningPlayer();
			if(OwningPlayer == -1)
			{
				RTSFunctionLibrary::ReportError("Owning player is -1 for infantry unit: " + InfantryUnit->GetName());
				return;
			}
			UpgradeGameStateForAffectedWeapons(InfantryUnit, OwningPlayer);
			bHasUpdatedGameState = true;
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("InfantryWeaponMaster is invalid on infantry unit: " + InfantryUnit->GetName());
	}
}

TSet<TSubclassOf<AActor>> UTE_InfantryWeaponUpgradeBase::GetTargetActorClasses() const
{
	return M_AffectedInfantryUnits;
}

void UTE_InfantryWeaponUpgradeBase::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	// To be implemented by child classes
}

void UTE_InfantryWeaponUpgradeBase::UpgradeGameStateForAffectedWeapons(UObject* WorldContextObject,
                                                                       const int32 OwningPlayer)
{
	if (ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
	{
		for (auto EachWeapon : M_AffectedWeapons)
		{
			FWeaponData* Data = GameState->GetWeaponDataOfPlayer(OwningPlayer, EachWeapon);
			if (Data)
			{
				ApplyEffectToWeapon(Data);
				if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString(
						"Upgraded weapon: " + Global_GetWeaponEnumAsString(EachWeapon) + " in game state.");
				}
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					"Data not found for weapon: " + Global_GetWeaponEnumAsString(EachWeapon) + "\n in player: " +
					FString::FromInt(OwningPlayer) + "\n in upgrade: " + GetName());
			}
		}
	}
}
