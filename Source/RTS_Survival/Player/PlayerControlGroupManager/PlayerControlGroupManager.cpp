// PlayerControlGroupManager.cpp

#include "PlayerControlGroupManager.h"

#include "RTS_Survival/GameUI/ControlGroups/W_ControlGroups.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/SelectionHelpers/PlayerSelectionHelpers.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

void FControlGroup::EnsureControlGroupIsValid()
{
	// Remove invalid squad references
	CGSquads.RemoveAll([](const TWeakObjectPtr<ASquadController>& Ptr)
	{
		return !Ptr.IsValid();
	});

	// Remove invalid pawn references
	CGPawns.RemoveAll([](const TWeakObjectPtr<ASelectablePawnMaster>& Ptr)
	{
		return !Ptr.IsValid();
	});

	// Remove invalid actor references
	GCActors.RemoveAll([](const TWeakObjectPtr<ASelectableActorObjectsMaster>& Ptr)
	{
		return !Ptr.IsValid();
	});
}

bool FControlGroup::IsEmpty() const
{
	return CGSquads.Num() == 0 && CGPawns.Num() == 0 && GCActors.Num() == 0;
}

UPlayerControlGroupManager::UPlayerControlGroupManager(): M_PlayerController(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;

	// Initialize ControlGroups with 10 empty FControlGroup structs
	M_ControlGroups.SetNum(10);
}

FVector UPlayerControlGroupManager::GetControlGroupLocation(const int32 GroupIndex, bool& bIsValidLocation)
{
	if (!M_ControlGroups.IsValidIndex(GroupIndex))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid group index provided for the UPlayerControlGroupManager! Expected a valid index, got " +
			FString::FromInt(GroupIndex) + " in function: UPlayerControlGroupManager::GetControlGroupLocation");
		bIsValidLocation = false;
		return FVector::ZeroVector;
	}

	AActor* MostFrequentUnit = nullptr;
	FTrainingOption MostFrequentOption = GetMostFrequentUnitOfGroup(GroupIndex, MostFrequentUnit);

	if (IsValid(MostFrequentUnit))
	{
		bIsValidLocation = true;
		return MostFrequentUnit->GetActorLocation();
	}
	// No valid unit found
	bIsValidLocation = false;
	return FVector::ZeroVector;
}


void UPlayerControlGroupManager::BeginPlay()
{
	Super::BeginPlay();

	// Cache the player controller and selected units arrays
	M_PlayerController = Cast<ACPPController>(GetOwner());
}

ESelectionChangeAction UPlayerControlGroupManager::UseControlGroup(
	const int32 GroupIndex,
	const bool bIsHoldingShift,
	const bool bIsHoldingControl, AActor*& AddedActorByControlGroup)
{
	if (not EnsureValidUseControlGroupRequest(GroupIndex))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (bIsHoldingControl)
	{
		SetControlGroupFromSelection(GroupIndex);
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (bIsHoldingShift)
	{
		return AddControlGroupToSelection_UpdateDecals(GroupIndex, AddedActorByControlGroup);
	}
	// Deselect current selection and select control group
	SelectOnlyControlGroup(GroupIndex, AddedActorByControlGroup);
	return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
}


ESelectionChangeAction UPlayerControlGroupManager::AddControlGroupToSelection_UpdateDecals(const int32 GroupIndex,
	AActor*& AddedActorByControlGroup)
{
	using DeveloperSettings::Debugging::GControlGroups_Compile_DebugSymbols;

	FControlGroup& ControlGroup = M_ControlGroups[GroupIndex];
	ControlGroup.EnsureControlGroupIsValid();
	if (ControlGroup.IsEmpty())
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	bool bRebuildHierarchy = false;


	// Add squads to selection
	if (AddSquadsFromControlGroupToSelection(ControlGroup, GroupIndex, AddedActorByControlGroup) ==
		ESelectionChangeAction::UnitAdded_RebuildUIHierarchy)
	{
		bRebuildHierarchy = true;
	}

	// Add pawns to selection
	if (AddPawnsFromControlGroupToSelection(ControlGroup, GroupIndex, AddedActorByControlGroup) ==
		ESelectionChangeAction::UnitAdded_RebuildUIHierarchy)
	{
		bRebuildHierarchy = true;
	}

	// Add actors to selection
	if (AddActorsFromControlGroupToSelection(ControlGroup, GroupIndex, AddedActorByControlGroup) ==
		ESelectionChangeAction::UnitAdded_RebuildUIHierarchy)
	{
		bRebuildHierarchy = true;
	}
	if (bRebuildHierarchy)
	{
		return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
	}
	return ESelectionChangeAction::UnitAdded_HierarchyInvariant;
}


void UPlayerControlGroupManager::SetControlGroupFromSelection(const int32 GroupIndex)
{
	FControlGroup& ControlGroup = M_ControlGroups[GroupIndex];

	// Clear existing references
	ControlGroup.CGSquads.Empty();
	ControlGroup.CGPawns.Empty();
	ControlGroup.GCActors.Empty();

	// Add current squads to control group
	for (ASquadController* Squad : M_PlayerController->TSelectedSquadControllers)
	{
		if (IsValid(Squad))
		{
			ControlGroup.CGSquads.Add(Squad);
		}
	}

	// Add current pawns to control group
	for (ASelectablePawnMaster* Pawn : M_PlayerController->TSelectedPawnMasters)
	{
		if (IsValid(Pawn))
		{
			ControlGroup.CGPawns.Add(Pawn);
		}
	}

	// Add current actors to control group
	for (ASelectableActorObjectsMaster* Actor : M_PlayerController->TSelectedActorsMasters)
	{
		if (IsValid(Actor))
		{
			ControlGroup.GCActors.Add(Actor);
		}
	}

	if (M_ControlGroupsWidget)
	{
		AActor* OutMostFrequentUnit = nullptr;
		const FTrainingOption MostFrequentUnit = GetMostFrequentUnitOfGroup(GroupIndex, OutMostFrequentUnit);
		M_ControlGroupsWidget->UpdateMostFrequentUnitInGroup(GroupIndex, MostFrequentUnit);
	}
}


void UPlayerControlGroupManager::SelectOnlyControlGroup(const int32 GroupIndex, AActor*& AddedActorByControlGroup)
{
	using DeveloperSettings::Debugging::GControlGroups_Compile_DebugSymbols;

	FControlGroup& ControlGroup = M_ControlGroups[GroupIndex];
	ControlGroup.EnsureControlGroupIsValid();
	// Set units to no longer be selected.
	DeselectCurrentSelection();

	// Clear current selection
	M_PlayerController->TSelectedSquadControllers.Empty();
	M_PlayerController->TSelectedPawnMasters.Empty();
	M_PlayerController->TSelectedActorsMasters.Empty();

	// Add squads to selection
	for (TWeakObjectPtr<ASquadController> SquadPtr : ControlGroup.CGSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not Squad)
		{
			continue;
		}
		M_PlayerController->TSelectedSquadControllers.Add(Squad);
		Squad->SetSquadSelected(true);

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Squad->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Selected from Control Group %d"), GroupIndex),
			                nullptr, FColor::Blue, 2.0f, false);
		}
	}

	// Add pawns to selection
	for (TWeakObjectPtr<ASelectablePawnMaster> PawnPtr : ControlGroup.CGPawns)
	{
		ASelectablePawnMaster* Pawn = PawnPtr.Get();
		if (not Pawn)
		{
			continue;
		}
		M_PlayerController->TSelectedPawnMasters.Add(Pawn);
		Pawn->SetUnitSelected(true);

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Pawn->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Selected from Control Group %d"), GroupIndex),
			                nullptr, FColor::Blue, 2.0f, false);
		}
	}

	// Add actors to selection
	for (TWeakObjectPtr<ASelectableActorObjectsMaster> ActorPtr : ControlGroup.GCActors)
	{
		ASelectableActorObjectsMaster* Actor = ActorPtr.Get();
		if (not Actor)
		{
			continue;
		}
		M_PlayerController->TSelectedActorsMasters.Add(Actor);
		Actor->SetUnitSelected(true);

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Actor->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Selected from Control Group %d"), GroupIndex),
			                nullptr, FColor::Blue, 2.0f, false);
		}
	}
}

void UPlayerControlGroupManager::EnsureControlGroupIsValid()
{
	for (FControlGroup& ControlGroup : M_ControlGroups)
	{
		ControlGroup.EnsureControlGroupIsValid();
	}
}

void UPlayerControlGroupManager::DeselectCurrentSelection() const
{
	for (const auto EachPawn : M_PlayerController->TSelectedPawnMasters)
	{
		if (not EachPawn)
		{
			continue;
		}
		if (IsValid(EachPawn))
		{
			EachPawn->SetUnitSelected(false);
		}
	}
	for (const auto EachSquadController : M_PlayerController->TSelectedSquadControllers)
	{
		if (not EachSquadController)
		{
			continue;
		}
		if (IsValid(EachSquadController))
		{
			EachSquadController->SetSquadSelected(false);
		}
	}
	for (const auto EachActor : M_PlayerController->TSelectedActorsMasters)
	{
		if (not EachActor)
		{
			continue;
		}
		if (IsValid(EachActor))
		{
			EachActor->SetUnitSelected(false);
		}
	}
}

FTrainingOption UPlayerControlGroupManager::GetMostFrequentUnitOfGroup(const int32 GroupIndex,
                                                                       AActor*& OutMostFrequentUnit)
{
	if (!M_ControlGroups.IsValidIndex(GroupIndex))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid group index provided for the UPlayerControlGroupManager! Expected a valid index, got " +
			FString::FromInt(GroupIndex) + " in function: UPlayerControlGroupManager::GetMostFrequentUnitOfGroup");
		OutMostFrequentUnit = nullptr;
		return FTrainingOption();
	}

	FControlGroup& ControlGroup = M_ControlGroups[GroupIndex];

	// Ensure the control group contains valid references
	ControlGroup.EnsureControlGroupIsValid();

	// Map to store the frequency of each FTrainingOption and an example actor
	TMap<FTrainingOption, TPair<int32, AActor*>> UnitFrequencies;

	// Iterate over squads
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : ControlGroup.CGSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (IsValid(Squad))
		{
			URTSComponent* RTSComp = Squad->GetRTSComponent();
			if (IsValid(RTSComp))
			{
				EAllUnitType UnitType = RTSComp->GetUnitType();
				uint8 SubtypeValue = RTSComp->GetUnitSubType();
				FTrainingOption TrainingOption(UnitType, SubtypeValue);

				// Update frequency and store the actor
				TPair<int32, AActor*>* EntryPtr = UnitFrequencies.Find(TrainingOption);
				if (EntryPtr)
				{
					EntryPtr->Key++;
				}
				else
				{
					// First time we see this TrainingOption
					UnitFrequencies.Add(TrainingOption, TPair<int32, AActor*>(1, Squad));
				}
			}
		}
	}

	// Iterate over pawns
	for (const TWeakObjectPtr<ASelectablePawnMaster>& PawnPtr : ControlGroup.CGPawns)
	{
		ASelectablePawnMaster* Pawn = PawnPtr.Get();
		if (IsValid(Pawn))
		{
			URTSComponent* RTSComp = Pawn->GetRTSComponent();
			if (IsValid(RTSComp))
			{
				EAllUnitType UnitType = RTSComp->GetUnitType();
				uint8 SubtypeValue = RTSComp->GetUnitSubType();
				FTrainingOption TrainingOption(UnitType, SubtypeValue);

				TPair<int32, AActor*>* EntryPtr = UnitFrequencies.Find(TrainingOption);
				if (EntryPtr)
				{
					EntryPtr->Key++;
				}
				else
				{
					UnitFrequencies.Add(TrainingOption, TPair<int32, AActor*>(1, Pawn));
				}
			}
		}
	}

	// Iterate over actors
	for (const TWeakObjectPtr<ASelectableActorObjectsMaster>& ActorPtr : ControlGroup.GCActors)
	{
		ASelectableActorObjectsMaster* Actor = ActorPtr.Get();
		if (IsValid(Actor))
		{
			URTSComponent* RTSComp = Actor->GetRTSComponent();
			if (IsValid(RTSComp))
			{
				EAllUnitType UnitType = RTSComp->GetUnitType();
				uint8 SubtypeValue = RTSComp->GetUnitSubType();
				FTrainingOption TrainingOption(UnitType, SubtypeValue);

				TPair<int32, AActor*>* EntryPtr = UnitFrequencies.Find(TrainingOption);
				if (EntryPtr)
				{
					EntryPtr->Key++;
				}
				else
				{
					UnitFrequencies.Add(TrainingOption, TPair<int32, AActor*>(1, Actor));
				}
			}
		}
	}

	// Find the most frequent TrainingOption
	FTrainingOption MostFrequentOption;
	int32 MaxFrequency = 0;
	OutMostFrequentUnit = nullptr;

	for (const TPair<FTrainingOption, TPair<int32, AActor*>>& Pair : UnitFrequencies)
	{
		int32 Frequency = Pair.Value.Key;
		if (Frequency > MaxFrequency)
		{
			MaxFrequency = Frequency;
			MostFrequentOption = Pair.Key;
			OutMostFrequentUnit = Pair.Value.Value;
		}
	}

	return MostFrequentOption;
}

bool UPlayerControlGroupManager::EnsureValidUseControlGroupRequest(const int32 GroupIndex) const
{
	if (not IsValid(M_PlayerController))
	{
		RTSFunctionLibrary::ReportError("PlayerController not found in UPlayerControlGroupManager."
			"\n See UseControlGroup() function.");
		return false;
	}
	if (not IsValid(M_PlayerController->GetGameUIController()))
	{
		RTSFunctionLibrary::ReportError("Invalid ActionUIController in UPlayerControlGroupManager."
			"\n See UseControlGroup() function."
			"\n Will cancel control group action.");
		return false;
	}

	if (GroupIndex < 0 || GroupIndex >= M_ControlGroups.Num())
	{
		RTSFunctionLibrary::ReportError("Invalid control group index: " + FString::FromInt(GroupIndex));
		return false;
	}
	return true;
}

ESelectionChangeAction UPlayerControlGroupManager::AddSquadsFromControlGroupToSelection(
	FControlGroup& ControlGroup,
	const int32 GroupIndex, AActor*& NewTypeActor) const
{
	using DeveloperSettings::Debugging::GControlGroups_Compile_DebugSymbols;
	bool bAddedNewType = false;
	bool bAddedAnyUnits = false;
	for (TWeakObjectPtr<ASquadController> SquadPtr : ControlGroup.CGSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not IsSquadValidAndNotInSelection(Squad))
		{
			continue;
		}
		if (FPlayerSelectionHelpers::IsSquadOfNotSelectedType(&M_PlayerController->TSelectedSquadControllers, Squad))
		{
			NewTypeActor = Squad;
			bAddedNewType = true;
		}
		M_PlayerController->TSelectedSquadControllers.Add(Squad);
		Squad->SetSquadSelected(true);
		bAddedAnyUnits = true;

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Squad->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Added to Selection from Control Group %d"), GroupIndex),
			                nullptr, FColor::Yellow, 2.0f, false);
		}
	}
	if (not bAddedAnyUnits)
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	return bAddedNewType
		       ? ESelectionChangeAction::UnitAdded_RebuildUIHierarchy
		       : ESelectionChangeAction::UnitAdded_HierarchyInvariant;
}


ESelectionChangeAction UPlayerControlGroupManager::AddPawnsFromControlGroupToSelection(
	FControlGroup& ControlGroup,
	const int32 GroupIndex, AActor*& NewTypeActor) const
{
	using DeveloperSettings::Debugging::GControlGroups_Compile_DebugSymbols;
	bool bAddedNewType = false;
	bool bAddedAnyUnits = false;
	for (TWeakObjectPtr<ASelectablePawnMaster> PawnPtr : ControlGroup.CGPawns)
	{
		ASelectablePawnMaster* Pawn = PawnPtr.Get();
		if (not IsPawnValidAndNotInSelection(Pawn))
		{
			continue;
		}
		if (FPlayerSelectionHelpers::IsPawnOfNotSelectedType(&M_PlayerController->TSelectedPawnMasters, Pawn))
		{
			NewTypeActor = Pawn;
			bAddedNewType = true;
		}
		M_PlayerController->TSelectedPawnMasters.Add(Pawn);
		Pawn->SetUnitSelected(true);
		bAddedAnyUnits = true;

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Pawn->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Added to Selection from Control Group %d"), GroupIndex),
			                nullptr, FColor::Yellow, 2.0f, false);
		}
	}
	if (not bAddedAnyUnits)
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	return bAddedNewType
		       ? ESelectionChangeAction::UnitAdded_RebuildUIHierarchy
		       : ESelectionChangeAction::UnitAdded_HierarchyInvariant;
}

ESelectionChangeAction UPlayerControlGroupManager::AddActorsFromControlGroupToSelection(
	FControlGroup& ControlGroup,
	const int32 GroupIndex, AActor*& NewTypeActor) const
{
	using DeveloperSettings::Debugging::GControlGroups_Compile_DebugSymbols;
	bool bAddedNewType = false;
	bool bAddedAnyUnits = false;
	for (TWeakObjectPtr<ASelectableActorObjectsMaster> ActorPtr : ControlGroup.GCActors)
	{
		ASelectableActorObjectsMaster* Actor = ActorPtr.Get();
		if (not IsActorValidAndNotInSelection(Actor))
		{
			continue;
		}
		if (FPlayerSelectionHelpers::IsActorOfNotSelectedType(&M_PlayerController->TSelectedActorsMasters, Actor))
		{
			bAddedNewType = true;
		}
		M_PlayerController->TSelectedActorsMasters.Add(Actor);
		bAddedAnyUnits = true;
		if (IsValid(Actor->GetSelectionComponent()))
		{
			Actor->GetSelectionComponent()->SetUnitSelected();
		}

		// Debugging
		if (GControlGroups_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = Actor->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation,
			                FString::Printf(TEXT("Added to Selection from Control Group %d"), GroupIndex),
			                nullptr, FColor::Yellow, 2.0f, false);
		}
	}

	if (not bAddedAnyUnits)
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	return bAddedNewType
		       ? ESelectionChangeAction::UnitAdded_RebuildUIHierarchy
		       : ESelectionChangeAction::UnitAdded_HierarchyInvariant;
}

bool UPlayerControlGroupManager::IsSquadValidAndNotInSelection(ASquadController* SquadController) const
{
	if (SquadController && !M_PlayerController->TSelectedSquadControllers.Contains(SquadController))
	{
		return true;
	}
	return false;
}

bool UPlayerControlGroupManager::IsPawnValidAndNotInSelection(ASelectablePawnMaster* PawnMaster) const
{
	if (PawnMaster && !M_PlayerController->TSelectedPawnMasters.Contains(PawnMaster))
	{
		return true;
	}
	return false;
}

bool UPlayerControlGroupManager::IsActorValidAndNotInSelection(ASelectableActorObjectsMaster* ActorMaster) const
{
	if (ActorMaster && !M_PlayerController->TSelectedActorsMasters.Contains(ActorMaster))
	{
		return true;
	}
	return false;
}
