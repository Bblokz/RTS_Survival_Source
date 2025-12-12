#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/SelectionHelpers/SelectionChangeAction.h"
#include "PlayerControlGroupManager.generated.h"

struct FTrainingOption;
class UW_ControlGroups;
class ASelectableActorObjectsMaster;
class ASelectablePawnMaster;
class ASquadController;
class ACPPController; // Replace with your player controller class name

// We put 10 FControlGroup structs in one array to store all ControlGroups
USTRUCT()
struct FControlGroup
{
	GENERATED_BODY()

	/** @brief Ensures all weak references in the control group are valid. */
	void EnsureControlGroupIsValid();

	bool IsEmpty()const;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadController>> CGSquads;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASelectablePawnMaster>> CGPawns;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASelectableActorObjectsMaster>> GCActors;

	FControlGroup()
	{
	}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerControlGroupManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerControlGroupManager();

	/**
	 * @brief Manages control groups based on input.
	 *
	 * - If Control is held: Sets the control group from current selection.
	 * - If Shift is held: Adds the control group to current selection.
	 * - If neither: Replaces current selection with the control group.
	 *
	 * @param GroupIndex Index of the control group (typically 0-9).
	 * @param bIsHoldingShift True if Shift key is held.
	 * @param bIsHoldingControl True if Control key is held.
	 * @param AddedActorByControlGroup
	 */
	UFUNCTION(BlueprintCallable)
	ESelectionChangeAction UseControlGroup(int32 GroupIndex, bool bIsHoldingShift, bool bIsHoldingControl, AActor*& AddedActorByControlGroup);

	inline void SetControlGroupsWidget(UW_ControlGroups* ControlGroupsWidget)
	{
		M_ControlGroupsWidget = ControlGroupsWidget;
	};

	FVector GetControlGroupLocation(const int32 GroupIndex, bool& bIsValidLocation);

protected:
	virtual void BeginPlay() override;

private:
	/** Array containing 10 ControlGroups for selection (indices 0-9). */
	UPROPERTY()
	TArray<FControlGroup> M_ControlGroups;

	/** Cached reference to the player controller. */
	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	/**
	 * @brief Adds units from the control group to the current selection.
	 * Called when Shift is held.
	 * @param GroupIndex Index of the control group to use.
	 * @param AddedActorByControlGroup
	 */
	ESelectionChangeAction AddControlGroupToSelection_UpdateDecals(int32 GroupIndex, AActor*& AddedActorByControlGroup);

	/**
	 * @brief Sets the control group from the current selection.
	 * Called when Control is held.
	 * @param GroupIndex Index of the control group to set.
	 */
	void SetControlGroupFromSelection(int32 GroupIndex);

	/**
	 * @brief Replaces the current selection with units from the control group.
	 * Called when neither Shift nor Control is held.
	 * @param GroupIndex Index of the control group to select.
	 * @param AddedActorByControlGroup
	 */
	void SelectOnlyControlGroup(int32 GroupIndex, AActor*& AddedActorByControlGroup);

	/** Ensures all control groups contain valid references. */
	void EnsureControlGroupIsValid();

	/**
	 * @brief For each selected unit set them to deselected if they have a valid
	 * selection component
	 */
	void DeselectCurrentSelection() const;

	FTrainingOption GetMostFrequentUnitOfGroup(int32 GroupIndex, AActor*& OutMostFrequentUnit);

	bool EnsureValidUseControlGroupRequest(const int32 GroupIndex) const;

	/** @return Whether we added a new type of unit to the selection that the hierarchy needs to be updated for.*/
	ESelectionChangeAction AddSquadsFromControlGroupToSelection(FControlGroup& ControlGroup, const int32 GroupIndex,
	                                                            AActor*& NewTypeActor) const;
	/** @return Whether we added a new type of unit to the selection that the hierarchy needs to be updated for.*/
	ESelectionChangeAction AddPawnsFromControlGroupToSelection(FControlGroup& ControlGroup, const int32 GroupIndex,
	                                                           AActor*& NewTypeActor) const;
	/** @return Whether we added a new type of unit to the selection that the hierarchy needs to be updated for.*/
	ESelectionChangeAction AddActorsFromControlGroupToSelection(FControlGroup& ControlGroup, const int32 GroupIndex,
	                                                            AActor*& NewTypeActor) const;

	/** @return Whether the player can select this squad.
	 * @pre Assumes valid player controller reference.
	 */
	bool IsSquadValidAndNotInSelection(ASquadController* SquadController) const;
	/**
	 * @pre Assumes valid player controller reference.
	 */
	bool IsPawnValidAndNotInSelection(ASelectablePawnMaster* PawnMaster) const;
	/**
	 * @pre Assumes valid player controller reference.
	 */
	bool IsActorValidAndNotInSelection(ASelectableActorObjectsMaster* ActorMaster) const;


	UPROPERTY()
	TObjectPtr<UW_ControlGroups> M_ControlGroupsWidget;
};
