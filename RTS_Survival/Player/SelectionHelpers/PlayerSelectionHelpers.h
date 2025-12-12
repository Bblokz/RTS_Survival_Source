#pragma once

#include "CoreMinimal.h"
#include "SelectionChangeAction.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"

class ASquadUnit;
struct FTrainingOption;
class AActor;
class ASquadController;
class ASelectableActorObjectsMaster;
class ASelectablePawnMaster;

/**
* @brief Utility helpers for reasoning about, filtering, and mutating the current player selection.
*
* @note ******************************
* @note Overview:
* @note - Centralizes selection-related logic (type checks, filtering, removal) for pawns, squads, and actors.
* @note - Avoids templates and lambdas for readability and easier debugging.
* @note - All helpers are stateless and implemented as static functions.
* @note ******************************
* @note Expected usage:
* @note - Controller code calls these helpers before/after selection changes to decide if UI needs rebuilding.
* @note - UI code can use BuildTypeSet to understand which main unit types are currently selected.
*/
class FPlayerSelectionHelpers
{
public:

	/** @return Whether the pawn's type is not yet represented in the current selection. */
	static bool IsPawnOfNotSelectedType(TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	                                    ASelectablePawnMaster* NewPawn);

	/** @return Whether the actor's type is not yet represented in the current selection. */
	static bool IsActorOfNotSelectedType(TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
	                                     ASelectableActorObjectsMaster* NewActor);

	/** @return Whether the squad's type is not yet represented in the current selection. */
	static bool IsSquadOfNotSelectedType(TArray<ASquadController*>* TSelectedSquads,
	                                     ASquadController* NewSquad);

	static bool IsSquadUnitSelected(ASquadUnit* SquadUnit);
	static bool IsPawnSelected(ASelectablePawnMaster* Pawn);
	static bool IsActorSelected(ASelectableActorObjectsMaster* Actor);

	/**
	* @brief Determine the selection change impact when adding a pawn.
	*
	* @param TSelectedPawnMasters Pointer to the current selected pawns array.
	* @param NewPawn The pawn that would be added.
	* @return The selection change action to perform (e.g., rebuild hierarchy, invariant, etc.).
	*/
	static ESelectionChangeAction OnAddingPawn_GetSelectionChange(TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	                                                              ASelectablePawnMaster* NewPawn);

	/**
	* @brief Determine the selection change impact when adding an actor.
	*
	* @param TSelectedActorMaseters Pointer to the current selected actors array.
	* @param NewActor The actor that would be added.
	* @return The selection change action to perform (e.g., rebuild hierarchy, invariant, etc.).
	*/
	static ESelectionChangeAction OnAddingActor_GetSelectionChange(
		TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
		ASelectableActorObjectsMaster* NewActor);

	/**
	* @brief Determine the selection change impact when adding a squad.
	*
	* @param TSelectedSquads Pointer to the current selected squads array.
	* @param NewSquad The squad that would be added.
	* @return The selection change action to perform (e.g., rebuild hierarchy, invariant, etc.).
	*/
	static ESelectionChangeAction OnAddingSquad_GetSelectionChange(TArray<ASquadController*>* TSelectedSquads,
	                                                               ASquadController* NewSquad);

	/**
	* @brief Build a UnitID (FTrainingOption) from an actor's RTS component.
	*
	* @param InActor The source actor to inspect.
	* @param OutUnitId Filled with the unit type and subtype if an RTS component is found.
	* @return True if OutUnitId was filled; false if InActor is invalid or has no RTS component.
	*/
	static bool TryBuildUnitIDFromActor(const AActor* InActor, FTrainingOption& OutUnitId);

	/**
	* @brief Extract the main unit type from a selectable actor that owns an RTS component.
	*
	* @param Actor The actor to inspect.
	* @param OutType Filled with the unit's main type on success.
	* @return True if the type could be extracted; false otherwise.
	*/
	static bool GetTypeFromSelectable(const AActor* Actor, EAllUnitType& OutType);

/** Build the set of full training options (type + subtype) currently selected. */
static TSet<FTrainingOption> BuildTrainingOptionSet(
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectableActorObjectsMaster*>& Actors);

/** @return True if there is any difference between the two sets of training options (either direction). */
static bool AreThereTypeDifferencesBetweenSets(const TSet<FTrainingOption>& SetA,
                                               const TSet<FTrainingOption>& SetB);
	/**
	* @brief Remove a pawn at the given index if its FTrainingOption matches.
	*
	* @param InOutPawns The selection array to mutate.
	* @param Index The index to check/remove.
	* @param Match The expected training option (type + subtype).
	* @return True if the entry matched and was removed (selection visuals are cleared); false otherwise.
	*/
	static bool RemovePawnAtIndexIfMatches(
		TArray<ASelectablePawnMaster*>& InOutPawns,
		int32 Index,
		const FTrainingOption& Match);

	static ASelectablePawnMaster* GetPawnAtIndexIfMatches(
		const TArray<ASelectablePawnMaster*>& InPawns,
		int32 Index,
		const FTrainingOption& Match);

	/**
	* @brief Remove a squad at the given index if its FTrainingOption matches.
	*
	* @param InOutSquads The selection array to mutate.
	* @param Index The index to check/remove.
	* @param Match The expected training option (type + subtype).
	* @return True if the entry matched and was removed (selection visuals are cleared); false otherwise.
	*/
	static bool RemoveSquadAtIndexIfMatches(
		TArray<ASquadController*>& InOutSquads,
		int32 Index,
		const FTrainingOption& Match);

	static ASquadController* GetSquadAtIndexIfMatches(
		const TArray<ASquadController*>& InSquads,
		int32 Index,
		const FTrainingOption& Match);
	

	/**
	* @brief Remove an actor at the given index if its FTrainingOption matches.
	*
	* @param InOutActors The selection array to mutate.
	* @param Index The index to check/remove.
	* @param Match The expected training option (type + subtype).
	* @return True if the entry matched and was removed (selection visuals are cleared); false otherwise.
	*/
	static bool RemoveActorAtIndexIfMatches(
		TArray<ASelectableActorObjectsMaster*>& InOutActors,
		int32 Index,
		const FTrainingOption& Match);

	static ASelectableActorObjectsMaster* GetActorAtIndexIfMatches(
		const TArray<ASelectableActorObjectsMaster*>& InActors,
		int32 Index,
		const FTrainingOption& Match);
	/**
	* @brief Filter arrays in place to keep only entries of the provided type.
	*
	* @param InOutPawns Selection array to filter; non-matching entries are deselected and removed.
	* @param FilterID The unit main type to keep.
	* @post Non-matching entries have their selection visuals turned off.
	*/
	static void DeselectPawnsNotMatchingID(TArray<ASelectablePawnMaster*>& InOutPawns, const FTrainingOption& FilterID);

	/**
	* @brief Filter arrays in place to keep only entries of the provided type.
	*
	* @param InOutSquads Selection array to filter; non-matching entries are deselected and removed.
	* @param FilterID The unit main type to keep.
	* @post Non-matching entries have their selection visuals turned off.
	*/
	static void DeselectSquadsNotMatchingID(TArray<ASquadController*>& InOutSquads, const FTrainingOption& FilterID);

	/**
	* @brief Filter arrays in place to keep only entries of the provided type.
	*
	* @param InOutActors Selection array to filter; non-matching entries are deselected and removed.
	* @param FilterID The unit main type to keep.
	* @post Non-matching entries have their selection visuals turned off.
	*/
	static void DeselectActorsNotMatchingID(TArray<ASelectableActorObjectsMaster*>& InOutActors, const FTrainingOption FilterID);

private:

	/** @return Whether a pawn of the same training option already exists in the selection. */
	static bool IsPawnTypeAlreadySelected(TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	                                      ASelectablePawnMaster* NewPawn);

	/** @return Whether an actor of the same training option already exists in the selection. */
	static bool IsActorTypeAlreadySelected(TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
	                                       ASelectableActorObjectsMaster* NewActor);

	/** @return Whether a squad of the same training option already exists in the selection. */
	static bool IsSquadTypeAlreadySelected(TArray<ASquadController*>* TSelectedSquads,
	                                       ASquadController* NewSquad);
};
