#include "UTE_TurretWeaponUpgradeBase.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

void UTE_TurretWeaponUpgradeBase::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("applying technology effect:" + GetName() + " to turrets.");
	}
	Super::ApplyTechnologyEffect(WorldContextObject);
}

void UTE_TurretWeaponUpgradeBase::OnApplyEffectToActor(AActor* ValidActor)
{
	if (ACPPTurretsMaster* TurretMaster = Cast<ACPPTurretsMaster>(ValidActor); IsValid(TurretMaster))
	{
		// Check if the weapons array of the turret is big enough to access the weapon at ToUpgradeWeaponIndex
		if (!TurretMaster->GetWeapons().IsEmpty() && TurretMaster->GetWeapons().Num() >= ToUpgradeWeaponIndex)
		{
			if (UWeaponState* Weapon = TurretMaster->GetWeapons()[ToUpgradeWeaponIndex]; IsValid(Weapon))
			{
				FWeaponData* WeaponData = Weapon->GetWeaponDataToUpgrade();
				if (WeaponData)
				{
					ApplyEffectToWeapon(WeaponData);
					if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
					{
						RTSFunctionLibrary::PrintString("Upgraded weapon: " + Weapon->GetName() + " with APCR shell");
					}
				}
				else
				{
					RTSFunctionLibrary::ReportError(
						"Data not found for weapon: " + Weapon->GetName() + "\n in upgrade: " + GetName());
				}
			}
		}
		if (!bHasUpdatedGameState)
		{
			UpgradeGameStateForAffectedWeapons(TurretMaster, TurretMaster->GetOwningPlayer());
			bHasUpdatedGameState = true;
		}
	}
}

TSet<TSubclassOf<AActor>> UTE_TurretWeaponUpgradeBase::GetTargetActorClasses() const
{
	return M_AffectedTurrets;
}

void UTE_TurretWeaponUpgradeBase::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
}

void UTE_TurretWeaponUpgradeBase::UpgradeGameStateForAffectedWeapons(UObject* WorldContextObject,
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
				if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString(
						"Upgraded weapon: " + Global_GetWeaponEnumAsString(EachWeapon) + "IN GAMESTATE");
				}
				continue;
			}
			RTSFunctionLibrary::ReportError("Data not found for weapon: " + Global_GetWeaponEnumAsString(EachWeapon) +
				"\n in player: " + FString::FromInt(OwningPlayer) +
				"\n in upgrade: " + GetName());
		}
	}
}
