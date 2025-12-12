// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/PlayerError/PlayerError.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "PlayerResourceManager.generated.h"

class UPlayerAudioController;
struct FTrainingOption;
class ANomadicVehicle;
class UResourceDropOff;
class ACPPController;
enum class EBuildingExpansionType : uint8;
class UEnergyComp;
enum class ERTSResourceType : uint8;
class URTSGameSettingsHandler;

USTRUCT()
struct FRTSResourceStorage
{
	GENERATED_BODY()

	int32 Amount = 0;
	int32 Storage = 0;
};

/**
 * This class is responsible for storing and updating the player resources.
 * The MainGameUI uses this to fill out the resource display.
 * @note -------------------------
 * @note On DropOffs:
 * @note This component has an array of the DropOffs registered for the player, just like the GameResourceManager,
 * @note Unlike the GameResourceManager this list is not used for harvester logic but instead to propagate resource
 * changes outside of the harvester system. When we scavenge resources from the map they need to be added to the DropOffs, 
 * that is when we use that list. Also when resources are spent they are removed from the DropOffs
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerResourceManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerResourceManager();

	/** Returns the list of registered energy components */
	const TArray<TObjectPtr<UEnergyComp>>& GetEnergyComponents() const { return M_EnergyComponents; }


	void CheatAddStorage(ERTSResourceType Resource, int32 Amount);


	/**
	 * @brief Used in missions to add fake cards as bonuses for the player.
	 * @param CustomResourceCardBonuses Resource bonuses to add.
	 */
	void AddCustomResourceBonuses(const TArray<TPair<ERTSResourceType, int32>>& CustomResourceCardBonuses);

	bool GetHasEnoughOfResource(ERTSResourceType Resource, int32 Amount) const;
	bool GetHasEnoughEnergy(int32 Amount) const;
	TMap<ERTSResourceType, int32> GetResourceCostsOfTrainingOption(const FTrainingOption& TrainingOption,
	                                                               bool& bIsValidCosts) const;


	void SetupPlayerHQDropOffReference(const ANomadicVehicle* PlayerHQ);
	void OnPlayerProfileLoaded_SetupHQResourceBonuses();

	/**
	 * @return Whether we can pay for this training option.
	 * @note Handles resource insufficient errors with player controller
	 */
	bool GetCanPayForTrainingOption(const FTrainingOption& TrainingOption);

	/** @return Whether the player can pay for the cost, if error_none then the player can pay and otherwise the
	 * resource that is missing has its error is returned. */
	EPlayerError GetCanPayForCost(const TMap<ERTSResourceType, int32>& Cost) const;


	/** @return Whether we can pay for the bxp.
	 * @note Handles resource insufficient errors with player controller*/
	bool GetCanPayForBxp(const EBuildingExpansionType BxpType);

	void RefundBxp(const EBuildingExpansionType BxpType);

	/** @return Whether the bxp is payed for.*/
	bool PayForBxp(const EBuildingExpansionType BxpType);

	/**
	 * @brief Checks the resources in storage and adjusts the amount of resources owned by the player.
	 * @param Costs The map cost of the technology or unit.
	 * @return Whether the player successfully paid for the costs.
	 */
	bool PayForCosts(const TMap<ERTSResourceType, int32>& Costs);

	/**
	 * @param Costs The map cost of the technology or unit.
	 * @return Whether all cost resources could be added back to the player's supply.
	 */
	bool RefundCosts(const TMap<ERTSResourceType, int32>& Costs);

	/**
	 * @brief no resource checking -> instant reduce of resources. 
	 * @param Costs The map cost of the technology or unit.
	 */
	void PayForCostsNoCheck(
		const TMap<ERTSResourceType, int32>& Costs);

	/**
	 * Adds teh amount of provided resources for the player.
	 * @param Resource The resource type to add.
	 * @param Amount The amount to add.
	 * @return Whether the resource could be fully added.
	 * @post The Resource drop offs are updated with the amount that was added.
	 */
	bool AddResource(ERTSResourceType Resource, int32 Amount);

	/** @brief Adds the resource but without updating the DropOff.
	 * @pre Expects the DropOff to have updated its own visuals*/
	void DropOffNoVisualUpdate_AddResource(const ERTSResourceType Resource, const int32 Amount);

	void SubtractResource(ERTSResourceType Resource, int32 Amount);
	void ApplyResourceCards(const TArray<FUnitCost>& ResourceCards);

	/** @return The amount of resources the player owns. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Resources")
	int32 GetResourceAmount(ERTSResourceType Resource) const;

	/** @return The amount of storage capacity the player has for this resource type. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Resources")
	int32 GetResourceStorage(ERTSResourceType Resource) const;

	void InitializeResourcesSettings(URTSGameSettingsHandler* GameUpdate);

	void RegisterEnergyComponent(UEnergyComp* EnergyComponent);
	void DeregisterEnergyComponent(UEnergyComp* EnergyComponent);
	void RegisterDropOff(TWeakObjectPtr<UResourceDropOff> DropOff, const bool bRegister);
	void UpdateEnergySupplyForUpgradedComponent(UEnergyComp* EnergyCompUpgraded, const int32 OldEnergyAmount);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Resources")
	int32 GetPlayerEnergySupply() const { return M_EnergySupply; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Resources")
	int32 GetPlayerEnergyDemand() const { return M_EnergyDemand; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	TMap<ERTSResourceType, FRTSResourceStorage> M_PlayerResources;

	void UpdateEnergySupplyWithComponent(UEnergyComp* EnergyComponent, const bool bRegister);


	UPROPERTY()
	int32 M_EnergySupply;

	UPROPERTY()
	int32 M_EnergyDemand;

	UPROPERTY()
	TArray<TObjectPtr<UEnergyComp>> M_EnergyComponents;

	UPROPERTY()
	TArray<TWeakObjectPtr<UResourceDropOff>> M_DropOffs;

	// Reference to the drop off component on the player HQ, this is used to store the extra resources gained
	// from player cards.
	UPROPERTY()
	TWeakObjectPtr<UResourceDropOff> M_HQDropOff;

	bool GetIsValidPlayerController();
	bool GetIsValidHQDropOff() const;

	/** @brief  Adds the max capacity of each harvester slot for each resource type that is supported on the DropOff to the
	 * PlayerResources storage. */
	void AddDropOffToPlayerResources(const TWeakObjectPtr<UResourceDropOff> DropOff);

	/**
	 * @brief Removes the max capacity of each harvester slot for each resource type that is supported on the DropOff from the
	 * player resources storage, also removes the resources that were stored in that drop off. 
	 * @param DropOff The drop off to remove.
	 */
	void RemoveDropOffFromPlayerResources(const TWeakObjectPtr<UResourceDropOff> DropOff);

	/** @brief Updates the registered drop offs to take in the amount of added resources.*/
	void UpdateDropOffsWithAddedResource(const ERTSResourceType Resource, const int32 Amount);

	void UpdateDropOffsWithSubtractedResource(const ERTSResourceType Resource, const int32 Amount);

	UPROPERTY()
	ACPPController* M_PlayerController;


	/** @brief Determine if we can apply the resources instantly as the player profile is already loaded or if we need
	 * to wait for the profile to be fully loaded first.
	 * @post If the Profile was not fully loaded then the bonuses are stored in M_HqResourceBonusesAwaitingHqSpawn
	 * and added after the PlayerController calls OnPlayerProfileLoadComplete.
	 */
	void DetermineHowToApplyHQResourceCards(const TArray<TPair<ERTSResourceType, int32>> HqResourceBonuses);

	// Filled with bonuses from Resource Cards from the player profile when the HQ is not loaded but the profile is.
	TArray<TPair<ERTSResourceType, int32>> M_HqResourceBonusesAwaitingHqSpawn;

	/**
	 * @brief Adds resources to HQ DropOff.
	 * @pre Assumes the HQ DropOff is valid.
	 * @post the HQ visuals are changed based on the percentages that are stored.
	 * @post The Player resources are updated with the values added to the HQ DropOff.
	 */
	void AddResourceCardsToHQ(const TArray<TPair<ERTSResourceType, int32>>& ResourceCardBonuses);

	/** @return Whether the resource types stored for the HQ DropOff in the GameState supports the resource type. */
	bool IsResourceTypeToBeStoredInHQ(const ERTSResourceType ResourceType) const;
	/**
	 * @brief Provide a pair of resource type with amount to add to the player resources.
	 * @post the Storage and CurrentAmount in PlayerResources are updated.
	 * @note Does not update any DropOff visuals.
	 * @param ResourceBonusPair The resource type and amount to add.
	 */
	void ApplyCardResourcesInstantly(const TPair<ERTSResourceType, int32>& ResourceBonusPair);

	void ReportInvalidDataError(const FTrainingOption& TrainingOption) const;

	// To play resource tick sound.
	TWeakObjectPtr<UPlayerAudioController> M_PlayerAudioController;
	bool GetIsValidPlayerAudioController() const;
	void BeginPlay_InitPlayerAudioController();
	void ResourceWatcher();
	UPROPERTY()
	FTimerHandle M_ResourceWatchTimer;

	void BeginPlay_InitResourceWatcher();

	TMap<ERTSResourceType, int32> M_LastResourceSnapShot;
	const TArray<ERTSResourceType> M_ResourcesToWatch = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts
	};

	// Will only prompt the resource tick audio component to start again if it was stopped before.
	void PlayResourceTickSound(const bool bPlay, const float IntensityRequested) const;

	void BeginPlay_InitResourceFullCheckTimerManager();
	FTimerHandle M_ResourceFullCheckTimerManager;
	void OnResourceFullCheck();
	void ResourceFullAnnounce(const ERTSResourceType ResourceType);
	ERTSResourceType M_ResourceFullCheck_ToCheckResource = ERTSResourceType::Resource_Radixite;
};
