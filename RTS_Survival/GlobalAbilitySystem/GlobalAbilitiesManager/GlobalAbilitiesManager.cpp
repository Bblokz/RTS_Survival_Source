// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Description/W_GA_Description.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Item/W_GA_Item.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"


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
		M_GlobalAbilityPanel= W_GlobalAbilityPanel;
		if (EnsureIsValidGlobalAbilityPanel())
		{
			M_GA_Description = M_GlobalAbilityPanel.Get()->GetDescription();
			if (EnsureIsValidGlobalAbilityDescription())
			{
				M_GA_Description.Get()->HideDescription();
			}
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
	if (not EnsureIsValidPlayerTechManager() || not EnsureIsValidGameUnitManager())
	{
		return false;
	}
	if (Ability->M_AbilityRequirements.RequiredTechnology != ETechnology::Tech_NONE)
	{
		if (not M_PlayerTechManager.Get()->HasTechResearched(Ability->M_AbilityRequirements.RequiredTechnology))
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
	EPlayerError Error =  M_PlayerResourceManager.Get()->GetCanPayForCost(Ability->M_AbilityCosts.Costs.ResourceCosts);
	if (Error != EPlayerError::Error_None)
	{
		return false;
	}
	return true;
}

void UGlobalAbilitiesManager::OnHoveredAbilityButton(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	if (not IsPlayerAbilityManager() || not EnsureIsValidGlobalAbilityDescription())
	{
		return;
	}
	if (not bIsHover || GetHasActiveAbility() || not IsValid(HoveredAbility))
	{
		M_GA_Description.Get()->HideDescription();
		return;
	}
	M_GA_Description.Get()->ShowDescription(
		HoveredAbility->M_UISettings,
		HoveredAbility->M_AbilityCosts,
		GetHoverDescriptionForAbility(HoveredAbility));
}

void UGlobalAbilitiesManager::OnClickedAbilityButton(UGlobalAbility* ClickedAbility)
{
	if (not IsValid(ClickedAbility) || GetHasActiveAbility())
	{
		return;
	}
	ClickedAbility->OnClickedAbilityButton();
	if (EnsureIsValidGlobalAbilityDescription())
	{
		M_GA_Description.Get()->HideDescription();
	}
}

bool UGlobalAbilitiesManager::IsPlayerAbilityManager() const
{
	return M_OwningPlayer == 1;
}

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityPanel() const
{
	if (M_GlobalAbilityPanel.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GlobalAbilityPanel"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityPanel"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityDescription() const
{
	if (M_GA_Description.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GA_Description"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityDescription"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidPlayerTechManager() const
{
	if (M_PlayerTechManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerTechManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidPlayerTechManager"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerResourceManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidPlayerResourceManager"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidGameUnitManager() const
{
	if (M_GameUnitManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GameUnitManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGameUnitManager"),
		this
	);
	return false;
}

void UGlobalAbilitiesManager::LoadGlobalAbilities(TArray<EGlobalAbility> AbilityEnums)
{
	const URTSGlobalAbilitySettings* GlobalAbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	if (not IsValid(GlobalAbilitySettings))
	{
		return;
	}
	UGlobalAbilitySetDataAsset* AbilitySetDataAsset = GlobalAbilitySettings->GlobalAbilityDataAsset.LoadSynchronous();
	if (not IsValid(AbilitySetDataAsset))
	{
		RTSFunctionLibrary::ReportError(TEXT("Global ability data asset is not valid in global ability settings."));
		return;
	}

	TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects;
	for (const EGlobalAbility AbilityEnum : AbilityEnums)
	{
		UGlobalAbility* AbilityCopy = AbilitySetDataAsset->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityEnum);
		if (not IsValid(AbilityCopy))
		{
			continue;
		}
		AbilityCopy->InitGlobalAbility(M_OwningPlayer, this, Cast<ACPPController>(GetOwner()));
		AbilitiesLoadedCopyUObjects.Add(AbilityCopy);
	}
	OnLoadedAbilities(AbilitiesLoadedCopyUObjects);
}

void UGlobalAbilitiesManager::OnLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects)
{
	M_GlobalAbilities = AbilitiesLoadedCopyUObjects;
	if (IsPlayerAbilityManager())
	{
		InitAbilityPanel(AbilitiesLoadedCopyUObjects);
		CheckRequirements();
		StartPlayerRequirementTimer();
	}
}

void UGlobalAbilitiesManager::InitAbilityPanel(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities)
{
	if (not IsPlayerAbilityManager() || not EnsureIsValidGlobalAbilityPanel())
	{
		return;
	}
	M_GlobalAbilityPanel.Get()->InitWithLoadedAbilities(LoadedAbilities, this);
}

void UGlobalAbilitiesManager::StartPlayerRequirementTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	constexpr float RequirementCheckInterval = 1.f;
	World->GetTimerManager().SetTimer(
		M_CheckRequirementsTimerHandle,
		this,
		&UGlobalAbilitiesManager::CheckRequirements,
		RequirementCheckInterval,
		true);
}

void UGlobalAbilitiesManager::CheckRequirements()
{
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		UpdateAbilityAvailability(Ability);
	}
}

void UGlobalAbilitiesManager::UpdateAbilityAvailability(UGlobalAbility* Ability)
{
	if (not IsValid(Ability))
	{
		return;
	}
	if (Ability->M_AbilityCosts.CoolDownRemaining > 0)
	{
		Ability->M_AbilityCosts.CoolDownRemaining = FMath::Max(0, Ability->M_AbilityCosts.CoolDownRemaining - 1);
	}
	const bool bMeetsRequirements = QueryRequirementForAbility(Ability);
	const bool bCanPayCost = QueryCostsForAbility(Ability);
	const bool bIsOnCooldown = Ability->M_AbilityCosts.CoolDownRemaining > 0;
	UpdateAbilityItemAvailability(
		Ability,
		bMeetsRequirements && bCanPayCost && not bIsOnCooldown,
		not bMeetsRequirements);
}

void UGlobalAbilitiesManager::UpdateAbilityItemAvailability(const UGlobalAbility* Ability, const bool bIsEnabled, const bool bUseGreyTint) const
{
	if (not EnsureIsValidGlobalAbilityPanel())
	{
		return;
	}
	for (UW_GA_Item* AbilityItem : M_GlobalAbilityPanel.Get()->GetAbilityItems())
	{
		if (IsValid(AbilityItem) && AbilityItem->GetLoadedAbility() == Ability)
		{
			AbilityItem->SetAbilityAvailable(bIsEnabled, bUseGreyTint);
			return;
		}
	}
}

FText UGlobalAbilitiesManager::GetHoverDescriptionForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return FText::GetEmpty();
	}
	if (Ability->M_AbilityRequirements.RequiredTechnology != ETechnology::Tech_NONE
		&& not M_PlayerTechManager.Get()->HasTechResearched(Ability->M_AbilityRequirements.RequiredTechnology))
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString(TEXT("Required Tech: ")) + Global_GetTechDisplayName(Ability->M_AbilityRequirements.RequiredTechnology),
			ERTSRichText::Text_Bad14));
	}
	if (not Ability->M_AbilityRequirements.RequiredUnit.IsNone()
		&& not CheckDoesPlayerHaveUnit(Ability->M_AbilityRequirements.RequiredUnit))
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString(TEXT("Required Unit: ")) + GetUnitRequirementDisplayName(Ability->M_AbilityRequirements.RequiredUnit),
			ERTSRichText::Text_Bad14));
	}
	if (Ability->M_AbilityCosts.CoolDownRemaining > 0)
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString::Printf(TEXT("On Cooldown: %d"), Ability->M_AbilityCosts.CoolDownRemaining),
			ERTSRichText::Text_Bad14));
	}
	return Ability->M_UISettings.Description;
}

FString UGlobalAbilitiesManager::GetUnitRequirementDisplayName(const FTrainingOption& UnitId) const
{
	switch (UnitId.UnitType)
	{
	case EAllUnitType::UNType_Squad:
		return Global_GetSquadDisplayName(static_cast<ESquadSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Tank:
		return Global_GetTankDisplayName(static_cast<ETankSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Nomadic:
		return Global_GetNomadicDisplayName(static_cast<ENomadicSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_BuildingExpansion:
		return Global_GetBxpDisplayString(static_cast<EBuildingExpansionType>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Aircraft:
		return Global_GetAircraftDisplayName(static_cast<EAircraftSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Harvester:
		return TEXT("Harvester");
	default:
		return TEXT("Unit");
	}
}

void UGlobalAbilitiesManager::OnAbilityFinishedExecuting(UGlobalAbility* Ability)
{
	if (not IsValid(Ability))
	{
		return;
	}
	Ability->M_AbilityCosts.CoolDownRemaining = Ability->M_AbilityCosts.CoolDownTime;
	if (IsPlayerAbilityManager())
	{
		UpdateAbilityItemAvailability(Ability, false, false);
	}
}

bool UGlobalAbilitiesManager::TryPayForAbility(UGlobalAbility* Ability) const
{
	if (not IsValid(Ability) || not EnsureIsValidPlayerResourceManager())
	{
		return false;
	}
	return M_PlayerResourceManager.Get()->PayForCosts(Ability->M_AbilityCosts.Costs.ResourceCosts);
}

bool UGlobalAbilitiesManager::GetHasActiveAbility() const
{
	for (const UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (IsValid(Ability) && Ability->M_AbilityState == EGlobalAbilityState::Activated)
		{
			return true;
		}
	}
	return false;
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveUnit(const FTrainingOption& UnitId) const
{
	// We assume a valid game unit manager here.
	switch (UnitId.UnitType)
	{
	case EAllUnitType::UNType_None:
	case EAllUnitType::UNType_Harvester:
		return true;
	case EAllUnitType::UNType_Squad:
		return CheckDoesPlayerHaveSquad(UnitId);
	case EAllUnitType::UNType_Tank:
		return CheckDoesPlayerHaveTank(UnitId);
	case EAllUnitType::UNType_Nomadic:
		return CheckDoesPlayerHaveNomadic(UnitId);
	case EAllUnitType::UNType_BuildingExpansion:
		return CheckDoesPlayerHaveBuildingExpansion(UnitId);
	case EAllUnitType::UNType_Aircraft:
		return CheckDoesPlayerHaveAircraft(UnitId);
	}
	return true;
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveSquad(const FTrainingOption& UnitId) const
{
	const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerSquadOfType(SquadSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveTank(const FTrainingOption& UnitId) const
{
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerTankOfType(TankSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveNomadic(const FTrainingOption& UnitId) const
{
	const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerNomadicOfType(NomadicSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveBuildingExpansion(const FTrainingOption& UnitId) const
{
	const EBuildingExpansionType BuildingExpansionSubtype = static_cast<EBuildingExpansionType>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerBxpOfType(BuildingExpansionSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveAircraft(const FTrainingOption& UnitId) const
{
	const EAircraftSubtype AircraftSubtype = static_cast<EAircraftSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerAircraftOfType(AircraftSubtype);
}

