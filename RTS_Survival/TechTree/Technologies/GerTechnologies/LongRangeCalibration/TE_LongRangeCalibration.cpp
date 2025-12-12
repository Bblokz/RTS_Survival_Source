// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_LongRangeCalibration.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

class ACPPGameState;

void UTE_LongRangeCalibration::OnApplyEffectToActor(AActor* ValidActor)
{
	Super::OnApplyEffectToActor(ValidActor);
	if (ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(ValidActor); IsValid(Turret))
	{
		for (const auto EachWeapon : Turret->GetWeapons())
		{
			if (IsValid(EachWeapon) && M_AffectedWeapons.Contains(EachWeapon->GetRawWeaponData().WeaponName))
			{
				ApplyeffectToWeapon(EachWeapon, Turret);
			}
		}
		if (!bHasUpgradedGameState)
		{
			bHasUpgradedGameState = true;
			UpgradeGameStateForAffectedWeapons(Turret, Turret->GetOwningPlayer());
		}
	}
}

void UTE_LongRangeCalibration::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	Super::ApplyTechnologyEffect(WorldContextObject);
}

TSet<TSubclassOf<AActor>> UTE_LongRangeCalibration::GetTargetActorClasses() const
{
	return M_AffectedTurrets;
}

void UTE_LongRangeCalibration::ApplyeffectToWeapon(UWeaponState* ValidWeaponState, ACPPTurretsMaster* ValidTurret)
{
	ValidTurret->UpgradeWeaponRange(ValidWeaponState, M_RangeMlt);
}

void UTE_LongRangeCalibration::UpgradeGameStateForAffectedWeapons(ACPPTurretsMaster* ValidTurret,
                                                                  const int32 OwningPlayer)
{
	if (ACPPGameState* GameState = FRTS_Statics::GetGameState(ValidTurret))
	{
		for (auto EachWeapon : M_AffectedWeapons)
		{
			FWeaponData* Data = GameState->GetWeaponDataOfPlayer(OwningPlayer, EachWeapon);
			if (Data)
			{
				Data->Range *= M_RangeMlt;
				GameState->UpgradeWeaponDataForPlayer(OwningPlayer, EachWeapon, *Data);
				if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString(
						"Upgraded weapon: " + Global_GetWeaponEnumAsString(EachWeapon) + " Range upgrade");
				}
			}
		}
	}
}
