// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "Math/UnitConversion.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "TrainerComponent/TrainingQueueItem/QueueItem.h"
#include "UObject/Object.h"
#include "TrainingOptions/TrainingOptions.h"
#include "TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "TrainingMenuManager.generated.h"

class UW_TrainingBuildingColumn;
class UW_TrainingMenu;
class UW_TrainingRequirement;
class UPlayerResourceManager;
class UW_TrainingDescription;
enum class ETechnology : uint8;
class UMainGameUI;
class RTS_SURVIVAL_API UTrainerComponent;
class RTS_SURVIVAL_API UW_TrainingItem;
struct FTrainingQueueItem;

/**
 * @brief Manages the training menu and interacts with trainer components.
 */
UCLASS()
class RTS_SURVIVAL_API UTrainingMenuManager : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the training manager with the training menu widget.
	 * @param TrainingMenu Holds the training items, sub section tabs and quick navigation buttons.
	 * @param BuildingColumn Lists the player's other buildings of the type the training menu shows.
	 * @param TrainingDescription
	 */
	void InitTrainingManager(
		UW_TrainingMenu* TrainingMenu,
		UW_TrainingBuildingColumn* BuildingColumn,
		UMainGameUI* MainGameUI,
		UW_TrainingRequirement* TrainingRequirementWidget, UW_TrainingDescription* TrainingDescription);

	/** Shows the training items of another sub section of the primary selected trainer. */
	void OnSubSectionTabClicked(const int32 SubSectionIndex);

	/** Makes the player's first expanded building of this type the primary selected unit. */
	void OnQuickNavButtonClicked(const ENomadicSubtype Building);

	/** Makes the building behind this entry of the building column the primary selected unit. */
	void OnBuildingColumnEntryClicked(const int32 EntryIndex);

	void OnTrainingItemClicked(
		const FTrainingWidgetState& TrainingItem,
		const int32 Index,
		const bool bIsLeftClick);

	void OnTrainingItemHovered(const FTrainingWidgetState& TrainingWidgetState, const bool bIsHovered);

	/**
	 * @brief Sets the primary selected trainer; this trainer's training queue will be loaded into the menu.
	 * @param Trainer The trainer component to set as the primary selected trainer.
	 * @post The Training UI options are initialized acccording to the Tarray of options saved in the trainer.
	 */
	void SetupTrainingOptionsForTrainer(
		UTrainerComponent* Trainer,
		const bool bIsAbleToTrain);

	/**
	 * @brief Updates the UI as if just loaded again from the primary selected trainer.
	 * @param RequestingTrainer The trainer component that requested the update.
	 */
	void RequestRefreshTrainingUI(
		const UTrainerComponent* const RequestingTrainer);

	/**
	 * 
	 * @param bAtInsufficient Whether the queue is paused or unpaused from insufficient resources.
	 * @param ActiveItemIndex The index of the training item that is active.
	 * @param TimeRemaining The time remaining of the active item.
	 * @param ActiveItemOption The training option of the active item.
	 * @param RequestingTrainer The trainer that requests the update.
	 * @pre IMPORTANT: IF the queue jumped from paused by resources to unpaused by resources then make sure that
	 * the remaining time for this first unpaused-tick is already updated to prevent sync errors.
	 */
	void UpdateUIClockAtInsufficientResources(const bool bAtInsufficient,
	                                          const int32 ActiveItemIndex,
	                                          const int32 TimeRemaining,
	                                          const FTrainingOption& ActiveItemOption,
	                                          UTrainerComponent* RequestingTrainer
	);

	/**
	 * @brief Updates the training UI when a trainer requests to enable or disable training.
	 * @param RequestingTrainer The trainer that requests to update the UI. 
	 * @param bSetEnabled Whether this trainer has training enabled.
	 */
	void RequestEnableTrainingUI(
		const UTrainerComponent* RequestingTrainer,
		const bool bSetEnabled);

	FTrainingOptionState GetTrainingOptionState(const FTrainingOption& TrainingOption) const;
	// For debugging, allows to train units faster.
	void SetTrainingTimeForEachOption(const float NewTime);

	void HideTrainingRequirement() const;

private:
	UPROPERTY()
	UTrainerComponent* M_PrimarySelectedTrainer;

	bool GetIsValidPrimarySelectedTrainer() const;

	void PauseQueueOrRemove(const FTrainingWidgetState& TrainingItem) const;

	void PlayAnnouncerTrainingSound(EAnnouncerVoiceLineType VoiceLineType) const;

	// An array of all the training widgets in the menu.
	UPROPERTY()
	TArray<UW_TrainingItem*> M_TTrainingMenuWidgets;

	UPROPERTY()
	TObjectPtr<UW_TrainingMenu> M_TrainingMenu;
	bool GetIsValidTrainingMenu() const;

	// The sub section of the primary selected trainer whose options are on the training widgets.
	int32 M_ShownSubSectionIndex = 0;

	void UpdateSubSectionTabsWithPrimaryTrainer() const;
	void UpdateWidgetsWithShownSubSection(const TMap<FTrainingOption, int32>& QueueState);
	FTrainingWidgetState CreateWidgetStateForOption(const FTrainingOption& Option, const int32 QueuedCount) const;
	// Base presentation (all overrides off) outside a mission, where no player variant manager exists.
	FUnitVariantTrainingPresentation GetVariantPresentationForOption(const FTrainingOption& Option) const;

	/**
	 * @param FlatOptionIndex Index into the training options of the primary selected trainer.
	 * @return The widget that shows this option, or INDEX_NONE when its sub section is not on screen.
	 */
	int32 GetWidgetIndexOfFlatOptionIndex(const int32 FlatOptionIndex) const;

	// Shows the quick navigation buttons of the buildings the player can train at right now.
	void RefreshQuickNavButtons() const;

	/** @return The player's first building of this type whose training is enabled, null when there is none. */
	AActor* FindFirstTrainingBuildingOfPlayer(const ENomadicSubtype Building) const;

	/**
	 * @param Building The building type to look for.
	 * @param OutBuildings The player's buildings of that type whose training is enabled; trucks that are
	 * not expanded are left out.
	 */
	void GetTrainingBuildingsOfPlayer(const ENomadicSubtype Building, TArray<AActor*>& OutBuildings) const;

	UPROPERTY()
	TObjectPtr<UW_TrainingBuildingColumn> M_BuildingColumn;
	bool GetIsValidBuildingColumn() const;

	// The buildings behind the entries of the building column, in entry order; refreshed with the column
	// so that a click on an entry selects the building it showed.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_BuildingColumnBuildings;

	// Gives every expanded building of the shown type an entry; a single building gets no column.
	void RefreshBuildingColumn();

	ENomadicSubtype GetBuildingOfPrimarySelectedTrainer() const;

	// float M_RequirementNotMetOpacity = 0.33f;

	// The widget item that currently has a clock running.
	UPROPERTY()
	UW_TrainingItem* M_ActiveTrainingItem;

	/**
	 * @param FlatIndexActiveOption Index of the active option in the trainer's training options; no clock
	 * starts when that option is in a sub section that is not on screen.
	 */
	void StartClockOnTrainingItem(
		const int32 FlatIndexActiveOption,
		const int32 TimeRemaining,
		const FTrainingOption ActiveOption,
		const bool bIsPaused = false);

	// Defines all training options; binds an identifying enum to a training item state.
	UPROPERTY()
	TMap<FTrainingOption, FTrainingOptionState> M_TrainingOptionsMap;

	/** For each enum value in FTrainingOption, initialize a training option; with time and costs */
	void InitAllGameTrainingOptions();
	FTrainingOptionState CreateTrainingOptionState(FTrainingOption ItemID, int32 TrainingTime,
	                                               EAllUnitType UnitType = EAllUnitType::UNType_None,
	                                               ETankSubtype TankSubtype = ETankSubtype::Tank_None,
	                                               ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None,
	                                               ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None,
	                                               EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None,
	                                               URTSRequirement* RTSRequirement = nullptr);

	void InitAllGameTankTrainingOptions();
	void InitAllGameAircraftTrainingOptions();
	void InitAllGameNomadicTrainingOptions();
	void InitAllGameSquadTrainingOptions();
	void InitAllGameAntiTankGunTWTrainingOptions();
	void InitAllGameMortarTWTrainingOptions();
	void InitAllGameFlakTWTrainingOptions();
	void InitAllGameArtilleryTWTrainingOptions();
	void InitAllGameMachineGunTWTrainingOptions();
	// Units of the faction training layouts that have no hand written entry above.
	void InitAllGameTrainingLayoutOnlyOptions();
	// The pits a trainer builds at a planned spot; see OnLeftClickFieldDefenseItem.
	void InitAllGameFieldDefenseTrainingOptions();

	// Units marked as a tier II unlock need the communication center, the tier II building that also unlocks
	// the airbase and the experimental factory.
	static URTSRequirement* CreateTierTwoUnlockRequirement();
	// For units that unlock with either the tier II building or an earlier factory of their own line.
	static URTSRequirement* CreateTierTwoOrBuildingUnlockRequirement(const ENomadicSubtype AlternativeBuilding);

	// Makes sure that training times are not too short to cause load issues on AsyncSpawner.
	void PostInitTrainingOptions();

	void OnLeftClickTrainingItem(
	const FTrainingWidgetState& TrainingItem,
	const int32 Index);

	/**
	 * @brief Field defenses are not queued on click: the player first places them, and the placement click queues
	 * the item at the trainer. Requirements and the limit of planned defenses apply as for any training item.
	 * @param TrainingItem The clicked field defense.
	 * @param FlatOptionIndex The index of the option over all sub sections of the trainer.
	 */
	void OnLeftClickFieldDefenseItem(const FTrainingWidgetState& TrainingItem, const int32 FlatOptionIndex);

	/**
	 * @param TrainingItem The clicked option.
	 * @param FlatOptionIndex The index of the option over all sub sections of the trainer.
	 * @return A queue item with the option's costs and training time; without requirement snapshot.
	 */
	FTrainingQueueItem CreateQueueItemForOption(const FTrainingWidgetState& TrainingItem, const int32 FlatOptionIndex) const;

	/**
	 * @brief Adds a training item to the trainer's queue.
	 * @param TrainingItem The training item to add.
	 * @pre Assumes M_PrimarySelectedTrainer is valid.
	 * @post The widget that was clicked is updated with the count and possibly clock.
	 */
	void AddTrainingItemToQueue(const FTrainingQueueItem& TrainingItem);

	void RemoveLastOptionFromQueue(const FTrainingOption& TrainingOption) const;

	/**
	 * @brief Obtains the queue from the primary selected trainer and updates the FTrainingWidgetState of each widget.
	 * @pre M_PrimarySelectedTrainer is not nullptr.
	 */
	void UpdateWidgetsWithPrimaryQueue();
	// void UpdateRequirementOpacityForWidget(
	// 	UW_TrainingItem* TrainingItemWidget,
	// 	const FTrainingWidgetState& WidgetState) const;

	/**
	 * @brief Decodes the requirement of a a training option and turns it into a state for the widget.
	 * @param RequirementForTraining The requirement that holds for training this item.
	 * @param OutRequirementCount
	 * @param OutRequiredUnit
	 * @param OutRequiredTechnology
	 * @param OutSecondaryStatus
	 * @param bOutIsSecondRequirementMet
	 * @param bOutIsFirstRequirementMet
	 * @return The status for the widget that displays the item.
	 */
ETrainingItemStatus CheckRequirement(
	const TObjectPtr<URTSRequirement> RequirementForTraining,
	ERequirementCount& OutRequirementCount,
	FTrainingOption& OutRequiredUnit,
	ETechnology& OutRequiredTechnology,
	FTrainingOption& OutSecondaryRequiredUnit,
	ETechnology& OutSecondaryRequiredTech, ETrainingItemStatus& OutSecondaryStatus, bool& bOutIsSecondRequirementMet, bool
	& bOutIsFirstRequirementMet) const;

	bool GetWidgetIndexInBounds(const int32 Index) const;

	/** 
	 * @param RequestingTrainer The trainer that requests to update the UI.
	 * @return Whether this trainer is primary selected.
	 */
	bool GetRequestIsPrimary(const UTrainerComponent* const RequestingTrainer) const;

	UPROPERTY()
	UMainGameUI* M_MainGameUI;

	bool GetIsValidMainGameUI() const;

	// Stops the clock on the active item and resets it.
	void ResetActiveItem();

	void SetTrainingPause(const bool bPause) const;

	// Goes through the requirements of the current widgets; updates their status.
	void RefreshRequirements();


	UPROPERTY()
	UW_TrainingRequirement* M_TrainingRequirementWidget;

	void SetRequirementPanelVisiblity(const bool bIsVisible) const;

	/** @return Whether we can savely access this training option in the map. */
	bool CheckTrainingOptionInMap(const FTrainingOption TrainingOption) const;

	static FTrainingWidgetState M_EmptyTrainingOptionState;

	UPROPERTY()
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;
	bool GetIsValidPlayerResourceManager() const;
	void Init_SetupPlayerResourceManager(const UObject* WorldContextObject,
	                                     UW_TrainingDescription* TrainingDescription);

	bool EnsureIsValidTrainingRequirementWidget() const;
	void UpdateRequirementWidget(const FTrainingWidgetState& TrainingWidgetState) const;

	void ReportInvalidActiveItemForQueueClock(int32 IndexActiveTrainingItem) const;
	TMap<ERTSResourceType, int32> GetTrainingCosts(const FTrainingOption& TrainingOption) const;

	void OnRequirementNotMetWhenAttemptingToAddToQueue(URTSRequirement* Requirement);
};
