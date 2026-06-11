// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "PlayerTechManager.h"

#include "Engine/AssetManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ResearchTechnologyAbilityComponent/ResearchTechnologyAbilityComp.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

TArray<ETechnology> UPlayerTechManager::GetMissingRequiredTechnologies(
	const TArray<ETechnology>& RequiredTechnologies) const
{
	TArray<ETechnology> MissingRequiredTechnologies;
	for (const ETechnology RequiredTechnology : RequiredTechnologies)
	{
		if (HasTechResearched(RequiredTechnology))
		{
			continue;
		}

		MissingRequiredTechnologies.Add(RequiredTechnology);
	}

	return MissingRequiredTechnologies;
}

bool UPlayerTechManager::HasAllRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const
{
	return GetMissingRequiredTechnologies(RequiredTechnologies).IsEmpty();
}

bool UPlayerTechManager::GetIsTechnologyResearchedOrPending(const ETechnology Tech) const
{
	return M_ResearchedTechs.Contains(Tech) || M_TechnologiesWaitingForEffectLoad.Contains(Tech);
}

void UPlayerTechManager::RegisterResearchTechnologyAbilityComp(
	UResearchTechnologyAbilityComp* ResearchTechnologyAbilityComp)
{
	if (not IsValid(ResearchTechnologyAbilityComp))
	{
		return;
	}

	RemoveInvalidResearchTechnologyAbilityComps();
	M_ResearchTechnologyAbilityComps.AddUnique(
		TWeakObjectPtr<UResearchTechnologyAbilityComp>(ResearchTechnologyAbilityComp));

	const TArray<ETechnology> ResearchedAndPendingTechnologies = GetResearchedAndPendingTechnologies();
	for (const ETechnology ResearchedTechnology : ResearchedAndPendingTechnologies)
	{
		ResearchTechnologyAbilityComp->AccountForResearchedTechnology(ResearchedTechnology);
	}
}

void UPlayerTechManager::OnTechResearched(ETechnology Tech)
{
	if (GetIsTechnologyResearchedOrPending(Tech))
	{
		return;
	}

	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		const FString TechName = UEnum::GetValueAsString(Tech);
		RTSFunctionLibrary::PrintString("Technology researched: " + TechName);
	}

	const TSoftClassPtr<UTechnologyEffect>* EffectClassPtr = M_TechnologyEffectsMap.Find(Tech);
	if (EffectClassPtr == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"The technology effect for " + UEnum::GetValueAsString(Tech) +
			" is not found in the TechnologyEffectsMap.\nAdd the effect to the map in the PlayerTechManager actor component.");
		return;
	}

	if (EffectClassPtr->IsValid())
	{
		RegisterAndApplyLoadedTechnology(Tech, NewObject<UTechnologyEffect>(this, EffectClassPtr->Get()));
		if (M_ResearchedTechs.Contains(Tech))
		{
			NotifyResearchTechnologyAbilityComps(Tech);
		}
		return;
	}

	M_TechnologiesWaitingForEffectLoad.Add(Tech);
	NotifyResearchTechnologyAbilityComps(Tech);
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(
		EffectClassPtr->ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UPlayerTechManager::OnTechnologyEffectLoaded, Tech));
}

void UPlayerTechManager::NotifyResearchTechnologyAbilityComps(const ETechnology Tech)
{
	RemoveInvalidResearchTechnologyAbilityComps();
	const TArray<TWeakObjectPtr<UResearchTechnologyAbilityComp>> RegisteredResearchTechnologyAbilityComps =
		M_ResearchTechnologyAbilityComps;
	bool bHasPlayedCustomAnnouncerVoiceLine = false;
	for (const TWeakObjectPtr<UResearchTechnologyAbilityComp>& ResearchTechnologyAbilityComp :
		 RegisteredResearchTechnologyAbilityComps)
	{
		if (not ResearchTechnologyAbilityComp.IsValid())
		{
			continue;
		}

		if (not bHasPlayedCustomAnnouncerVoiceLine)
		{
			bHasPlayedCustomAnnouncerVoiceLine =
				ResearchTechnologyAbilityComp->TryPlayCompletedAnnouncerVoiceLine(Tech);
		}

		ResearchTechnologyAbilityComp->AccountForResearchedTechnology(Tech);
	}
}

void UPlayerTechManager::RemoveInvalidResearchTechnologyAbilityComps()
{
	M_ResearchTechnologyAbilityComps.RemoveAll(
		[](const TWeakObjectPtr<UResearchTechnologyAbilityComp>& ResearchTechnologyAbilityComp)
		{
			return not ResearchTechnologyAbilityComp.IsValid();
		});
}

TArray<ETechnology> UPlayerTechManager::GetResearchedAndPendingTechnologies() const
{
	TArray<ETechnology> ResearchedAndPendingTechnologies = M_ResearchedTechs.Array();
	for (const ETechnology PendingTechnology : M_TechnologiesWaitingForEffectLoad)
	{
		ResearchedAndPendingTechnologies.AddUnique(PendingTechnology);
	}

	return ResearchedAndPendingTechnologies;
}

void UPlayerTechManager::CheckTechsToApplyToTank(ATankMaster* Tank) const
{
	if (not IsValid(Tank))
	{
		return;
	}

	const URTSComponent* RTSComponent = Tank->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetUnitType() != EAllUnitType::UNType_Tank)
	{
		return;
	}

	const ETankSubtype TankSubtype = RTSComponent->GetSubtypeAsTankSubtype();
	for (UTechnologyEffect* TechEffect : M_ResearchedTechnologyEffects)
	{
		if (IsValid(TechEffect) && TechEffect->GetTanksToApplyTo().Contains(TankSubtype))
		{
			TechEffect->ApplyOnTank(Tank);
		}
	}
}

void UPlayerTechManager::CheckTechsToApplyToNomadic(ANomadicVehicle* Nomadic) const
{
	if (not IsValid(Nomadic))
	{
		return;
	}

	const URTSComponent* RTSComponent = Nomadic->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetUnitType() != EAllUnitType::UNType_Nomadic)
	{
		return;
	}

	const ENomadicSubtype NomadicSubtype = RTSComponent->GetSubtypeAsNomadicSubtype();
	for (UTechnologyEffect* TechEffect : M_ResearchedTechnologyEffects)
	{
		if (IsValid(TechEffect) && TechEffect->GetNomadicsToApplyTo().Contains(NomadicSubtype))
		{
			TechEffect->ApplyOnNomadic(Nomadic);
		}
	}
}

void UPlayerTechManager::CheckTechsToApplyToSquad(ASquadController* Squad) const
{
	if (not IsValid(Squad))
	{
		return;
	}

	const URTSComponent* RTSComponent = Squad->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetUnitType() != EAllUnitType::UNType_Squad)
	{
		return;
	}

	const ESquadSubtype SquadSubtype = RTSComponent->GetSubtypeAsSquadSubtype();
	for (UTechnologyEffect* TechEffect : M_ResearchedTechnologyEffects)
	{
		if (IsValid(TechEffect) && TechEffect->GetSquadsToApplyTo().Contains(SquadSubtype))
		{
			TechEffect->ApplyOnSquad(Squad);
		}
	}
}

void UPlayerTechManager::CheckTechsToApplyToBuildingExpansion(ABuildingExpansion* BuildingExpansion) const
{
	if (not IsValid(BuildingExpansion))
	{
		return;
	}

	const URTSComponent* RTSComponent = BuildingExpansion->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetUnitType() != EAllUnitType::UNType_BuildingExpansion)
	{
		return;
	}

	const EBuildingExpansionType BxpSubtype = RTSComponent->GetSubtypeAsBxpSubtype();
	for (UTechnologyEffect* TechEffect : M_ResearchedTechnologyEffects)
	{
		if (IsValid(TechEffect) && TechEffect->GetBuildingExpansionsToApplyTo().Contains(BxpSubtype))
		{
			TechEffect->ApplyOnBuildingExpansion(BuildingExpansion);
		}
	}
}

void UPlayerTechManager::CheckTechsToApplyToAircraft(AAircraftMaster* Aircraft) const
{
	if (not IsValid(Aircraft))
	{
		return;
	}

	const URTSComponent* RTSComponent = Aircraft->GetRTSComponent();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	if (RTSComponent->GetUnitType() != EAllUnitType::UNType_Aircraft)
	{
		return;
	}

	const EAircraftSubtype AircraftSubtype = RTSComponent->GetSubtypeAsAircraftSubtype();
	for (UTechnologyEffect* TechEffect : M_ResearchedTechnologyEffects)
	{
		if (IsValid(TechEffect) && TechEffect->GetAircraftToApplyTo().Contains(AircraftSubtype))
		{
			TechEffect->ApplyOnAircraft(Aircraft);
		}
	}
}

void UPlayerTechManager::InitTechsInManager(ACPPController* Controller)
{
	if (not IsValid(Controller))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "Controller", "InitTechsInManager");
		return;
	}

	M_TechnologyEffectsMap = Controller->GetTechnologyEffectsMap();
}

void UPlayerTechManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerTechManager::OnTechnologyEffectLoaded(ETechnology Tech)
{
	M_TechnologiesWaitingForEffectLoad.Remove(Tech);
	const TSoftClassPtr<UTechnologyEffect>* EffectClassPtr = M_TechnologyEffectsMap.Find(Tech);
	if (EffectClassPtr == nullptr || not EffectClassPtr->IsValid())
	{
		RTSFunctionLibrary::ReportError("Technology effect class for " + UEnum::GetValueAsString(Tech) +
			" is not loaded after async load.");
		return;
	}

	RegisterAndApplyLoadedTechnology(Tech, NewObject<UTechnologyEffect>(this, EffectClassPtr->Get()));
}

void UPlayerTechManager::RegisterAndApplyLoadedTechnology(ETechnology Tech, UTechnologyEffect* TechEffect)
{
	if (M_ResearchedTechs.Contains(Tech))
	{
		return;
	}

	if (not IsValid(TechEffect))
	{
		RTSFunctionLibrary::ReportError("Failed to create technology effect instance for " + UEnum::GetValueAsString(Tech));
		return;
	}

	TechEffect->SetTechnology(Tech);
	M_ResearchedTechs.Add(Tech);
	M_ResearchedTechnologyEffects.Add(TechEffect);
	ApplyTechnologyToCurrentUnits(TechEffect);
}

void UPlayerTechManager::ApplyTechnologyToCurrentUnits(UTechnologyEffect* TechEffect) const
{
	if (not IsValid(TechEffect))
	{
		return;
	}

	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}

	const TArray<ETankSubtype> TankSubtypes = TechEffect->GetTanksToApplyTo();
	if (not TankSubtypes.IsEmpty())
	{
		GameUnitManager->ApplyTechToTanksOfPlayer(TechEffect, TankSubtypes, PlayerTechOwnerIndex);
	}

	const TArray<ENomadicSubtype> NomadicSubtypes = TechEffect->GetNomadicsToApplyTo();
	if (not NomadicSubtypes.IsEmpty())
	{
		GameUnitManager->ApplyTechToNomadicsOfPlayer(TechEffect, NomadicSubtypes, PlayerTechOwnerIndex);
	}

	const TArray<ESquadSubtype> SquadSubtypes = TechEffect->GetSquadsToApplyTo();
	if (not SquadSubtypes.IsEmpty())
	{
		GameUnitManager->ApplyTechToSquadsOfPlayer(TechEffect, SquadSubtypes, PlayerTechOwnerIndex);
	}

	const TArray<EBuildingExpansionType> BxpSubtypes = TechEffect->GetBuildingExpansionsToApplyTo();
	if (not BxpSubtypes.IsEmpty())
	{
		GameUnitManager->ApplyTechToBuildingExpansionsOfPlayer(TechEffect, BxpSubtypes, PlayerTechOwnerIndex);
	}

	const TArray<EAircraftSubtype> AircraftSubtypes = TechEffect->GetAircraftToApplyTo();
	if (not AircraftSubtypes.IsEmpty())
	{
		GameUnitManager->ApplyTechToAircraftOfPlayer(TechEffect, AircraftSubtypes, PlayerTechOwnerIndex);
	}
}
