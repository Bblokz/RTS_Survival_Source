// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AllGlobalAbilitiesUnlock.h"

#if RTS_WITH_ALL_GLOBAL_ABILITIES

#include "HAL/IConsoleManager.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

TArray<EGlobalAbility> RTS::AllGlobalAbilities::GetAllTemplatedAbilities()
{
	TArray<EGlobalAbility> Abilities;
	const URTSGlobalAbilitySettings* Settings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* AbilitySet = IsValid(Settings)
		                                               ? Settings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	const UEnum* AbilityEnum = StaticEnum<EGlobalAbility>();
	if (not IsValid(AbilitySet) || not IsValid(AbilityEnum))
	{
		UE_LOG(LogTemp, Error, TEXT("RTS_ALL_GLOBAL_ABILITIES no global ability data asset; the player gets none."));
		return Abilities;
	}
	// NumEnums includes the generated _MAX entry.
	for (int32 EnumIndex = 0; EnumIndex < AbilityEnum->NumEnums() - 1; ++EnumIndex)
	{
		const EGlobalAbility Ability = static_cast<EGlobalAbility>(AbilityEnum->GetValueByIndex(EnumIndex));
		if (Ability == EGlobalAbility::GA_None)
		{
			continue;
		}
		if (IsValid(AbilitySet->FindAbilityTemplate(Ability)))
		{
			Abilities.Add(Ability);
			continue;
		}
		UE_LOG(LogTemp, Display, TEXT("RTS_ALL_GLOBAL_ABILITIES no template for %s"),
		       *AbilityEnum->GetNameStringByIndex(EnumIndex));
	}
	UE_LOG(LogTemp, Display, TEXT("RTS_ALL_GLOBAL_ABILITIES the player gets %d global abilities"), Abilities.Num());
	return Abilities;
}

namespace
{
	void PageGlobalAbilities(const TArray<FString>& Args, UWorld* World)
	{
		const ACPPController* Controller = FRTS_Statics::GetRTSController(World);
		UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
		if (not IsValid(Manager))
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS.GlobalAbilities.Page: no player global ability manager."));
			return;
		}
		int32 Page = Manager->GetAbilityPanelPage() + 1;
		if (Args.Num() > 0)
		{
			if (Args[0].Equals(TEXT("prev"), ESearchCase::IgnoreCase))
			{
				Page = Manager->GetAbilityPanelPage() - 1;
			}
			else if (Args[0].IsNumeric())
			{
				// Pages are numbered from 1 for the tester.
				Page = FCString::Atoi(*Args[0]) - 1;
			}
		}
		Manager->ShowAbilityPanelPage(Page);
	}

	FAutoConsoleCommandWithWorldAndArgs GPageGlobalAbilitiesCommand(
		TEXT("RTS.GlobalAbilities.Page"),
		TEXT("Ability test build: shows the next page of 8 global abilities in the panel. Args: none (next), prev, or a page number from 1."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PageGlobalAbilities));
}

#endif
