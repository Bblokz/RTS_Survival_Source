// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values for this component's properties
UGlobalAbilitiesManager::UGlobalAbilitiesManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UGlobalAbilitiesManager::InitGlobalAbilitiesManager(const int32 OwningPlayer,
                                                         TArray<EGlobalAbility> StartingGlobalAbilities, 
                                                         UW_GlobalAbilityPanel* W_GlobalAbilityPanel)
{
	M_OwningPlayer = OwningPlayer;
	if (IsPlayerAbilityManager())
	{
		Mw_GlobalAbilityPanel= W_GlobalAbilityPanel;
		(void)EnsureIsValidGlobalAbilityPanel();
	}
}



void UGlobalAbilitiesManager::BeginPlay()
{
	Super::BeginPlay();

	
}

bool UGlobalAbilitiesManager::IsPlayerAbilityManager() const
{
	return M_OwningPlayer == 1;
}

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityPanel() const
{
	if (not Mw_GlobalAbilityPanel.IsValid())
	{
		RTSFunctionLibrary::ReportError("GlobalAbilityManager: Invalid GlobalAbilityPanel reference."
								  "see player global ability manager");
		return false;
	}
	return true;
}



