// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrainingMenuManager.h"

#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/BottomRightUIPanelState/BottomRightUIPanelState.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/TrainingUI/Interface/Trainer.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "TrainerComponent/TrainerComponent.h"
#include "TrainingLayout/TrainingLayoutSubsystem.h"
#include "TrainingMenuWidget/W_TrainingBuildingColumn.h"
#include "TrainingMenuWidget/W_TrainingMenu.h"
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
		M_ShownSubSectionIndex = Trainer->GetLastViewedSubSectionIndex();
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
		RemoveLastOptionFromQueue(TrainingItem.ItemID);
		PlayAnnouncerTrainingSound(EAnnouncerVoiceLineType::Cancelled);
	}
	else if (M_ActiveTrainingItem)
	{
		SetTrainingPause(true);
		PlayAnnouncerTrainingSound(EAnnouncerVoiceLineType::OnHold);
	}
}

void UTrainingMenuManager::PlayAnnouncerTrainingSound(const EAnnouncerVoiceLineType VoiceLineType) const
{
	if(not GetIsValidMainGameUI())
	{
		return;
	}
	ACPPController* PlayerController = M_MainGameUI->GetPlayerController();
	if(not IsValid(PlayerController))
	{
		return;
	}
	PlayerController->PlayAnnouncerVoiceLine(VoiceLineType);
}

void UTrainingMenuManager::InitTrainingManager(
	UW_TrainingMenu* TrainingMenu,
	UW_TrainingBuildingColumn* BuildingColumn,
	UMainGameUI* MainGameUI,
	UW_TrainingRequirement* TrainingRequirementWidget, UW_TrainingDescription* TrainingDescription)
{
	InitAllGameTrainingOptions();
	M_TrainingMenu = TrainingMenu;
	M_BuildingColumn = BuildingColumn;
	if (GetIsValidBuildingColumn())
	{
		M_BuildingColumn->InitBuildingColumn(this);
	}
	M_TTrainingMenuWidgets.Reset();
	if (GetIsValidTrainingMenu())
	{
		M_TrainingMenu->InitTrainingMenu(this);
		for (UW_TrainingItem* TrainingItem : M_TrainingMenu->GetTrainingItems())
		{
			// Initialize the training item.
			TrainingItem->InitW_TrainingItem(this, M_TTrainingMenuWidgets.Num());
			M_TTrainingMenuWidgets.Add(TrainingItem);
		}
	}
	if (M_TTrainingMenuWidgets.Num() < TrainingLayoutConstants::SlotsPerSubSection)
	{
		RTSFunctionLibrary::ReportError("The training menu has fewer training items than a sub section has slots."
			"\n Training items: " + FString::FromInt(M_TTrainingMenuWidgets.Num()) +
			"\n Slots per sub section: " + FString::FromInt(TrainingLayoutConstants::SlotsPerSubSection) +
			"\n See UTrainingMenuManager::InitTrainingManager.");
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

void UTrainingMenuManager::OnSubSectionTabClicked(const int32 SubSectionIndex)
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return;
	}
	if (not M_PrimarySelectedTrainer->GetTrainingSubSections().IsValidIndex(SubSectionIndex))
	{
		return;
	}
	M_ShownSubSectionIndex = SubSectionIndex;
	M_PrimarySelectedTrainer->SetLastViewedSubSectionIndex(SubSectionIndex);
	// The hovered item is about to show another unit, so its requirement no longer applies.
	SetRequirementPanelVisiblity(false);
	UpdateWidgetsWithPrimaryQueue();
}

void UTrainingMenuManager::OnQuickNavButtonClicked(const ENomadicSubtype Building)
{
	if (not GetIsValidMainGameUI())
	{
		return;
	}
	AActor* TrainingBuilding = FindFirstTrainingBuildingOfPlayer(Building);
	if (not IsValid(TrainingBuilding))
	{
		// The building was packed up or destroyed after the last refresh.
		RefreshQuickNavButtons();
		return;
	}
	ACPPController* PlayerController = M_MainGameUI->GetPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	PlayerController->SelectSingleUnitFromUI(TrainingBuilding);
}

void UTrainingMenuManager::OnBuildingColumnEntryClicked(const int32 EntryIndex)
{
	if (not GetIsValidMainGameUI() || not M_BuildingColumnBuildings.IsValidIndex(EntryIndex))
	{
		return;
	}
	AActor* Building = M_BuildingColumnBuildings[EntryIndex].Get();
	ACPPController* PlayerController = M_MainGameUI->GetPlayerController();
	if (not IsValid(Building) || not IsValid(PlayerController))
	{
		// The building was destroyed after the last refresh.
		RefreshBuildingColumn();
		return;
	}
	PlayerController->SelectSingleUnitFromUI(Building);
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
	const int32 FlatIndexActiveOption,
	const int32 TimeRemaining,
	const FTrainingOption ActiveOption,
	const bool bIsPaused)
{
	if (M_ActiveTrainingItem)
	{
		// Stop clock on previous item
		ResetActiveItem();
	}
	const int32 IndexActiveTrainingItem = GetWidgetIndexOfFlatOptionIndex(FlatIndexActiveOption);
	if (IndexActiveTrainingItem == INDEX_NONE)
	{
		// The active item trains in a sub section that is not on screen.
		return;
	}
	M_ActiveTrainingItem = M_TTrainingMenuWidgets[IndexActiveTrainingItem];
	if (not M_ActiveTrainingItem)
	{
		ReportInvalidActiveItemForQueueClock(IndexActiveTrainingItem);
		return;
	}
	// Get the total training time for the active item
	const FTrainingOptionState* ActiveOptionState = M_TrainingOptionsMap.Find(ActiveOption);
	if (not ActiveOptionState)
	{
		RTSFunctionLibrary::ReportError("Training option not found in map."
			"\n see UTrainingMenuManager::StartClockOnTrainingItem."
			"\n ActiveOption: " + ActiveOption.GetTrainingName());
		return;
	}
	M_ActiveTrainingItem->StartClock(TimeRemaining, ActiveOptionState->TrainingTime, bIsPaused);
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
	InitAllGameTrainingLayoutOnlyOptions();
	InitAllGameFieldDefenseTrainingOptions();
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
			HarvesterTrainingTime ,
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

	// Ger PzJager Laser
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzJagerLaser)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankDestroyerTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzJagerLaser,
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


	// Ger Pz II Flame
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PZII_Flame)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PZII_Flame,
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


	// Ger Pz 38t Railgun; unlocked by either the medium tank factory or the tier II communication center.
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Pz38t_RailGun)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Pz38t_RailGun,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			CreateTierTwoOrBuildingUnlockRequirement(ENomadicSubtype::Nomadic_GerMedTankFactory)
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz 38t Rockets; unlocked by either the medium tank factory or the tier II communication center.
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Pz38t_R)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Pz38t_R,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			CreateTierTwoOrBuildingUnlockRequirement(ENomadicSubtype::Nomadic_GerMedTankFactory)
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Pz II Luchs
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PzII_Luchs)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PzII_Luchs,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger Wespe
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Wespe)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Wespe,
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

	// GER ARMORED CARS

	// Ger SdKfz 251/22; unlocked by either the light tank factory or the tier II communication center.
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz251_22)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz251_22,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			CreateTierTwoOrBuildingUnlockRequirement(ENomadicSubtype::Nomadic_GerLightSteelForge)
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SdKfz 251 Mortar
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz251_Mortar)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz251_Mortar,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SdKfz 251 Transport
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz251_Transport)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz251_Transport,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


	// Ger SdKfz 9 37mm
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz9_37mm)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			ArmoredCarTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz9_37mm,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SdKfz 7/1 Flakvierling
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz_7_Flakvierling)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			ArmoredCarTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz_7_Flakvierling,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger SdKfz 221 20mm
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Sdkfz_221)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			ArmoredCarTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Sdkfz_221,
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

	// Ger Pz IV Rockets; tier II unlock.
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_PZIV_Rockets)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_PZIV_Rockets,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			CreateTierTwoUnlockRequirement()
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

	// Ger Brumbar; tier II unlock.
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
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			CreateTierTwoUnlockRequirement()
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

	// Ger Tiger Rail
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_TigerRail)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T2HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_TigerRail,
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

	// Ger E-75 (a quarter longer than the King Tiger)
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_E75)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime + T3HeavyTankTrainingTime / 4,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_E75,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Ger E-50 (same as the King Tiger)
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_E50)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_E50,
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

	// RUS ARMORED CARS

	// Rus BA-12
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Ba12)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Ba12,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus BA-14
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_Ba14)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_Ba14,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}


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

	// Rus T-70F
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T70F)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T70F,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus SU-76
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_76)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			LightTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_76,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// RUS MEDIUM TANKS

	// Rus T-34 Harvester
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T34_Harvester)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			HarvesterTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T34_Harvester,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);

		FTrainingOption HarvesterItemID = FTrainingOption(
			EAllUnitType::UNType_Harvester,
			static_cast<uint8>(ETankSubtype::Tank_T34_Harvester)
		);

		FTrainingOptionState HarvesterTrainingOptionState = CreateTrainingOptionState(
			HarvesterItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Harvester,
			ETankSubtype::Tank_T34_Harvester,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(HarvesterItemID, HarvesterTrainingOptionState);
	}

	// Rus T-34/76
	{
		const int32 T34AaExtraTrainingSeconds = 12;

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

		FTrainingOption T34AaItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T34_AA)
		);

		FTrainingOptionState T34AaTrainingOptionState = CreateTrainingOptionState(
			T34AaItemID,
			MediumTankTrainingTime + T34AaExtraTrainingSeconds * GameTimeMlt,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T34_AA,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(T34AaItemID, T34AaTrainingOptionState);
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

	// Rus SU-85
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_85)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_85,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus SU-85-L
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_85_L)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_85_L,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus SU-100
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_100)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_100,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus SU-122
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_122)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_122,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-44/100L
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T44_100L)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T44_100L,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus T-44/155
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T44_155)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T44_155,
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

	// Rus KV-1 Arc
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_KV_1_Arc)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_KV_1_Arc,
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

	// Rus T-28F
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_T28F)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MediumTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_T28F,
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

	// Rus SU-152
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_SU_152)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_SU_152,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}

	// Rus ISU-152
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(ETankSubtype::Tank_ISU_152)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			T3HeavyTankTrainingTime,
			EAllUnitType::UNType_Tank,
			ETankSubtype::Tank_ISU_152,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None
		);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	}
}

void UTrainingMenuManager::InitAllGameAircraftTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	const TArray<EAircraftSubtype> AircraftSubtypes = {
		EAircraftSubtype::Aircraft_Ju87,
		EAircraftSubtype::Aircraft_Me410,
		EAircraftSubtype::Aircraft_Bf109,
		EAircraftSubtype::Aircraft_Ju390B,
		EAircraftSubtype::Aircraft_PE8,
		EAircraftSubtype::Aircraft_PE8_4x1000,
		EAircraftSubtype::Aircraft_PE8_1x3000,
		EAircraftSubtype::Aircraft_PE2,
		EAircraftSubtype::Aircraft_Horten229,
		EAircraftSubtype::Aircraft_Sturmovic,
		EAircraftSubtype::Aircraft_Yak
	};

	for (const EAircraftSubtype AircraftSubtype : AircraftSubtypes)
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(AircraftSubtype)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			BasicAircaftTrainingTime,
			EAllUnitType::UNType_Aircraft,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			ESquadSubtype::Squad_None,
			AircraftSubtype,
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

	// Train Armory
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerArmory)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT2ArmoryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerArmory,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(),
			                                               FTrainingOption(EAllUnitType::UNType_Nomadic,
			                                                               static_cast<uint8>(
				                                                               ENomadicSubtype::Nomadic_GerBarracks)))
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

	// -------------------------------------------------------------------------------------------------------
	// -------------------------------------- T3 BUILDINGS
	// -------------------------------------------------------------------------------------------------------

	// Train T3 Experimental Units Factory
	{
		FTrainingOption ItemID = FTrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory)
		);

		FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			NomadicT3ExperimentalUnitsFactoryTrainingTime,
			EAllUnitType::UNType_Nomadic,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory,
			ESquadSubtype::Squad_None,
			EAircraftSubtype::Aircarft_None,
			FRTS_RequirementHelpers::CreateDouble_Unit_Unit(GetTransientPackage(),
			                                                FTrainingOption(
				                                                EAllUnitType::UNType_Nomadic,
				                                                static_cast<uint8>(
					                                                ENomadicSubtype::Nomadic_GerMedTankFactory)),
			                                                FTrainingOption(
				                                                EAllUnitType::UNType_Nomadic,
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
	InitAllGameAntiTankGunTWTrainingOptions();
	InitAllGameMortarTWTrainingOptions();
	InitAllGameFlakTWTrainingOptions();
	InitAllGameArtilleryTWTrainingOptions();
	InitAllGameMachineGunTWTrainingOptions();

}


void UTrainingMenuManager::InitAllGameAntiTankGunTWTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;

	auto AddTeamWeaponTrainingOption = [this](const ESquadSubtype SquadSubtype, const int32 TrainingTime,
	                                          URTSRequirement* Requirement = nullptr)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		const FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			TrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			SquadSubtype,
			EAircraftSubtype::Aircarft_None,
			Requirement);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	};

	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_PaK38, MediumAntiTankGunTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_PaK40, HeavyAntiTankGunTeamWeaponTrainingTime,
	                            CreateTierTwoUnlockRequirement());
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_Zis_57MM, MediumAntiTankGunTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_53K_45MM, MediumAntiTankGunTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_Zis_76MM, HeavyAntiTankGunTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_BS3_100MM, HeavyAntiTankGunTeamWeaponTrainingTime);
}

void UTrainingMenuManager::InitAllGameMortarTWTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	auto AddTeamWeaponTrainingOption = [this](const ESquadSubtype SquadSubtype, const int32 TrainingTime)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		const FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			TrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			SquadSubtype);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	};

	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_GrW42_80mm, MediumMortarTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_80mm_Mortar, MediumMortarTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_120mm_Mortar, HeavyMortarTeamWeaponTrainingTime);
}

void UTrainingMenuManager::InitAllGameFlakTWTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	auto AddTeamWeaponTrainingOption = [this](const ESquadSubtype SquadSubtype, const int32 TrainingTime,
	                                          URTSRequirement* Requirement = nullptr)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		const FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			TrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			SquadSubtype,
			EAircraftSubtype::Aircarft_None,
			Requirement);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	};

	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_37mmFlak, MediumFlakTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_88mmFlak, HeavyFlakTeamWeaponTrainingTime,
	                            CreateTierTwoUnlockRequirement());
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_Bofors, MediumFlakTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_KS30_130MM, HeavyFlakTeamWeaponTrainingTime);
}

void UTrainingMenuManager::InitAllGameArtilleryTWTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	auto AddTeamWeaponTrainingOption = [this](const ESquadSubtype SquadSubtype, const int32 TrainingTime,
	                                          URTSRequirement* Requirement = nullptr)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		const FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			TrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			SquadSubtype,
			EAircraftSubtype::Aircarft_None,
			Requirement);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	};

	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_LefH18, MediumArtilleryTeamWeaponTrainingTime,
	                            CreateTierTwoUnlockRequirement());
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_210MM_Morser, HeavyArtilleryTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_SFH18_150mm, HeavyArtilleryTeamWeaponTrainingTime,
	                            CreateTierTwoUnlockRequirement());
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_Nebelwerfer, MediumArtilleryTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_M1938_122mm, HeavyArtilleryTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_B4_203mm, HeavyArtilleryTeamWeaponTrainingTime);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_ML20_152mm, HeavyArtilleryTeamWeaponTrainingTime);
}

void UTrainingMenuManager::InitAllGameMachineGunTWTrainingOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;
	auto AddTeamWeaponTrainingOption = [this](const ESquadSubtype SquadSubtype)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		const FTrainingOptionState NewTrainingOptionState = CreateTrainingOptionState(
			ItemID,
			MachineGunTeamWeaponTrainingTime,
			EAllUnitType::UNType_Squad,
			ETankSubtype::Tank_None,
			ENomadicSubtype::Nomadic_None,
			SquadSubtype);
		M_TrainingOptionsMap.Add(ItemID, NewTrainingOptionState);
	};

	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Ger_LMGSquad);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_Maxim);
	AddTeamWeaponTrainingOption(ESquadSubtype::Squad_Rus_DShK);
}

void UTrainingMenuManager::InitAllGameTrainingLayoutOnlyOptions()
{
	using namespace DeveloperSettings::GameBalance::TrainingTime;

	// Never replaces an entry made above, so a unit that gets its own entry with a requirement keeps it.
	auto AddTankOptionIfMissing = [this](const ETankSubtype TankSubtype, const int32 TrainingTime)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype));
		if (M_TrainingOptionsMap.Contains(ItemID))
		{
			return;
		}
		M_TrainingOptionsMap.Add(ItemID, CreateTrainingOptionState(
			                         ItemID, TrainingTime, EAllUnitType::UNType_Tank, TankSubtype));
	};
	auto AddSquadOptionIfMissing = [this](const ESquadSubtype SquadSubtype, const int32 TrainingTime)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
		if (M_TrainingOptionsMap.Contains(ItemID))
		{
			return;
		}
		M_TrainingOptionsMap.Add(ItemID, CreateTrainingOptionState(
			                         ItemID, TrainingTime, EAllUnitType::UNType_Squad, ETankSubtype::Tank_None,
			                         ENomadicSubtype::Nomadic_None, SquadSubtype));
	};

	AddSquadOptionIfMissing(ESquadSubtype::Squad_Ger_IronStorm, EliteInfantryTrainingTime);
	AddSquadOptionIfMissing(ESquadSubtype::Squad_Rus_Dyakonov, BasicInfantryTrainingTime);
	AddSquadOptionIfMissing(ESquadSubtype::Squad_Rus_KorovinAssault, ArmoredInfantryTrainingTime);
	AddSquadOptionIfMissing(ESquadSubtype::Squad_Rus_ROKS3_PPSH, ArmoredInfantryTrainingTime);

	AddTankOptionIfMissing(ETankSubtype::Tank_Sdkfz250, ArmoredCarTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Sdkfz250_37mm, ArmoredCarTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Sdkfz_231, ArmoredCarTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Keubel, ArmoredCarTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Kettenkrad, ArmoredCarTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Puma, ArmoredCarMediumCalibreTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_FlamePuma, ArmoredCarMediumCalibreTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Sdkfz_232_3, ArmoredCarMediumCalibreTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Sdkfz251_PZIV, ArmoredCarMediumCalibreTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_Panzerwerfer, ArmoredCarMediumCalibreTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_PzIII_AA, MediumTankTrainingTime);
	AddTankOptionIfMissing(ETankSubtype::Tank_PzIII_FLamm, MediumTankTrainingTime);
}

void UTrainingMenuManager::InitAllGameFieldDefenseTrainingOptions()
{
	// The queue item drives the construction, so the training time is the construction time of the pit.
	using DeveloperSettings::GameBalance::TrainingTime::BxpT2BuildTime;
	using DeveloperSettings::GameBalance::TrainingTime::GerPuppchenPitBuildTime;
	using DeveloperSettings::GameBalance::TrainingTime::GerPanzerschreckPitBuildTime;
	using DeveloperSettings::GameBalance::TrainingTime::GerRailPakBunkerBuildTime;
	using DeveloperSettings::GameBalance::TrainingTime::GerTwinMG42PitBuildTime;

	auto AddFieldDefenseTrainingOption = [this](const EBuildingExpansionType BxpType, URTSRequirement* Requirement,
	                                            const int32 TrainingTime = BxpT2BuildTime)
	{
		const FTrainingOption ItemID(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(BxpType));
		M_TrainingOptionsMap.Add(ItemID, CreateTrainingOptionState(
			                         ItemID, TrainingTime, EAllUnitType::UNType_BuildingExpansion,
			                         ETankSubtype::Tank_None, ENomadicSubtype::Nomadic_None, ESquadSubtype::Squad_None,
			                         EAircraftSubtype::Aircarft_None, Requirement));
	};
	// The tier II building that also unlocks the airbase and the experimental factory.
	const FTrainingOption RequiredTierTwoBuilding(
		EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));

	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerPak40Pit, nullptr);
	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerFlak37Pit, nullptr);
	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerMortarPit, nullptr);
	// The small two-man pits: cheap, quick to build, no requirement.
	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerPuppchenPit, nullptr, GerPuppchenPitBuildTime);
	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerPanzerschreckPit, nullptr, GerPanzerschreckPitBuildTime);
	AddFieldDefenseTrainingOption(EBuildingExpansionType::BTX_GerTwinMG42Pit, nullptr, GerTwinMG42PitBuildTime);
	AddFieldDefenseTrainingOption(
		EBuildingExpansionType::BTX_GerLeFH18Pit,
		FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), RequiredTierTwoBuilding));
	AddFieldDefenseTrainingOption(
		EBuildingExpansionType::BTX_GerSFH18Pit,
		FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), RequiredTierTwoBuilding));
	AddFieldDefenseTrainingOption(
		EBuildingExpansionType::BTX_GerFlak88Pit,
		FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), RequiredTierTwoBuilding));
	// Only the breakthrough doctrine's headquarters layout shows it.
	AddFieldDefenseTrainingOption(
		EBuildingExpansionType::BTX_GerRailPakBunker,
		FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), RequiredTierTwoBuilding),
		GerRailPakBunkerBuildTime);
}

URTSRequirement* UTrainingMenuManager::CreateTierTwoUnlockRequirement()
{
	const FTrainingOption RequiredTierTwoBuilding(
		EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));
	return FRTS_RequirementHelpers::CreateUnitRequirement(GetTransientPackage(), RequiredTierTwoBuilding);
}

URTSRequirement* UTrainingMenuManager::CreateTierTwoOrBuildingUnlockRequirement(
	const ENomadicSubtype AlternativeBuilding)
{
	const FTrainingOption RequiredAlternativeBuilding(
		EAllUnitType::UNType_Nomadic, static_cast<uint8>(AlternativeBuilding));
	const FTrainingOption RequiredTierTwoBuilding(
		EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));
	return FRTS_RequirementHelpers::CreateOr_Unit_Unit(GetTransientPackage(), RequiredAlternativeBuilding,
	                                                   RequiredTierTwoBuilding);
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
	const TArray<FTrainingSubSectionState>& SubSections = M_PrimarySelectedTrainer->GetTrainingSubSections();
	if (not SubSections.IsValidIndex(M_ShownSubSectionIndex) || TrainingItem.ItemID.IsNone())
	{
		return;
	}
	// Queue items remember their option by its index over all sub sections.
	const int32 FlatOptionIndex = SubSections[M_ShownSubSectionIndex].FirstFlatOptionIndex + Index;
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
	if (TrainingItem.ItemID.UnitType == EAllUnitType::UNType_BuildingExpansion)
	{
		OnLeftClickFieldDefenseItem(TrainingItem, FlatOptionIndex);
		return;
	}
	//  Costs + requirement snapshot + enqueue
	const int32 AmountAddToQueue = FRTS_Statics::IsShiftPressed(M_MainGameUI) ? 5 : 1;
	const FTrainingQueueItem QueueItemTemplate = CreateQueueItemForOption(TrainingItem, FlatOptionIndex);

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

		FTrainingQueueItem NewTrainingItem = QueueItemTemplate;
		NewTrainingItem.RequirementKey = RequirementKey;

		// Also calls OnTrainingItemAddedToQueue on the trainer that owns the primary trainer component.
		// also updates the UI through a refresh request.
		AddTrainingItemToQueue(NewTrainingItem);
		PlayAnnouncerTrainingSound(EAnnouncerVoiceLineType::Training);
		
	}
}

void UTrainingMenuManager::OnLeftClickFieldDefenseItem(const FTrainingWidgetState& TrainingItem,
                                                       const int32 FlatOptionIndex)
{
	URTSRequirement* RequirementTemplate = nullptr;
	if (const FTrainingOptionState* OptionState = M_TrainingOptionsMap.Find(TrainingItem.ItemID))
	{
		RequirementTemplate = OptionState->RTSRequirement;
	}
	constexpr bool bIsCheckingForQueuedItem = false;
	if (IsValid(RequirementTemplate)
		&& not RequirementTemplate->CheckIsRequirementMet(M_PrimarySelectedTrainer, bIsCheckingForQueuedItem))
	{
		OnRequirementNotMetWhenAttemptingToAddToQueue(RequirementTemplate);
		return;
	}
	if (not GetIsValidMainGameUI())
	{
		return;
	}
	ACPPController* PlayerController = M_MainGameUI->GetPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	// The controller owns building placement; it checks the limit of planned field defenses and shows its error.
	PlayerController->StartFieldDefensePlacement(
		M_PrimarySelectedTrainer,
		CreateQueueItemForOption(TrainingItem, FlatOptionIndex),
		RequirementTemplate);
}

FTrainingQueueItem UTrainingMenuManager::CreateQueueItemForOption(const FTrainingWidgetState& TrainingItem,
                                                                  const int32 FlatOptionIndex) const
{
	const TMap<ERTSResourceType, int32> Costs = GetTrainingCosts(TrainingItem.ItemID);
	FTrainingQueueItem NewTrainingItem;
	NewTrainingItem.ItemID = TrainingItem.ItemID;
	NewTrainingItem.TotalTrainingTime = TrainingItem.TotalTrainingTime;
	NewTrainingItem.RemainingTrainingTime = TrainingItem.TotalTrainingTime;
	NewTrainingItem.WidgetIndex = FlatOptionIndex;
	NewTrainingItem.AsyncRequestState = EAsyncTrainingRequestState::Async_NoTrainingRequest;
	NewTrainingItem.RemainingResourceCosts = Costs;
	NewTrainingItem.TotalResourceCosts = Costs;
	return NewTrainingItem;
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
	const TMap<FTrainingOption, int32> QueueState = M_PrimarySelectedTrainer->GetCurrentTrainingQueueState(
		ActiveItemIndex, TimeRemaining, bIsPausedByResourceInsufficiency);
	// Get the order of the training options this unit has.
	const TArray<FTrainingOption> TrainingOptions = M_PrimarySelectedTrainer->GetTrainingOptions();

	const int32 SubSectionCount = M_PrimarySelectedTrainer->GetTrainingSubSections().Num();
	M_ShownSubSectionIndex = FMath::Clamp(M_ShownSubSectionIndex, 0, FMath::Max(SubSectionCount - 1, 0));

	UpdateSubSectionTabsWithPrimaryTrainer();
	UpdateWidgetsWithShownSubSection(QueueState);
	RefreshQuickNavButtons();
	RefreshBuildingColumn();

	// Check if there is an active item.
	if (TrainingOptions.IsValidIndex(ActiveItemIndex))
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

void UTrainingMenuManager::UpdateSubSectionTabsWithPrimaryTrainer() const
{
	if (not GetIsValidTrainingMenu())
	{
		return;
	}
	const TArray<FTrainingSubSectionState>& SubSections = M_PrimarySelectedTrainer->GetTrainingSubSections();
	TArray<FTrainingSubSectionTabState> TabStates;
	TabStates.Reserve(SubSections.Num());
	for (int32 SubSectionIndex = 0; SubSectionIndex < SubSections.Num(); ++SubSectionIndex)
	{
		FTrainingSubSectionTabState& TabState = TabStates.AddDefaulted_GetRef();
		TabState.DisplayName = SubSections[SubSectionIndex].DisplayName;
		TabState.QueuedCount = M_PrimarySelectedTrainer->GetQueuedCountInSubSection(SubSectionIndex);
	}
	M_TrainingMenu->UpdateSubSectionTabs(TabStates, M_ShownSubSectionIndex);
}

void UTrainingMenuManager::UpdateWidgetsWithShownSubSection(const TMap<FTrainingOption, int32>& QueueState)
{
	const TArray<FTrainingSubSectionState>& SubSections = M_PrimarySelectedTrainer->GetTrainingSubSections();
	const TArray<FTrainingOption> NoOptions;
	const TArray<FTrainingOption>& ShownOptions = SubSections.IsValidIndex(M_ShownSubSectionIndex)
		                                              ? SubSections[M_ShownSubSectionIndex].Options
		                                              : NoOptions;
	if (ShownOptions.Num() > M_TTrainingMenuWidgets.Num())
	{
		RTSFunctionLibrary::ReportError("Too many training options for the UI."
			"\n see UTrainingMenuManager::UpdateWidgetsWithShownSubSection."
			"\n Available widgets: " + FString::FromInt(M_TTrainingMenuWidgets.Num()) +
			"\n Primary name selected: " + M_PrimarySelectedTrainer->GetName());
	}

	// Each option keeps the slot it has in the sub section; slots without an option show the empty state.
	for (int32 WidgetIndex = 0; WidgetIndex < M_TTrainingMenuWidgets.Num(); ++WidgetIndex)
	{
		const bool bSlotHasOption = ShownOptions.IsValidIndex(WidgetIndex)
			&& not ShownOptions[WidgetIndex].IsNone()
			&& CheckTrainingOptionInMap(ShownOptions[WidgetIndex]);
		if (not bSlotHasOption)
		{
			M_TTrainingMenuWidgets[WidgetIndex]->UpdateTrainingItem(M_EmptyTrainingOptionState);
			continue;
		}
		const FTrainingOption& Option = ShownOptions[WidgetIndex];
		M_TTrainingMenuWidgets[WidgetIndex]->UpdateTrainingItem(
			CreateWidgetStateForOption(Option, QueueState.FindRef(Option)));
	}
}

FTrainingWidgetState UTrainingMenuManager::CreateWidgetStateForOption(const FTrainingOption& Option,
                                                                      const int32 QueuedCount) const
{
	const FTrainingOptionState& OptionState = M_TrainingOptionsMap.FindChecked(Option);

	// Stores the state the widget needs to display.
	FTrainingWidgetState WidgetState;
	WidgetState.ItemID = Option;
	// Set how often this option is in the queue.
	WidgetState.Count = QueuedCount;
	// Set the time remaining for this widget to default time.
	WidgetState.TotalTrainingTime = OptionState.TrainingTime;

	WidgetState.TrainingStatus = CheckRequirement(
		OptionState.RTSRequirement,
		WidgetState.RequirementCount,
		WidgetState.RequiredUnit,
		WidgetState.RequiredTech,
		WidgetState.SecondaryRequiredUnit,
		WidgetState.SecondaryRequiredTech,
		WidgetState.TrainingStatusSecondRequirement,
		WidgetState.bIsSecondRequirementMet,
		WidgetState.bIsFirstRequirementMet);
	WidgetState.VariantPresentation = GetVariantPresentationForOption(Option);
	return WidgetState;
}

FUnitVariantTrainingPresentation UTrainingMenuManager::GetVariantPresentationForOption(
	const FTrainingOption& Option) const
{
	// The training menu is the player's own UI, so the player's selection applies without an owner check.
	// The menu widget is the world context, as for the game unit manager lookups in this class.
	const UPlayerUnitVariantManager* PlayerUnitVariantManager =
		FRTS_Statics::GetPlayerUnitVariantManager(M_TrainingMenu);
	if (not PlayerUnitVariantManager)
	{
		return FUnitVariantTrainingPresentation();
	}
	return PlayerUnitVariantManager->GetTrainingPresentation(Option);
}

int32 UTrainingMenuManager::GetWidgetIndexOfFlatOptionIndex(const int32 FlatOptionIndex) const
{
	if (not GetIsValidPrimarySelectedTrainer())
	{
		return INDEX_NONE;
	}
	const TArray<FTrainingSubSectionState>& SubSections = M_PrimarySelectedTrainer->GetTrainingSubSections();
	if (not SubSections.IsValidIndex(M_ShownSubSectionIndex))
	{
		return INDEX_NONE;
	}
	const FTrainingSubSectionState& ShownSubSection = SubSections[M_ShownSubSectionIndex];
	const int32 WidgetIndex = FlatOptionIndex - ShownSubSection.FirstFlatOptionIndex;
	if (not ShownSubSection.Options.IsValidIndex(WidgetIndex) || not M_TTrainingMenuWidgets.IsValidIndex(WidgetIndex))
	{
		return INDEX_NONE;
	}
	return WidgetIndex;
}

void UTrainingMenuManager::RefreshQuickNavButtons() const
{
	if (not GetIsValidTrainingMenu())
	{
		return;
	}
	UTrainingLayoutSubsystem* TrainingLayoutSubsystem = UTrainingLayoutSubsystem::Get(M_TrainingMenu);
	if (not IsValid(TrainingLayoutSubsystem))
	{
		return;
	}
	TSet<ENomadicSubtype> AvailableBuildings;
	for (const FTrainingQuickNavEntry& QuickNavEntry : TrainingLayoutSubsystem->GetQuickNavEntries())
	{
		if (IsValid(FindFirstTrainingBuildingOfPlayer(QuickNavEntry.Building)))
		{
			AvailableBuildings.Add(QuickNavEntry.Building);
		}
	}
	M_TrainingMenu->UpdateQuickNavButtons(AvailableBuildings, GetBuildingOfPrimarySelectedTrainer());
}

AActor* UTrainingMenuManager::FindFirstTrainingBuildingOfPlayer(const ENomadicSubtype Building) const
{
	TArray<AActor*> TrainingBuildings;
	GetTrainingBuildingsOfPlayer(Building, TrainingBuildings);
	return TrainingBuildings.IsEmpty() ? nullptr : TrainingBuildings[0];
}

void UTrainingMenuManager::GetTrainingBuildingsOfPlayer(const ENomadicSubtype Building,
                                                        TArray<AActor*>& OutBuildings) const
{
	OutBuildings.Reset();
	constexpr uint8 PlayerOwnerIndex = 1;
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(M_TrainingMenu);
	if (not IsValid(GameUnitManager))
	{
		return;
	}
	// Nomadic vehicles are stored with the tanks.
	const TArray<ATankMaster*> PlayerTanks = GameUnitManager->GetPlayerTanks(PlayerOwnerIndex);
	for (ATankMaster* PlayerTank : PlayerTanks)
	{
		if (not IsValid(PlayerTank))
		{
			continue;
		}
		const URTSComponent* RTSComponent = PlayerTank->GetRTSComponent();
		const bool bIsBuildingOfType = IsValid(RTSComponent)
			&& RTSComponent->GetUnitType() == EAllUnitType::UNType_Nomadic
			&& RTSComponent->GetSubtypeAsNomadicSubtype() == Building;
		if (not bIsBuildingOfType)
		{
			continue;
		}
		ITrainer* Trainer = Cast<ITrainer>(PlayerTank);
		if (not Trainer)
		{
			continue;
		}
		// Training is only enabled while the vehicle is expanded into its building.
		const UTrainerComponent* TrainerComponent = Trainer->GetTrainerComponent();
		if (IsValid(TrainerComponent) && TrainerComponent->GetIsAbleToTrain())
		{
			OutBuildings.Add(PlayerTank);
		}
	}
}

void UTrainingMenuManager::RefreshBuildingColumn()
{
	if (not GetIsValidBuildingColumn())
	{
		return;
	}
	M_BuildingColumnBuildings.Reset();
	TArray<FTrainingBuildingEntryState> EntryStates;

	TArray<AActor*> SameTypeBuildings;
	const ENomadicSubtype ShownBuilding = GetBuildingOfPrimarySelectedTrainer();
	if (ShownBuilding != ENomadicSubtype::Nomadic_None)
	{
		GetTrainingBuildingsOfPlayer(ShownBuilding, SameTypeBuildings);
	}

	// A single building has no other building to switch to, so it gets no column.
	constexpr int32 MinBuildingsForColumn = 2;
	const int32 EntryCount = SameTypeBuildings.Num() >= MinBuildingsForColumn
		                         ? FMath::Min(SameTypeBuildings.Num(), M_BuildingColumn->GetMaxEntries())
		                         : 0;
	for (int32 EntryIndex = 0; EntryIndex < EntryCount; ++EntryIndex)
	{
		AActor* Building = SameTypeBuildings[EntryIndex];
		// GetTrainingBuildingsOfPlayer only returns trainers with a valid trainer component.
		const UTrainerComponent* TrainerComponent = Cast<ITrainer>(Building)->GetTrainerComponent();

		FTrainingBuildingEntryState& EntryState = EntryStates.AddDefaulted_GetRef();
		EntryState.bIsShownBuilding = TrainerComponent == M_PrimarySelectedTrainer;
		EntryState.bIsTraining = TrainerComponent->GetActiveTrainingProgress(EntryState.TrainingProgress);
		M_BuildingColumnBuildings.Add(Building);
	}
	M_BuildingColumn->UpdateBuildingEntries(EntryStates);
}

bool UTrainingMenuManager::GetIsValidBuildingColumn() const
{
	if (IsValid(M_BuildingColumn))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_BuildingColumn",
		"GetIsValidBuildingColumn",
		this
	);

	return false;
}

ENomadicSubtype UTrainingMenuManager::GetBuildingOfPrimarySelectedTrainer() const
{
	if (not GetIsValidPrimarySelectedTrainer() || not IsValid(M_PrimarySelectedTrainer->GetOwner()))
	{
		return ENomadicSubtype::Nomadic_None;
	}
	const URTSComponent* RTSComponent = M_PrimarySelectedTrainer->GetOwner()->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent) || RTSComponent->GetUnitType() != EAllUnitType::UNType_Nomadic)
	{
		return ENomadicSubtype::Nomadic_None;
	}
	return RTSComponent->GetSubtypeAsNomadicSubtype();
}

bool UTrainingMenuManager::GetIsValidTrainingMenu() const
{
	if (IsValid(M_TrainingMenu))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrainingMenu",
		"GetIsValidTrainingMenu",
		this
	);

	return false;
}

// void UTrainingMenuManager::UpdateRequirementOpacityForWidget(
// 	UW_TrainingItem* TrainingItemWidget,
// 	const FTrainingWidgetState& WidgetState) const
// {
// 	if (not IsValid(TrainingItemWidget))
// 	{
// 		RTSFunctionLibrary::ReportError("Training widget is not valid."
// 			"\n see UTrainingMenuManager::UpdateRequirementOpacityForWidget.");
// 		return;
// 	}
//
// 	const bool bIsRequirementMet = WidgetState.TrainingStatus == ETrainingItemStatus::TS_Unlocked;
// 	const float RenderOpacity = bIsRequirementMet ? 1.0f : M_RequirementNotMetOpacity;
// 	TrainingItemWidget->SetTrainingButtonRenderOpacity(RenderOpacity);
// }

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
	const bool bIsInBounds = M_TTrainingMenuWidgets.IsValidIndex(Index);
	if (not bIsInBounds)
	{
		RTSFunctionLibrary::ReportError("Widget index out of bounds."
			"\n see UTrainingMenuManager::GetWidgetIndexInBounds."
			"\n Index: " + FString::FromInt(Index) +
			"\n Widgets available: " + FString::FromInt(M_TTrainingMenuWidgets.Num()));
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
