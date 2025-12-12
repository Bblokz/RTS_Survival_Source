#pragma once

#include "CoreMinimal.h"


#include "SelectionChangeAction.generated.h"


UENUM()
enum class ESelectionChangeAction : uint8
{
	None,
	// The selection needs to be fully reset. 
	FullResetSelection,
	// No change happened to selection.
	SelectionInvariant,
	// When we either shift selected units or clicked a single allied unit to select and the types added require
	// a change in the UI hierarchy.
	UnitAdded_RebuildUIHierarchy,
	// When we used control click to remove an allied unit from the selection.
	// And Either:
	// this unit was the last selected of its type; meaning the UI hierarchy needs to be rebuilt.
	// Or: This unit was primary selected and now the UI hierarchy needs to be rebuilt.
	UnitRemoved_RebuildUIHierarchy,
	// A unit removal does not require a UIHierarchy rebuild, but the Selection UI does need to adjust.
	UnitRemoved_HierarchyInvariant,
	// A unit addition does not require a UIHierarchy rebuild, but the Selection UI does need to adjust.
	UnitAdded_HierarchyInvariant,
};


static FString Global_GetRTSSelectedTypeDisplayName(const ESelectionChangeAction SelectedType)
{
	switch (SelectedType)
	{
	case ESelectionChangeAction::None:
		return "None";
	case ESelectionChangeAction::FullResetSelection:
		return "Full Reset Selection";
	case ESelectionChangeAction::SelectionInvariant:
		return "Selection Invariant";
	case ESelectionChangeAction::UnitAdded_RebuildUIHierarchy:
		return "Unit Added";
	case ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy:
		return "UnitRemoved_RebuildUIHierarchy";
	case ESelectionChangeAction::UnitRemoved_HierarchyInvariant:
		return "Unit Removed - Hierarchy Invariant";
	case ESelectionChangeAction::UnitAdded_HierarchyInvariant:
		return "Unit Added - Hierarchy Invariant";
	default:
		return "Unknown RTS Selected Type";
	}
}