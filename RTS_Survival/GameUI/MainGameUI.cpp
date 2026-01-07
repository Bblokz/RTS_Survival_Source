// Copyright (C) Bas Blokzijl - All rights reserved.


#include "MainGameUI.h"

#include "BottomRightUIPanelState/BottomRightUIPanelState.h"
#include "BuildingExpansion/W_ItemBuildingExpansion.h"
#include "BuildingExpansion/W_OptionBuildingExpansion.h"
#include "BuildingExpansion/BxpDescriptionWidget/W_BxpDescription.h"
#include "ControlGroups/W_ControlGroups.h"
#include "Resources/WPlayerResource/W_PlayerResource.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BottomCenterUIPanel/W_BottomCenterUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
// To set render targets on the mini map.
#include "GameUI_InitData.h"
#include "Archive/W_Archive/W_Archive.h"
#include "Components/Border.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/W_WeaponItem.h"
#include "RTS_Survival/GameUI/MiniMap/W_MiniMap.h"
#include "RTS_Survival/TechTree/TechTreeBaseWidget/TechTree.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MiniMap/MiniMapStartDirection/MiniMapStartDirection.h"
#include "Resources/PlayerEnergyBar/W_PlayerEnergyBar.h"
#include "RTS_Survival/Player/PlayerControlGroupManager/PlayerControlGroupManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "TrainingUI/TrainingDescription/W_TrainingDescription.h"
#include "RTS_Survival/GameUI/Archive/W_ArchiveNotificationHolder/W_ArchiveNotificationHolder.h"


void UMainGameUI::OpenTechTree()
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	if (bWasHiddenByAllGameUI)
	{
		return;
	}

	M_PlayerController->SetIsPlayerInTechTree(true);

	if (IsValid(M_TechTree))
	{
		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->bDisableWorldRendering = true;
		}
		SetMainMenuVisiblity(false);
		M_TechTree->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	LoadTechTree();
}

void UMainGameUI::SetMainMenuVisiblity(const bool bVisible)
{
	if (not IsValid(MainUICanvas))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid MainUICanvas in SetMainMenuVisiblity!\n at function SetMainMenuVisiblity in MainGameUI.cpp");
		return;
	}

	const bool bShouldShow = bVisible && (not bWasHiddenByAllGameUI);
	const ESlateVisibility NewVisibility = bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden;

	MainUICanvas->SetVisibility(NewVisibility);
	SetVisibility(NewVisibility);

	if (IsValid(M_ArchiveNotificationHolder))
	{
		M_ArchiveNotificationHolder->SetVisibility(NewVisibility);
	}
	if (M_MissionManagerWidget.IsValid())
	{
		M_MissionManagerWidget->SetVisibility(NewVisibility);
	}
}

void UMainGameUI::SetMissionManagerWidget(UUserWidget* MissionManagerWidget)
{
	M_MissionManagerWidget = MissionManagerWidget;
}

bool UMainGameUI::CLoseOptionUIOnLeftClick()
{
	if (M_ActiveBxpItemForBxpOptionUI_Index == INDEX_NONE)
	{
		// nothing to close
		return false;
	}
	CloseOptionUI();
	return true;
}

void UMainGameUI::RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates,
                                     const int32 ActivePageIndex) const
{
	if (not GetIsValidBottomCenterUI())
	{
		return;
	}
	M_BottomCenterUI->RebuildSelectionUI(AllWidgetStates, ActivePageIndex);
}


void UMainGameUI::OnHideAllGameUI(const bool bHide)
{
	if (bHide)
	{
		if (bWasHiddenByAllGameUI)
		{
			return;
		}

		bWasHiddenByAllGameUI = true;

		// Hide the main HUD.
		SetMainMenuVisiblity(false);

		// Also collapse overlays that are added to the viewport separately.
		if (IsValid(M_Archive))
		{
			M_Archive->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IsValid(M_TechTree))
		{
			M_TechTree->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// Un-hide
	if (not bWasHiddenByAllGameUI)
	{
		return;
	}

	bWasHiddenByAllGameUI = false;
	SetMainMenuVisiblity(true);
}

void UMainGameUI::AddNewArchiveItem(const ERTSArchiveItem ItemType, const FTrainingOption OptionalUnit,
                                    const int32 SortingPriority, const float NotificationTime)
{
	if (not GetIsValidArchive() || not GetIsValidArchiveNotificationHolder())
	{
		TWeakObjectPtr<UMainGameUI> WeakThis(this);
		auto CallbackItem = [WeakThis, ItemType, OptionalUnit, SortingPriority, NotificationTime]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			UMainGameUI* StrongThis = WeakThis.Get();
			if (StrongThis->GetIsValidArchive())
			{
				StrongThis->AddNewArchiveItem(ItemType, OptionalUnit, SortingPriority, NotificationTime);
			}
			if (StrongThis->GetIsValidArchiveNotificationHolder())
			{
				StrongThis->M_ArchiveNotificationHolder->AddNewArchiveNotification(
					ItemType, OptionalUnit, NotificationTime);
			}
		};
		OnArchiveLoadedCallbacks.CallbackOnArchiveReady(
			CallbackItem,
			WeakThis
		);
		return;
	}
	M_Archive->AddArchiveItem(ItemType, OptionalUnit, SortingPriority);
	M_ArchiveNotificationHolder->AddNewArchiveNotification(ItemType, OptionalUnit, NotificationTime);
}

TObjectPtr<UW_MiniMap> UMainGameUI::GetIsValidMiniMap() const
{
	if (not IsValid(M_MiniMap))
	{
		RTSFunctionLibrary::ReportError("No valid mini map widget set on main menu, cannot provide valid reference.");
		return nullptr;
	}
	return M_MiniMap;
}

void UMainGameUI::RequestShowCancelBuilding(const AActor* RequestingActor) const
{
	if (not GetIsValidBottomCenterUI() || not GetIsProvidedActorPrimarySelected(RequestingActor))
	{
		return;
	}
	M_BottomCenterUI->ShowCancelBuilding();
}

void UMainGameUI::RequestShowConstructBuilding(const AActor* RequestingActor) const
{
	if (not GetIsValidBottomCenterUI() || not GetIsProvidedActorPrimarySelected(RequestingActor))
	{
		return;
	}
	M_BottomCenterUI->ShowConstructBuilding();
}

void UMainGameUI::RequestShowConvertToVehicle(const AActor* RequestingActor) const
{
	if (not GetIsValidBottomCenterUI() || not GetIsProvidedActorPrimarySelected(RequestingActor))
	{
		return;
	}
	M_BottomCenterUI->ShowConvertToVehicle();
}

void UMainGameUI::RequestShowCancelVehicleConversion(const AActor* RequestingActor) const
{
	if (not GetIsValidBottomCenterUI() || not GetIsProvidedActorPrimarySelected(RequestingActor))
	{
		return;
	}
	M_BottomCenterUI->ShowCancelVehicleConversion();
}

void UMainGameUI::OnTruckConverted(
	AActor* ConvertedTruck,
	const bool bConvertedToBuilding)
{
	if (not PrimarySelectedActor)
	{
		return;
	}
	const IBuildingExpansionOwner* BuildingExpansionOwner = Cast<IBuildingExpansionOwner>(ConvertedTruck);
	if (PrimarySelectedActor == ConvertedTruck && BuildingExpansionOwner)
	{
		for (int IndexInScrollBar = 0; IndexInScrollBar < M_TItemBuildingExpansionWidgets.Num(); ++IndexInScrollBar)
		{
			// Enable the item.
			M_TItemBuildingExpansionWidgets[IndexInScrollBar]->EnableDisableItem(bConvertedToBuilding);
		}
		if (bConvertedToBuilding)
		{
			RequestShowConvertToVehicle(ConvertedTruck);
		}
		else
		{
			RequestShowConstructBuilding(ConvertedTruck);
		}
	}
}

void UMainGameUI::RequestUpdateSpecificBuildingExpansionItem(
	AActor* RequestingActor,
	const EBuildingExpansionType BuildingExpansionType,
	const EBuildingExpansionStatus BuildingExpansionStatus,
	const FBxpConstructionRules& ConstructionRules,
	const int IndexExpansionSlot)
{
	// no errors if null; requests can be made when no actor is selected.
	if (PrimarySelectedActor && RequestingActor == PrimarySelectedActor)
	{
		// check if index is valid.
		if (IndexExpansionSlot >= 0 && IndexExpansionSlot < M_TItemBuildingExpansionWidgets.Num())
		{
			// Update the widget with the new status.
			M_TItemBuildingExpansionWidgets[IndexExpansionSlot]->UpdateItemBuildingExpansionData(
				BuildingExpansionType, BuildingExpansionStatus, ConstructionRules);
		}
		else
		{
			RTSFunctionLibrary::ReportError("IndexExpansionSlot is invalid!");
		}
	}
}

void UMainGameUI::InitBuildingExpansionUIForNewUnit(const IBuildingExpansionOwner* BuildingExpansionOwner)
{
	if (BuildingExpansionOwner)
	{
		TArray<FBuildingExpansionItem>& BuildingExpansions = BuildingExpansionOwner->GetBuildingExpansions();
		// Setup the items for the building expansion.
		SetupBuildingExpansionItems(BuildingExpansions, BuildingExpansionOwner->IsBuildingAbleToExpand());

		TArray<FBxpOptionData> TOptionsDataForBuildingExpansion = BuildingExpansionOwner->
			GetUnlockedBuildingExpansionTypes();
		// Setup the options for the building expansion.
		SetupBuildingExpansionOptions(TOptionsDataForBuildingExpansion, BuildingExpansionOwner);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Building expansion owner is null!"
			"\n at function SetupBuildingExpansionUI in MainGameUI.cpp");
	}
}

void UMainGameUI::SetupTrainingUIForNewTrainer(
	UTrainerComponent* Trainer,
	const bool bIsAbleToTrain)
{
	if (M_TrainingMenuManager)
	{
		M_TrainingMenuManager->SetupTrainingOptionsForTrainer(Trainer, bIsAbleToTrain);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"NULL: MainGameUI has no TrainingMenuManager! \n At function: SetupTrainingOptionsUI");
	}
}


void UMainGameUI::SetBottomUIPanel(const EShowBottomRightUIPanel NewBottomRightUIPanel,
                                   const EBuildingExpansionType BxpType,
                                   const bool bIsBuilt,
                                   const FTrainingWidgetState TrainingWidgetState)
{
	if (!IsValid(M_BxpDescription) || !IsValid(M_TrainingDescription))
	{
		RTSFunctionLibrary::ReportError("No valid bxp or training description found in MainGameUI!"
			"\n at function SetBottomUIPanel in MainGameUI.cpp	");
		return;
	}
	if (NewBottomRightUIPanel == EShowBottomRightUIPanel::Show_BxpDescription && M_BottomRightUIPanel ==
		EShowBottomRightUIPanel::Show_TrainingDescription)
	{
		RTSFunctionLibrary::ReportError("Impossible configuration detected!"
			"\n the bottom right UI panel is currently set to display the training description, but we attempted"
			"to display bxp description instead. This is not allowed, the training description must be closed first.");
		return;
	}
	if (M_BottomRightUIPanel == EShowBottomRightUIPanel::Show_BxpDescription && NewBottomRightUIPanel ==
		EShowBottomRightUIPanel::Show_TrainingDescription)
	{
		RTSFunctionLibrary::PrintString("Hover Training UI while bxp description is open."
			"\n will not cancel bxp");
		return;
	}
	bool bShowActionUI, bShowBxpDescription, bShowTrainingDescription;
	switch (NewBottomRightUIPanel)
	{
	case EShowBottomRightUIPanel::Show_None:
		bShowActionUI = false;
		bShowBxpDescription = false;
		bShowTrainingDescription = false;
		break;
	case EShowBottomRightUIPanel::Show_ActionUI:
		bShowActionUI = true;
		bShowBxpDescription = false;
		bShowTrainingDescription = false;
		break;
	case EShowBottomRightUIPanel::Show_TrainingDescription:
		bShowActionUI = false;
		bShowBxpDescription = false;
		bShowTrainingDescription = true;
		break;
	case EShowBottomRightUIPanel::Show_BxpDescription:
		bShowActionUI = false;
		bShowBxpDescription = true;
		bShowTrainingDescription = false;
		break;
	default:
		bShowActionUI = false;
		bShowBxpDescription = false;
		bShowTrainingDescription = false;
	}
	if (GetIsValidActionUIManager())
	{
		M_ActionUIManager->SetActionUIVisibility(bShowActionUI);
	}
	M_BxpDescription->SetBxpDescriptionVisibility(bShowBxpDescription, BxpType, bIsBuilt);
	M_TrainingDescription->SetTrainingDescriptionVisibility(bShowTrainingDescription, TrainingWidgetState);
	M_BottomRightUIPanel = NewBottomRightUIPanel;
}

void UMainGameUI::InitMainGameUI_InitActionAndWeaponUI(const FActionUIContainer& ActionUIContainerWidgets,
                                                       const FInit_WeaponUI& WeaponUIWidgets,
                                                       const FInit_ActionUI& ActionUIWidgets,
                                                       const FInit_BehaviourUI& BehaviourUIWidgets,
                                                       ACPPController* PlayerController)
{
	M_ActionUIManager = NewObject<UActionUIManager>();
	// Error check.
	if (not GetIsValidActionUIManager())
	{
		return;
	}
	// Sets the ammo picker on each weapon element as well as the reference to the weapon description widget.
        M_ActionUIManager->InitActionUIManager(
                WeaponUIWidgets.WeaponUIElements,
                this,
                ActionUIWidgets.ActionUIElementsInMenu,
                PlayerController,
                ActionUIWidgets.SelectedUnitInfo,
                WeaponUIWidgets.AmmoPicker,
                WeaponUIWidgets.WeaponDescription,
                ActionUIContainerWidgets,
                ActionUIWidgets.SelectedUnitDescription,
                ActionUIWidgets.ActionUIDescription,
                BehaviourUIWidgets);
}

void UMainGameUI::InitMainGameUI_InitBuildingUI(UW_BottomCenterUI* NewBottomCenterUI,
                                                ACPPController* PlayerController,
                                                UW_BxpDescription* BxpDescriptionWidget)
{
	M_BottomCenterUI = NewBottomCenterUI;
	if (GetIsValidBottomCenterUI())
	{
		M_BottomCenterUI->InitBottomCenterUI(this, PlayerController);

		M_TItemBuildingExpansionWidgets = M_BottomCenterUI->GetBxpItemsInItemPanel();
		M_TOptionsBuildingExpansionWidgets = M_BottomCenterUI->GetBxpOptionsInPanel();

		// Init the Buidling Expansion items.
		for (int i = 0; i < M_TItemBuildingExpansionWidgets.Num(); ++i)
		{
			M_TItemBuildingExpansionWidgets[i]->InitW_ItemBuildingExpansion(this, i);
		}
		// Init the Buidling Expansion options.
		for (int i = 0; i < M_TOptionsBuildingExpansionWidgets.Num(); ++i)
		{
			M_TOptionsBuildingExpansionWidgets[i]->InitW_OptionBuildingExpansion(this, i);
		}
	}
	if (IsValid(BxpDescriptionWidget))
	{
		M_BxpDescription = BxpDescriptionWidget;
		M_BxpDescription->SetBxpDescriptionVisibility(false);
	}
	else
	{
		RTSFunctionLibrary::ReportError("BxpDescriptionWidget is null in InitMainGameUI"
			"\n at function InitMainGameUI in MainGameUI.cpp");
	}
}

void UMainGameUI::InitMainGameUI_HideWidgets()
{
	CloseOptionUI();
	SetBottomUIPanel(EShowBottomRightUIPanel::Show_None);
	if (GetIsValidBottomCenterUI())
	{
		M_BottomCenterUI->SetBuildingUIVisibility(false);
	}
	SetTrainingUIVisibility(false);
	if (GetIsValidArchiveNotificationHolder())
	{
		// Starts with no notifications so collapse. Will auto visibility depending on notifications added.
		M_ArchiveNotificationHolder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainGameUI::SetTrainingUIVisibility(const bool bVisible)
{
	if (not GetIsValidTrainingMenuManager() || not GetIsValidArchiveNotificationHolder() || not
		GetIsValidTrainingUIBorder())
	{
		return;
	}
	if (bVisible)
	{
		M_TrainingUIBorder->SetVisibility(ESlateVisibility::Visible);
		// Regardless of how the archive notification holder wants to handle visibility due to adding/removing notifications
		// we set it to suppressed so it does not show up under the training UI.
		M_ArchiveNotificationHolder->SetSuppressAchive(true);
		return;
	}
	M_TrainingUIBorder->SetVisibility(ESlateVisibility::Hidden);
	// Unsuppress the archive notification holder so it can show up again.
	M_ArchiveNotificationHolder->SetSuppressAchive(false);
	// If we hide the training UI we always want to hide the requirement widget too.
	M_TrainingMenuManager->HideTrainingRequirement();
}

void UMainGameUI::OnHoverInfoWidget_HandleTrainingUI(const bool bIsHovering)
{
	if (bIsHovering)
	{
		if (GetIsTrainingUIVisible())
		{
			SetTrainingUIVisibility(false);
			bM_WasTrainingUIHiddenDueToHoverInfoWidget = true;
		}
		return;
	}
	if (bM_WasTrainingUIHiddenDueToHoverInfoWidget)
	{
		SetTrainingUIVisibility(true);
		bM_WasTrainingUIHiddenDueToHoverInfoWidget = false;
	}
}

bool UMainGameUI::GetIsTrainingUIVisible() const
{
	if (not GetIsValidTrainingUIBorder())
	{
		return false;
	}
	return M_TrainingUIBorder->GetVisibility() == ESlateVisibility::Visible;
}


void UMainGameUI::UpdateBuildingUIForNewUnit(const bool bShowBuildingUI,
                                             const EActionUINomadicButton NomadicButtonState,
                                             const ENomadicSubtype NomadicSubtype)
{
	if (not GetIsValidBottomCenterUI())
	{
		return;
	}
	// Do not show options UI as we refresh the building UI for a new unit.
	CloseOptionUI();
	// Set up the nomadic button and show or hide the building UI.
	M_BottomCenterUI->UpdateBuildingUIForNewUnit(bShowBuildingUI, NomadicButtonState, NomadicSubtype);
}


bool UMainGameUI::GetIsValidBottomCenterUI() const
{
	if (not IsValid(M_BottomCenterUI))
	{
		RTSFunctionLibrary::ReportError("No valid bottom center UI found in MainGameUI!"
			"\n at function GetIsValidBottomCenterUI in MainGameUI.cpp");
		return false;
	}
	return true;
}

TScriptInterface<IBuildingExpansionOwner> UMainGameUI::GetPrimaryAsBxpOwner() const
{
	if (not IsValid(PrimarySelectedActor))
	{
		return nullptr;
	}
	if (not PrimarySelectedActor->Implements<UBuildingExpansionOwner>())
	{
		RTSFunctionLibrary::ReportError("Attempted to get primary selected actor as BXP owner but does not implement"
			"interface");
		return nullptr;
	}
	TScriptInterface<IBuildingExpansionOwner> ScriptInterfaceBxp = TScriptInterface<IBuildingExpansionOwner>(
		PrimarySelectedActor);
	if (not ScriptInterfaceBxp.GetObject())
	{
		RTSFunctionLibrary::ReportError("Primary selected actor could not be set as valid object for"
			"bxp script interface!"
			"\n at function GetPrimaryAsBxpOwner in MainGameUI.cpp");
		return nullptr;
	}
	return ScriptInterfaceBxp;
}

bool UMainGameUI::GetIsValidPlayerController() const
{
	if (IsValid(M_PlayerController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player controller is null in GetIsValidPlayerController!"
		"\n at function GetIsValidPlayerController in MainGameUI.cpp");
	return false;
}

bool UMainGameUI::GetIsValidArchive() const
{
	if (IsValid(M_Archive))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Archive is null in GetIsValidArchive!"
		"\n at function GetIsValidArchive in MainGameUI.cpp");
	return false;
}

bool UMainGameUI::GetIsValidArchiveNotificationHolder() const
{
	if (IsValid(M_ArchiveNotificationHolder))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("ArchiveNotificationHolder is null in GetIsValidArchiveNotificationHolder!"
		"\n at function GetIsValidArchiveNotificationHolder in MainGameUI.cpp");
	return false;
}

void UMainGameUI::SetupBuildingExpansionItems(
	TArray<FBuildingExpansionItem>& TBuildingExpansions,
	const bool bAreBxpItemsEnabled)
{
	if (TBuildingExpansions.Num() > M_TItemBuildingExpansionWidgets.Num())
	{
		RTSFunctionLibrary::ReportError("Amount BXP Items do not fit scrollbar! Adjusting...");
		// Too many items for this ExpansionOwner, remove until same size.
		while (TBuildingExpansions.Num() > M_TItemBuildingExpansionWidgets.Num())
		{
			// remove last element.
			TBuildingExpansions.Pop();
		}
	}
	int IndexInScrollBar = -1;
	for (const auto& [Expansion, ExpansionType, ExpansionStatus, BxpConstructionRules] : TBuildingExpansions)
	{
		++IndexInScrollBar;
		// Show the item.
		M_TItemBuildingExpansionWidgets[IndexInScrollBar]->SetVisibility(ESlateVisibility::Visible);
		// Update this item widget with the new data also updates the UI in the blueprint.
		M_TItemBuildingExpansionWidgets[IndexInScrollBar]->UpdateItemBuildingExpansionData(
			ExpansionType, ExpansionStatus, BxpConstructionRules);
		// Adjust the item to be enabled or disabled.
		M_TItemBuildingExpansionWidgets[IndexInScrollBar]->EnableDisableItem(bAreBxpItemsEnabled);
	}
	// Hide the rest of the items.
	for (int i = IndexInScrollBar + 1; i < M_TItemBuildingExpansionWidgets.Num(); ++i)
	{
		M_TItemBuildingExpansionWidgets[i]->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UMainGameUI::SetupBuildingExpansionOptions(TArray<FBxpOptionData>& TOptionsDataForBuildingExpansion,
                                                const IBuildingExpansionOwner* BxpOwner)
{
	if (not BxpOwner)
	{
		return;
	}
	if (not GetIsValidBottomCenterUI())
	{
		return;
	}

	// If the flat array was used before section init, keep legacy guard:
	if (TOptionsDataForBuildingExpansion.Num() > M_TOptionsBuildingExpansionWidgets.Num())
	{
		RTSFunctionLibrary::ReportError("Amount BXP Options do not fit scrollbar! Adjusting...");
		while (TOptionsDataForBuildingExpansion.Num() > M_TOptionsBuildingExpansionWidgets.Num())
		{
			TOptionsDataForBuildingExpansion.Pop();
		}
	}

	M_BottomCenterUI->SetupBxpOptionsPerSection(TOptionsDataForBuildingExpansion, BxpOwner);
}


void UMainGameUI::ClickedItemBuildingExpansion(
	const EBuildingExpansionType BuildingExpansionType,
	const EBuildingExpansionStatus BuildingExpansionStatus,
	const FBxpConstructionRules& ConstructionRules,
	const int IndexItemExpansionSlot)
{
	switch (BuildingExpansionStatus)
	{
	case EBuildingExpansionStatus::BXS_UnlockedButNotBuild:
		if (GetIsPrimarySelectedAbleToExpand())
		{
			OpenOptionUI(IndexItemExpansionSlot);
		}
		break;
	case EBuildingExpansionStatus::BXS_LookingForPlacement:
		// Stop the preview for not-unpacking.
		CancelBuildingExpansionPlacement(false, BuildingExpansionType);
		break;
	case EBuildingExpansionStatus::BXS_BeingBuild:
		// Regular expansion building, clean cancel.
		CancelBuildingExpansionConstruction(IndexItemExpansionSlot, false);
		break;
	case EBuildingExpansionStatus::BXS_Built:
		SetBottomUIPanel(EShowBottomRightUIPanel::Show_BxpDescription, BuildingExpansionType, true);
		MoveCameraToBuildingExpansion(IndexItemExpansionSlot);
		break;
	case EBuildingExpansionStatus::BXS_BeingPackedUp:
		MoveCameraToBuildingExpansion(IndexItemExpansionSlot);
		break;
	case EBuildingExpansionStatus::BXS_PackedUp:
		if (GetIsPrimarySelectedAbleToExpand())
		{
			PlacePackedUpBuildingExpansion(IndexItemExpansionSlot, BuildingExpansionType, ConstructionRules);
		}
		break;
	case EBuildingExpansionStatus::BXS_LookingForUnpackingLocation:
		// Stop the preview for unpacking.
		CancelBuildingExpansionPlacement(true, BuildingExpansionType);
		break;
	case EBuildingExpansionStatus::BXS_BeingUnpacked:
		// Stop the unpacking; destroy bxp, save the type and set status to packed.
		CancelBuildingExpansionConstruction(IndexItemExpansionSlot, true);
		break;
	case EBuildingExpansionStatus::BXS_NotUnlocked:
		RTSFunctionLibrary::ReportError("This building expansion is not unlocked, something went wrong!");
		break;
	default:
		break;
	}
}

void UMainGameUI::ClickedOptionBuildingExpansion(
	const FBxpOptionData& BxpOptionConstructionAndTypeData,
	const int IndexOptionExpansionSlot)
{
	if (not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor))
	{
		RTSFunctionLibrary::ReportError("PrimarySelectedActor is nullptr in ClickedBuildingExpansionOption"
			"\n The primary actor may also be RTS invalid!");
	}

	const TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner = GetPrimaryAsBxpOwner();
	if (BuildingExpansionOwner)
	{
		// Clean create a bxp, no unpacking.
		M_PlayerController->ExpandBuildingWithBxpAndStartPreview(BxpOptionConstructionAndTypeData,
		                                                         BuildingExpansionOwner,
		                                                         M_ActiveBxpItemForBxpOptionUI_Index,
		                                                         false);
		CloseOptionUI();
	}
}


bool UMainGameUI::GetIsPrimarySelectedAbleToExpand() const
{
	if (not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor))
	{
		return false;
	}

	if (const IBuildingExpansionOwner* Building = Cast<IBuildingExpansionOwner>(PrimarySelectedActor))
	{
		return Building && Building->IsBuildingAbleToExpand();
	}
	RTSFunctionLibrary::ReportError("Could not cast to bxp owner in GetIsPrimarySelectedAbleToExpand!"
		"\n At MainGameUI::GetIsPrimarySelectedAbleToExpand."
		"\n Primary actor: " + PrimarySelectedActor->GetName());
	return false;
}

bool UMainGameUI::GetIsProvidedActorPrimarySelected(const AActor* RequestingActor) const
{
	if (not IsValid(PrimarySelectedActor))
	{
		return false;
	}
	return RequestingActor == PrimarySelectedActor;
}


void UMainGameUI::CancelBuildingExpansionConstruction(
	const int IndexItemExpansionSlot,
	const bool bIsCancelledPackedBxp) const
{
	if (not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor) || not GetIsValidPlayerController())
	{
		return;
	}
	if (const TScriptInterface<IBuildingExpansionOwner> BxpOwner = GetPrimaryAsBxpOwner())
	{
		ABuildingExpansion* BuildingExpansion = BxpOwner->GetBuildingExpansionAtIndex(IndexItemExpansionSlot);
		M_PlayerController->CancelBuildingExpansionConstruction(BxpOwner, BuildingExpansion,
		                                                        bIsCancelledPackedBxp);
		return;
	}
	const FString PrimarySelectedActorName =
		IsValid(PrimarySelectedActor) ? PrimarySelectedActor->GetName() : "Nullptr";
	RTSFunctionLibrary::ReportError(
		"Could not cast to bxp owner in CancelBuildingExpansionConstruction!"
		"\n At MainGameUI::CancelBuildingExpansionConstruction."
		"\n Primary actor: " + PrimarySelectedActorName);
}

void UMainGameUI::MoveCameraToBuildingExpansion(const int IndexExpansionSlot) const
{
	const TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner = GetPrimaryAsBxpOwner();
	if (not BuildingExpansionOwner)
	{
		const FString PrimarySelectedActorName =
			IsValid(PrimarySelectedActor) ? PrimarySelectedActor->GetName() : "nullptr";
		RTSFunctionLibrary::ReportError(
			"Could not cast to bxp owner in MoveCameraToBuildingExpansion!"
			"\n At MainGameUI::MoveCameraToBuildingExpansion."
			"\n Primary actor: " + PrimarySelectedActorName);
		return;
	}

	const ABuildingExpansion* Expansion = BuildingExpansionOwner->GetBuildingExpansionAtIndex(IndexExpansionSlot);
	if (Expansion)
	{
		M_PlayerController->FocusCameraOnActor(Expansion);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Obtained null expansion from building expansion owner  \n at function UMainGameUI::MoveCameraToBuildingExpansion");
	}
}

void UMainGameUI::CancelBuildingExpansionPlacement(const bool bIsCancelledPackedBxp,
                                                   const EBuildingExpansionType BxpType)
{
	if (not GetIsValidPlayerController() || not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor))
	{
		return;
	}
	if (const TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner = GetPrimaryAsBxpOwner())
	{
		CloseOptionUI();
		// Destroys the building expansion and stops preview mode.
		M_PlayerController->OnClickedCancelBxpPlacement(BuildingExpansionOwner, bIsCancelledPackedBxp, BxpType);
		return;
	}
	RTSFunctionLibrary::ReportError(
		"Could not cast to bxp owner in CancelBuildingExpansionPlacement!"
		"\n At MainGameUI::CancelBuildingExpansionPlacement."
		"\n Primary actor: " + PrimarySelectedActor->GetName());
}

void UMainGameUI::OpenOptionUI(const int ActiveItemIndex)
{
	if (ActiveItemIndex == M_ActiveBxpItemForBxpOptionUI_Index)
	{
		RTSFunctionLibrary::PrintString("ATTEMPTED open bxp option but option is already open!");
		return;
	}

	if (not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor) || not GetIsValidPlayerController())
	{
		return;
	}
	// There is possibly another item active, cancel the preview.
	if (const TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner = GetPrimaryAsBxpOwner())
	{
		bool bIsUnpacking = false;
		const EBuildingExpansionType TypeOfActivePlacement = BuildingExpansionOwner->GetTypeOfBxpLookingForPlacement(
			bIsUnpacking);
		M_PlayerController->OnClickedCancelBxpPlacement(BuildingExpansionOwner, bIsUnpacking, TypeOfActivePlacement);
		// Update the option UI with possibly new sockets being used or unique Bxps being placed.
		TArray<FBxpOptionData> BxpOptions = BuildingExpansionOwner->GetUnlockedBuildingExpansionTypes();
		SetupBuildingExpansionOptions(BxpOptions, BuildingExpansionOwner.GetInterface());
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Could not cast to bxp owner in OpenOptionUI!"
			"\n At MainGameUI::OpenOptionUI."
			"\n Primary actor: " + PrimarySelectedActor->GetName());
	}
	M_ActiveBxpItemForBxpOptionUI_Index = ActiveItemIndex;
	if (GetIsValidBottomCenterUI())
	{
		M_BottomCenterUI->SetBuildingOptionUIVisibility(true);
	}
}


void UMainGameUI::CloseOptionUI()
{
	// When we close the option UI make sure there is no longer an active index used for it.
	M_ActiveBxpItemForBxpOptionUI_Index = INDEX_NONE;
	if (GetIsValidBottomCenterUI())
	{
		M_BottomCenterUI->SetBuildingOptionUIVisibility(false);
	}
}

void UMainGameUI::PlacePackedUpBuildingExpansion(
	const int IndexExpansionSlot,
	const EBuildingExpansionType BuildingExpansionType,
	const FBxpConstructionRules& ConstructionRules) const
{
	if (not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor) || not GetIsValidPlayerController())
	{
		return;
	}
	const TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner = GetPrimaryAsBxpOwner();
	if (not BuildingExpansionOwner)
	{
		RTSFunctionLibrary::ReportError(
			"Could not cast Primary selected actor to bxp owner in PlacePackedUpBuildingExpansion!"
			"\n At MainGameUI::PlacePackedUpBuildingExpansion."
			"\n Primary actor: " + PrimarySelectedActor->GetName());
		return;
	}
	const bool bIsSocketExpansion = ConstructionRules.ConstructionType == EBxpConstructionType::Socket;
	const bool bIsOriginexpansion = ConstructionRules.ConstructionType == EBxpConstructionType::AtBuildingOrigin;
	const bool bIsAttached = bIsOriginexpansion || bIsSocketExpansion;
	if (bIsAttached && BuildingExpansionOwner->GetIsAsyncBatchLoadingAttachedPackedExpansions())
	{
		if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::DisplayNotification(FText::FromString("Skipping user manually attempting to place"
				"packed bxp that is attached as this bxp is currently being"
				"async loaded in a batch."));
		}
		return;
	}
	FBxpOptionData TypeAndConstructionRules;
	TypeAndConstructionRules.ExpansionType = BuildingExpansionType;
	TypeAndConstructionRules.BxpConstructionRules = ConstructionRules;
	M_PlayerController->RebuildPackedExpansion(TypeAndConstructionRules, BuildingExpansionOwner, IndexExpansionSlot);
}

bool UMainGameUI::GetIsValidTrainingMenuManager() const
{
	if (not IsValid(M_TrainingMenuManager))
	{
		RTSFunctionLibrary::ReportError(
			"No valid training menu manager set on main menu, cannot provide valid reference."
			"\n Failed to create the UObject for it? Check MainGameUI INIT");
		return false;
	}
	return true;
}

bool UMainGameUI::GetIsValidTrainingUIBorder() const
{
	if (not IsValid(M_TrainingUIBorder))
	{
		RTSFunctionLibrary::ReportError("No valid training UI border set on main menu, cannot provide valid reference."
			"\n Failed to create the UObject for it? Check MainGameUI INIT");
		return false;
	}
	return true;
}

bool UMainGameUI::GetIsValidActionUIManager() const
{
	if (not IsValid(M_ActionUIManager))
	{
		RTSFunctionLibrary::ReportError("No valid action UI manager set on main menu, cannot provide valid reference."
			"\n Failed to create the UObject for it? Check MainGameUI INIT");
		return false;
	}
	return true;
}

void UMainGameUI::SetupResources(
	TArray<UW_PlayerResource*>& TResourceWidgets,
	const ACPPController* NewPlayerController, UW_PlayerEnergyBar* NewPlayerEnergyBar, UW_EnergyBarInfo*
	NewPlayerEnergyBarInfo) const
{
	TArray<ERTSResourceType> ResourcesLeftToRight = {
		ERTSResourceType::Resource_Radixite,
		ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts,
	};
	UPlayerResourceManager* PlayerResourceManager = nullptr;
	if (IsValid(NewPlayerController))
	{
		PlayerResourceManager = NewPlayerController->GetPlayerResourceManager();
	}
	else
	{
		RTSFunctionLibrary::ReportError("Player controller is null!"
			"\n at function SetupResources in MainGameUI.cpp");
		return;
	}

	if (!IsValid(PlayerResourceManager))
	{
		RTSFunctionLibrary::ReportError("Player controller has no resource manager!!"
			"at function SetupResources in MainGameUI.cpp");
		return;
	}
	if (IsValid(NewPlayerEnergyBar))
	{
		NewPlayerEnergyBar->InitPlayerEnergyBar(PlayerResourceManager, NewPlayerEnergyBarInfo);
	}
	else
	{
		RTSFunctionLibrary::ReportError("PlayerEnergyBar is null in SetupResources"
			"\n at function SetupResources in MainGameUI.cpp");
	}
	if (TResourceWidgets.Num() != ResourcesLeftToRight.Num())
	{
		RTSFunctionLibrary::ReportError("Amount of resources does not match the amount of widgets!"
			"\n See void UMainGameUI::SetupResources"
			"\n Amount of resources: " + FString::FromInt(ResourcesLeftToRight.Num()) +
			"\n Amount of widgets: " + FString::FromInt(TResourceWidgets.Num()));
	}
	else
	{
		for (int i = 0; i < TResourceWidgets.Num(); ++i)
		{
			TResourceWidgets[i]->InitUwPlayerResource(PlayerResourceManager, ResourcesLeftToRight[i]);
		}
	}
}

void UMainGameUI::PropagateControlGroupsToPlayer(UW_ControlGroups* ControlGroupsWidget) const
{
	if (not IsValid(ControlGroupsWidget))
	{
		RTSFunctionLibrary::ReportError("no valid controlgroups widget provided!"
			"\n Cannot setup the control groups in the player controller."
			"\n see function PropagateControlGroupsToPlayer in MainGameUI.cpp");
		return;
	}
	if (UPlayerControlGroupManager* Manager = FRTS_Statics::GetPlayerControlGroupManager(M_PlayerController))
	{
		Manager->SetControlGroupsWidget(ControlGroupsWidget);
	}
}

bool UMainGameUI::EnsureMiniMapIsValid() const
{
	if (not IsValid(M_MiniMap))
	{
		RTSFunctionLibrary::ReportError("Invalid minimap on MainGameUI!"
			"\n See function EnsureMiniMapIsValid in MainGameUI.cpp");
		return false;
	}
	return true;
}


bool UMainGameUI::SetupWeaponUIForSelected(AActor* PrimarySelected)
{
	if (not GetIsValidActionUIManager())
	{
		return false;
	}
	return M_ActionUIManager->SetupWeaponUIForSelectedActor(PrimarySelected);
}

bool UMainGameUI::SetupUnitActionUIForUnit(
        const TArray<FUnitAbilityEntry>& TAbilities,
        EAllUnitType PrimaryUnitType,
        AActor* PrimarySelected,
        const ENomadicSubtype NomadicSubtype,
        const ETankSubtype TankSubtype,
        const ESquadSubtype SquadSubtype,
	const EBuildingExpansionType BxpSubtype) const
{
	if (not GetIsValidActionUIManager())
	{
		return false;
	}
	// If this returns false then all abilities are none and we need to hide the action UI.
	return M_ActionUIManager->SetUpActionUIForSelectedActor(TAbilities, PrimaryUnitType, NomadicSubtype,
	                                                        TankSubtype, SquadSubtype, BxpSubtype, PrimarySelected);
}

void UMainGameUI::UpdateActionUIForNewUnit(const FActionUIParameters& ActionUIParameters,
                                           const TObjectPtr<AActor>& NewPrimarySelectedActor)
{
	if (not GetIsValidActionUIManager())
	{
		return;
	}
	PrimarySelectedActor = NewPrimarySelectedActor;
	// Hide action UI, bxp description, and training Description.
	SetBottomUIPanel(EShowBottomRightUIPanel::Show_None);
	M_ActionUIManager->HideAllHoverInfoWidgets();
	M_ActionUIManager->HideAmmoPicker();


	UpdateBuildingUIForNewUnit(ActionUIParameters.bShowBuildingUI, ActionUIParameters.NomadicButtonState,
	                           ActionUIParameters.NomadicSubtype);

	SetTrainingUIVisibility(ActionUIParameters.bShowTrainingUI);

	if (IsValid(PrimarySelectedActor))
	{
		SetBottomUIPanel(EShowBottomRightUIPanel::Show_ActionUI);
	}
	else
	{
		SetBottomUIPanel(EShowBottomRightUIPanel::Show_None);
	}
}

UTrainingMenuManager* UMainGameUI::GetTrainingMenuManager() const
{
	return M_TrainingMenuManager;
}

void UMainGameUI::InitMiniMap(const TObjectPtr<AFowManager>& FowManager, const EMiniMapStartDirection StartDirection)
{
	if (not IsValid(FowManager))
	{
		RTSFunctionLibrary::ReportError("Cannot setup mini map with render targets as FOW manager is InValid!"
			"\n See UMainGameUI::InitMiniMap");
		return;
	}
	if (not EnsureMiniMapIsValid() || not M_MiniMap->GetIsValidMiniMapImg())
	{
		return;
	}
	M_MiniMap->InitMiniMapRTs(FowManager->GetIsValidActiveRT(), FowManager->GetIsValidPassiveRT());
	M_MiniMap->SetRenderTransform(Global_GetMiniMapTransform(StartDirection, M_MiniMap->GetRenderTransform()));
}

void UMainGameUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnArchiveLoadedCallbacks.SetMainMenu(this);
}


void UMainGameUI::LoadTechTree()
{
	if (!TechTreeClass.IsNull())
	{
		if (TechTreeClass.IsValid())
		{
			// The class is already loaded, create the widget instantly.
			OnTechTreeClassLoaded();
		}
		else
		{
			// Load the class asynchronously
			FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
			StreamableManager.RequestAsyncLoad(TechTreeClass.ToSoftObjectPath(),
			                                   FStreamableDelegate::CreateUObject(
				                                   this, &UMainGameUI::OnTechTreeClassLoaded));
		}
		return;
	}
	RTSFunctionLibrary::ReportError("TechTree Class soft pointer is not set in MainGameUI!");
}

void UMainGameUI::OnTechTreeClassLoaded()
{
	if (not TechTreeClass.IsValid())
	{
		RTSFunctionLibrary::ReportError("failed to load TechTree class in MainGameUI!");
		return;
	}
	UTechTree* TechTreeWidget = CreateWidget<UTechTree>(GetWorld(), TechTreeClass.Get());
	AddTechTreeToViewport(TechTreeWidget);
}

void UMainGameUI::LoadArchiveMenu()
{
	if (not ArchiveClass.IsNull())
	{
		if (ArchiveClass.IsValid())
		{
			// The class is already loaded, create the widget instantly.
			OnArchiveClassLoaded();
		}
		else
		{
			// Load the class asynchronously
			FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
			StreamableManager.RequestAsyncLoad(ArchiveClass.ToSoftObjectPath(),
			                                   FStreamableDelegate::CreateUObject(
				                                   this, &UMainGameUI::OnArchiveClassLoaded));
		}
		return;
	}
	RTSFunctionLibrary::ReportError("Archive Class soft pointer is not set in MainGameUI!");
}

void UMainGameUI::OnArchiveClassLoaded()
{
	if (not ArchiveClass.IsValid())
	{
		RTSFunctionLibrary::ReportError("failed to load Archive class in MainGameUI!");
		return;
	}
	UW_Archive* ArchiveWidget = CreateWidget<UW_Archive>(GetWorld(), ArchiveClass.Get());
	AddArchiveToViewPort(ArchiveWidget);
}

void UMainGameUI::AddArchiveToViewPort(UW_Archive* ArchiveWidget)
{
	if (not ArchiveWidget)
	{
		RTSFunctionLibrary::ReportError("ArchiveWidget is null in AddArchiveToViewPort"
			"\n at function AddArchiveToViewPort in MainGameUI.cpp");
		return;
	}
	ArchiveWidget->AddToViewport(500);
	M_Archive = ArchiveWidget;
	M_Archive->SetMainGameUIReference(this);
	OnArchiveLoadedCallbacks.OnArchiveReady.Broadcast();
	// Hide archive.
	M_Archive->SetVisibility(ESlateVisibility::Collapsed);
}

void UMainGameUI::AddTechTreeToViewport(UTechTree* TechTreeWidget)
{
	if (not TechTreeWidget)
	{
		RTSFunctionLibrary::ReportError("TechTreeWidget is null in AddTechTreeToViewport"
			"\n at function AddTechTreeToViewport in MainGameUI.cpp");
		return;
	}
	TechTreeWidget->AddToViewport(1000);
	// Disable world rendering
	UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
	if (ViewportClient)
	{
		ViewportClient->bDisableWorldRendering = true;
	}
	SetMainMenuVisiblity(false);
	TechTreeWidget->SetMainGameUI(this);
	M_TechTree = TechTreeWidget;
}

void UMainGameUI::OnCloseTechTree()
{
	if (IsValid(M_PlayerController))
	{
		M_PlayerController->SetIsPlayerInTechTree(false);
	}
	SetMainMenuVisiblity(true);
}

void UMainGameUI::OnOpenArchive()
{
	if (not GetIsValidArchive() || not GetIsValidPlayerController())
	{
		return;
	}
	if (bWasHiddenByAllGameUI)
	{
		return;
	}

	M_PlayerController->SetIsPlayerInArchive(true);
	SetMainMenuVisiblity(false);
	M_Archive->SetVisibility(ESlateVisibility::Visible);
	M_Archive->OnOpenArchive();
}

void UMainGameUI::OnCloseArchive()
{
	if (not GetIsValidArchive() || not GetIsValidPlayerController())
	{
		return;
	}
	M_PlayerController->SetIsPlayerInArchive(false);
	// Hide archive.
	M_Archive->SetVisibility(ESlateVisibility::Collapsed);
	// Show menu.
	SetMainMenuVisiblity(true);
}

void UMainGameUI::OnHoverWeaponItem(const bool bIsHovering)
{
	OnHoverInfoWidget_HandleTrainingUI(bIsHovering);
}

void UMainGameUI::OnHoverActionUIItem(const bool bIsHovering)
{
	OnHoverInfoWidget_HandleTrainingUI(bIsHovering);
}

void UMainGameUI::OnHoverSelectedUnitInfo(const bool bIsHovering)
{
	OnHoverInfoWidget_HandleTrainingUI(bIsHovering);
}

void UMainGameUI::InitMainGameUI(
	FActionUIContainer ActionUIContainerWidgets,
	FInit_ActionUI ActionUIWidgets,
	FInit_WeaponUI WeaponUIWidgets,
	UW_BxpDescription* NewBxpDescriptionWidget,
	ACPPController* NewPlayerController,
	UBorder* NewTrainingUIBorder,
	const TArray<UW_TrainingItem*> NewTrainingItemWidgets,
	UW_TrainingRequirement* TrainingRequirementWidget,
	TArray<UW_PlayerResource*> NewPlayerResourceWidgets,
	UW_PlayerEnergyBar* NewPlayerEnergyBar,
	UW_EnergyBarInfo* NewPlayerEnergyBarInfo,
	UW_TrainingDescription* NewTrainingDescription,
	UW_ControlGroups* NewControlGroups,
	UW_ArchiveNotificationHolder* NewArchiveNotificiationHolder, UW_BottomCenterUI* NewBottomCenterUI,
	UW_Portrait* NewPortrait,
	FInit_BehaviourUI BehaviourUIWidgets)

{
	// M_TItemBuildingExpansionWidgets = NewBuildingExpansionWidgets;
	// M_TOptionsBuildingExpansionWidgets = NewBuildingExpansionOptions;
	M_PlayerController = NewPlayerController;
	if(M_PlayerController)
	{
		M_PlayerController->InitPortrait(NewPortrait);
	}
	InitMainGameUI_InitBuildingUI(NewBottomCenterUI, NewPlayerController, NewBxpDescriptionWidget);
	M_ControlGroups = NewControlGroups;

	M_TrainingMenuManager = NewObject<UTrainingMenuManager>(this);
	if (M_TrainingMenuManager)
	{
		M_TrainingMenuManager->InitTrainingManager(NewTrainingItemWidgets, this, TrainingRequirementWidget,
		                                           NewTrainingDescription);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"NULL: MainGameUI failed to create TrainingMenuManager! \n At function: InitMainGameUI");
	}
	InitMainGameUI_InitActionAndWeaponUI(
		ActionUIContainerWidgets,
		WeaponUIWidgets, ActionUIWidgets, BehaviourUIWidgets, NewPlayerController);

	if (IsValid(NewTrainingDescription))
	{
		M_TrainingDescription = NewTrainingDescription;
	}
	else
	{
		RTSFunctionLibrary::ReportError("NewTrainingDescription is null in InitMainGameUI"
			"\n at function InitMainGameUI in MainGameUI.cpp");
	}
	M_TrainingUIBorder = NewTrainingUIBorder;
	(void)GetIsValidTrainingUIBorder();
	M_ArchiveNotificationHolder = NewArchiveNotificiationHolder;
	// error check.
	if (GetIsValidArchiveNotificationHolder())
	{
		M_ArchiveNotificationHolder->SetMainMenuReference(this);
	}
	InitMainGameUI_HideWidgets();

	SetupResources(NewPlayerResourceWidgets, NewPlayerController, NewPlayerEnergyBar, NewPlayerEnergyBarInfo);
	PropagateControlGroupsToPlayer(NewControlGroups);
	// Start async loading archive.
	LoadArchiveMenu();
}
