// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "PlayerResourceManager.h"

#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"
#include "RTS_Survival/Game/GameSettings/RTSGameSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/BxpDataHelpers/BxpConstructionRulesHelpers.h"
#include "RTS_Survival/Game/GameUpdateComponent/RTSGameSettingsHandler.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UPlayerResourceManager::UPlayerResourceManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// fill resource mapping.
	M_PlayerResources.Add(ERTSResourceType::Resource_Radixite, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Resource_Metal, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Resource_VehicleParts, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Resource_Fuel, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Resource_Ammo, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Blueprint_Weapon, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Blueprint_Vehicle, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Blueprint_Energy, FRTSResourceStorage{0, 0});
	M_PlayerResources.Add(ERTSResourceType::Blueprint_Construction, FRTSResourceStorage{0, 0});
}

void UPlayerResourceManager::BeginPlay_InitResourceWatcher()
{
	using DeveloperSettings::GamePlay::Audio::Anouncer::ResourceWatcherUpdateInterval;

	if (UWorld* World = GetWorld())
	{
		for (const auto EachWatchResource : M_ResourcesToWatch)
		{
			if (M_PlayerResources.Contains(EachWatchResource))
			{
				M_LastResourceSnapShot.Add(EachWatchResource, M_PlayerResources[EachWatchResource].Amount);
			}
			else
			{
				RTSFunctionLibrary::ReportError("Failed to setup resource watcher for resource: " +
					Global_GetResourceTypeAsString(EachWatchResource) +
					"\n See function BeginPlay_InitResourceWatcher in PlayerResourceManager.cpp.");
			}
		}
		World->GetTimerManager().SetTimer(M_ResourceWatchTimer, this,
		                                  &UPlayerResourceManager::ResourceWatcher, ResourceWatcherUpdateInterval,
		                                  true);
	}
}

void UPlayerResourceManager::PlayResourceTickSound(const bool bPlay, const float IntensityRequested) const
{
	if (GetIsValidPlayerAudioController())
	{
		M_PlayerAudioController->PlayResourceTickSound(bPlay, IntensityRequested);
	}
}

void UPlayerResourceManager::BeginPlay_InitResourceFullCheckTimerManager()
{
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate TimerDel;
		TimerDel.BindLambda([this]()
		{
			OnResourceFullCheck();
		});
		float Interval = DeveloperSettings::GamePlay::Audio::CheckResourceStorageFullInterval;
		World->GetTimerManager().SetTimer(M_ResourceFullCheckTimerManager,
		                                  TimerDel,
		                                  Interval,
		                                  true);
	}
}

void UPlayerResourceManager::OnResourceFullCheck()
{
	switch (M_ResourceFullCheck_ToCheckResource)
	{
	case ERTSResourceType::Resource_None:
		break;
	case ERTSResourceType::Resource_Radixite:
		if (M_PlayerResources.Contains(ERTSResourceType::Resource_Radixite))
		{
			const bool bStorageBiggerThanZero =
				M_PlayerResources[ERTSResourceType::Resource_Radixite].Storage > 0;
			if (bStorageBiggerThanZero && M_PlayerResources[ERTSResourceType::Resource_Radixite].Amount >=
				M_PlayerResources[ERTSResourceType::Resource_Radixite].Storage)
			{
				ResourceFullAnnounce(ERTSResourceType::Resource_Radixite);
			}
			M_ResourceFullCheck_ToCheckResource = ERTSResourceType::Resource_Metal;
		}
		break;
	case ERTSResourceType::Resource_Metal:
		if (M_PlayerResources.Contains(ERTSResourceType::Resource_Metal))
		{
			const bool bStorageBiggerThanZero =
				M_PlayerResources[ERTSResourceType::Resource_Metal].Storage > 0;
			if ( bStorageBiggerThanZero && M_PlayerResources[ERTSResourceType::Resource_Metal].Amount >=
				M_PlayerResources[ERTSResourceType::Resource_Metal].Storage)
			{
				ResourceFullAnnounce(ERTSResourceType::Resource_Metal);
			}
		}
		M_ResourceFullCheck_ToCheckResource = ERTSResourceType::Resource_VehicleParts;
		break;
	case ERTSResourceType::Resource_VehicleParts:
		if (M_PlayerResources.Contains(ERTSResourceType::Resource_VehicleParts))
		{
			const bool bStorageBiggerThanZero =
				M_PlayerResources[ERTSResourceType::Resource_VehicleParts].Storage > 0;
			if ( bStorageBiggerThanZero && M_PlayerResources[ERTSResourceType::Resource_VehicleParts].Amount >=
				M_PlayerResources[ERTSResourceType::Resource_VehicleParts].Storage)
			{
				ResourceFullAnnounce(ERTSResourceType::Resource_VehicleParts);
			}
		}
		M_ResourceFullCheck_ToCheckResource = ERTSResourceType::Resource_Radixite;
		break;
	default:
		M_ResourceFullCheck_ToCheckResource = ERTSResourceType::Resource_Radixite;
		break;
	}
}

void UPlayerResourceManager::ResourceFullAnnounce(const ERTSResourceType ResourceType)
{
	EAnnouncerVoiceLineType VoiceLineType = EAnnouncerVoiceLineType::RadixiteStorageFull;
	bool bIsValidRequest = false;
	switch (ResourceType)
	{
	case ERTSResourceType::Resource_None:
		break;
	case ERTSResourceType::Resource_Radixite:
		bIsValidRequest = true;
		break;
	case ERTSResourceType::Resource_Metal:
		bIsValidRequest = true;
		VoiceLineType = EAnnouncerVoiceLineType::MetalStorageFull;
		break;
	case ERTSResourceType::Resource_VehicleParts:
		bIsValidRequest = true;
		VoiceLineType = EAnnouncerVoiceLineType::VehiclePartsStorageFull;
		break;
	case ERTSResourceType::Resource_Fuel:
		break;
	case ERTSResourceType::Resource_Ammo:
		break;
	case ERTSResourceType::Blueprint_Weapon:
		break;
	case ERTSResourceType::Blueprint_Vehicle:
		break;
	case ERTSResourceType::Blueprint_Energy:
		break;
	case ERTSResourceType::Blueprint_Construction:
		break;
	}
	if (bIsValidRequest && GetIsValidPlayerController())
	{
		M_PlayerController->PlayAnnouncerVoiceLine(VoiceLineType, true, false);
	}
}

void UPlayerResourceManager::CheatAddStorage(const ERTSResourceType Resource, const int32 Amount)
{
	if (M_PlayerResources.Contains(Resource))
	{
		M_PlayerResources[Resource].Storage += Amount;
		M_PlayerResources[Resource].Amount += Amount;
	}
}

void UPlayerResourceManager::AddCustomResourceBonuses(
	const TArray<TPair<ERTSResourceType, int32>>& CustomResourceCardBonuses)
{
	for (auto EachBonus : CustomResourceCardBonuses)
	{
		M_HqResourceBonusesAwaitingHqSpawn.Add(EachBonus);
	}
}

bool UPlayerResourceManager::GetHasEnoughOfResource(const ERTSResourceType Resource, const int32 Amount) const
{
	if (M_PlayerResources.Contains(Resource))
	{
		return M_PlayerResources[Resource].Amount >= Amount;
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function GetHasEnoughOfResource in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
	return false;
}

bool UPlayerResourceManager::GetHasEnoughEnergy(const int32 Amount) const
{
	const int32 Supply = M_EnergySupply - M_EnergyDemand;
	return Supply - Amount >= 0;
}

void UPlayerResourceManager::SetupPlayerHQDropOffReference(const ANomadicVehicle* PlayerHQ)
{
	if (not IsValid(PlayerHQ) || not IsValid(PlayerHQ->GetResourceDropOff()))
	{
		const FString HQName = IsValid(PlayerHQ) ? PlayerHQ->GetName() : "Invalid";
		const FString DropOffName = IsValid(PlayerHQ)
			                            ? IsValid(PlayerHQ->GetResourceDropOff())
				                              ? PlayerHQ->GetResourceDropOff()->GetName()
				                              : "Invalid HQ->DropOff"
			                            : "Invalid HQ->DropOff";
		RTSFunctionLibrary::ReportError("PlayerHQ or DropOff on HQ is not valid at SetupPlayerHQDropOFFReference!"
			"HQ : " + HQName + " DropOff : " + DropOffName);
		return;
	}
	M_HQDropOff = PlayerHQ->GetResourceDropOff();
}

void UPlayerResourceManager::OnPlayerProfileLoaded_SetupHQResourceBonuses()
{
	AddResourceCardsToHQ(M_HqResourceBonusesAwaitingHqSpawn);
}

bool UPlayerResourceManager::GetCanPayForTrainingOption(const FTrainingOption& TrainingOption)
{
	if (not GetIsValidPlayerController())
	{
		return false;
	}
	bool bIsValidData = false;
	TMap<ERTSResourceType, int32> ResourceCosts = GetResourceCostsOfTrainingOption(TrainingOption, bIsValidData);
	if (not bIsValidData)
	{
		ReportInvalidDataError(TrainingOption);
		return false;
	}
	EPlayerError Error = GetCanPayForCost(ResourceCosts);
	if (Error == EPlayerError::Error_None)
	{
		return true;
	}
	M_PlayerController->DisplayErrorMessage(Error);
	return false;
}

bool UPlayerResourceManager::GetCanPayForBxp(const EBuildingExpansionType BxpType)
{
	if (!GetIsValidPlayerController())
	{
		return false;
	}
	const FBxpData BxpData = URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(BxpType, this);
	const EPlayerError Error = GetCanPayForCost(BxpData.Cost.ResourceCosts);
	if (Error == EPlayerError::Error_None)
	{
		if (BxpData.EnergySupply < 0)
		{
			if (GetHasEnoughEnergy(-1 * BxpData.EnergySupply))
			{
				return true;
			}
			M_PlayerController->DisplayErrorMessage(EPlayerError::Error_NotEnoughEnergy);
			return false;
		}
		return true;
	}
	M_PlayerController->DisplayErrorMessage(Error);
	return false;
}

void UPlayerResourceManager::RefundBxp(const EBuildingExpansionType BxpType)
{
	if (!GetIsValidPlayerController())
	{
		return;
	}
	const FBxpData BxpData = URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(BxpType, this);
	(void)RefundCosts(BxpData.Cost.ResourceCosts);
}

bool UPlayerResourceManager::PayForBxp(const EBuildingExpansionType BxpType)
{
	if (!GetIsValidPlayerController())
	{
		return false;
	}
	FBxpData BxpData = URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(BxpType, this);
	EPlayerError Error = GetCanPayForCost(BxpData.Cost.ResourceCosts);
	if (Error == EPlayerError::Error_None)
	{
		if (BxpData.EnergySupply < 0)
		{
			if (GetHasEnoughEnergy(FMath::Abs(BxpData.EnergySupply)))
			{
				PayForCosts(BxpData.Cost.ResourceCosts);
				return true;
			}
			M_PlayerController->DisplayErrorMessage(EPlayerError::Error_NotEnoughEnergy);
			return false;
		}
		PayForCosts(BxpData.Cost.ResourceCosts);
		return true;
	}
	M_PlayerController->DisplayErrorMessage(Error);
	return false;
}

EPlayerError UPlayerResourceManager::GetCanPayForCost(const TMap<ERTSResourceType, int32>& Cost) const
{
	ERTSResourceType ResourceInsufficient = {};
	bool bInsufficient = false;

	if (Cost.Num() == 0)
	{
		return EPlayerError::Error_None;
	}

	// Iterate over the cost map and check if the player has enough resources
	for (auto eachCost : Cost)
	{
		if (!GetHasEnoughOfResource(eachCost.Key, eachCost.Value))
		{
			bInsufficient = true;
			ResourceInsufficient = eachCost.Key;
			break;
		}
	}

	// If insufficient resources are found, map the resource type to a player error
	if (bInsufficient)
	{
		switch (ResourceInsufficient)
		{
		case ERTSResourceType::Resource_None:
			return EPlayerError::Error_None;
		case ERTSResourceType::Resource_Radixite:
			return EPlayerError::Error_NotEnoughRadixite;
		case ERTSResourceType::Resource_Metal:
			return EPlayerError::Error_NotEnoughMetal;
		case ERTSResourceType::Resource_VehicleParts:
			return EPlayerError::Error_NotEnoughVehicleParts;
		case ERTSResourceType::Resource_Fuel:
			return EPlayerError::Error_NotEnoughtFuel;
		case ERTSResourceType::Resource_Ammo:
			return EPlayerError::Error_NotENoughAmmo;
		case ERTSResourceType::Blueprint_Weapon:
			return EPlayerError::Error_NotEnoughWeaponBlueprints;
		case ERTSResourceType::Blueprint_Vehicle:
			return EPlayerError::Error_NotEnoughVehicleBlueprints;
		case ERTSResourceType::Blueprint_Energy:
			return EPlayerError::Error_NotEnoughEnergyBlueprints;
		case ERTSResourceType::Blueprint_Construction:
			return EPlayerError::Error_NotEnoughConstructionBlueprints;
		default:
			return EPlayerError::Error_None; // If no matching error is found
		}
	}

	// If no insufficient resource is found, return no error
	return EPlayerError::Error_None;
}

TMap<ERTSResourceType, int32> UPlayerResourceManager::GetResourceCostsOfTrainingOption(
	const FTrainingOption& TrainingOption, bool& bIsValidCosts) const
{
	bIsValidCosts = true;
	return FRTS_Statics::GetResourceCostsOfTrainingOption(TrainingOption,
	                                                      bIsValidCosts, this);;
}


bool UPlayerResourceManager::PayForCosts(const TMap<ERTSResourceType, int32>& Costs)
{
	// Do check.
	for (const auto EachCost : Costs)
	{
		if (!GetHasEnoughOfResource(EachCost.Key, EachCost.Value))
		{
			return false;
		}
	}
	for (const auto EachCost : Costs)
	{
		SubtractResource(EachCost.Key, EachCost.Value);
	}
	return true;
}

bool UPlayerResourceManager::RefundCosts(const TMap<ERTSResourceType, int32>& Costs)
{
	bool bFailedToAddAll = false;
	for (const auto EachCost : Costs)
	{
		if (not AddResource(EachCost.Key, EachCost.Value))
		{
			bFailedToAddAll = true;
		}
	}
	return !bFailedToAddAll;
}

void UPlayerResourceManager::PayForCostsNoCheck(const TMap<ERTSResourceType, int32>& Costs)
{
	for (const auto EachCost : Costs)
	{
		SubtractResource(EachCost.Key, EachCost.Value);
	}
}


bool UPlayerResourceManager::AddResource(const ERTSResourceType Resource, const int32 Amount)
{
	if (M_PlayerResources.Contains(Resource))
	{
		// Check if the provided amount fits in storage.
		if (M_PlayerResources[Resource].Amount + Amount <= M_PlayerResources[Resource].Storage)
		{
			M_PlayerResources[Resource].Amount += Amount;
			UpdateDropOffsWithAddedResource(Resource, Amount);
			return true;
		}
		// If the amount does not fit, fill to Max storage.
		const int32 AmountCanBeAdded = M_PlayerResources[Resource].Storage - M_PlayerResources[Resource].Amount;
		M_PlayerResources[Resource].Amount = M_PlayerResources[Resource].Storage;
		UpdateDropOffsWithAddedResource(Resource, AmountCanBeAdded);
		return false;
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function AddResource in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
	return false;
}

void UPlayerResourceManager::DropOffNoVisualUpdate_AddResource(const ERTSResourceType Resource, const int32 Amount)
{
	if (M_PlayerResources.Contains(Resource))
	{
		// Check if the provided amount fits in storage.
		if (M_PlayerResources[Resource].Amount + Amount <= M_PlayerResources[Resource].Storage)
		{
			M_PlayerResources[Resource].Amount += Amount;
			return;
		}
		RTSFunctionLibrary::ReportError("DropOfff_AddResource; Resources does not fit in storage."
			"\n As the dropoff should have stored this already this is not expected."
			"\n Resource type : " + Global_GetResourceTypeAsString(Resource));
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function AddResource in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
}

void UPlayerResourceManager::SubtractResource(const ERTSResourceType Resource, const int32 Amount)
{
	if (M_PlayerResources.Contains(Resource))
	{
		if (M_PlayerResources[Resource].Amount >= Amount)
		{
			M_PlayerResources[Resource].Amount -= Amount;
			UpdateDropOffsWithSubtractedResource(Resource, Amount);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Could not subtract resource as player has not enough!"
				"\n Resource: " + Global_GetResourceTypeAsString(Resource));
		}
		return;
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function SubtractResource in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
}

void UPlayerResourceManager::ApplyResourceCards(const TArray<FUnitCost>& ResourceCards)
{
	// Keep track of the resource bonuses that require the HQ DropOffs visuals to be altered.
	TArray<TPair<ERTSResourceType, int32>> ResourceBonusesToApplyOnHQ;
	for (auto EachCard : ResourceCards)
	{
		// Go through map of resources on card each card; note that a card can contain more than one resource type.
		for (auto EachBonus : EachCard.ResourceCosts)
		{
			if (IsResourceTypeToBeStoredInHQ(EachBonus.Key))
			{
				// Save to apply resources on HQ; create a new pair of resource type and amount.
				ResourceBonusesToApplyOnHQ.Add({EachBonus.Key, EachBonus.Value});
				continue;
			}
			// This resource type can be added immediately.
			ApplyCardResourcesInstantly({EachBonus.Key, EachBonus.Value});
		}
	}
	if (not ResourceBonusesToApplyOnHQ.IsEmpty())
	{
		DetermineHowToApplyHQResourceCards(ResourceBonusesToApplyOnHQ);
	}
}

void UPlayerResourceManager::DetermineHowToApplyHQResourceCards(
	const TArray<TPair<ERTSResourceType, int32>> HqResourceBonuses)
{
	if (not GetIsValidPlayerController())
	{
		RTSFunctionLibrary::ReportError("Failed to setup resource cards on HQ as player controller failed to load."
			"\n See DetermineHowToApplyHQResourceCards in PlayerResourceManager.cpp.");
		return;
	}
	if (M_PlayerController->GetHasPlayerProfileLoaded())
	{
		if (GetIsValidHQDropOff())
		{
			AddResourceCardsToHQ(HqResourceBonuses);
			return;
		}
	}
	// Save the resource bonuses to apply when the HQ is spawned.
	M_HqResourceBonusesAwaitingHqSpawn = HqResourceBonuses;
	// Notify the player controller than upon finishing the profile load we need to apply these bonuses.
	M_PlayerController->M_PlayerProfileLoadingStatus.bInitializeHqResourceBonusesFromProfileCardsOnLoad = true;
}


int32 UPlayerResourceManager::GetResourceAmount(const ERTSResourceType Resource) const
{
	if (M_PlayerResources.Contains(Resource))
	{
		return M_PlayerResources[Resource].Amount;
	}
	RTSFunctionLibrary::ReportError("Error resource not found. \n At function GetResource in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
	return int32();
}

int32 UPlayerResourceManager::GetResourceStorage(ERTSResourceType Resource) const
{
	if (M_PlayerResources.Contains(Resource))
	{
		return M_PlayerResources[Resource].Storage;
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function GetResourceStorage in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(Resource));
	return int32();
}

void UPlayerResourceManager::InitializeResourcesSettings(URTSGameSettingsHandler* GameUpdate)
{
	if (not GetIsValidHQDropOff())
	{
		RTSFunctionLibrary::ReportError("Cannot init start resources for player as HQ DropOff is invalid!"
			"\n See function InitializeResourcesSettings in PlayerResourceManager.cpp.");
		return;
	}
	const int32 RadixiteStart = GameUpdate->GetGameSetting<int32>(ERTSGameSetting::StartRadixite);
	M_PlayerResources[ERTSResourceType::Resource_Radixite].Amount += RadixiteStart;
	M_HQDropOff->AddResourcesNotHarvested(ERTSResourceType::Resource_Radixite, RadixiteStart);

	const int32 MetalStart = GameUpdate->GetGameSetting<int32>(ERTSGameSetting::StartMetal);
	M_PlayerResources[ERTSResourceType::Resource_Metal].Amount = MetalStart;
	M_HQDropOff->AddResourcesNotHarvested(ERTSResourceType::Resource_Metal, MetalStart);

	const int32 VehiclePartsStart = GameUpdate->GetGameSetting<int32>(ERTSGameSetting::StartVehicleParts);
	M_PlayerResources[ERTSResourceType::Resource_VehicleParts].Amount = VehiclePartsStart;
	M_HQDropOff->AddResourcesNotHarvested(ERTSResourceType::Resource_VehicleParts, VehiclePartsStart);

	M_PlayerResources[ERTSResourceType::Resource_Fuel].Amount = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartFuel);
	M_PlayerResources[ERTSResourceType::Resource_Ammo].Amount = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartAmmo);

	// Initialise blueprint settings.
	M_PlayerResources[ERTSResourceType::Blueprint_Weapon].Storage = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartBlueprintsStorage);
	M_PlayerResources[ERTSResourceType::Blueprint_Vehicle].Storage = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartBlueprintsStorage);
	M_PlayerResources[ERTSResourceType::Blueprint_Energy].Storage = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartBlueprintsStorage);
	M_PlayerResources[ERTSResourceType::Blueprint_Construction].Storage = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartBlueprintsStorage);

	M_PlayerResources[ERTSResourceType::Blueprint_Weapon].Amount = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartWeaponBlueprints);
	M_PlayerResources[ERTSResourceType::Blueprint_Vehicle].Amount = GameUpdate->GetGameSetting<int32>(
		ERTSGameSetting::StartVehicleBlueprints);
}

void UPlayerResourceManager::RegisterEnergyComponent(UEnergyComp* EnergyComponent)
{
	if (IsValid(EnergyComponent))
	{
		if (not M_EnergyComponents.Contains(EnergyComponent))
		{
			// Adjusts the supply and adds the component to the list.
			UpdateEnergySupplyWithComponent(EnergyComponent, true);
			return;
		}
		FString EnergyCompName = EnergyComponent->GetName();
		RTSFunctionLibrary::ReportError(
			"Attempted to add energy component: " + EnergyCompName +
			" to player resource manager, but it is already registered.");
		return;
	}
	RTSFunctionLibrary::ReportError("UPlayer Resource Manager was supplied with an invalid energy component"
		"\n see function RegisterEnergyComponent in PlayerResourceManager.cpp.");
}

void UPlayerResourceManager::DeregisterEnergyComponent(UEnergyComp* EnergyComponent)
{
	if (IsValid(EnergyComponent))
	{
		if (M_EnergyComponents.Contains(EnergyComponent))
		{
			// Adjusts the supply and removes the component from the list.
			UpdateEnergySupplyWithComponent(EnergyComponent, false);
			return;
		}
		const FString EnergyCompName = EnergyComponent->GetName();
		RTSFunctionLibrary::ReportError("Attempted to remove energy component: " + EnergyCompName +
			" from player resource manager, but it is not registered.");
		return;
	}
	RTSFunctionLibrary::ReportError("UPlayer Resource Manager was supplied with an invalid energy component"
		"\n see function DeregisterEnergyComponent in PlayerResourceManager.cpp.");
}

void UPlayerResourceManager::RegisterDropOff(const TWeakObjectPtr<UResourceDropOff> DropOff, const bool bRegister)
{
	if (bRegister && not M_DropOffs.Contains(DropOff))
	{
		if (not DropOff.IsValid())
		{
			RTSFunctionLibrary::ReportError("Attempted to register invalid drop off in player resource manager."
				"\n See function RegisterDropOff in PlayerResourceManager.cpp.");
			return;
		}

		M_DropOffs.Add(DropOff);
		// Sync the resource storage with this new drop off.
		AddDropOffToPlayerResources(DropOff);
		return;
	}
	if (not bRegister && M_DropOffs.Contains(DropOff))
	{
		M_DropOffs.Remove(DropOff);
		// Remove the drop off storage and amount for each resource type supported by it on the PlayerResources.
		RemoveDropOffFromPlayerResources(DropOff);
		return;
	}
	const FString DropOffName = DropOff->GetName();
	const FString OwnerName = IsValid(DropOff->GetOwner()) ? DropOff->GetOwner()->GetName() : "Invalid";
	const FString RegisterAction = bRegister ? "Wanted to register" : "Wanted to deregister";
	RTSFunctionLibrary::ReportError(RegisterAction + " drop off: " + DropOffName +
		" in player resource manager, but it is already registered/deregistered or it is Invalid!"
		"\n Owner: " + OwnerName);
}

void UPlayerResourceManager::UpdateEnergySupplyForUpgradedComponent(UEnergyComp* EnergyCompUpgraded,
                                                                    const int32 OldEnergyAmount)
{
	if (IsValid(EnergyCompUpgraded))
	{
		if (M_EnergyComponents.Contains(EnergyCompUpgraded))
		{
			M_EnergySupply -= OldEnergyAmount;
			M_EnergySupply += EnergyCompUpgraded->GetEnergySupplied();
			return;
		}
		RTSFunctionLibrary::ReportError(
			"Attempted to update energy supply for upgraded component: " + EnergyCompUpgraded->GetName() +
			" in player resource manager, but it is not registered.");
	}
}


void UPlayerResourceManager::BeginPlay()
{
	Super::BeginPlay();
	GetIsValidPlayerController();
	BeginPlay_InitPlayerAudioController();
	BeginPlay_InitResourceFullCheckTimerManager();
	BeginPlay_InitResourceWatcher();
}

void UPlayerResourceManager::UpdateEnergySupplyWithComponent(UEnergyComp* EnergyComponent, const bool bRegister)
{
	if (!IsValid(EnergyComponent))
	{
		return;
	}
	const int32 EnergySupplied = EnergyComponent->GetEnergySupplied();
	if (bRegister)
	{
		if (EnergySupplied < 0)
		{
			M_EnergyDemand -= EnergySupplied;
		}
		else
		{
			M_EnergySupply += EnergySupplied;
		}
		M_EnergyComponents.Add(EnergyComponent);
	}
	else
	{
		if (EnergySupplied < 0)
		{
			M_EnergyDemand += EnergySupplied;
		}
		else
		{
			M_EnergySupply -= EnergySupplied;
		}
		M_EnergyComponents.Remove(EnergyComponent);
	}
}

bool UPlayerResourceManager::GetIsValidPlayerController()
{
	if (IsValid(M_PlayerController))
	{
		return true;
	}
	M_PlayerController = FRTS_Statics::GetRTSController(this);
	return IsValid(M_PlayerController);
}

bool UPlayerResourceManager::GetIsValidHQDropOff() const
{
	if (M_HQDropOff.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player HQ DropOff reference is not valid for UPlayerResourceManager!");
	return false;
}

void UPlayerResourceManager::AddDropOffToPlayerResources(const TWeakObjectPtr<UResourceDropOff> DropOff)
{
	if (not DropOff.IsValid())
	{
		RTSFunctionLibrary::ReportError("Attempted to sync player resources with invalid drop off."
			"\n See function SyncPlayerResourcesWithNewDropOff in PlayerResourceManager.cpp.");
		return;
	}
	for (auto ResourceHarvesterCargSlot : DropOff->GetResourceDropOffCapacity())
	{
		// Add the max capacity this drop off has for the resource to the player resources.
		M_PlayerResources[ResourceHarvesterCargSlot.Key].Storage += ResourceHarvesterCargSlot.Value.MaxCapacity;
	}
}

void UPlayerResourceManager::RemoveDropOffFromPlayerResources(const TWeakObjectPtr<UResourceDropOff> DropOff)
{
	if (DropOff.IsStale(false, false))
	{
		RTSFunctionLibrary::ReportError("Attempted to sync player resources with STALE PTR to DropOff."
			"\n See function SyncPlayerResourcesWithNewDropOff in PlayerResourceManager.cpp.");
		return;
	}
	for (auto ResourceHarvesterCargoSlot : DropOff->GetResourceDropOffCapacity())
	{
		const int32 DropOffCapacity = ResourceHarvesterCargoSlot.Value.MaxCapacity;
		M_PlayerResources[ResourceHarvesterCargoSlot.Key].Storage -= DropOffCapacity;
		// Remove the resources that were stored in the drop off.
		M_PlayerResources[ResourceHarvesterCargoSlot.Key].Amount -= ResourceHarvesterCargoSlot.Value.CurrentAmount;
	}
}

void UPlayerResourceManager::UpdateDropOffsWithAddedResource(const ERTSResourceType Resource, const int32 Amount)
{
	int32 AmountLeft = Amount;
	for (auto EachDropOff : M_DropOffs)
	{
		if (not EachDropOff.IsValid())
		{
			continue;
		}
		const int32 DropOffCapacity = EachDropOff->GetDropOffCapacity(Resource);
		if (DropOffCapacity > 0)
		{
			// Fill drop off maximally.
			if (DropOffCapacity >= AmountLeft)
			{
				EachDropOff->AddResourcesNotHarvested(Resource, AmountLeft);
				return;
			}
			// Fill drop off partially.
			EachDropOff->AddResourcesNotHarvested(Resource, DropOffCapacity);
			AmountLeft -= DropOffCapacity;
			if (AmountLeft <= 0)
			{
				return;
			}
		}
	}
}

void UPlayerResourceManager::UpdateDropOffsWithSubtractedResource(const ERTSResourceType Resource, const int32 Amount)
{
	int32 AmountLeft = Amount;
	for (auto EachDropOff : M_DropOffs)
	{
		if (not EachDropOff.IsValid())
		{
			continue;
		}
		const int32 AmountInDropOff = EachDropOff->GetDropOffAmountStored(Resource);
		if (AmountInDropOff > 0)
		{
			// Check if this drop off has enough to remove the total amount left.
			if (AmountInDropOff >= AmountLeft)
			{
				EachDropOff->RemoveResources(Resource, AmountLeft);
				return;
			}
			// The drop off has less than the total amount left, empty the drop off.
			EachDropOff->RemoveResources(Resource, AmountInDropOff);
			AmountLeft -= AmountInDropOff;
			if (AmountLeft <= 0)
			{
				return;
			}
		}
	}
}

void UPlayerResourceManager::AddResourceCardsToHQ(const TArray<TPair<ERTSResourceType, int32>>& ResourceCardBonuses)
{
	if (not GetIsValidHQDropOff())
	{
		RTSFunctionLibrary::ReportError("Cannot setup resource cards on HQ DropOff as it is invalid in the"
			"Player Resource Manager!");
		return;
	}
	const TArray<ERTSResourceType> SupportedResourcesByHQ = M_HQDropOff->GetResourcesSupported();
	// Go through the provided cards that contain the resources to add to the HQ.
	for (auto EachBonusOfACard : ResourceCardBonuses)
	{
		if (not M_PlayerResources.Contains(EachBonusOfACard.Key) || not SupportedResourcesByHQ.Contains(
			EachBonusOfACard.Key))
		{
			FString ResourceName = Global_GetResourceTypeAsString(EachBonusOfACard.Key);
			RTSFunctionLibrary::ReportError(
				"A Resource card from the player profile provided a resource that is not supported"
				"by the HQ or the PlayerResourceManager!"
				"\n Resource: " + ResourceName);
			continue;
		}
		// Add the storage and value to the PlayerResources.
		M_PlayerResources[EachBonusOfACard.Key].Storage += EachBonusOfACard.Value;
		M_PlayerResources[EachBonusOfACard.Key].Amount += EachBonusOfACard.Value;
		// Update the capacity of the drop off and add the amount of the resource card as well.
		M_HQDropOff->UpgradeDropOffCapacity(EachBonusOfACard.Key, EachBonusOfACard.Value, true);
	}
}

bool UPlayerResourceManager::IsResourceTypeToBeStoredInHQ(const ERTSResourceType ResourceType) const
{
	if (const auto GameState = FRTS_Statics::GetGameState(this))
	{
		const auto HQData = GameState->GetNomadicDataOfPlayer(1, ENomadicSubtype::Nomadic_GerHq);
		return HQData.ResourceDropOffCapacity.Contains(ResourceType);
	}
	RTSFunctionLibrary::ReportError(
		"Cannot determine whether resource is to be stored in HQ at ISresourceTypeToBeStoredInHQ"
		"Due to invalid game state.");
	return false;
}

void UPlayerResourceManager::ApplyCardResourcesInstantly(const TPair<ERTSResourceType, int32>& ResourceBonusPair)
{
	if (not M_PlayerResources.Contains(ResourceBonusPair.Key))
	{
		const FString ResourceName = Global_GetResourceTypeAsString(ResourceBonusPair.Key);
		RTSFunctionLibrary::ReportError(
			"Attempted to instantly add a resource bonuse loaded from a card of the player profile"
			"but the resource bonus type is not supported by the player resources!!"
			"resource : " + ResourceName);
		return;
	}
	// Add the storage and value of this bonus.
	M_PlayerResources[ResourceBonusPair.Key].Storage += ResourceBonusPair.Value;
	M_PlayerResources[ResourceBonusPair.Key].Amount += ResourceBonusPair.Value;
}

void UPlayerResourceManager::ReportInvalidDataError(const FTrainingOption& TrainingOption) const
{
	const FString UnitType = Global_GetUnitTypeString(TrainingOption.UnitType);
	const FString SubType = FString::FromInt(TrainingOption.SubtypeValue);
	const FString ErrorMessage = "Invalid data for training option: " + UnitType + " " + SubType +
		"\n See function ReportInvalidDataError in PlayerResourceManager.cpp.";
	RTSFunctionLibrary::ReportError(ErrorMessage);
}

bool UPlayerResourceManager::GetIsValidPlayerAudioController() const
{
	if (M_PlayerAudioController.IsValid())
	{
		return true;
	}
	const FString Name = GetName();
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NullPtr";
	RTSFunctionLibrary::ReportError("Invalid M_PlayerAudioController "
		"\n For PlayerResourceManager: " + Name + "Of Owner: " + OwnerName);
	return false;
}

void UPlayerResourceManager::BeginPlay_InitPlayerAudioController()
{
	M_PlayerAudioController = FRTS_Statics::GetPlayerAudioController(this);
	// Validity test.
	(void)GetIsValidPlayerAudioController();
}

void UPlayerResourceManager::ResourceWatcher()
{
	using DeveloperSettings::GamePlay::Audio::Anouncer::ResourceWatcherAudioSpeedDivider;
	int32 TotalDifference = 0;
	int32 Difference = 0;
	for (auto EachResourceToWatch : M_ResourcesToWatch)
	{
		if (M_PlayerResources.Contains(EachResourceToWatch))
		{
			Difference = M_PlayerResources[EachResourceToWatch].Amount - M_LastResourceSnapShot[EachResourceToWatch];
			if (Difference < 0)
			{
				TotalDifference += FMath::Abs(Difference);
			}
			M_LastResourceSnapShot[EachResourceToWatch] = M_PlayerResources[EachResourceToWatch].Amount;
		}
	}
	if (TotalDifference > 1)
	{
		PlayResourceTickSound(true, TotalDifference / ResourceWatcherAudioSpeedDivider);
	}
	else
	{
		PlayResourceTickSound(false, 0);
	}
}
