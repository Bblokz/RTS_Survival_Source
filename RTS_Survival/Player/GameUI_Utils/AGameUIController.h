// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameUIState/GameUIState.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"


#include "AGameUIController.generated.h"


class ASquadController;
struct FActionUIParameters;
class RTS_SURVIVAL_API ASelectablePawnMaster;
class RTS_SURVIVAL_API ACPPController;
class RTS_SURVIVAL_API ANomadicVehicle;

enum class EAllUnitType : uint8;
enum class EActionUINomadicButton : uint8;
struct FAbilityStruct;

/** @brief Struct to store selection data for a unit subtype. */
USTRUCT()
struct FSelectionData
{
	GENERATED_BODY()

	int32 SelectionPriority = 0;
	EAllUnitType UnitType = EAllUnitType::UNType_None;
	uint8 SubTypeValue = 0;
};

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API AGameUIController : public AActor
{
	GENERATED_BODY()

public:
	AGameUIController();

	/**
	 * @brief Run a function for each valid actor in the current 'PrimarySameType' cache.
	 * @param Func Callback invoked for each valid actor reference.
	 *
	 * Usage:
	 *   ForEachPrimarySameType([](AActor& Actor){
	 *       // Do something to just the PrimarySameType units.
	 *   });
	 */
	void ForEachPrimarySameType(const TFunctionRef<void(AActor&)>& Func);

	/**
	 * @brief Copy the current 'PrimarySameType' actors (strong pointers) into OutActors.
	 */
	void GetPrimarySameTypeActors(TArray<AActor*>& OutActors);

/**
 * @brief Cache + notify the current "PrimarySameType" group (including the PrimarySelected).
 * @param PrimarySameTypeActors  Already-filtered list: all actors of the active bucket,
 *                               including the primary.
 *
 * Behavior:
 *  - First notifies 'false' to everyone in the previous cache (idempotent).
 *  - Then notifies 'true' to everyone in the incoming list (idempotent).
 *  - Rebuilds cache from the incoming list using TWeakObjectPtr.
 *
 * Examples for notification are included below as commented lines;
 * wire them to your components / interfaces as needed.
 */
	void UpdatePrimarySameTypeCacheAndNotify_FromList(
    	const TArray<AActor*>& PrimarySameTypeActors);

	/** @return True if the primary selected has an enabled training component */
	bool GetIsPrimarySelectedEnabledTrainer() const;
		
	/**
	 * @brief DO NOT CALL; only from Controller::RebuildGameUIHierarchy.
	 * Rebuilds the action hierarchy depending on the selected unit types.
	 * @note Empties the current ActionUIHierarchy and resets the ActiveActionUIArrayIndex.
	 * @note Also sets all booleans of TSelectedUnitTypes to false. Then loops through all selected units
	 * and sets each unique type to true in TSelectedUnitTypes.
	 * @param TPlayerSelectedPawnMasters The pawns selected by the player.
	 * @param TPlayerSelectedSquadControllers The squads selected by the player.
	 * @post The THierarchyActionUI is refilled with selected unit types. Calls UpdateActionUI on controller.
	 * @note Will remove invalid selected units.
	 */
	void RebuildGameUIHierarchy(
		TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors);

	void OnlyRebuildSelectionUI(TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	                            TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	                            TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors);


	/**
	 * @brief Tab through the action UI hierarchy.
	 * @param TPlayerSelectedPawnMasters The pawns selected by the player.
	 * @param TPlayerSelectedSquadControllers The squads selected by the player.
	 * @param TPlayerSelectedActors The actors selected by the player.
	 * @note If the hierarchy is empty, nothing happens.
	 * @note If the hierarchy has only one unit type, nothing happens.
	 * @note If the hierarchy has more than one unit type, the index is incremented and the new unit type is set as the primary selection.
	 * @post calls UpdateActionUI on controller if needed.
	 */
	void TabThroughGameUIHierarchy(TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	                               TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	                               TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors);


	//  Try to move the active hierarchy index until the primary selected type equals DesiredType.
	// Returns true if the type was found (or was already active). Updates UI only if the index advanced.
	bool TryAdvancePrimaryToUnitType(
		EAllUnitType DesiredType,
		TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors, AActor* OverwritePrimaryActor);

	/** @return  The primary selected unit as determined by the last update of the action UI. */
	AActor* GetPrimarySelectedUnit() const;

	inline void SetPlayerController(ACPPController* NewController) { M_PlayerController = NewController; }

	/**
	 * @return The unit type that is currently the main selection and of which the abilities make up the
	 * actionUI at this moment.
	 */
	EAllUnitType GetPrimarySelectedUnitType() const;

	/**
	 * @brief Obtain the unit abilities of the primary selected unit.
	 * @return The EAbility Array.
	 */
        TArray<FUnitAbilityEntry> GetPrimarySelectedAbilityArray();

	/**
	 * @param ButtonIndex The index of the actionUI button that was clicked.
	 * @return The Ability ID of the primary selected unitType at the index in that unitType their ability array.
	 */
	EAbilityID GetActiveAbility(const int ButtonIndex);

	UFUNCTION(BlueprintCallable, Category="CurrentSelection", NotBlueprintable)
	bool GetIsUnitTypeSelected(EAllUnitType UnitTypeToCheck, const uint8 UnitSubtypeToCheck) const;

private:
	/** @brief Generates a unique unit subtype ID by combining UnitType and SubTypeValue. */
	uint32 GetUniqueUnitSubtypeID(EAllUnitType UnitType, const uint8 SubTypeValue) const
	{
		return (static_cast<uint32>(UnitType) << 8) | static_cast<uint32>(SubTypeValue);
	}

	/**
	 * Returns the first unit of the primary selected unit type.
	 * @param TValidPlayerSelectedPawnMasters The pawns selected by the player.
	 * @param TValidPlayerSelectedSquadControllers The squads selected by the player.
	 * @param TValidPlayerSelectedActors The actors selected by the player.
	 * @param OutUnitSubtype Returns the subtype of the unit.
	 * @note Will remove invalid selected units.
	 * @return unit as AActor.
	 */
	AActor* GetPrimarySelectedUnit(TArray<ASelectablePawnMaster*>& TValidPlayerSelectedPawnMasters,
	                               TArray<ASquadController*>& TValidPlayerSelectedSquadControllers,
	                               TArray<ASelectableActorObjectsMaster*>& TValidPlayerSelectedActors,
	                               int32& OutUnitSubtype);

	UPROPERTY()
	TObjectPtr<UMainGameUI> M_MainGameUI;

	// Increments index for ActionUI; new unit type will be the primary selection.
	void IncrementGameUI_Index();

	inline int GetActiveGameUIArrayIndex() const { return M_ActiveGameUIArrayIndex; }

	/**
	* @brief Collects data for the Action UI from the (possibly overwritten) primary unit and propagates it to the UI.
	*
	* @param TPlayerSelectedPawnMasters  Current selected pawns (raw-pointer array).
	* @param TPlayerSelectedSquadControllers Current selected squads (raw-pointer array).
	* @param TPlayerSelectedActors       Current selected actors (raw-pointer array).
	* @param OverWritePrimarySelectedUnit If valid, this actor is used as the primary selected (instead of the first of the active type).
	* can be left null to use the first of the active type.
	* @pre Main game UI must be valid (checked internally).
	* @post Updates the Action UI (abilities, health/exp, description, etc.) for the resolved primary unit.
	* @note When OverWritePrimarySelectedUnit is valid, its URTSComponent is read to derive the raw subtype (and type).
	*/
	void CalculatePropagateGameUIState(
		TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors,
		AActor* OverWritePrimarySelectedUnit = nullptr);

	/** @return Whether a valid overwrite actor was applied as primary (and RTS subtype/type was consumed if available). */
	bool TryApplyOverwritePrimarySelectedUnit(
		AActor* OverwritePrimarySelectedUnit,
		EAllUnitType& InOutPrimaryUnitType,
		int32& InOutRawSubtype,
		AActor*& OutPrimaryUnit) const;


	/** @brief Stores unique unit subtype IDs present in the current selection. This Id is made with the EAllUnitType and
	 * the spedific unit subtype set in the unit's RTS component, see GetUniqueUnitSubtypeID for details.
	 * In this way we can differentiate between units that have the same subtype value (e.g tank 0 and nomadic 0)
	 * but are of different EAllUnitTypes
	 */
	TSet<uint32> M_SelectedUniqueSubTypeIDs;

	/** @brief Hierarchy of unit subtype IDs based on selection priority and unit type. */
	TArray<uint32> M_THierarchyGameUI;


	// Last index (in THierarchyActionUI) of which the action UI buttons are shown.
	UPROPERTY()
	int M_ActiveGameUIArrayIndex;

	/** @brief Maps unique unit subtype IDs to their selection data. */
	TMap<uint32, FSelectionData> M_UniqueSubTypeIDToSelectionData;


	UPROPERTY()
	ACPPController* M_PlayerController;

	/**
	 * @brief Returns by reference the action UI parameters for the specified unit type.
	 * @param OutGameUIParameters Determines how the UI should be displayed.
	 * @param UnitType The unit type of the primary selected unit.
	 * @param PrimaryUnit The primary selected unit.
	 * @param CurrentAbility The ability that is currently active for the unit.
	 * @param RawSubtype The subtype of the unit that can be refined into a specific one.
	 */
	void GetGameUIParametersForType(
		FActionUIParameters& OutGameUIParameters,
		EAllUnitType UnitType,
		AActor* PrimaryUnit,
		EAbilityID CurrentAbility,
		const int32 RawSubtype);

	/**
	 * @brief Returns by reference the action UI parameters for the nomadic vehicle.
	 * @param OutGameUIParameters Determines how the UI should be displayed.
	 * @param PrimaryUnit The primary selected unit in this case of nomadic type.
	 * @param CurrentAbility The ability that is currently active for the vehicle.
	 */
	void GetActionUIParamsNomadic(
		FActionUIParameters& OutGameUIParameters,
		AActor* PrimaryUnit,
		EAbilityID CurrentAbility) const;

	/**
	 * @brief Go over the nomadic's status to derive what button we should display for converting building/vehicle.
	 * @param NomadicVehicle The nomadic vehicle actively selected.
	 * @param CurrentAbility The ability that is currently active for the vehicle.
	 * @return The status of the button.
	 */
	EActionUINomadicButton GetButtonStateFromTruck(const ANomadicVehicle* NomadicVehicle,
	                                               EAbilityID CurrentAbility) const;

	/**
	 * 
	 * @param OutGameUIParameters Will set the bShowtrainingUi to true if the primary actor is a trainer and has non zero
	 * training options in their training component.
	 * @param PrimaryUnit The primary selected unit.
	 */
	void GetTrainingUIParametersForType(
		FActionUIParameters& OutGameUIParameters,
		AActor* PrimaryUnit) const;

	/**
	 * @brief Set up the weapon UI for the primary selected unit.
	 * @param OutGameUIParameters By ref, whether to show the weapon UI will be set in there.
	 * @param PrimaryUnit The primary selected unit.
	 */
	void SetUpWeaponUIForUnit(
		FActionUIParameters& OutGameUIParameters,
		AActor* PrimaryUnit);

	/**
	 * @brief Set up the action UI abilities for the primary selected unit.
	 * @param OutGameUIParameters By ref, whether to show the action UI will be set in there.
	 * @param PrimaryUnitType The primary selected unit's Type.
	 * @param PrimaryUnit The primary selected unit.
	 */
	void SetUpActionUIForUnit(
		FActionUIParameters& OutGameUIParameters,
		const EAllUnitType PrimaryUnitType,
		AActor* PrimaryUnit);


	/**
	 * @brief The Action UI controller version of the same function implemented in the bp of the player controller.
	 * Allows for updating any cpp UI related functions before propagating to bp.
	 */
	void RefreshGameUI(
		EAllUnitType PrimarySelectedUnitType,
		AActor* PrimarySelectedActor,
		FActionUIParameters GameUIParameters);

	// Saves the last state with which we updated the game UI.
	FGameUIState M_currentGameUIState;

	/**
	 * @brief to setup the subtype in the actionui parameters.
	 * @param GameUIParameters The parameters to set the subtype in.
	 * @param UnitType The main type of the unit.
	 * @param Subtype The subtype of the unit.
	 * @post If the unit has a subtype set and the unit's main type allows for using subtypes then
	 * the specific subtype is set in the actionUI parameters.
	 * @note Will default all subtypes in the actionUI parameters to the None type.
	 */
	void SetUnitSubtypeInActionUIParameters(
		FActionUIParameters& GameUIParameters,
		const EAllUnitType UnitType,
		const int32 Subtype) const;

	bool GetIsValidMainGameUI();

        TArray<FUnitAbilityEntry> M_AbilityArrayWithEmptyAbilities;

	/**
 * @brief Retrieves the selection data of the currently active unit in the hierarchy.
 * @return Pointer to the FSelectionData of the active unit, or nullptr if not found.
 */
	const FSelectionData GetActiveSelectionData() const;

	/**
	 * @brief This is a helper function that will cast the primary selected unit against the selection types
	 * to find the rts components with the ability array that can then be stored in the GameUIState.
	 */
        TArray<FUnitAbilityEntry> GetPrimaryUnitAbilities(AActor* PrimarySelectedUnit) const;

	void EnsureProvidedArraysAreValid(
		TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors);

	/** @brief Build the flat widget-state array for the two-row SelectionPanel and push it to the UI. */
	void PushSelectionPanelState(
		TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors,
		AActor* OverWritePrimarySelectedUnit = nullptr);
	/** Prints the current selection (pawns, squads, actors) with display names from their TrainingOptions. */
	void Debug_PrintSelectedUnits(
		const TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
		const TArray<ASquadController*>* TPlayerSelectedSquadControllers,
		const TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors) const;

	/** Helper: attempts to read the TrainingOption display name from an actor's RTS component. */
	bool Debug_TryGetDisplayNameFromActor(const AActor* InActor, FString& OutDisplayName) const;

	/** Actors currently in the 'PrimarySameType' group (excludes the single PrimarySelected). */
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> M_PrimarySameTypeCache;

	/** Remove dead entries from M_PrimarySameTypeCache. */
	void EnsurePrimarySelectedSameTypeCacheIsValid();
};
