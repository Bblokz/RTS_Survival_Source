// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
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
		if(EnsureIsValidGlobalAbilityPanel())
		{
			Mw_GA_Description = Mw_GlobalAbilityPanel->GetDescription();
			(void)EnsureIsValidGlobalAbilityDescription();
		}
		
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
	if (not Ability->M_AbilityRequirements.RequiredUnit.IsNone())
	{
		return CheckDoesPlayerHaveUnit(Ability->M_AbilityRequirements.RequiredUnit);	
	}
	// No requirements.
	return true;
}

bool UGlobalAbilitiesManager::QueryCostsForAbility(TObjectPtr<UGlobalAbility> Ability) const
{
	
	if (not IsValid(Ability))
	{
		return false;
	}
	if (not EnsureIsValidPlayerResourceManager())
	{
		return false;
	}
	EPlayerError Error =  M_PlayerResourceManager->GetCanPayForCost(Ability->M_AbilityCosts.Costs.ResourceCosts);
	if (Error != EPlayerError::Error_None)
	{
		return false;
	}
	return true;
}

void UGlobalAbilitiesManager::OnHoveredAbilityButton(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	// todo if hovered set description to visible if not hovered set description widget to collapsed.
	// if blocked by technology set description with sBad Requires : [name of tech]
	// if blocked by unit requirement set description with sBad Requires [name of unit]
	// if on cooldown set sBad on cooldown
	// if all of that not the case: set description from ability on hover widget description.
	// use URTSRichTextConverter.
}

void UGlobalAbilitiesManager::OnClickedAbilityButton(UGlobalAbility* ClickedAbility)
{
	if (not IsValid(ClickedAbility))
	{
		return;
	}
	ClickedAbility->OnClickedAbilityButton();
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

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityDescription() const
{
	if (not Mw_GA_Description.IsValid())
	{
		RTSFunctionLibrary::ReportError("GlobalAbilityManager: Invalid GlobalAbilityDescription reference."
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
	switch (UnitId.UnitType) {
	case EAllUnitType::UNType_None:
		return true;
	case EAllUnitType::UNType_Squad:
		const ESquadSubtype SubtypeSquad = static_cast<ESquadSubtype>(UnitId.SubtypeValue);
		return M_GameUnitManager->GetHasPlayerSquadOfType(SubtypeSquad);
	case EAllUnitType::UNType_Harvester:
		return true;
	case EAllUnitType::UNType_Tank:
		const ETankSubtype SubtypeTank = static_cast<ETankSubtype>(UnitId.SubtypeValue);
		return M_GameUnitManager->GetHasPlayerTankOfType(SubtypeTank);
	case EAllUnitType::UNType_Nomadic:
		const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitId.SubtypeValue);
		return M_GameUnitManager->GetHasPlayerNomadicOfType(NomadicSubtype);
	case EAllUnitType::UNType_BuildingExpansion:
		const EBuildingExpansionType BxpSubtype = static_cast<EBuildingExpansionType>(UnitId.SubtypeValue);
		return M_GameUnitManager->GetHasPlayerBxpOfType(BxpSubtype);
	case EAllUnitType::UNType_Aircraft:
		const EAircraftSubtype AircraftSubtype = static_cast<EAircraftSubtype>(UnitId.SubtypeValue);
		return M_GameUnitManager->GetHasPlayerAircraftOfType(AircraftSubtype);
	}
	return true;
}



