// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values for this component's properties
UGlobalAbilitiesManager::UGlobalAbilitiesManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UGlobalAbilitiesManager::InitGlobalAbilitiesManager(const int32 OwningPlayer,
                                                         const TArray<EGlobalAbility>& GlobalAbilities,
                                                         UW_GlobalAbilityPanel* W_GlobalAbilityPanel,
                                                         TObjectPtr<UPlayerResourceManager> PlayerResourceMgr, 
                                                         TObjectPtr<UGameUnitManager> GameUnitMgr,
                                                         TObjectPtr<UPlayerTechManager> PlayerTechMgr)
{
	M_OwningPlayer = OwningPlayer;
	// Only the player will have to check for requirements and resources.
	if (IsPlayerAbilityManager())
	{
		Mw_GlobalAbilityPanel= W_GlobalAbilityPanel;
		(void)EnsureIsValidGlobalAbilityPanel();
		M_PlayerResourceManager = PlayerResourceMgr;
		M_GameUnitManager = GameUnitMgr;
		M_PlayerTechManager = PlayerTechMgr;
		(void)EnsureIsValidPlayerResourceManager();
		(void)EnsureIsValidPlayerTechManager();
		(void)EnsureIsValidGameUnitManager();
	}
	LoadGlobalAbilities(GlobalAbilities);
	
}



void UGlobalAbilitiesManager::BeginPlay()
{
	Super::BeginPlay();

	
}

bool UGlobalAbilitiesManager::QueryRequirementForAbility(TObjectPtr<UGlobalAbility> Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}
	if (not EnsureIsValidPlayerResourceManager() || not EnsureIsValidPlayerTechManager())
	{
		return false;
	}
	if (Ability->M_AbilityRequirements.RequiredTechnology != ETechnology::Tech_NONE)
	{
		if (not M_PlayerTechManager->HasTechResearched(Ability->M_AbilityRequirements.RequiredTechnology) )	
		{
			return false;
		}
	}
	if (not Ability->M_AbilityRequirements.RequiredUnit.IsNone()
	{
		
	}
}

bool UGlobalAbilitiesManager::QueryCostsForAbility(TObjectPtr<UGlobalAbility> Ability) const
{
	
	if (not IsValid(Ability))
	{
		return false;
	}
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

bool UGlobalAbilitiesManager::EnsureIsValidPlayerTechManager() const
{
	if (not M_PlayerTechManager.IsValid())
	{
		RTSFunctionLibrary::ReportError("GlobalAbilityManager: Invalid PlayerTechManager reference."
								  "see player global ability manager");
		return false;
	}
	return  true;
}

bool UGlobalAbilitiesManager::EnsureIsValidPlayerResourceManager() const
{
	if (not M_PlayerResourceManager.IsValid())
	{
		RTSFunctionLibrary::ReportError("GlobalAbilityManager: Invalid PlayerResourceManager reference."
								  "see player global ability manager");
		return false;
	}
	return true;
}

bool UGlobalAbilitiesManager::EnsureIsValidGameUnitManager() const
{
	if (not M_GameUnitManager.IsValid())
	{
		RTSFunctionLibrary::ReportError("GlobalAbilityManager: Invalid GameUnitManager reference."	
								  "see player global ability manager");
		return false;
	}
	return true;
}

void UGlobalAbilitiesManager::LoadGlobalAbilities(TArray<EGlobalAbility> AbilityEnums)
{
// todo load objects from	UGlobalAbilitySetDataAsset using the developer settings.
	// First async load the GlobalAbilityDataAsset.
	// then go through and get UObject copies
	// then call the OnLoadedAbiltiies with those abilities that were valid and obtained as copy objects from the
	// data asset.
}

void UGlobalAbilitiesManager::OnLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects)
{
	GlobalAbilities = AbilitiesLoadedCopyUObjects;
	if (IsPlayerAbilityManager())
	{
			
	}
}

void UGlobalAbilitiesManager::InitAbilityPanel(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities)
{
	if (not IsPlayerAbilityManager())
	{
		// For the enemy controller we do not need to setup any UI.
		return;
	}
	
}

void UGlobalAbilitiesManager::CheckRequirements() const
{
	
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveUnit(const FTrainingOption& UnitId) const
{
	// We assume a valid game unit manager here.
}



