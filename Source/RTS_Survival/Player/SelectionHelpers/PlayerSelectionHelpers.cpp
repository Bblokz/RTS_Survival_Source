#include "PlayerSelectionHelpers.h"

#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

/* =========================
 * Simple "different type?" checks
 * ========================= */

bool FPlayerSelectionHelpers::IsPawnOfNotSelectedType(
	TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	ASelectablePawnMaster* NewPawn)
{
	return !IsPawnTypeAlreadySelected(TSelectedPawnMasters, NewPawn);
}

bool FPlayerSelectionHelpers::IsActorOfNotSelectedType(
	TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
	ASelectableActorObjectsMaster* NewActor)
{
	return !IsActorTypeAlreadySelected(TSelectedActorMaseters, NewActor);
}

bool FPlayerSelectionHelpers::IsSquadOfNotSelectedType(
	TArray<ASquadController*>* TSelectedSquads,
	ASquadController* NewSquad)
{
	return !IsSquadTypeAlreadySelected(TSelectedSquads, NewSquad);
}

bool FPlayerSelectionHelpers::IsSquadUnitSelected(ASquadUnit* SquadUnit)
{
	if (not RTSFunctionLibrary::RTSIsValid(SquadUnit))
	{
		return false;
	}
	if (IsValid(SquadUnit->GetSelectionComponent()))
	{
		return SquadUnit->GetSelectionComponent()->GetIsSelected();
	}
	return false;
}

bool FPlayerSelectionHelpers::IsPawnSelected(ASelectablePawnMaster* Pawn)
{
	if (not RTSFunctionLibrary::RTSIsValid(Pawn))
	{
		return false;
	}
	if (IsValid(Pawn->GetSelectionComponent()))
	{
		return Pawn->GetSelectionComponent()->GetIsSelected();
	}
	return false;
}

bool FPlayerSelectionHelpers::IsActorSelected(ASelectableActorObjectsMaster* Actor)
{
	if (not RTSFunctionLibrary::RTSIsValid(Actor))
	{
		return false;
	}
	if (IsValid(Actor->GetSelectionComponent()))
	{
		return Actor->GetSelectionComponent()->GetIsSelected();
	}
	return false;
}

/* =========================
 * Selection-change helpers for adding units
 * ========================= */

ESelectionChangeAction FPlayerSelectionHelpers::OnAddingPawn_GetSelectionChange(
	TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	ASelectablePawnMaster* NewPawn)
{
	if (!RTSFunctionLibrary::RTSIsValid(NewPawn))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (IsPawnTypeAlreadySelected(TSelectedPawnMasters, NewPawn))
	{
		return ESelectionChangeAction::UnitAdded_HierarchyInvariant;
	}
	return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
}

ESelectionChangeAction FPlayerSelectionHelpers::OnAddingActor_GetSelectionChange(
	TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
	ASelectableActorObjectsMaster* NewActor)
{
	if (!RTSFunctionLibrary::RTSIsValid(NewActor))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (IsActorTypeAlreadySelected(TSelectedActorMaseters, NewActor))
	{
		return ESelectionChangeAction::UnitAdded_HierarchyInvariant;
	}
	return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
}

ESelectionChangeAction FPlayerSelectionHelpers::OnAddingSquad_GetSelectionChange(
	TArray<ASquadController*>* TSelectedSquads,
	ASquadController* NewSquad)
{
	if (!RTSFunctionLibrary::RTSIsValid(NewSquad))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (IsSquadTypeAlreadySelected(TSelectedSquads, NewSquad))
	{
		return ESelectionChangeAction::UnitAdded_HierarchyInvariant;
	}
	return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
}

/* =========================
 * Utilities
 * ========================= */

bool FPlayerSelectionHelpers::TryBuildUnitIDFromActor(const AActor* InActor, FTrainingOption& OutUnitId)
{
	if (!IsValid(InActor))
	{
		return false;
	}
	const URTSComponent* RTS = Cast<URTSComponent>(InActor->GetComponentByClass(URTSComponent::StaticClass()));
	if (!RTS)
	{
		return false;
	}
	OutUnitId.UnitType = RTS->GetUnitType();
	OutUnitId.SubtypeValue = RTS->GetUnitSubType();
	return true;
}

bool FPlayerSelectionHelpers::GetTypeFromSelectable(const AActor* Actor, EAllUnitType& OutType)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const URTSComponent* RTS = Cast<URTSComponent>(Actor->GetComponentByClass(URTSComponent::StaticClass()));
	if (!RTS)
	{
		return false;
	}
	OutType = RTS->GetUnitType();
	return true;
}

TSet<FTrainingOption> FPlayerSelectionHelpers::BuildTrainingOptionSet(
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectableActorObjectsMaster*>& Actors)
{
	TSet<FTrainingOption> Out;
	Out.Reserve(Pawns.Num() + Squads.Num() + Actors.Num());

	// Pawns
	for (int32 i = 0; i < Pawns.Num(); ++i)
	{
		const ASelectablePawnMaster* P = Pawns[i];
		if (!IsValid(P)) { continue; }
		const URTSComponent* RTS = P->GetRTSComponent();
		if (!RTS) { continue; }
		Out.Add(RTS->GetUnitTrainingOption());
	}

	// Squads
	for (int32 i = 0; i < Squads.Num(); ++i)
	{
		const ASquadController* S = Squads[i];
		if (!IsValid(S)) { continue; }
		const URTSComponent* RTS = S->GetRTSComponent();
		if (!RTS) { continue; }
		Out.Add(RTS->GetUnitTrainingOption());
	}

	// Actors
	for (int32 i = 0; i < Actors.Num(); ++i)
	{
		const ASelectableActorObjectsMaster* A = Actors[i];
		if (!IsValid(A)) { continue; }
		const URTSComponent* RTS = A->GetRTSComponent();
		if (!RTS) { continue; }
		Out.Add(RTS->GetUnitTrainingOption());
	}

	return Out;
}


bool FPlayerSelectionHelpers::AreThereTypeDifferencesBetweenSets(const TSet<FTrainingOption>& SetA,
                                                                 const TSet<FTrainingOption>& SetB)
{
	// Check A \ B
	for (TSet<FTrainingOption>::TConstIterator ItA(SetA); ItA; ++ItA)
	{
		if (!SetB.Contains(*ItA))
		{
			return true;
		}
	}
	// Check B \ A
	for (TSet<FTrainingOption>::TConstIterator ItB(SetB); ItB; ++ItB)
	{
		if (!SetA.Contains(*ItB))
		{
			return true;
		}
	}
	return false;
}

void FPlayerSelectionHelpers::DeselectPawnsNotMatchingID(
	TArray<ASelectablePawnMaster*>& InOutPawns,
	const FTrainingOption& FilterID)
{
	TArray<ASelectablePawnMaster*> Keep;
	Keep.Reserve(InOutPawns.Num());

	for (int32 i = 0; i < InOutPawns.Num(); ++i)
	{
		ASelectablePawnMaster* Ptr = InOutPawns[i];
		if (!IsValid(Ptr)) { continue; }

		const URTSComponent* RTS = Ptr->GetRTSComponent();
		if (!RTS) { continue; }

		if (RTS->GetUnitTrainingOption() == FilterID)
		{
			Keep.Add(Ptr);
		}
		else
		{
			Ptr->SetUnitSelected(false);
		}
	}
	InOutPawns = Keep;
}

void FPlayerSelectionHelpers::DeselectSquadsNotMatchingID(
	TArray<ASquadController*>& InOutSquads,
	const FTrainingOption& FilterID)
{
	TArray<ASquadController*> Keep;
	Keep.Reserve(InOutSquads.Num());

	for (int32 i = 0; i < InOutSquads.Num(); ++i)
	{
		ASquadController* Ptr = InOutSquads[i];
		if (!IsValid(Ptr)) { continue; }

		const URTSComponent* RTS = Ptr->GetRTSComponent();
		if (!RTS) { continue; }

		if (RTS->GetUnitTrainingOption() == FilterID)
		{
			Keep.Add(Ptr);
		}
		else
		{
			Ptr->SetSquadSelected(false);
		}
	}
	InOutSquads = Keep;
}

void FPlayerSelectionHelpers::DeselectActorsNotMatchingID(
	TArray<ASelectableActorObjectsMaster*>& InOutActors,
	const FTrainingOption FilterID)
{
	TArray<ASelectableActorObjectsMaster*> Keep;
	Keep.Reserve(InOutActors.Num());

	for (int32 i = 0; i < InOutActors.Num(); ++i)
	{
		ASelectableActorObjectsMaster* Ptr = InOutActors[i];
		if (!IsValid(Ptr)) { continue; }

		const URTSComponent* RTS = Ptr->GetRTSComponent();
		if (!RTS) { continue; }

		if (RTS->GetUnitTrainingOption() == FilterID)
		{
			Keep.Add(Ptr);
		}
		else
		{
			Ptr->SetUnitSelected(false);
		}
	}
	InOutActors = Keep;
}

/* =========================
 * Remove-at-index-if-matches (raw-pointer arrays)
 * Turns off selection visuals when removing
 * ========================= */

bool FPlayerSelectionHelpers::RemovePawnAtIndexIfMatches(
	TArray<ASelectablePawnMaster*>& InOutPawns,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InOutPawns.IsValidIndex(Index)) { return false; }

	ASelectablePawnMaster* Candidate = InOutPawns[Index];
	if (!IsValid(Candidate)) { return false; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return false; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	if (Actual.UnitType != Match.UnitType || Actual.SubtypeValue != Match.SubtypeValue)
	{
		return false;
	}

	Candidate->SetUnitSelected(false);
	InOutPawns.RemoveAt(Index);
	return true;
}

ASelectablePawnMaster* FPlayerSelectionHelpers::GetPawnAtIndexIfMatches(
	const TArray<ASelectablePawnMaster*>& InPawns,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InPawns.IsValidIndex(Index)) { return nullptr; }

	ASelectablePawnMaster* Candidate = InPawns[Index];
	if (!IsValid(Candidate)) { return nullptr; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return nullptr; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	return (Actual.UnitType == Match.UnitType && Actual.SubtypeValue == Match.SubtypeValue) ? Candidate : nullptr;
}

bool FPlayerSelectionHelpers::RemoveSquadAtIndexIfMatches(
	TArray<ASquadController*>& InOutSquads,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InOutSquads.IsValidIndex(Index)) { return false; }

	ASquadController* Candidate = InOutSquads[Index];
	if (!IsValid(Candidate)) { return false; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return false; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	if (Actual.UnitType != Match.UnitType || Actual.SubtypeValue != Match.SubtypeValue)
	{
		return false;
	}

	Candidate->SetSquadSelected(false);
	InOutSquads.RemoveAt(Index);
	return true;
}

ASquadController* FPlayerSelectionHelpers::GetSquadAtIndexIfMatches(
	const TArray<ASquadController*>& InSquads,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InSquads.IsValidIndex(Index)) { return nullptr; }

	ASquadController* Candidate = InSquads[Index];
	if (!IsValid(Candidate)) { return nullptr; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return nullptr; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	return (Actual.UnitType == Match.UnitType && Actual.SubtypeValue == Match.SubtypeValue) ? Candidate : nullptr;
}

bool FPlayerSelectionHelpers::RemoveActorAtIndexIfMatches(
	TArray<ASelectableActorObjectsMaster*>& InOutActors,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InOutActors.IsValidIndex(Index)) { return false; }

	ASelectableActorObjectsMaster* Candidate = InOutActors[Index];
	if (!IsValid(Candidate)) { return false; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return false; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	if (Actual.UnitType != Match.UnitType || Actual.SubtypeValue != Match.SubtypeValue)
	{
		return false;
	}

	Candidate->SetUnitSelected(false);
	InOutActors.RemoveAt(Index);
	return true;
}

ASelectableActorObjectsMaster* FPlayerSelectionHelpers::GetActorAtIndexIfMatches(
	const TArray<ASelectableActorObjectsMaster*>& InActors,
	int32 Index,
	const FTrainingOption& Match)
{
	if (!InActors.IsValidIndex(Index)) { return nullptr; }

	ASelectableActorObjectsMaster* Candidate = InActors[Index];
	if (!IsValid(Candidate)) { return nullptr; }

	const URTSComponent* RTS = Candidate->GetRTSComponent();
	if (!RTS) { return nullptr; }

	const FTrainingOption Actual = RTS->GetUnitTrainingOption();
	return (Actual.UnitType == Match.UnitType && Actual.SubtypeValue == Match.SubtypeValue) ? Candidate : nullptr;
}

/* =========================
 * "Already selected" private checks
 * ========================= */

bool FPlayerSelectionHelpers::IsPawnTypeAlreadySelected(
	TArray<ASelectablePawnMaster*>* TSelectedPawnMasters,
	ASelectablePawnMaster* NewPawn)
{
	if (NewPawn == nullptr || TSelectedPawnMasters == nullptr)
	{
		return true;
	}
	URTSComponent* RTSComp = NewPawn->GetRTSComponent();
	if (!RTSComp)
	{
		return true;
	}

	for (int32 i = 0; i < TSelectedPawnMasters->Num(); ++i)
	{
		ASelectablePawnMaster* EachPawn = (*TSelectedPawnMasters)[i];
		if (!IsValid(EachPawn)) { continue; }
		const URTSComponent* EachRTS = EachPawn->GetRTSComponent();
		if (!IsValid(EachRTS)) { continue; }

		if (RTSComp->GetUnitTrainingOption() == EachRTS->GetUnitTrainingOption())
		{
			return true;
		}
	}
	return false;
}

bool FPlayerSelectionHelpers::IsActorTypeAlreadySelected(
	TArray<ASelectableActorObjectsMaster*>* TSelectedActorMaseters,
	ASelectableActorObjectsMaster* NewActor)
{
	if (NewActor == nullptr || TSelectedActorMaseters == nullptr)
	{
		return true;
	}
	URTSComponent* RTSComp = NewActor->GetRTSComponent();
	if (!RTSComp)
	{
		return true;
	}

	for (int32 i = 0; i < TSelectedActorMaseters->Num(); ++i)
	{
		ASelectableActorObjectsMaster* EachActor = (*TSelectedActorMaseters)[i];
		if (!IsValid(EachActor)) { continue; }
		const URTSComponent* EachRTS = EachActor->GetRTSComponent();
		if (!IsValid(EachRTS)) { continue; }

		if (RTSComp->GetUnitTrainingOption() == EachRTS->GetUnitTrainingOption())
		{
			return true;
		}
	}
	return false;
}

bool FPlayerSelectionHelpers::IsSquadTypeAlreadySelected(
	TArray<ASquadController*>* TSelectedSquads,
	ASquadController* NewSquad)
{
	if (NewSquad == nullptr || TSelectedSquads == nullptr)
	{
		return true;
	}
	URTSComponent* RTSComp = NewSquad->GetRTSComponent();
	if (!RTSComp)
	{
		return true;
	}

	for (int32 i = 0; i < TSelectedSquads->Num(); ++i)
	{
		ASquadController* EachSquad = (*TSelectedSquads)[i];
		if (!IsValid(EachSquad)) { continue; }
		const URTSComponent* EachRTS = EachSquad->GetRTSComponent();
		if (!IsValid(EachRTS)) { continue; }

		if (RTSComp->GetUnitTrainingOption() == EachRTS->GetUnitTrainingOption())
		{
			return true;
		}
	}
	return false;
}
