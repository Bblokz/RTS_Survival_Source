// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.


#include "AGameUIController.h"

#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectedUnit/SelectedUnitWidgetState/SelectedUnitsWidgetState.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


class ASelectablePawnMaster;

namespace SelectionUIRebuilder
{
	/** @brief Convert a selected actor to a UnitID (TrainingOption). Adapt to your project as needed. */
	static bool TryBuildUnitIDFromActor(const AActor* InActor, FTrainingOption& OutUnitId)
	{
		if (not IsValid(InActor))
		{
			return false;
		}
		if (const URTSComponent* RTS = Cast<URTSComponent>(InActor->GetComponentByClass(URTSComponent::StaticClass())))
		{
			OutUnitId.UnitType = RTS->GetUnitType();
			OutUnitId.SubtypeValue = RTS->GetUnitSubType();
			return true;
		}
		return false;
	}

	struct FFlatEntry
	{
		AActor* Actor = nullptr;
		FTrainingOption UnitID;
		uint32 UniqueSubtypeId = 0;
		int32 SelectionArrayIndex = INDEX_NONE;
	};
}


AGameUIController::AGameUIController()
        : M_THierarchyGameUI(),
          M_ActiveGameUIArrayIndex(0),
          M_PlayerController(nullptr),
          M_currentGameUIState()
{
        const TArray<EAbilityID> EmptyAbilityIds = {
                EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
                EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
                EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility
        };
        M_AbilityArrayWithEmptyAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries(EmptyAbilityIds);
        M_currentGameUIState.PrimaryUnitAbilities = M_AbilityArrayWithEmptyAbilities;
}

void AGameUIController::ForEachPrimarySameType(const TFunctionRef<void(AActor&)>& Func)
{
	EnsurePrimarySelectedSameTypeCacheIsValid();
	for (const TWeakObjectPtr<AActor>& Weak : M_PrimarySameTypeCache)
	{
		if (AActor* Ptr = Weak.Get())
		{
			Func(*Ptr);
		}
	}
}

void AGameUIController::GetPrimarySameTypeActors(TArray<AActor*>& OutActors)
{
	EnsurePrimarySelectedSameTypeCacheIsValid();
	OutActors.Reset();
	for (const TWeakObjectPtr<AActor>& Weak : M_PrimarySameTypeCache)
	{
		if (AActor* Ptr = Weak.Get())
		{
			OutActors.Add(Ptr);
		}
	}
}

void AGameUIController::UpdatePrimarySameTypeCacheAndNotify_FromList(
	const TArray<AActor*>& PrimarySameTypeActors)
{
	EnsurePrimarySelectedSameTypeCacheIsValid();

	// Build a raw-pointer membership set for the incoming group.
	TSet<AActor*> NewRawSet;
	NewRawSet.Reserve(PrimarySameTypeActors.Num());
	for (AActor* const Each : PrimarySameTypeActors)
	{
		if (IsValid(Each))
		{
			NewRawSet.Add(Each);
		}
	}

	// Notify actors that LEFT the group (were cached but are not in NewRawSet).
	for (const TWeakObjectPtr<AActor>& Weak : M_PrimarySameTypeCache)
	{
		AActor* const Actor = Weak.Get();
		if (not Actor)
		{
			continue;
		}
		if (not NewRawSet.Contains(Actor))
		{
			if (USelectionComponent* SelectionComponent = Actor->FindComponentByClass<USelectionComponent>())
			{
				SelectionComponent->SetIsInPrimarySameType(false);
			}
		}
	}

	// Notify actors that ENTER (or remain) in the group – idempotent if already true.
	TSet<TWeakObjectPtr<AActor>> NewCache;
	NewCache.Reserve(NewRawSet.Num());
	for (AActor* const Each : NewRawSet)
	{
		if (USelectionComponent* SelectionComponent = Each->FindComponentByClass<USelectionComponent>())
		{
			SelectionComponent->SetIsInPrimarySameType(true);
		}
		NewCache.Add(TWeakObjectPtr<AActor>(Each));
	}

	// Commit new cache.
	M_PrimarySameTypeCache = MoveTemp(NewCache);
}

bool AGameUIController::GetIsPrimarySelectedEnabledTrainer() const
{
	if (not IsValid(M_currentGameUIState.PrimarySelectedUnit))
	{
		return false;
	}
	if (M_currentGameUIState.PrimarySelectedUnit->GetClass()->ImplementsInterface(UTrainer::StaticClass()))
	{
		if (ITrainer* TrainerInterface = Cast<ITrainer>(M_currentGameUIState.PrimarySelectedUnit))
		{
			if (UTrainerComponent* TrainerComponent = TrainerInterface->GetTrainerComponent())
			{
				return TrainerComponent->GetIsAbleToTrain();
			}
		}
	}
	return false;
}

void AGameUIController::RebuildGameUIHierarchy(
	TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors)
{
	// Reset hierarchy and selected unit subtype IDs
	M_THierarchyGameUI.Empty();
	M_SelectedUniqueSubTypeIDs.Empty();
	M_UniqueSubTypeIDToSelectionData.Empty();
	M_ActiveGameUIArrayIndex = 0;

	EnsureProvidedArraysAreValid(
		TPlayerSelectedPawnMasters,
		TPlayerSelectedSquadControllers,
		TPlayerSelectedActors
	);

	auto CollectUnitSubTypes = [&](auto* SelectedUnits)
	{
		for (auto* EachUnit : *SelectedUnits)
		{
			const URTSComponent* RTSComp = EachUnit->GetRTSComponent();
			if (!RTSComp)
			{
				continue;
			}

			const EAllUnitType UnitType = RTSComp->GetUnitType();
			const uint8 SubTypeValue = RTSComp->GetUnitSubType();
			const int32 SelectionPriority = RTSComp->GetSelectionPriority();

			uint32 UnitSubTypeID = GetUniqueUnitSubtypeID(UnitType, SubTypeValue);

			if (!M_SelectedUniqueSubTypeIDs.Contains(UnitSubTypeID))
			{
				FSelectionData SelectionData;
				SelectionData.SelectionPriority = SelectionPriority;
				SelectionData.UnitType = UnitType;
				SelectionData.SubTypeValue = SubTypeValue;

				M_SelectedUniqueSubTypeIDs.Add(UnitSubTypeID);
				M_UniqueSubTypeIDToSelectionData.Add(UnitSubTypeID, SelectionData);
			}
		}
	};

	// Collect unit subtypes from all selected units
	CollectUnitSubTypes(TPlayerSelectedPawnMasters);
	CollectUnitSubTypes(TPlayerSelectedSquadControllers);
	CollectUnitSubTypes(TPlayerSelectedActors);

	// Convert selection data map to an array for sorting
	TArray<FSelectionData> SelectionArray;
	M_UniqueSubTypeIDToSelectionData.GenerateValueArray(SelectionArray);

	// Sort the selection array based on SelectionPriority and UnitType
	// 2 level sorting, selection priority is strongest, otherwise rely on EAllUnitType.
	SelectionArray.Sort([](const FSelectionData& A, const FSelectionData& B)
	{
		if (A.SelectionPriority != B.SelectionPriority)
		{
			return A.SelectionPriority > B.SelectionPriority;
		}
		if (A.UnitType == B.UnitType)
		{
			return A.SubTypeValue < B.SubTypeValue;
		}
		return A.UnitType < B.UnitType;
	});

	// Build the hierarchy using sorted unit subtype IDs
	for (const FSelectionData& Data : SelectionArray)
	{
		uint32 UnitSubTypeID = GetUniqueUnitSubtypeID(Data.UnitType, Data.SubTypeValue);
		M_THierarchyGameUI.Add(UnitSubTypeID);
	}

	// Debugging output
	if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("UI HIERARCHY:", FColor::Red);
		RTSFunctionLibrary::PrintString("Active index: " + FString::FromInt(M_ActiveGameUIArrayIndex));
		for (int32 i = 0; i < M_THierarchyGameUI.Num(); ++i)
		{
			const uint32 UnitSubTypeID = M_THierarchyGameUI[i];
			const FSelectionData* DataPtr = M_UniqueSubTypeIDToSelectionData.Find(UnitSubTypeID);
			if (DataPtr)
			{
				const FSelectionData& Data = *DataPtr;
				FString UnitTypeName = Global_GetUnitTypeString(Data.UnitType);
				RTSFunctionLibrary::PrintString(
					"index: " + FString::FromInt(i) +
					" UnitType: " + UnitTypeName +
					" SubTypeValue: " + FString::FromInt(Data.SubTypeValue) +
					" SelectionPriority: " + FString::FromInt(Data.SelectionPriority));
			}
		}
	}

	// Update the UI state
	CalculatePropagateGameUIState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
	PushSelectionPanelState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
}

void AGameUIController::OnlyRebuildSelectionUI(TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
                                               TArray<ASquadController*>* TPlayerSelectedSquadControllers,
                                               TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors)
{
	EnsureProvidedArraysAreValid(
		TPlayerSelectedPawnMasters,
		TPlayerSelectedSquadControllers,
		TPlayerSelectedActors);
	PushSelectionPanelState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
}

void AGameUIController::TabThroughGameUIHierarchy(TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
                                                  TArray<ASquadController*>* TPlayerSelectedSquadControllers,
                                                  TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors)
{
	IncrementGameUI_Index();
	EnsureProvidedArraysAreValid(
		TPlayerSelectedPawnMasters,
		TPlayerSelectedSquadControllers,
		TPlayerSelectedActors);
	// Updates the UI state 
	CalculatePropagateGameUIState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
	PushSelectionPanelState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
}


bool AGameUIController::TryAdvancePrimaryToUnitType(
	EAllUnitType DesiredType,
	TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors, AActor* OverwritePrimaryActor)
{
	EnsureProvidedArraysAreValid(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);

	// No hierarchy => nothing to tab through.
	if (M_THierarchyGameUI.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("TryAdvancePrimaryToUnitType: hierarchy is empty (no selection)."));
		return false;
	}

	// Check the mapping to ensure the desired type is present somewhere.
	bool bDesiredExists = false;
	for (const auto& Pair : M_UniqueSubTypeIDToSelectionData)
	{
		if (Pair.Value.UnitType == DesiredType)
		{
			bDesiredExists = true;
			break;
		}
	}
	if (!bDesiredExists)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("TryAdvancePrimaryToUnitType: type %s not found in hierarchy."),
			                *Global_GetUnitTypeString(DesiredType)));
		return false;
	}

	bool bAdvanced = false;
	// If already the active type, instantly succeed.
	if (GetPrimarySelectedUnitType() == DesiredType)
	{
		bAdvanced = true;
	}

	// Advance (tab) until the primary type matches or we looped through the whole list.
	const int32 Limit = M_THierarchyGameUI.Num();
	if (not bAdvanced)
	{
		for (int32 i = 0; i < Limit; ++i)
		{
			IncrementGameUI_Index();
			if (GetPrimarySelectedUnitType() == DesiredType)
			{
				bAdvanced = true;
				break;
			}
		}
	}

	if (bAdvanced)
	{
		// Apply both: propagate action UI state and rebuild the selection panel.
		CalculatePropagateGameUIState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers,
		                              TPlayerSelectedActors, OverwritePrimaryActor);
		PushSelectionPanelState(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors,
		                        OverwritePrimaryActor);
		return true;
	}

	// Shouldn't happen (we verified presence), but keep a safety net.
	RTSFunctionLibrary::ReportError(TEXT("TryAdvancePrimaryToUnitType: failed to advance to requested type."));
	return false;
}


AActor* AGameUIController::GetPrimarySelectedUnit() const
{
	return M_currentGameUIState.PrimarySelectedUnit;
}

void AGameUIController::IncrementGameUI_Index()
{
	M_ActiveGameUIArrayIndex++;

	if (M_ActiveGameUIArrayIndex >= (M_THierarchyGameUI.Num()))
	{
		// Reset to select the first priority unitType again.
		M_ActiveGameUIArrayIndex = 0;
	}
}

void AGameUIController::CalculatePropagateGameUIState(
	TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors,
	AActor* OverWritePrimarySelectedUnit)
{
        FActionUIParameters ActionUIParameters;
        EAbilityID CurrentAbility = EAbilityID::IdIdle;
        TArray<FUnitAbilityEntry> UnitAbilities = M_AbilityArrayWithEmptyAbilities;

	// Raw subtype that drives per-type enum assignment inside GetGameUIParametersForType.
	int32 UnitSubtype = 0;

	// Start from the controller’s current primary unit type; may be adjusted by overwrite actor.
	EAllUnitType PrimarySelectedUnitType = GetPrimarySelectedUnitType();

	AActor* PrimaryUnit = nullptr;

        // Try to apply the explicit overwrite primary; fall back to legacy resolution if not applied.
        if (not TryApplyOverwritePrimarySelectedUnit(OverWritePrimarySelectedUnit, PrimarySelectedUnitType, UnitSubtype,
                                                     PrimaryUnit))
        {
                // Legacy path: pick the first unit of the active type and derive UnitSubtype there.
                PrimaryUnit = GetPrimarySelectedUnit(*TPlayerSelectedPawnMasters,
                                                     *TPlayerSelectedSquadControllers,
                                                     *TPlayerSelectedActors,
		                                     UnitSubtype);
	}

	// Query current ability from the primary unit if it supports ICommands.
        if (PrimaryUnit && PrimaryUnit->GetClass()->ImplementsInterface(UCommands::StaticClass()))
        {
                if (ICommands* CommandsInterface = Cast<ICommands>(PrimaryUnit))
                {
                        CurrentAbility = CommandsInterface->GetActiveCommandID();
                }
                UnitAbilities = GetPrimaryUnitAbilities(PrimaryUnit);
        }

        M_currentGameUIState.PrimaryUnitAbilities = UnitAbilities;

	if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Current Ability: " + Global_GetAbilityIDAsString(CurrentAbility) + " for " +
			Global_GetUnitTypeString(PrimarySelectedUnitType) + " is primary selected.");
	}

	// Fill parameters (this maps UnitSubtype -> per-type subtype enums and sets visibility flags).
	GetGameUIParametersForType(ActionUIParameters, PrimarySelectedUnitType, PrimaryUnit, CurrentAbility, UnitSubtype);

	// Push new state to the main game UI.
	M_MainGameUI->UpdateActionUIForNewUnit(ActionUIParameters, PrimaryUnit);
}


bool AGameUIController::TryApplyOverwritePrimarySelectedUnit(
	AActor* OverwritePrimarySelectedUnit,
	EAllUnitType& InOutPrimaryUnitType,
	int32& InOutRawSubtype,
	AActor*& OutPrimaryUnit) const
{
	if (!IsValid(OverwritePrimarySelectedUnit))
	{
		return false;
	}

	OutPrimaryUnit = OverwritePrimarySelectedUnit;

	// Pull subtype (and type) from RTS if available; otherwise keep existing values.
	if (const URTSComponent* RTS =
		Cast<URTSComponent>(OverwritePrimarySelectedUnit->GetComponentByClass(URTSComponent::StaticClass())))
	{
		InOutRawSubtype = RTS->GetUnitSubType();

		const EAllUnitType ActorType = RTS->GetUnitType();
		if (ActorType != InOutPrimaryUnitType)
		{
			InOutPrimaryUnitType = ActorType;
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			TEXT("TryApplyOverwritePrimarySelectedUnit: overwrite actor has no RTSComponent."));
	}

	return true;
}

void AGameUIController::GetGameUIParametersForType(
	FActionUIParameters& OutGameUIParameters,
	const EAllUnitType UnitType,
	AActor* PrimaryUnit,
	const EAbilityID CurrentAbility,
	const int32 RawSubtype)
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_Nomadic:
		GetActionUIParamsNomadic(OutGameUIParameters, PrimaryUnit, CurrentAbility);
		break;
	default:
		// Ignore building UI.
		OutGameUIParameters.NomadicButtonState = EActionUINomadicButton::EAUI_ShowConvertToBuilding;
		OutGameUIParameters.bShowBuildingUI = false;
	}
	// As of now all subtypes are set to none, determine the subtype and set it in the action ui parameters.
	SetUnitSubtypeInActionUIParameters(OutGameUIParameters, UnitType, RawSubtype);
	M_currentGameUIState.PrimarySelectedUnit = PrimaryUnit;
	M_currentGameUIState.PrimarySelectedUnitType = UnitType;
	M_currentGameUIState.ActionUIParameters = OutGameUIParameters;
	// Determine whether we should show the training UI.
	GetTrainingUIParametersForType(OutGameUIParameters, PrimaryUnit);
	SetUpWeaponUIForUnit(OutGameUIParameters, PrimaryUnit);
	SetUpActionUIForUnit(OutGameUIParameters, UnitType, PrimaryUnit);
}

void AGameUIController::GetActionUIParamsNomadic(
	FActionUIParameters& OutGameUIParameters,
	AActor* PrimaryUnit,
	EAbilityID CurrentAbility) const
{
	OutGameUIParameters.bShowBuildingUI = true;
	if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(PrimaryUnit))
	{
		OutGameUIParameters.NomadicButtonState = GetButtonStateFromTruck(NomadicVehicle, CurrentAbility);
		// Update the BuildingExpansionUI; note that this is a separate UI function that needs to be executed before
		// we update the whole action UI with Playercontroller:UpdateActionUI
		// as otherwise the bxp options are not initialised correctly. 
		M_PlayerController->GetMainMenuUI()->InitBuildingExpansionUIForNewUnit(NomadicVehicle);
	}
}

EActionUINomadicButton AGameUIController::GetButtonStateFromTruck(
	const ANomadicVehicle* NomadicVehicle,
	EAbilityID CurrentAbility) const
{
	EActionUINomadicButton NomadicButtonState = EActionUINomadicButton::EAUI_ShowConvertToBuilding;
	if (NomadicVehicle)
	{
		switch (NomadicVehicle->GetNomadicStatus())
		{
		case ENomadStatus::Truck:
			if (CurrentAbility == EAbilityID::IdCreateBuilding)
			{
				// the nomadic truck wants to place a building.
				NomadicButtonState = EActionUINomadicButton::EAUI_ShowCancelBuilding;
			}
			else
			{
				NomadicButtonState = EActionUINomadicButton::EAUI_ShowConvertToBuilding;
			}
			break;
		case ENomadStatus::CreatingBuildingRotating:
		case ENomadStatus::CreatingBuildingTruckAnim:
		case ENomadStatus::CreatingBuildingMeshAnim:
			NomadicButtonState = EActionUINomadicButton::EAUI_ShowCancelBuilding;
			break;
		case ENomadStatus::Building:
			NomadicButtonState = EActionUINomadicButton::EAUI_ShowConvertToVehicle;
			break;
		case ENomadStatus::CreatingTruck:
			NomadicButtonState = EActionUINomadicButton::EAUI_ShowCancelVehicleConversion;
			break;
		default:
			NomadicButtonState = EActionUINomadicButton::EAUI_ShowConvertToBuilding;
			break;
		}
	}
	return NomadicButtonState;
}

void AGameUIController::GetTrainingUIParametersForType(
	FActionUIParameters& OutGameUIParameters,
	AActor* PrimaryUnit) const
{
	if (!IsValid(M_PlayerController) || !IsValid(M_PlayerController->GetMainMenuUI()))
	{
		return;
	}
	bool bValidTrainerSelected = false;
	if (IsValid(PrimaryUnit) && PrimaryUnit->GetClass()->ImplementsInterface(UTrainer::StaticClass()))
	{
		if (ITrainer* TrainerInterface = Cast<ITrainer>(PrimaryUnit))
		{
			if (UTrainerComponent* TrainerComponent = TrainerInterface->GetTrainerComponent())
			{
				// do not Allow for Itrainers that do not have any training options.
				if (TrainerComponent->GetTrainingOptions().Num() > 0)
				{
					const bool bIsAbleToTrain = TrainerComponent->GetIsAbleToTrain();
					M_PlayerController->GetMainMenuUI()->SetupTrainingUIForNewTrainer(TrainerComponent, bIsAbleToTrain);
					OutGameUIParameters.bShowTrainingUI = bIsAbleToTrain;
					bValidTrainerSelected = true;
				}
			}
		}
	}
	if (not bValidTrainerSelected)
	{
		// Reset primary selected trainer.
		M_PlayerController->GetMainMenuUI()->SetupTrainingUIForNewTrainer(nullptr, false);
	}
}

void AGameUIController::SetUpWeaponUIForUnit(
	FActionUIParameters& OutGameUIParameters,
	AActor* PrimaryUnit)
{
	if (GetIsValidMainGameUI())
	{
		OutGameUIParameters.bShowWeaponUI = M_MainGameUI->SetupWeaponUIForSelected(PrimaryUnit);
	}
}

void AGameUIController::SetUpActionUIForUnit(
	FActionUIParameters& OutGameUIParameters,
	const EAllUnitType PrimaryUnitType,
	AActor* PrimaryUnit)
{
        if (GetIsValidMainGameUI())
        {
                const TArray<FUnitAbilityEntry> UnitAbilities = GetPrimarySelectedAbilityArray();
                OutGameUIParameters.bShowActionUIAbilities = M_MainGameUI->SetupUnitActionUIForUnit(
                        UnitAbilities, PrimaryUnitType, PrimaryUnit, OutGameUIParameters.NomadicSubtype,
                        OutGameUIParameters.TankSubtype, OutGameUIParameters.SquadSubtype, OutGameUIParameters.BxpSubtype);
        }
}

void AGameUIController::RefreshGameUI(
	EAllUnitType PrimarySelectedUnitType,
	AActor* PrimarySelectedActor,
	FActionUIParameters GameUIParameters)
{
	if (GetIsValidMainGameUI())
	{
		M_MainGameUI->UpdateActionUIForNewUnit(GameUIParameters, PrimarySelectedActor);
	}
}

void AGameUIController::SetUnitSubtypeInActionUIParameters(
	FActionUIParameters& GameUIParameters,
	const EAllUnitType UnitType,
	const int32 Subtype) const
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_Nomadic:
		GameUIParameters.NomadicSubtype = static_cast<ENomadicSubtype>(Subtype);
		break;
	case EAllUnitType::UNType_Tank:
		GameUIParameters.TankSubtype = static_cast<ETankSubtype>(Subtype);
		break;
	case EAllUnitType::UNType_Squad:
		GameUIParameters.SquadSubtype = static_cast<ESquadSubtype>(Subtype);
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		GameUIParameters.BxpSubtype = static_cast<EBuildingExpansionType>(Subtype);
		break;
	case EAllUnitType::UNType_Aircraft:
		GameUIParameters.AircraftSubType = static_cast<EAircraftSubtype>(Subtype);
		break;
	default:
		GameUIParameters.NomadicSubtype = ENomadicSubtype::Nomadic_None;
	}
}

bool AGameUIController::GetIsValidMainGameUI()
{
	if (IsValid(M_MainGameUI))
	{
		return true;
	}
	if (IsValid(M_PlayerController) && IsValid(M_PlayerController->GetMainMenuUI()))
	{
		M_MainGameUI = M_PlayerController->GetMainMenuUI();
		return true;
	}
	RTSFunctionLibrary::ReportError("Player controller or MainGameUI is null for AACtionUIController"
		"\n @function: GetISValidMainGameUI");
	return false;
}

const FSelectionData AGameUIController::GetActiveSelectionData() const
{
	if (M_THierarchyGameUI.IsEmpty())
	{
		// No error, no unit is selected.
		return FSelectionData();
	}
	if (M_THierarchyGameUI.IsValidIndex(M_ActiveGameUIArrayIndex))
	{
		uint32 ActiveUnitSubTypeID = M_THierarchyGameUI[M_ActiveGameUIArrayIndex];
		const FSelectionData* ActiveSelectionData = M_UniqueSubTypeIDToSelectionData.Find(ActiveUnitSubTypeID);

		if (ActiveSelectionData)
		{
			return *ActiveSelectionData;
		}
		RTSFunctionLibrary::ReportError("Active unit subtype ID not found in selection data.");
		// Has non-type on all enum fields.
		return FSelectionData();
	}
	RTSFunctionLibrary::ReportError("ActiveGameUIArrayIndex out of bounds in GetActiveSelectionData.");
	return FSelectionData();
}

TArray<FUnitAbilityEntry> AGameUIController::GetPrimaryUnitAbilities(
        AActor* PrimarySelectedUnit) const
{
        if (not IsValid(PrimarySelectedUnit))
        {
                return M_AbilityArrayWithEmptyAbilities;
        }
	ICommands* UnitCommands = Cast<ICommands>(PrimarySelectedUnit);
	if (not UnitCommands)
	{
		RTSFunctionLibrary::ReportError(
                        "Selected valid actor that cannot be casted to ICommands in GetPrimaryUnitAbilities in"
                        "GameUIController.cpp"
                        "\n Will set empty ability array for actor: " + PrimarySelectedUnit->GetName());
                return M_AbilityArrayWithEmptyAbilities;
        }
        return UnitCommands->GetUnitAbilityEntries();
}

void AGameUIController::EnsureProvidedArraysAreValid(
	TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors)
{
	// 1) Clean up the array of ASelectablePawnMaster pointers
	if (TPlayerSelectedPawnMasters)
	{
		TArray<ASelectablePawnMaster*> ValidPawnMasters;
		ValidPawnMasters.Reserve(TPlayerSelectedPawnMasters->Num());
		for (ASelectablePawnMaster* Each : *TPlayerSelectedPawnMasters)
		{
			if (IsValid(Each))
			{
				ValidPawnMasters.Add(Each);
			}
		}
		// Overwrite the original array with the now-filtered array
		*TPlayerSelectedPawnMasters = MoveTemp(ValidPawnMasters);
	}

	// 2) Clean up the array of ASquadController pointers
	if (TPlayerSelectedSquadControllers)
	{
		TArray<ASquadController*> ValidSquadControllers;
		ValidSquadControllers.Reserve(TPlayerSelectedSquadControllers->Num());
		for (ASquadController* Each : *TPlayerSelectedSquadControllers)
		{
			if (IsValid(Each))
			{
				ValidSquadControllers.Add(Each);
			}
		}
		*TPlayerSelectedSquadControllers = MoveTemp(ValidSquadControllers);
	}

	// 3) Clean up the array of ASelectableActorObjectsMaster pointers
	if (TPlayerSelectedActors)
	{
		TArray<ASelectableActorObjectsMaster*> ValidActors;
		ValidActors.Reserve(TPlayerSelectedActors->Num());
		for (ASelectableActorObjectsMaster* Each : *TPlayerSelectedActors)
		{
			if (IsValid(Each))
			{
				ValidActors.Add(Each);
			}
		}
		*TPlayerSelectedActors = MoveTemp(ValidActors);
	}
}

void AGameUIController::PushSelectionPanelState(
	TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors,
	AActor* OverWritePrimarySelectedUnit)
{
	if (DeveloperSettings::Debugging::GPlayerSelection_Compile_DebugSymbols)
	{
		Debug_PrintSelectedUnits(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);
	}
	// --- Debug print of incoming selection (names from TrainingOption) ---
	// ---------------------------------------------------------------------

	const bool bHasOverWritePrimary = IsValid(OverWritePrimarySelectedUnit);

	if (!GetIsValidMainGameUI())
	{
		return;
	}

	EnsureProvidedArraysAreValid(TPlayerSelectedPawnMasters, TPlayerSelectedSquadControllers, TPlayerSelectedActors);

	TArray<SelectionUIRebuilder::FFlatEntry> Flat;
	Flat.Reserve(
		(TPlayerSelectedPawnMasters ? TPlayerSelectedPawnMasters->Num() : 0) +
		(TPlayerSelectedSquadControllers ? TPlayerSelectedSquadControllers->Num() : 0) +
		(TPlayerSelectedActors ? TPlayerSelectedActors->Num() : 0));

	auto AppendFrom = [this, &Flat](auto* Arr)
	{
		if (!Arr) { return; }
		for (int32 i = 0; i < Arr->Num(); ++i)
		{
			AActor* Each = (*Arr)[i];
			if (!IsValid(Each))
			{
				continue;
			}
			FTrainingOption UnitID{};
			if (!SelectionUIRebuilder::TryBuildUnitIDFromActor(Each, UnitID))
			{
				continue;
			}
			const URTSComponent* RTS = Cast<URTSComponent>(Each->GetComponentByClass(URTSComponent::StaticClass()));
			if (!RTS)
			{
				continue;
			}
			SelectionUIRebuilder::FFlatEntry E;
			E.Actor = Each;
			E.UnitID = UnitID;
			E.SelectionArrayIndex = i; // index within its originating array – OK for controller verification
			E.UniqueSubtypeId = GetUniqueUnitSubtypeID(RTS->GetUnitType(), RTS->GetUnitSubType());
			Flat.Add(MoveTemp(E));
		}
	};

	AppendFrom(TPlayerSelectedPawnMasters);
	AppendFrom(TPlayerSelectedSquadControllers);
	AppendFrom(TPlayerSelectedActors);

	// Determine primary selection type (active in hierarchy) – default from current hierarchy.
	const FSelectionData ActiveSelData = GetActiveSelectionData();
	uint32 PrimaryBucketId = GetUniqueUnitSubtypeID(ActiveSelData.UnitType, ActiveSelData.SubTypeValue);

	// Order the flat entries by hierarchy bucket order (all of type A, then B, etc.)
	TMap<uint32, int32> HierarchyRank;
	HierarchyRank.Reserve(M_THierarchyGameUI.Num());
	for (int32 idx = 0; idx < M_THierarchyGameUI.Num(); ++idx)
	{
		HierarchyRank.Add(M_THierarchyGameUI[idx], idx);
	}

	Flat.Sort([&HierarchyRank](const SelectionUIRebuilder::FFlatEntry& L, const SelectionUIRebuilder::FFlatEntry& R)
	{
		const int32* RankL = HierarchyRank.Find(L.UniqueSubtypeId);
		const int32* RankR = HierarchyRank.Find(R.UniqueSubtypeId);
		const int32 RL = RankL ? *RankL : TNumericLimits<int32>::Max();
		const int32 RR = RankR ? *RankR : TNumericLimits<int32>::Max();
		if (RL != RR) { return RL < RR; }
		// Keep relative order within the same bucket.
		return false;
	});

	// Preferred primary: try to locate the exact flattened index for the overwrite actor.
	int32 PreferredPrimaryFlatIndex = INDEX_NONE;
	if (bHasOverWritePrimary)
	{
		for (int32 i = 0; i < Flat.Num(); ++i)
		{
			if (Flat[i].Actor == OverWritePrimarySelectedUnit)
			{
				PreferredPrimaryFlatIndex = i;
				PrimaryBucketId = Flat[i].UniqueSubtypeId; // drive bucket from the actual overwrite entry
				break;
			}
		}
		if (PreferredPrimaryFlatIndex == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError(TEXT(
				"PushSelectionPanelState: OverwritePrimarySelectedUnit was valid but not part of the current selection; falling back to first-of-type."));
		}
	}

	// Convert to widget states & mark primary tile
	TArray<FSelectedUnitsWidgetState> States;
	States.Reserve(Flat.Num());

	bool bPrimaryTileMarked = false;

	for (int32 i = 0; i < Flat.Num(); ++i)
	{
		const SelectionUIRebuilder::FFlatEntry& E = Flat[i];

		FSelectedUnitsWidgetState S;
		S.UnitID = E.UnitID;
		S.SelectionArrayIndex = E.SelectionArrayIndex;

		if (E.UniqueSubtypeId == PrimaryBucketId)
		{
			// If we have an explicit preferred index, use exactly that one.
			if (PreferredPrimaryFlatIndex != INDEX_NONE)
			{
				if (i == PreferredPrimaryFlatIndex)
				{
					S.WidgetType = ESelectedWidgetType::PrimarySelected;
					bPrimaryTileMarked = true;
				}
				else
				{
					S.WidgetType = ESelectedWidgetType::PrimarySameType;
				}
			}
			else
			{
				// No overwrite: first encountered in this bucket is primary.
				if (!bPrimaryTileMarked)
				{
					S.WidgetType = ESelectedWidgetType::PrimarySelected;
					bPrimaryTileMarked = true;
				}
				else
				{
					S.WidgetType = ESelectedWidgetType::PrimarySameType;
				}
			}
		}
		else
		{
			S.WidgetType = ESelectedWidgetType::NotPrimary;
		}

		States.Add(MoveTemp(S));
	}

	// Safety: if nothing marked (e.g. empty bucket), pick first of bucket.
	if (!bPrimaryTileMarked)
	{
		for (int32 i = 0; i < Flat.Num(); ++i)
		{
			if (Flat[i].UniqueSubtypeId == PrimaryBucketId)
			{
				States[i].WidgetType = ESelectedWidgetType::PrimarySelected;
				bPrimaryTileMarked = true;
				break;
			}
		}
	}

	constexpr int32 DefaultActivePage = 0;
	M_MainGameUI->RebuildSelectionUI(States, DefaultActivePage);
	// Collect all actors of the active bucket (PrimarySelected + PrimarySameType).
	TArray<AActor*> PrimarySameTypeActors;
	PrimarySameTypeActors.Reserve(Flat.Num());
	for (const SelectionUIRebuilder::FFlatEntry& E : Flat)
	{
		if (E.UniqueSubtypeId == PrimaryBucketId && IsValid(E.Actor))
		{
			PrimarySameTypeActors.Add(E.Actor);
		}
	}

	// Cache + notify in one place (idempotent).
	UpdatePrimarySameTypeCacheAndNotify_FromList(PrimarySameTypeActors);
}


void AGameUIController::Debug_PrintSelectedUnits(
	const TArray<ASelectablePawnMaster*>* TPlayerSelectedPawnMasters,
	const TArray<ASquadController*>* TPlayerSelectedSquadControllers,
	const TArray<ASelectableActorObjectsMaster*>* TPlayerSelectedActors) const
{
	FString Debug;
	Debug += TEXT("AGameUIController::PushSelectionPanelState -- Incoming Selection\n");

	int32 TotalCount = 0;

	// Pawns
	Debug += TEXT("Pawns:\n");
	if (TPlayerSelectedPawnMasters)
	{
		for (int32 i = 0; i < TPlayerSelectedPawnMasters->Num(); ++i)
		{
			const ASelectablePawnMaster* Each = (*TPlayerSelectedPawnMasters)[i];
			FString Name;
			if (Debug_TryGetDisplayNameFromActor(Each, Name))
			{
				Debug += FString::Printf(TEXT("  [%d] %s\n"), i, *Name);
				++TotalCount;
			}
		}
	}

	// Squads
	Debug += TEXT("Squads:\n");
	if (TPlayerSelectedSquadControllers)
	{
		for (int32 i = 0; i < TPlayerSelectedSquadControllers->Num(); ++i)
		{
			const ASquadController* Each = (*TPlayerSelectedSquadControllers)[i];
			FString Name;
			if (Debug_TryGetDisplayNameFromActor(Each, Name))
			{
				Debug += FString::Printf(TEXT("  [%d] %s\n"), i, *Name);
				++TotalCount;
			}
		}
	}

	// Actors
	Debug += TEXT("Actors:\n");
	if (TPlayerSelectedActors)
	{
		for (int32 i = 0; i < TPlayerSelectedActors->Num(); ++i)
		{
			const ASelectableActorObjectsMaster* Each = (*TPlayerSelectedActors)[i];
			FString Name;
			if (Debug_TryGetDisplayNameFromActor(Each, Name))
			{
				Debug += FString::Printf(TEXT("  [%d] %s\n"), i, *Name);
				++TotalCount;
			}
		}
	}

	Debug += FString::Printf(TEXT("Total Selected: %d\n"), TotalCount);

	// Print once, nicely formatted with newlines.
	RTSFunctionLibrary::PrintString(Debug, FColor::White, 10);
}

bool AGameUIController::Debug_TryGetDisplayNameFromActor(const AActor* InActor, FString& OutDisplayName) const
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

	const FTrainingOption Opt = RTS->GetUnitTrainingOption();
	OutDisplayName = Opt.GetDisplayName();
	return true;
}

void AGameUIController::EnsurePrimarySelectedSameTypeCacheIsValid()
{
	if (M_PrimarySameTypeCache.Num() == 0)
	{
		return;
	}

	for (auto It = M_PrimarySameTypeCache.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}


EAllUnitType AGameUIController::GetPrimarySelectedUnitType() const
{
	const FSelectionData ActiveSelectionData = GetActiveSelectionData();
	return ActiveSelectionData.UnitType;
}


AActor* AGameUIController::GetPrimarySelectedUnit(
	TArray<ASelectablePawnMaster*>& TValidPlayerSelectedPawnMasters,
	TArray<ASquadController*>& TValidPlayerSelectedSquadControllers,
	TArray<ASelectableActorObjectsMaster*>& TValidPlayerSelectedActors,
	int32& OutUnitSubtype)
{
	const FSelectionData ActiveSelectionData = GetActiveSelectionData();

	uint32 ActiveUnitSubTypeID = GetUniqueUnitSubtypeID(ActiveSelectionData.UnitType, ActiveSelectionData.SubTypeValue);


	auto GetSelected = [ActiveUnitSubTypeID, this, &OutUnitSubtype](auto& SelectedUnits) -> AActor* {
		for (auto* EachUnit : SelectedUnits)
		{
			URTSComponent* RTSComp = EachUnit->GetRTSComponent();
			if (RTSComp)
			{
				uint32 UnitSubTypeID = GetUniqueUnitSubtypeID(RTSComp->GetUnitType(), RTSComp->GetUnitSubType());
				if (UnitSubTypeID == ActiveUnitSubTypeID)
				{
					OutUnitSubtype = RTSComp->GetUnitSubType();
					return EachUnit;
				}
			}
		}
		return nullptr;
	};

	if (AActor* SelectedActor = GetSelected(TValidPlayerSelectedPawnMasters))
	{
		return SelectedActor;
	}

	if (AActor* SelectedActor = GetSelected(TValidPlayerSelectedSquadControllers))
	{
		return SelectedActor;
	}

	if (AActor* SelectedActor = GetSelected(TValidPlayerSelectedActors))
	{
		return SelectedActor;
	}

	return nullptr;
}

TArray<FUnitAbilityEntry> AGameUIController::GetPrimarySelectedAbilityArray()
{
        if (M_currentGameUIState.PrimaryUnitAbilities.IsEmpty())
        {
                return GetPrimaryUnitAbilities(M_currentGameUIState.PrimarySelectedUnit);
        }
        return M_currentGameUIState.PrimaryUnitAbilities;
}

FUnitAbilityEntry AGameUIController::GetActiveAbilityEntry(const int ButtonIndex)
{
        const TArray<FUnitAbilityEntry> Abilities = GetPrimarySelectedAbilityArray();
        if (Abilities.IsValidIndex(ButtonIndex))
        {
                return Abilities[ButtonIndex];
        }
	const FString PrimaryUnitName = M_currentGameUIState.PrimarySelectedUnit
		                                ? M_currentGameUIState.PrimarySelectedUnit->GetName()
		                                : "No Primary Unit";
	RTSFunctionLibrary::ReportError("Attempted to get ActiveAbility with invlid index."
		"\n see AActionUIController::GetActiveAbility"
		"\n Primary Unit: " + PrimaryUnitName);
	return FUnitAbilityEntry();
}

bool AGameUIController::GetIsUnitTypeSelected(const EAllUnitType UnitTypeToCheck,
                                              const uint8 UnitSubtypeToCheck) const
{
	const uint32 UniqueId = GetUniqueUnitSubtypeID(UnitTypeToCheck, UnitSubtypeToCheck);
	return M_SelectedUniqueSubTypeIDs.Contains(UniqueId);
}
