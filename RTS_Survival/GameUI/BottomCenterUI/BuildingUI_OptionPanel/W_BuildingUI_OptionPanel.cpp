// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_BuildingUI_OptionPanel.h"

#include "Animation/WidgetAnimation.h"
#include "Components/VerticalBox.h"
#include "RTS_Survival/GameUI/BuildingExpansion/W_OptionBuildingExpansion.h"
#include "RTS_Survival/GameUI/BuildingExpansion/BxpOptionHoverDescription/W_BxpOptionHoverDescription.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_BuildingUI_OptionPanel::CloseOptionUI()
{
	SetVisibility(ESlateVisibility::Hidden);
}

TArray<UW_OptionBuildingExpansion*> UW_BuildingUI_OptionPanel::GetBxpOptions() const
{
	if (M_BxpOptions.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("No bxp options set in building option panel but getter is used.");
	}
	TArray<UW_OptionBuildingExpansion*> Result;
	Result.Reserve(M_BxpOptions.Num());
	for (const TObjectPtr<UW_OptionBuildingExpansion>& Each : M_BxpOptions)
	{
		Result.Add(Each.Get());
	}
	return Result;
}

void UW_BuildingUI_OptionPanel::InitArray(TArray<UW_OptionBuildingExpansion*> BxpOptions)
{
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> ValidOptions;
	ValidOptions.Reserve(BxpOptions.Num());
	for (UW_OptionBuildingExpansion* EachOption : BxpOptions)
	{
		if (not IsValid(EachOption))
		{
			RTSFunctionLibrary::ReportError("Invalid bxp option provided to InitBuildingUI_OptionPanel");
			continue;
		}
		ValidOptions.Add(EachOption);
		EachOption->SetHoverDescription(HoverDescription);
	}
	M_BxpOptions = MoveTemp(ValidOptions);
	HideHoverDescription();
}

void UW_BuildingUI_OptionPanel::PlayOpenAnimation()
{
	if (not IsValid(Anim_PanelOpen))
	{
		return;
	}
	PlayAnimation(Anim_PanelOpen, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
}

bool UW_BuildingUI_OptionPanel::GetIsValidSectionBoxes() const
{
	const bool bHasAll = IsValid(TechVerticalBox) && IsValid(TechOptionsBox)
		&& IsValid(EconomicVerticalBox) && IsValid(EconomicOptionsBox)
		&& IsValid(DefenseVerticalBox) && IsValid(DefenseOptionsBox);

	if (bHasAll)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Invalid section boxes on UW_BuildingUI_OptionPanel. Ensure all VerticalBoxes and inner OptionBoxes are bound.");
	return false;
}

void UW_BuildingUI_OptionPanel::InitSectionArrays(
	TArray<UW_OptionBuildingExpansion*> TechOptions,
	TArray<UW_OptionBuildingExpansion*> EconomicOptions,
	TArray<UW_OptionBuildingExpansion*> DefenseOptions)
{
	if (not GetIsValidSectionBoxes())
	{
		return;
	}

	auto MakeValid = [](const TArray<UW_OptionBuildingExpansion*>& Src,
	                    TArray<TObjectPtr<UW_OptionBuildingExpansion>>& Dst)
	{
		Dst.Reset();
		for (UW_OptionBuildingExpansion* Each : Src)
		{
			if (not IsValid(Each))
			{
				RTSFunctionLibrary::ReportError("Invalid UW_OptionBuildingExpansion in InitSectionArrays.");
				continue;
			}
			Dst.Add(Each);
		}
	};

	MakeValid(TechOptions, M_TechOptionSlots);
	MakeValid(EconomicOptions, M_EconomicOptionSlots);
	MakeValid(DefenseOptions, M_DefenseOptionSlots);

	// Fill from bottom-to-top: reverse slot order so index 0 maps to bottom-most widget.
	Algo::Reverse(M_TechOptionSlots);
	Algo::Reverse(M_EconomicOptionSlots);
	Algo::Reverse(M_DefenseOptionSlots);
}

TArray<UW_OptionBuildingExpansion*> UW_BuildingUI_OptionPanel::GetSectionSlots(const EBxpOptionSection Section) const
{
	const TArray<TObjectPtr<UW_OptionBuildingExpansion>>* Slots = nullptr;
	switch (Section)
	{
	case EBxpOptionSection::BOS_Tech: Slots = &M_TechOptionSlots;
		break;
	case EBxpOptionSection::BOS_Economic: Slots = &M_EconomicOptionSlots;
		break;
	case EBxpOptionSection::BOS_Defense: Slots = &M_DefenseOptionSlots;
		break;
	default: break;
	}

	TArray<UW_OptionBuildingExpansion*> Result;
	if (Slots)
	{
		Result.Reserve(Slots->Num());
		for (const TObjectPtr<UW_OptionBuildingExpansion>& Each : *Slots)
		{
			Result.Add(Each.Get());
		}
	}
	return Result;
}

void UW_BuildingUI_OptionPanel::SetSectionCollapsed(const EBxpOptionSection Section, const bool bCollapsed)
{
	if (not GetIsValidSectionBoxes())
	{
		return;
	}

	UVerticalBox* Target = nullptr;
	switch (Section)
	{
	case EBxpOptionSection::BOS_Tech: Target = TechVerticalBox;
		break;
	case EBxpOptionSection::BOS_Economic: Target = EconomicVerticalBox;
		break;
	case EBxpOptionSection::BOS_Defense: Target = DefenseVerticalBox;
		break;
	default: break;
	}

	if (not IsValid(Target))
	{
		RTSFunctionLibrary::ReportError("Invalid target section box in SetSectionCollapsed.");
		return;
	}

	Target->SetVisibility(bCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UW_BuildingUI_OptionPanel::HideHoverDescription() const
{
	if(HoverDescription)
	{
		HoverDescription->SetVisibility(ESlateVisibility::Hidden);
	}
}
