// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrainingMenuManager.h"

#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/BottomRightUIPanelState/BottomRightUIPanelState.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TrainerComponent/TrainerComponent.h"
#include "TrainingUIWidget/W_TrainingItem.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/Requirement/RTSRequirementHelpers/FRTSRequirementHelpers.h"
#include "RTS_Survival/Requirement/DoubleRequirement/DoubleRequirement.h"
#include "RTS_Survival/Requirement/OrRequirement/OrDoubleRequirement.h"
#include "RTS_Survival/Requirement/TechRequirement/TechRequirement.h"
#include "RTS_Survival/Requirement/UnitRequirement/UnitRequirement.h"
#include "RTS_Survival/Requirement/VacantAirPadRequirement/VacantAirPadRequirement.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "TrainingDescription/W_TrainingDescription.h"
#include "TrainingRequirementWidget/W_TrainingRequirement.h"
#include "TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"

class UTrainerComponent;

FTrainingWidgetState UTrainingMenuManager::M_EmptyTrainingOptionState = FTrainingWidgetState();

void UTrainingMenuManager::SetupTrainingOptionsForTrainer(
	UTrainerComponent* Trainer,
	const bool bIsAbleToTrain)
{
	if (GetIsValidPrimarySelectedTrainer())
	{
		// Remove previous primary selected trainer.
		M_PrimarySelectedTrainer->SetIsPrimarySelectedTraining(false, this);
	}
	if (IsValid(Trainer))
	{
		M_PrimarySelectedTrainer = Trainer;
		Trainer->SetIsPrimarySelectedTraining(true, this);
		// Check if this unit can already train, if not we only set the reference so it can update the UI
		// when it is able to train.
		if (bIsAbleToTrain)
		{
			// Update the UI with the primary selected trainer's queue.
			UpdateWidgetsWithPrimaryQueue();
		}
	}
}

void UTrainingMenuManager::RequestRefreshTrainingUI(
	const UTrainerComponent* const RequestingTrainer)
{
	if (not GetRequestIsPrimary(RequestingTrainer))
	{
		return;
	}
	// Reload Training UI.
	UpdateWidgetsWithPrimaryQueue();
}

void UTrainingMenuManager::UpdateUIClockAtInsufficientResources(const bool bAtInsufficient, const int32 ActiveItemIndex,
                                                                const int32 TimeRemaining,
                                                                const FTrainingOption& ActiveItemOption,
                                                                UTrainerComponent* RequestingTrainer)
{
	if (not GetRequestIsPrimary(RequestingTrainer))
	{
		return;
	}
	const bool bPauseClock = bAtInsufficient;
	if constexpr (DeveloperSettings::Debugging::GTrainingComponent_Compile_DebugSymbols)
	{
		const FString StrTime = "TimeRemaining: " + FString::Printf(TEXT("%d"), TimeRemaining);
		const FString StrPause = "Clock Pause status: " + FString::Printf(
			TEXT("%s"), bPauseClock ? TEXT("true") : TEXT("false"));
		const FColor Color = bPauseClock ? FColor::Red : FColor::Green;
		RTSFunctionLibrary::PrintString("Clock UI Update:"
		                                "\n" + StrTime +
		                                "\n Color" + StrPause,
		                                Color);
	}
	StartClockOnTrainingItem(
		ActiveItemIndex,
		TimeRemaining,
		ActiveItemOption,
		bPauseClock);
}


void UTrainingMenuManager::RequestEnableTrainingUI(
	const UTrainerComponent* RequestingTrainer,
	const bool bSetEnabled)
{
	if (not GetRequestIsPrimary(RequestingTrainer) || not GetIsValidMainGameUI())
	{
		return;
	}
	if (bSetEnabled)
	{
		M_MainGameUI->SetTrainingUIVisibility(true);
		// Reload Training UI.
		UpdateWidgetsWithPrimaryQueue();
	}
	else
	{
		M_MainGameUI->SetTrainingUIVisibility(false);
		// Stop the clock on the active item and reset it.
		ResetActiveItem();
	}
}

FTrainingOptionState UTrainingMenuManager::GetTrainingOptionState(const FTrainingOption& TrainingOption) const
{
	if (const FTrainingOptionState* Found = M_TrainingOptionsMap.Find(TrainingOption))
	{
		return *Found;
	}
	const FString ToName = FRTS_Statics::Global_GetTrainingOptionString(TrainingOption);
	RTSFunctionLibrary::ReportError("Training option not found in map."
		"\n Option: " + ToName +
		"\n see UTrainingMenuManager::GetTrainingOptionState.");
	// not found → return a fresh default
	return FTrainingOptionState();
}

void UTrainingMenuManager::SetTrainingTimeForEachOption(const float NewTime)
{
	for (auto& TrainingOption : M_TrainingOptionsMap)
	{
		TrainingOption.Value.TrainingTime = NewTime;
	}
}

void UTrainingMenuManager::HideTrainingRequirement() const
{
	if (not EnsureIsValidTrainingRequirementWidget())
	{
		return;
	}
	M_TrainingRequirementWidget->SetVisibility(ESlateVisibility::Hidden);
}

bool UTrainingMenuManager::GetIsValidPrimarySelectedTrainer() const
{
	if (IsValid(M_PrimarySelectedTrainer))
	{
		return true;
	}


	// No error report as we may not have selected anyone before.
	return false;
}

void UTrainingMenuManager::PauseQueueOrRemove(const FTrainingWidgetState& TrainingItem) const
{
	if (M_PrimarySelectedTrainer->GetIsPaused())
	{
		RTSFunctionLibrary::PrintString("right click on widget is paused: delete!");
		RemoveLastOptionFromQueue(TrainingItem.ItemID);
	}
	else if (M_ActiveTrainingItem)
	{
		SetTrainingPause(true);
	}
}

void UTrainingMenuManager::InitTrainingManager(
	TArray<UW_TrainingItem*> TrainingItemsInMenu,
	UMainGameUI* MainGameUI,
	UW_TrainingRequirement* TrainingRequirementWidget, UW_TrainingDescription* TrainingDescription)
{
	InitAllGameTrainingOptions();
	DeveloperSettings::GamePlay::Training::MaxTrainingOptions = TrainingItemsInMenu.Num();
	for (int i = 0; i < TrainingItemsInMenu.Num(); i++)
	{
		if (!IsValid(TrainingItemsInMenu[i]))
		{
			RTSFunctionLibrary::ReportError("Training item in menu is nullptr or not valid."
				"at function UTrainingMenuManager::InitTrainingManager.");
			continue;
		}
		// Initialize the training item.
		TrainingItemsInMenu[i]->InitW_TrainingItem(this, i);
		M_TTrainingMenuWidgets.Add(TrainingItemsInMenu[i]);
	}
	if (IsValid(MainGameUI))
	{
		M_MainGameUI = MainGameUI;
		Init_SetupPlayerResourceManager(MainGameUI, TrainingDescription);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "MainGameUI", "InitTrainingMasnager");
	}
	if (IsValid(TrainingRequirementWidget))
	{
		M_TrainingRequirementWidget = TrainingRequirementWidget;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "TrainingInfoCanvas", "InitTrainingMasnager");
	}
}

void UTrainingMenuManager::OnTrainingItemClicked(
	const FTrainingWidgetState& TrainingItem,
	const int32 Index,
	const bool bIsLeftClick)
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return;
	}
	//  Ensure fresh requirement/UI state
	M_PrimarySelectedTrainer->CheckRequirementsAndRefreshUI();

	if (not GetWidgetIndexInBounds(Index))
	{
		return;
	}
	if (bIsLeftClick)
	{
		OnLeftClickTrainingItem(TrainingItem, Index);
	}
	else
	{
		PauseQueueOrRemove(TrainingItem);
	}
}

void UTrainingMenuManager::OnTrainingItemHovered(const FTrainingWidgetState& TrainingWidgetState, const bool bIsHovered)
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}
	if (GetIsValidPrimarySelectedTrainer())
	{
		M_PrimarySelectedTrainer->CheckRequirementsAndRefreshUI();
	}
	if (bIsHovered)
	{
		const bool bIsLockedByRequirement =
			TrainingWidgetState.TrainingStatus != ETrainingItemStatus::TS_Unlocked ||
			TrainingWidgetState.TrainingStatusSecondRequirement != ETrainingItemStatus::TS_Unlocked;

		if (bIsLockedByRequirement)
		{
			UpdateRequirementWidget(TrainingWidgetState);
		}
		SetRequirementPanelVisiblity(bIsLockedByRequirement);

		M_MainGameUI->SetBottomUIPanel(EShowBottomRightUIPanel::Show_TrainingDescription,
		                               EBuildingExpansionType::BXT_Invalid, false,
		                               TrainingWidgetState);
	}
	else
	{
		M_MainGameUI->SetBottomUIPanel(EShowBottomRightUIPanel::Show_ActionUI);
		SetRequirementPanelVisiblity(false);
	}
}

void UTrainingMenuManager::StartClockOnTrainingItem(
	const int32 IndexActiveTrainingItem,
	const int32 TimeRemaining,
	const FTrainingOption ActiveOption,
	const bool bIsPaused)
{
	if (not GetWidgetIndexInBounds(IndexActiveTrainingItem))
	{
		return;
	}
	if (M_ActiveTrainingItem)
	{
		// Stop clock on previous item
		ResetActiveItem();
	}
	M_ActiveTrainingItem = M_TTrainingMenuWidgets[IndexActiveTrainingItem];
	if (M_ActiveTrainingItem)
	{
		// Get the total training time for the active item
		if (M_TrainingOptionsMap.Contains(ActiveOption) == false)
		{
			RTSFunctionLibrary::ReportError("Training option not found in map."
				"\n see UTrainingMenuManager::StartClockOnTrainingItem."
				"\n ActiveOption: " + ActiveOption.GetTrainingName());
			return;
		}
		const int32 TotalTrainingTime = M_TrainingOptionsMap[ActiveOption].TrainingTime;

		M_ActiveTrainingItem->StartClock(TimeRemaining, TotalTrainingTime, bIsPaused);
	}
	else
	{
		ReportInvalidActiveItemForQueueClock(IndexActiveTrainingItem);
	}
}


void UTrainingMenuManager::InitAllGameTrainingOptions()
{
	// Default training option.
	FTrainingOptionState NewTrainingOptionState;
	NewTrainingOptionState.ItemID = FTrainingOption();
	NewTrainingOptionState.TrainingTime = 1.0f;
	M_TrainingOptionsMap.Add(FTrainingOption(), NewTrainingOptionState);

	InitAllGameTankTrainingOptions();
	InitAllGameNomadicTrainingOptions();
	InitAllGameSquadTrainingOptions();
	InitAllGameAircraftTrainingOptions();
}

FTrainingOptionState UTrainingMenuManager::CreateTrainingOptionState(
	const FTrainingOption ItemID,
	int32 TrainingTime,
	EAllUnitType UnitType,
	ETankSubtype TankSubtype,
	ENomadicSubtype NomadicSubtype,
	ESquadSubtype SquadSubtype,
	EAircraftSubtype AircraftSubtype, URTSRequirement* RTSRequirement)
{
	FTrainingOptionState State;
	State.ItemID = ItemID;
	State.TrainingTime = TrainingTime;
	State.UnitType = UnitType;
	State.TankSubtype = TankSubtype;
	State.NomadicSubtype = NomadicSubtype;
	State.SquadSubtype = SquadSubtype;
	State.RTSRequirement = RTSRequirement;
	return State;
}


void UTrainingMenuManager::InitAllGameTankTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;

	// GER TANKS
	// ----------------------

	// GER LIGHT TANKS

	// Ger Harvester PZ1
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			HarvesterTrainingTime * 10,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzI_Harvester,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None
			// FRTS_RequirementHelpers::CreateDouble_Unit_Unit(
			// 	GetTransientPackage(),
			// 	// Mission Center
			// 	FTrainingOption(EAllUnitType::UNType_Nomadic,
			// 	                static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter)),
			// 	FTrainingOption(EAllUnitType::UNType_Nomadic,
			// 	                static_cast<uint8>(ENomadicSubtype::Nomadic_GerGammaFacility))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger PzJager
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzJager)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzJager,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			{}, FRTS_RequirementHelpers::CreateTechRequirement(GetTransientPackage(), ETechnology::Tech_PzJager)
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz I Harvester
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Harvester,
			static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Harvester,
			ETankSubtype::Tank_PzI_Harvester,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz I Scout
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzI_Scout)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzI_Scout,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz I 15 cm
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzI_15cm)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzI_15cm,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz II F
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzII_F)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzII_F,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz 38t
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Pz38t)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Pz38t,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SdKfz 140
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz_140)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz_140,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// GER MEDIUM TANKS

	// Ger Pz III J
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIII_J)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIII_J,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz III J Commander
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIII_J_Commander)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIII_J_Commander,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz III M
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIII_M)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime + 10 * GameTimeMlt,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIII_M,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz IV F1 
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIV_F1)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime - 20 * GameTimeMlt,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIV_F1,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz IV F1 Commander
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIV_F1_Commander)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIV_F1_Commander,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
	// Ger Pz IV G
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIV_G)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIV_G,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz IV H
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIV_H)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIV_H,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger Jaguar
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Jaguar)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime + 10 * GameTimeMlt,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Jaguar,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Stug
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Stug)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Stug,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Marder
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Marder)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Marder,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz IV 70
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzIV_70)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzIV_70,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Brumbar
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Brumbar)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Brumbar,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Hetzer
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Hetzer)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Hetzer,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			{}, FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(),
			                                                   FTrainingOption(EAllUnitType::UNType_Nomadic,
			                                                                   static_cast<uint8>(
				                                                                   ENomadicSubtype::Nomadic_GerRefinery)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// GER HEAVY TANKS

	// Ger Tiger
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Tiger)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Tiger,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Tiger H1
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_TigerH1)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_TigerH1,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger King Tiger
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KingTiger)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KingTiger,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Tiger 105
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Tiger105)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Tiger105,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger Panther D
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PantherD)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PantherD,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Panther G
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PantherG)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PantherG,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger Panzer V_III
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PanzerV_III)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3SpecialMediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PanzerV_III,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Panzer V_IV
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PanzerV_IV)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3SpecialMediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PanzerV_IV,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Panther II
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PantherII)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PantherII,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Kugelblitz
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KeugelT38)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KeugelT38,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger E25
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_E25)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_E25,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger JagdTiger
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_JagdTiger)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_JagdTiger,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger JagdPanther
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_JagdPanther)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3MediumTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_JagdPanther,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SturmTiger
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SturmTiger)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SturmTiger,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Maus
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Maus)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			2 * T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Maus,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger E100 
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_E100)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			2 * T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_E100,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// RUS TANKS
	// ----------------------

	// RUS LIGHT TANKS

	// Rus BT-7
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_BT7)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_BT7,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-26
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T26)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T26,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus BT-7-4
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_BT7_4)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_BT7_4,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-70
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T70)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T70,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// RUS MEDIUM TANKS

	// Rus T-34/76
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T34_76)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T34_76,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-34/85
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T34_85)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T34_85,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-34/100
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T34_100)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T34_100,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// RUS HEAVY TANKS

	// Rus T35
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T35)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T35,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus KV-1
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KV_1)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KV_1,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus KV-1E
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KV_1E)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KV_1E,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-28
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T28)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T28,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus IS-1
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_IS_1)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_IS_1,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus KV-IS
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KV_IS)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KV_IS,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus IS-2
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_IS_2)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_IS_2,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus IS-3
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_IS_3)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			2 * T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_IS_3,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus KV-5
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KV_5)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			2 * T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KV_5,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
}

void UTrainingMenuManager::InitAllGameAircraftTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	// Train JU 87
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(EAircraftSubtype::Aircraft_Ju87)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicAircaftTrainingTime,
			EAllUnitType::UNType_Aircraft,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircraft_Ju87,
			FRTS_RequirementHelpers::CreateVacantAirPadRequirement(GetTransientPackage())
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
	// Train ME410 
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(EAircraftSubtype::Aircraft_Me410)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicAircaftTrainingTime,
			EAllUnitType::UNType_Aircraft,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircraft_Me410,
			FRTS_RequirementHelpers::CreateVacantAirPadRequirement(GetTransientPackage())
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
	// Train bf109 
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(EAircraftSubtype::Aircraft_Bf109)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicAircaftTrainingTime,
			EAllUnitType::UNType_Aircraft,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircraft_Bf109,
			FRTS_RequirementHelpers::CreateVacantAirPadRequirement(GetTransientPackage())
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
}

void UTrainingMenuManager::InitAllGameNomadicTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;


	// -------------------------------------------------------------------------------------------------------
	// -------------------------------------- ECO BUILDINGS
	// -------------------------------------------------------------------------------------------------------

	// Train Refinery Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerRefinery)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1RefineryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerRefinery,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train Metal Depot Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerMetalVault)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1RefineryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerMetalVault,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train Power Plant Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerGammaFacility)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1PowerPlantTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerGammaFacility,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// -------------------------------------------------------------------------------------------------------
	// // -------------------------------------- T1 BUILDINGS
	// -------------------------------------------------------------------------------------------------------

	// Train Light Steel Forge Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerLightSteelForge)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1VehicleFactoryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerLightSteelForge,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateDouble_Unit_Unit(GetTransientPackage(),
			                                                FTrainingOption(
				                                                EAllUnitType::UNType_Nomadic,
				                                                static_cast<uint8>(
					                                                ENomadicSubtype::Nomadic_GerGammaFacility)),
			                                                FTrainingOption(
				                                                EAllUnitType::UNType_Nomadic,
				                                                static_cast<uint8>(
					                                                ENomadicSubtype::Nomadic_GerBarracks)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train Mechanized Depot Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerMechanizedDepot)
		);
		const FTrainingOption RequiredGammaFacility = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerGammaFacility)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1MechanizedDepotTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerMechanizedDepot,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(),
			                                               RequiredGammaFacility)
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train Barracks Truck
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerBarracks)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT1BarracksTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerBarracks,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// -------------------------------------------------------------------------------------------------------
	// -------------------------------------- T2 BUILDINGS
	// -------------------------------------------------------------------------------------------------------

	// Train Communication Center
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT2CommunicationCenterTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerCommunicationCenter,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateOr_Unit_Unit(GetTransientPackage(),
			                                            FTrainingOption(
				                                            EAllUnitType::UNType_Nomadic,
				                                            static_cast<uint8>(
					                                            ENomadicSubtype::Nomadic_GerMechanizedDepot)),
			                                            FTrainingOption(
				                                            EAllUnitType::UNType_Nomadic,
				                                            static_cast<uint8>(
					                                            ENomadicSubtype::Nomadic_GerLightSteelForge)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train T2 Medium Tank Factory
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerMedTankFactory)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT2MedFactoryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerMedTankFactory,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateOr_Unit_Unit(GetTransientPackage(),
			                                            FTrainingOption(
				                                            EAllUnitType::UNType_Nomadic,
				                                            static_cast<uint8>(
					                                            ENomadicSubtype::Nomadic_GerMechanizedDepot)),
			                                            FTrainingOption(
				                                            EAllUnitType::UNType_Nomadic,
				                                            static_cast<uint8>(
					                                            ENomadicSubtype::Nomadic_GerLightSteelForge)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Train T2 Airbase
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerAirbase)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT2MedFactoryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerAirbase,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(),
			                                               FTrainingOption(EAllUnitType::UNType_Nomadic,
			                                                               static_cast<uint8>(
				                                                               ENomadicSubtype::Nomadic_GerCommunicationCenter)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
}


void UTrainingMenuManager::InitAllGameSquadTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;

	// Ger Scavengers
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicInfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_Scavengers
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger JagerTruppKar98k
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_JagerTruppKar98k)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicInfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_JagerTruppKar98k
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SteelFistAssaultSquad
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_SteelFistAssaultSquad)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			ArmoredInfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_SteelFistAssaultSquad
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger female sniper
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_Gebirgsjagerin)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			ArmoredInfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_Gebirgsjagerin
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Armory unlocked units.

	// Ger Armory Vulture assasin squad
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_Vultures)
		);
		const FTrainingOption RequiredUnit_CommCenter(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

		// Create single unit requirement.
		URTSRequirement* VulturesRequirement =
			FRTS_RequirementHelpers::CreateUnitRequirement(
				GetTransientPackage(),
				RequiredUnit_CommCenter);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2EngineersTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_Vultures,
			EAircraftSubtype::Aircarft_None,
			VulturesRequirement
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger Armory unlocked units.

	// Ger Armory Vulture sniper squad
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_SniperTeam)
		);
		const FTrainingOption RequiredUnit_CommCenter(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

		// Required: BXP Fuel Cell + Ger Communication Center
		const FTrainingOption RequiredExpansion_FuelCell(
			EAllUnitType::UNType_BuildingExpansion,
			static_cast<uint8>(EBuildingExpansionType::BTX_GerBarrackFuelCell));


		URTSRequirement* const VulturesRequirement =
			FRTS_RequirementHelpers::CreateDouble_Unit_Expansion(
				GetTransientPackage(),
				RequiredUnit_CommCenter,
				RequiredExpansion_FuelCell);


		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2EngineersTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_SniperTeam,
			EAircraftSubtype::Aircarft_None,
			VulturesRequirement
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
	// Ger T2 FeuerStrom Squad
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_FeuerSturm)
		);

		// Required: BXP Fuel Cell + Ger Communication Center
		const FTrainingOption RequiredExpansion_FuelCell(
			EAllUnitType::UNType_BuildingExpansion,
			static_cast<uint8>(EBuildingExpansionType::BTX_GerBarrackFuelCell));

		const FTrainingOption RequiredUnit_CommCenter(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

		URTSRequirement* const FeuerSturmRequirement =
			FRTS_RequirementHelpers::CreateDouble_Unit_Expansion(
				GetTransientPackage(),
				RequiredUnit_CommCenter,
				RequiredExpansion_FuelCell);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2InfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_FeuerSturm,
			EAircraftSubtype::Aircarft_None,
			FeuerSturmRequirement
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
	// Ger T2 SturmPioneer squad
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_SturmPionieren)
		);

		const FTrainingOption RequiredUnit_CommCenter(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

		// Create single unit requirement.
		URTSRequirement* SturmPioneerRequirement =
			FRTS_RequirementHelpers::CreateUnitRequirement(
				GetTransientPackage(),
				RequiredUnit_CommCenter);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2EngineersTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_SturmPionieren,
			EAircraftSubtype::Aircarft_None,
			SturmPioneerRequirement
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger T2 Panzergrenadiers
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_PanzerGrenadiere));
		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2InfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_PanzerGrenadiere,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), FTrainingOption(
				                                               EAllUnitType::UNType_Nomadic,
				                                               static_cast<uint8>(
					                                               ENomadicSubtype::Nomadic_GerCommunicationCenter)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger T2 sturmkommandos 
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_SturmKommandos));
		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2InfantryTrainingTime + 5,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_SturmKommandos,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), FTrainingOption(
				                                               EAllUnitType::UNType_Nomadic,
				                                               static_cast<uint8>(
					                                               ENomadicSubtype::Nomadic_GerMedTankFactory)))
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger T2 LightBringers; t2 at infantry.
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(ESquadSubtype::Squad_Ger_LightBringers));

		// Required: BXP Radix Reactor + Ger Communication Center
		const FTrainingOption RequiredExpansion_RadixReactor(
			EAllUnitType::UNType_BuildingExpansion,
			static_cast<uint8>(EBuildingExpansionType::BTX_GerBarracksRadixReactor));

		const FTrainingOption RequiredUnit_CommCenter(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

		URTSRequirement* const LightBringersRequirement =
			FRTS_RequirementHelpers::CreateDouble_Unit_Expansion(
				GetTransientPackage(),
				RequiredUnit_CommCenter,
				RequiredExpansion_RadixReactor);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2InfantryTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_Ger_LightBringers,
			EAircraftSubtype::Aircarft_None,
			LightBringersRequirement
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
}

void UTrainingMenuManager::PostInitTrainingOptions()
{
	for (auto& Option : M_TrainingOptionsMap)
	{
		if (Option.Value.TrainingTime < DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading)
		{
			RTSFunctionLibrary::ReportError("Training option has shorter training time than async loading time."
				"\n see UTrainingMenuManager::PostInitTrainingOptions."
				"\n Training time: " + FString::SanitizeFloat(Option.Value.TrainingTime) +
				"\n Async loading time: " + FString::SanitizeFloat(
					DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading) +
				"\n Option name: " + Option.Key.GetTrainingName());

			Option.Value.TrainingTime = DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading + 1;
		}
	}
}

void UTrainingMenuManager::OnLeftClickTrainingItem(const FTrainingWidgetState& TrainingItem, const int32 Index)
{
	const bool bIsQueueEmpty = M_PrimarySelectedTrainer->GetIsQueueEmpty();
	if (M_PrimarySelectedTrainer->GetIsPaused())
	{
		SetTrainingPause(false);
		// If we activated a queue that still has items in the queue then only unpause.
		// If the queue was empty then immediately add the new item too.
		if (not bIsQueueEmpty)
		{
			return;
		}
	}
	//  Costs + requirement snapshot + enqueue
	const int32 AmountAddToQueue = FRTS_Statics::IsShiftPressed(M_MainGameUI) ? 5 : 1;
	const TMap<ERTSResourceType, int32> Costs = GetTrainingCosts(TrainingItem.ItemID);

	// Template requirement (may be null)
	URTSRequirement* RequirementTemplate = nullptr;
	if (const FTrainingOptionState* OptState = M_TrainingOptionsMap.Find(TrainingItem.ItemID))
	{
		RequirementTemplate = OptState->RTSRequirement;
	}

	for (int32 i = 0; i < AmountAddToQueue; ++i)
	{
		// Duplicate a fresh snapshot per queue item (important!)
		URTSRequirement* Snapshot = nullptr;
		if (IsValid(RequirementTemplate))
		{
			// Perform the check for a NEW non-queued item (not yet in the queue).
			const bool bIsCheckingForQueuedItem = false;
			if (not RequirementTemplate->CheckIsRequirementMet(M_PrimarySelectedTrainer, bIsCheckingForQueuedItem))
			{
				OnRequirementNotMetWhenAttemptingToAddToQueue(RequirementTemplate);
				return;
			}
			Snapshot = DuplicateObject<URTSRequirement>(RequirementTemplate, M_PrimarySelectedTrainer);
		}

		// Register and obtain a unique key for this snapshot
		const int32 RequirementKey =
			M_PrimarySelectedTrainer->RegisterRequirementSnapshot(Snapshot);

		FTrainingQueueItem NewTrainingItem;
		NewTrainingItem.ItemID = TrainingItem.ItemID;
		NewTrainingItem.TotalTrainingTime = TrainingItem.TotalTrainingTime;
		NewTrainingItem.RemainingTrainingTime = TrainingItem.TotalTrainingTime;
		NewTrainingItem.WidgetIndex = Index;
		NewTrainingItem.AsyncRequestState = EAsyncTrainingRequestState::Async_NoTrainingRequest;
		NewTrainingItem.RemainingResourceCosts = Costs;
		NewTrainingItem.TotalResourceCosts = Costs;
		NewTrainingItem.RequirementKey = RequirementKey;

		// Also calls OnTrainingItemAddedToQueue on the trainer that owns the primary trainer component.
		// also updates the UI through a refresh request.
		AddTrainingItemToQueue(NewTrainingItem);
	}
}

void UTrainingMenuManager::AddTrainingItemToQueue(const FTrainingQueueItem& TrainingItem)
{
	const bool bIsFirstItem = M_PrimarySelectedTrainer->AddToQueue(TrainingItem);
}

void UTrainingMenuManager::RemoveLastOptionFromQueue(const FTrainingOption& TrainingOption) const
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return;
	}
	M_PrimarySelectedTrainer->RemoveLastInstanceOfTypeFromQueue(TrainingOption);
}

void UTrainingMenuManager::UpdateWidgetsWithPrimaryQueue()
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return;
	}
	int32 ActiveItemIndex = INDEX_NONE;
	int32 TimeRemaining = 0;
	bool bIsPausedByResourceInsufficiency = false;
	TMap<FTrainingOption, int32> QueueState = M_PrimarySelectedTrainer->GetCurrentTrainingQueueState(
		ActiveItemIndex, TimeRemaining, bIsPausedByResourceInsufficiency);
	// Get the order of the training options this unit has.
	TArray<FTrainingOption> TrainingOptions = M_PrimarySelectedTrainer->GetTrainingOptions();

	// Stores the state the widget needs to display.
	FTrainingWidgetState WidgetState;
	int32 WidgetIndex = 0;
	for (const auto EachOption : TrainingOptions)
	{
		if (not CheckTrainingOptionInMap(EachOption))
		{
			continue;
		}
		WidgetState.ItemID = EachOption;
		// Set how often this option is in the queue.
		WidgetState.Count = QueueState[EachOption];
		// Set the time remaining for this widget to default time.
		WidgetState.TotalTrainingTime = M_TrainingOptionsMap[EachOption].TrainingTime;

		WidgetState.SecondaryRequiredUnit = FTrainingOption();
		WidgetState.SecondaryRequiredTech = ETechnology::Tech_NONE;
		WidgetState.RequiredUnit = FTrainingOption();
		WidgetState.RequiredTech = ETechnology::Tech_NONE;

		WidgetState.TrainingStatus = CheckRequirement(
			M_TrainingOptionsMap[EachOption].RTSRequirement,
			WidgetState.RequirementCount,
			WidgetState.RequiredUnit,
			WidgetState.RequiredTech,
			WidgetState.SecondaryRequiredUnit,
			WidgetState.SecondaryRequiredTech,
			WidgetState.TrainingStatusSecondRequirement,
			WidgetState.bIsSecondRequirementMet,
			WidgetState.bIsFirstRequirementMet);

		M_TTrainingMenuWidgets[WidgetIndex]->UpdateTrainingItem(WidgetState);
		UpdateRequirementOpacityForWidget(M_TTrainingMenuWidgets[WidgetIndex], WidgetState);
		WidgetIndex++;
		if (WidgetIndex >= DeveloperSettings::GamePlay::Training::MaxTrainingOptions)
		{
			RTSFunctionLibrary::ReportError("Too many training options for the UI."
				"\n see UTrainingMenuManager::UpdateWidgetsWithPrimaryQueue."
				"\n Max options: " + FString::FromInt(DeveloperSettings::GamePlay::Training::MaxTrainingOptions) +
				"\n Primary name selected: " + M_PrimarySelectedTrainer->GetName());
			break;
		}
	}
	// Set the rest of the widgets to empty state.
	for (int32 i = WidgetIndex; i < DeveloperSettings::GamePlay::Training::MaxTrainingOptions; i++)
	{
		M_TTrainingMenuWidgets[i]->UpdateTrainingItem(M_EmptyTrainingOptionState);
		UpdateRequirementOpacityForWidget(M_TTrainingMenuWidgets[i], M_EmptyTrainingOptionState);
	}
	// Check if there is an active item.
	if (ActiveItemIndex != INDEX_NONE && ActiveItemIndex <
		DeveloperSettings::GamePlay::Training::MaxTrainingOptions)
	{
		const bool bSetClockToPausedState = M_PrimarySelectedTrainer->GetIsPaused() || bIsPausedByResourceInsufficiency;
		StartClockOnTrainingItem(ActiveItemIndex, TimeRemaining, TrainingOptions[ActiveItemIndex],
		                         bSetClockToPausedState);
	}
	// We do not start a new clock but make sure to stop the old one.
	else if (M_ActiveTrainingItem)
	{
		ResetActiveItem();
	}
}

void UTrainingMenuManager::UpdateRequirementOpacityForWidget(
	UW_TrainingItem* TrainingItemWidget,
	const FTrainingWidgetState& WidgetState) const
{
	if (not IsValid(TrainingItemWidget))
	{
		RTSFunctionLibrary::ReportError("Training widget is not valid."
			"\n see UTrainingMenuManager::UpdateRequirementOpacityForWidget.");
		return;
	}

	const bool bIsRequirementMet = WidgetState.TrainingStatus == ETrainingItemStatus::TS_Unlocked;
	const float RenderOpacity = bIsRequirementMet ? 1.0f : M_RequirementNotMetOpacity;
	TrainingItemWidget->SetTrainingButtonRenderOpacity(RenderOpacity);
}

ETrainingItemStatus UTrainingMenuManager::CheckRequirement(
	const TObjectPtr<URTSRequirement> RequirementForTraining,
	ERequirementCount& OutRequirementCount,
	FTrainingOption& OutRequiredUnit,
	ETechnology& OutRequiredTechnology,
	FTrainingOption& OutSecondaryRequiredUnit,
	ETechnology& OutSecondaryRequiredTech,
	ETrainingItemStatus& OutSecondaryStatus, bool& bOutIsSecondRequirementMet, bool& bOutIsFirstRequirementMet) const
{
	OutRequirementCount = ERequirementCount::SingleRequirement;
	OutRequiredUnit = FTrainingOption();
	OutRequiredTechnology = ETechnology::Tech_NONE;
	OutSecondaryRequiredUnit = FTrainingOption();
	OutSecondaryRequiredTech = ETechnology::Tech_NONE;
	OutSecondaryStatus = ETrainingItemStatus::TS_Unlocked;
	bOutIsSecondRequirementMet = false;

	if (!IsValid(RequirementForTraining))
	{
		return ETrainingItemStatus::TS_Unlocked;
	}

	constexpr bool bCheckForAlreadyQueuedItem = false;
	// For the double And and Double Or this also updates the bIsSecondRequirementMet bool in these requirements.
	if (RequirementForTraining->CheckIsRequirementMet(M_PrimarySelectedTrainer, bCheckForAlreadyQueuedItem))
	{
		return ETrainingItemStatus::TS_Unlocked;
	}

	// Not met → compose UI
	OutRequirementCount = RequirementForTraining->GetRequirementCount();
	OutRequiredUnit = RequirementForTraining->GetRequiredUnit();
	OutRequiredTechnology = RequirementForTraining->GetRequiredTechnology();

	// Secondary “what” (unit/tech IDs) for composites:
	if (const UDoubleRequirement* AsDouble = Cast<UDoubleRequirement>(RequirementForTraining.Get()))
	{
		OutSecondaryRequiredUnit = AsDouble->GetSecondaryRequiredUnit();
		OutSecondaryRequiredTech = AsDouble->GetSecondaryRequiredTech();
		OutSecondaryStatus = AsDouble->GetSecondaryRequirementStatus();
		OutRequirementCount = AsDouble->GetRequirementCount();
		bOutIsFirstRequirementMet = AsDouble->IsFirstRequirementMet();
		bOutIsSecondRequirementMet = AsDouble->IsSecondRequirementMet();
	}
	else if (const UOrDoubleRequirement* AsOr = Cast<UOrDoubleRequirement>(RequirementForTraining.Get()))
	{
		OutSecondaryRequiredUnit = AsOr->GetSecondaryRequiredUnit();
		OutSecondaryRequiredTech = AsOr->GetSecondaryRequiredTech();
		OutSecondaryStatus = AsOr->GetSecondaryRequirementStatus();
		OutRequirementCount = AsOr->GetRequirementCount();
		bOutIsFirstRequirementMet = AsOr->IsFirstRequirementMet();
		bOutIsSecondRequirementMet = AsOr->IsSecondRequirementMet();
	}

	// Primary icon/status from the requirement’s chosen presentation.
	return RequirementForTraining->GetTrainingStatusWhenRequirementNotMet();
}


bool UTrainingMenuManager::GetWidgetIndexInBounds(const int32 Index) const
{
	const bool bIsInBounds = (Index >= 0 && Index < DeveloperSettings::GamePlay::Training::MaxTrainingOptions);
	if (not bIsInBounds)
	{
		RTSFunctionLibrary::ReportError("Widget index out of bounds."
			"\n see UTrainingMenuManager::GetWidgetIndexInBounds."
			"\n Index: " + FString::FromInt(Index));
	}
	return bIsInBounds;
}

bool UTrainingMenuManager::GetRequestIsPrimary(const UTrainerComponent* const RequestingTrainer) const
{
	if (not GetIsValidMainGameUI() || not GetIsValidPrimarySelectedTrainer() || not IsValid(RequestingTrainer))
	{
		return false;
	}

	return M_PrimarySelectedTrainer == RequestingTrainer;
}

bool UTrainingMenuManager::GetIsValidMainGameUI() const
{
	if (IsValid(M_MainGameUI))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Main game UI is not valid."
		"\n see UTrainingMenuManager::GetIsValidMainGameUI.");
	return false;
}

void UTrainingMenuManager::ResetActiveItem()
{
	if (M_ActiveTrainingItem)
	{
		M_ActiveTrainingItem->StopClock();
	}
	M_ActiveTrainingItem = nullptr;
}

void UTrainingMenuManager::SetTrainingPause(const bool bPause) const
{
	const FString Pause = bPause ? "true" : "false";
	RTSFunctionLibrary::PrintString("Set training pause: " + Pause, FColor::Red, 20);
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return;
	}
	if (M_ActiveTrainingItem)
	{
		M_ActiveTrainingItem->SetClockPaused(bPause);
	}
	M_PrimarySelectedTrainer->SetPauseQueue(bPause);
}

void UTrainingMenuManager::RefreshRequirements()
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		// no trainer selected; nothing to do.
		return;
	}
	// Reload Training UI.
	UpdateWidgetsWithPrimaryQueue();
}


void UTrainingMenuManager::SetRequirementPanelVisiblity(const bool bIsVisible) const
{
	if (not EnsureIsValidTrainingRequirementWidget())
	{
		return;
	}
	if (bIsVisible)
	{
		M_TrainingRequirementWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		M_TrainingRequirementWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

bool UTrainingMenuManager::CheckTrainingOptionInMap(const FTrainingOption TrainingOption) const
{
	if (M_TrainingOptionsMap.Contains(TrainingOption))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Training option not found in map."
		"\n see UTrainingMenuManager::CheckTrainingOptionInMap."
		"\n Training option: " + TrainingOption.GetTrainingName());
	return false;
}

bool UTrainingMenuManager::GetIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player resource manager is not valid."
		"\n see UTrainingMenuManager::GetIsValidPlayerResourceManager.");
	return false;
}

void UTrainingMenuManager::Init_SetupPlayerResourceManager(
	const UObject* WorldContextObject,
	UW_TrainingDescription* TrainingDescription)
{
	if (UPlayerResourceManager* ResMngr = FRTS_Statics::GetPlayerResourceManager(WorldContextObject); IsValid(ResMngr))
	{
		M_PlayerResourceManager = ResMngr;
	}
	// Error report check if not valid.
	(void)GetIsValidPlayerResourceManager();
	TrainingDescription->SetResourceManager(M_PlayerResourceManager);
	TrainingDescription->SetTrainingMenuManager(this);
}

bool UTrainingMenuManager::EnsureIsValidTrainingRequirementWidget() const
{
	if (not IsValid(M_TrainingRequirementWidget))
	{
		RTSFunctionLibrary::ReportError("Training requirement widget is not valid."
			"\n see UTrainingMenuManager::EnsureIsValidTrainingRequirementWidget.");
		return false;
	}
	return true;
}

void UTrainingMenuManager::UpdateRequirementWidget(const FTrainingWidgetState& TrainingWidgetState) const
{
	if (not EnsureIsValidTrainingRequirementWidget())
	{
		return;
	}
	M_TrainingRequirementWidget->UpdateRequirement(TrainingWidgetState);
}

void UTrainingMenuManager::ReportInvalidActiveItemForQueueClock(const int32 IndexActiveTrainingItem) const
{
	const FString PrimaryName = GetIsValidPrimarySelectedTrainer() ? M_PrimarySelectedTrainer->GetName() : "Nullptr";
	RTSFunctionLibrary::ReportError(
		"Active training item is nullptr. This should not happen as we asked for starting the clock"
		"on a valid Training item Index."
		"\n see UTrainingMenuManager::StartClockOnTrainingItem."
		"\n IndexActiveTrainingItem: " + FString::FromInt(IndexActiveTrainingItem) +
		"\n PrimarySelected: " + PrimaryName);
}

TMap<ERTSResourceType, int32> UTrainingMenuManager::GetTrainingCosts(const FTrainingOption& TrainingOption) const
{
	if (not GetIsValidPlayerResourceManager())
	{
		return TMap<ERTSResourceType, int32>();
	}
	bool bIsValidCosts = true;
	TMap<ERTSResourceType, int32> TrainingCosts = FRTS_Statics::GetResourceCostsOfTrainingOption(
		TrainingOption, bIsValidCosts, M_PlayerResourceManager.Get());
	if (not bIsValidCosts)
	{
		RTSFunctionLibrary::ReportError("Training Menu Manager could not obtain valid costs for the training Option "
			"\n " + FRTS_Statics::Global_GetTrainingOptionString(TrainingOption));
	}
	return TrainingCosts;
}

void UTrainingMenuManager::OnRequirementNotMetWhenAttemptingToAddToQueue(URTSRequirement* Requirement)
{
	// todo message to player.
	RTSFunctionLibrary::PrintString("Requirement not met! Will not add to queue", FColor::Red);
}
