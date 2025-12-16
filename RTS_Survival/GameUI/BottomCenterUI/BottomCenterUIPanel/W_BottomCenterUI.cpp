// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BottomCenterUI.h"

#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BuildingUI_ItemPanel/W_BuildingUI_ItemPanel.h"
#include "RTS_Survival/GameUI/BottomCenterUI/BuildingUI_OptionPanel/W_BuildingUI_OptionPanel.h"
#include "RTS_Survival/GameUI/BottomCenterUI/SelectionUI/W_SelectionPanel/W_SelectionPanel.h"
#include "RTS_Survival/GameUI/BuildingExpansion/W_OptionBuildingExpansion.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	/** How many visible 'used' slots this section needs (clamped to capacity). */
	static int32 ComputeUsedForSection(
		const TMap<EBxpOptionSection, TArray<FBxpOptionData>>& OptionsBySection,
		const UW_BuildingUI_OptionPanel* OptionPanel,
		const EBxpOptionSection Section)
	{
		const TArray<FBxpOptionData>& SecOpts = OptionsBySection.FindRef(Section);
		if (SecOpts.Num() == 0 || not IsValid(OptionPanel))
		{
			return 0;
		}
		const TArray<UW_OptionBuildingExpansion*> Slots = OptionPanel->GetSectionSlots(Section);
		return FMath::Min(SecOpts.Num(), Slots.Num());
	}

	/** Set visibility on a contiguous range [Start, End) of slots (safe-checked). */
	static void SetSlotVisibilityRange(
		const TArray<UW_OptionBuildingExpansion*>& Slots,
		const int32 Start,
		const int32 End,
		const ESlateVisibility NewVis)
	{
		const int32 SafeStart = FMath::Clamp(Start, 0, Slots.Num());
		const int32 SafeEnd = FMath::Clamp(End, 0, Slots.Num());
		for (int32 i = SafeStart; i < SafeEnd; ++i)
		{
			if (IsValid(Slots[i]))
			{
				Slots[i]->SetVisibility(NewVis);
			}
		}
	}

	/** Fill first UsedCount slots as visible and update their data. */
	static void FillUsedSlots(
		const TArray<FBxpOptionData>& SectionOptions,
		const TArray<UW_OptionBuildingExpansion*>& Slots,
		const int32 UsedCount,
		const IBuildingExpansionOwner* BxpOwner,
		const bool bHasFreeSockets)
	{
		const int32 Count = FMath::Min3(UsedCount, SectionOptions.Num(), Slots.Num());
		for (int32 i = 0; i < Count; ++i)
		{
			UW_OptionBuildingExpansion* const Slot = Slots[i];
			if (not IsValid(Slot))
			{
				RTSFunctionLibrary::ReportError("Invalid UW_OptionBuildingExpansion slot while populating section.");
				continue;
			}
			const FBxpOptionData& Data = SectionOptions[i];
			const bool bHasBxpOfType = BxpOwner ? BxpOwner->HasBxpItemOfType(Data.ExpansionType) : false;

			Slot->SetVisibility(ESlateVisibility::Visible);
			Slot->UpdateOptionBuildingExpansion(Data, bHasFreeSockets, bHasBxpOfType);
		}
	}
} // namespace

void UW_BottomCenterUI::RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, const int32 ActivePageIndex)
{
        if (not EnsureIsValidSelectionPanel())
        {
                return;
        }

        const bool bHasSelection = AllWidgetStates.Num() > 0;
        const ESlateVisibility DesiredVisibility = bHasSelection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        SelectionPanel->SetVisibility(DesiredVisibility);
        if (not bHasSelection)
        {
                return;
        }

        SelectionPanel->RebuildSelectionUI(AllWidgetStates, ActivePageIndex);
}


void UW_BottomCenterUI::InitBottomCenterUI(UMainGameUI* MainGameUI, ACPPController* PlayerController)
{
	if (EnsureIsValidChildPanels())
	{
		BuildingUI_ItemPanel->SetParentWidget(this);
		BuildingUI_OptionPanel->SetParentWidget(this);
	}
	M_MainGameUI = MainGameUI;
	M_PlayerController = PlayerController;
	(void)EnsureMainGameUIIsValid();
	(void)EnsureIsValidPlayerController();
	if(EnsureIsValidSelectionPanel())
	{
		SelectionPanel->InitMainGameUI(PlayerController);
	}

	CacheOptionPanelBaseYOffset();
}

void UW_BottomCenterUI::SetupBxpOptionsPerSection(const TArray<FBxpOptionData>& Options,
                                                  const IBuildingExpansionOwner* BxpOwner)
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	if (not BxpOwner)
	{
		RTSFunctionLibrary::ReportError("Null BxpOwner in SetupBxpOptionsPerSection.");
		return;
	}

	const bool bHasFreeSockets = not BxpOwner->GetFreeSocketList(INDEX_NONE).IsEmpty();

	// 1) Group options by section
	TMap<EBxpOptionSection, TArray<FBxpOptionData>> OptionsBySection;
	SetupBxpOptionsPerSection_GroupBySection(Options, OptionsBySection);

	// 2) Compute used counts and the maximum used across sections
	const int32 Used_Tech =
		ComputeUsedForSection(OptionsBySection, BuildingUI_OptionPanel, EBxpOptionSection::BOS_Tech);
	const int32 Used_Economic = ComputeUsedForSection(OptionsBySection, BuildingUI_OptionPanel,
	                                                  EBxpOptionSection::BOS_Economic);
	const int32 Used_Defense = ComputeUsedForSection(OptionsBySection, BuildingUI_OptionPanel,
	                                                 EBxpOptionSection::BOS_Defense);
	const int32 MaxUsed = FMath::Max3(Used_Tech, Used_Economic, Used_Defense);

	// 3) Populate per-section with equalized height logic
	auto EqualizeAndPopulate = [&](const EBxpOptionSection Section, const int32 UsedCount)
	{
		TArray<UW_OptionBuildingExpansion*> Slots = BuildingUI_OptionPanel->GetSectionSlots(Section);
		const TArray<FBxpOptionData>& SecOpts = OptionsBySection.FindRef(Section);

		if (UsedCount == 0)
		{
			BuildingUI_OptionPanel->SetSectionCollapsed(Section, true);
			SetSlotVisibilityRange(Slots, 0, Slots.Num(), ESlateVisibility::Hidden);
			return;
		}

		BuildingUI_OptionPanel->SetSectionCollapsed(Section, false);

		if (SecOpts.Num() > Slots.Num())
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Not enough BXP option slots in section for %d options; have %d."),
				SecOpts.Num(), Slots.Num()));
		}
		if (MaxUsed > Slots.Num())
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Section lacks slots to match equalized height: need %d, have %d."),
				MaxUsed, Slots.Num()));
		}

		// Visible (used)
		FillUsedSlots(SecOpts, Slots, UsedCount, BxpOwner, bHasFreeSockets);
		// Hidden padding up to MaxUsed
		SetSlotVisibilityRange(Slots, UsedCount, FMath::Min(MaxUsed, Slots.Num()), ESlateVisibility::Hidden);
		// Collapse everything beyond MaxUsed
		SetSlotVisibilityRange(Slots, MaxUsed, Slots.Num(), ESlateVisibility::Collapsed);
	};

	EqualizeAndPopulate(EBxpOptionSection::BOS_Tech, Used_Tech);
	EqualizeAndPopulate(EBxpOptionSection::BOS_Economic, Used_Economic);
	EqualizeAndPopulate(EBxpOptionSection::BOS_Defense, Used_Defense);

	// 4) Compute how many rows we collapsed across ALL sections (from MaxUsed to end).
	const auto CountCollapsedAfterMax = [&](const EBxpOptionSection Section) -> int32
	{
		const int32 TotalSlots = BuildingUI_OptionPanel->GetSectionSlots(Section).Num();
		return TotalSlots > MaxUsed ? (TotalSlots - MaxUsed) : 0;
	};
	const int32 CollapsedRowsTotal =
		CountCollapsedAfterMax(EBxpOptionSection::BOS_Tech) +
		CountCollapsedAfterMax(EBxpOptionSection::BOS_Economic) +
		CountCollapsedAfterMax(EBxpOptionSection::BOS_Defense);

	// 5) Convert collapsed rows to a delta (negative when we collapsed) and adjust Y offset.
	if (M_OptionRowHeight <= 0.f)
	{
		RTSFunctionLibrary::ReportError("M_OptionRowHeight is not set (<= 0). Please set in Blueprint.");
	}

	const float DeltaYFromBase = CollapsedRowsTotal == 0
		                             ? 0.f
		                             : -static_cast<float>(CollapsedRowsTotal) * FMath::Max(0.f, M_OptionRowHeight);

	AdjustOptionPanelYOffsetByDelta(DeltaYFromBase);
}

void UW_BottomCenterUI::AdjustOptionPanelYOffsetByDelta(const float DeltaYFromBase)
{
	CacheOptionPanelBaseYOffset(); // make sure base is cached once

	UCanvasPanelSlot* const CanvasSlot = GetOptionPanelCanvasSlot();
	if (not CanvasSlot)
	{
		return;
	}

	// If zero collapsed -> restore exactly to base offset.
	if (FMath::IsNearlyZero(DeltaYFromBase))
	{
		const FVector2D Curr = CanvasSlot->GetPosition();
		CanvasSlot->SetPosition(FVector2D(Curr.X, M_BaseOptionPanelYOffset));
		return;
	}

	// Negative delta => panel is smaller than base; move DOWN by |delta| (increase Y).
	// Positive delta => panel is larger than base; move UP by delta (decrease Y).
	const float NewY = M_BaseOptionPanelYOffset - DeltaYFromBase;
	const FVector2D Curr = CanvasSlot->GetPosition();
	CanvasSlot->SetPosition(FVector2D(Curr.X, NewY));
}

void UW_BottomCenterUI::ShowCancelBuilding() const
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	BuildingUI_ItemPanel->ShowCancelBuilding();
}

void UW_BottomCenterUI::ShowConstructBuilding() const
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	BuildingUI_ItemPanel->ShowConstructBuilding();
}

void UW_BottomCenterUI::ShowConvertToVehicle() const
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	BuildingUI_ItemPanel->ShowConvertToVehicle();
}

void UW_BottomCenterUI::ShowCancelVehicleConversion() const
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	BuildingUI_ItemPanel->ShowCancelVehicleConversion();
}

void UW_BottomCenterUI::UpdateBuildingUIForNewUnit(const bool bShowBuildingUI,
                                                   const EActionUINomadicButton NomadicButtonState,
                                                   const ENomadicSubtype NomadicSubtype)
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	// Note that the MainGameUI handles the option panel visibility as it requires resetting the active index.
	SetBuildingUIVisibility(bShowBuildingUI);
	if (bShowBuildingUI)
	{
		BuildingUI_ItemPanel->DetermineNomadicButton(NomadicButtonState, NomadicSubtype);
	}
	if(BuildingUI_OptionPanel)
	{
		BuildingUI_OptionPanel->HideHoverDescription();
	}
}

TArray<UW_ItemBuildingExpansion*> UW_BottomCenterUI::GetBxpItemsInItemPanel()
{
	if (not EnsureIsValidChildPanels())
	{
		return {};
	}
	return BuildingUI_ItemPanel->GetBxpItems();
}

TArray<UW_OptionBuildingExpansion*> UW_BottomCenterUI::GetBxpOptionsInPanel()
{
	if (not EnsureIsValidChildPanels())
	{
		return {};
	}
	return BuildingUI_OptionPanel->GetBxpOptions();
}

void UW_BottomCenterUI::InitArraysOnChildPanels()
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	BuildingUI_ItemPanel->OnMainGameUIConstruct();
	BuildingUI_OptionPanel->OnMainGameUIConstruct();
	if(EnsureIsValidSelectionPanel())
	{
		SelectionPanel->InitBeforeMainGameUIInit();
	}
}



void UW_BottomCenterUI::SetBuildingUIVisibility(const bool bVisible)
{
	if (not EnsureIsValidChildPanels() || not EnsureIsValidBorderPanel())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	BuildingUI_ItemPanel->SetVisibility(NewVisibility);
	BuildingUIBorder->SetVisibility(NewVisibility);
}

void UW_BottomCenterUI::SetBuildingOptionUIVisibility(const bool bVisible) const
{
	if (not EnsureIsValidChildPanels())
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	BuildingUI_OptionPanel->SetVisibility(NewVisibility);
	if (bVisible)
	{
		BuildingUI_OptionPanel->PlayOpenAnimation();
	}
}


bool UW_BottomCenterUI::EnsureIsValidChildPanels() const
{
	if (not IsValid(BuildingUI_OptionPanel) || not IsValid(BuildingUI_ItemPanel))
	{
		const FString IsOptionPanelValid = IsValid(BuildingUI_OptionPanel) ? "valid" : "invalid";
		const FString IsItemPanelValid = IsValid(BuildingUI_ItemPanel) ? "valid" : "invalid";
		RTSFunctionLibrary::ReportError("Invalid option or item child panel on BOTTOM CENTER UI. "
			"Option panel is " + IsOptionPanelValid + ", item panel is " + IsItemPanelValid);
		return false;
	}
	return true;
}

bool UW_BottomCenterUI::EnsureIsValidSelectionPanel() const
{
	if(not IsValid(SelectionPanel))
	{
		RTSFunctionLibrary::ReportError("Invalid selection panel on BOTTOM CENTER UI.");
		return false;
	}
	return true;
}

bool UW_BottomCenterUI::EnsureIsValidBorderPanel() const
{
	if (not IsValid(BuildingUIBorder))
	{
		RTSFunctionLibrary::ReportError("Invalid border panel on BOTTOM CENTER UI.");
		return false;
	}
	return true;
}

void UW_BottomCenterUI::SetupBxpOptionsPerSection_GroupBySection(
	const TArray<FBxpOptionData>& Options,
	TMap<EBxpOptionSection, TArray<FBxpOptionData>>& OutBySection) const
{
	OutBySection.Reset();
	for (const FBxpOptionData& Data : Options)
	{
		OutBySection.FindOrAdd(Data.Section).Add(Data);
	}
}


void UW_BottomCenterUI::CancelBuilding()
{
	if (not EnsureIsValidPlayerController() || not HasMainUIValidSelectedActor() || not
		EnsureIsValidChildPanels())
	{
		return;
	}
	M_PlayerController->CancelBuilding(M_MainGameUI->PrimarySelectedActor);
	BuildingUI_ItemPanel->ShowConstructBuilding();
}

void UW_BottomCenterUI::ConstructBuilding()
{
	if (not EnsureIsValidPlayerController() || not HasMainUIValidSelectedActor() || not
		EnsureIsValidChildPanels())
	{
		return;
	}
	M_PlayerController->ConstructBuilding(M_MainGameUI->PrimarySelectedActor);
	BuildingUI_ItemPanel->ShowCancelBuilding();
}

void UW_BottomCenterUI::ConvertToVehicle()
{
	if (not EnsureIsValidPlayerController() || not HasMainUIValidSelectedActor() || not
		EnsureIsValidChildPanels())
	{
		return;
	}
	M_PlayerController->ConvertBackToVehicle(M_MainGameUI->PrimarySelectedActor);
	BuildingUI_ItemPanel->ShowCancelVehicleConversion();
	// If the user clicked an expansion and then instead of Building one converts
	BuildingUI_OptionPanel->CloseOptionUI();
}

void UW_BottomCenterUI::CancelVehicleConversion()
{
	if (not EnsureIsValidPlayerController() || not EnsureIsValidChildPanels() || not HasMainUIValidSelectedActor())
	{
		return;
	}
	M_PlayerController->CancelVehicleConversion(M_MainGameUI->PrimarySelectedActor);
	BuildingUI_ItemPanel->ShowConvertToVehicle();
}

bool UW_BottomCenterUI::EnsureMainGameUIIsValid() const
{
	if (not IsValid(M_MainGameUI))
	{
		RTSFunctionLibrary::ReportError("Main game UI is not valid for Bottom Center UI,"
			"for got to call init?");
		return false;
	}
	return true;
}

AActor* UW_BottomCenterUI::GetPrimarySelectedActor() const
{
	if (not EnsureMainGameUIIsValid())
	{
		return nullptr;
	}
	return M_MainGameUI->PrimarySelectedActor;
}

bool UW_BottomCenterUI::HasMainUIValidSelectedActor() const
{
	return IsValid(GetPrimarySelectedActor());
}

bool UW_BottomCenterUI::EnsureIsValidPlayerController()
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Invalid player controller on UW_BottomCenterUI");

	APlayerController* const BasePlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (not IsValid(BasePlayerController))
	{
		return false;
	}

	ACPPController* const TypedController = Cast<ACPPController>(BasePlayerController);
	if (not IsValid(TypedController))
	{
		RTSFunctionLibrary::ReportError(
			"Expected ACPPController, but found a different PlayerController on UW_BottomCenterUI");
		return false;
	}

	M_PlayerController = TypedController;
	return true;
}

UCanvasPanelSlot* UW_BottomCenterUI::GetOptionPanelCanvasSlot() const
{
	if (not IsValid(BuildingUI_OptionPanel))
	{
		RTSFunctionLibrary::ReportError("GetOptionPanelCanvasSlot: BuildingUI_OptionPanel is invalid.");
		return nullptr;
	}
	UCanvasPanelSlot* const CanvasSlot = Cast<UCanvasPanelSlot>(BuildingUI_OptionPanel->Slot);
	if (not IsValid(CanvasSlot))
	{
		RTSFunctionLibrary::ReportError("GetOptionPanelCanvasSlot: Option panel is not in a CanvasPanelSlot.");
		return nullptr;
	}
	return CanvasSlot;
}

void UW_BottomCenterUI::CacheOptionPanelBaseYOffset()
{
	if (bM_HasCachedBaseOptionPanelYOffset)
	{
		return;
	}
	UCanvasPanelSlot* const CanvasSlot = GetOptionPanelCanvasSlot();
	if (not CanvasSlot)
	{
		return;
	}
	M_BaseOptionPanelYOffset = CanvasSlot->GetPosition().Y;
	bM_HasCachedBaseOptionPanelYOffset = true;
}
