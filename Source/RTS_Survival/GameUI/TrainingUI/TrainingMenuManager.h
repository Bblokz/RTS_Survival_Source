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
	 * @brief Initializes the training manager with the provided training items.
	 * @param TrainingItemsInMenu Array of training items in the menu.
	 * @param TrainingDescription
	 */
	void InitTrainingManager(
		TArray<UW_TrainingItem*> TrainingItemsInMenu,
		UMainGameUI* MainGameUI,
		UW_TrainingRequirement* TrainingRequirementWidget, UW_TrainingDescription* TrainingDescription);

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

	// An array of all the training widgets in the menu.
	UPROPERTY()
	TArray<UW_TrainingItem*> M_TTrainingMenuWidgets;

	// The widget item that currently has a clock running.
	UPROPERTY()
	UW_TrainingItem* M_ActiveTrainingItem;

	void StartClockOnTrainingItem(
		const int32 IndexActiveTrainingItem,
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

	// Makes sure that training times are not too short to cause load issues on AsyncSpawner.
	void PostInitTrainingOptions();

	void OnLeftClickTrainingItem(
	const FTrainingWidgetState& TrainingItem,
	const int32 Index);

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
