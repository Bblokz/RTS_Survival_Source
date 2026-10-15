// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_TacticalUnitIconCorners.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_TacticalUnitIcon.h"

bool UW_TacticalUnitIconCorners::Initialize()
{
	const bool bInitializedThisCall = Super::Initialize();
	// Built here rather than in NativeOnInitialized so the designer preview gets the tree as well.
	BuildWidgetTree();
	return bInitializedThisCall;
}

void UW_TacticalUnitIconCorners::SetTacticalIcons(const FTacticalUnitIconSettings& LeftIcon,
                                                  const FTacticalUnitIconSettings& RightIcon)
{
	M_LeftIcon = LeftIcon;
	M_RightIcon = RightIcon;
	ApplyIcons();
}

void UW_TacticalUnitIconCorners::ClearTacticalIcons()
{
	SetTacticalIcons(FTacticalUnitIconSettings(), FTacticalUnitIconSettings());
}

void UW_TacticalUnitIconCorners::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIcons();
}

void UW_TacticalUnitIconCorners::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Overlay;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	M_LeftIconWidget = AddCornerIcon(*Overlay, HAlign_Left);
	M_RightIconWidget = AddCornerIcon(*Overlay, HAlign_Right);
	ApplyIcons();
}

UW_TacticalUnitIcon* UW_TacticalUnitIconCorners::AddCornerIcon(UOverlay& Overlay,
                                                               const EHorizontalAlignment HorizontalAlignment) const
{
	UW_TacticalUnitIcon* const Icon = WidgetTree->ConstructWidget<UW_TacticalUnitIcon>(
		UW_TacticalUnitIcon::StaticClass());
	if (not IsValid(Icon))
	{
		return nullptr;
	}
	UOverlaySlot* const IconSlot = Overlay.AddChildToOverlay(Icon);
	IconSlot->SetHorizontalAlignment(HorizontalAlignment);
	IconSlot->SetVerticalAlignment(VAlign_Bottom);
	IconSlot->SetPadding(FMargin(M_CornerInset));
	return Icon;
}

void UW_TacticalUnitIconCorners::ApplyIcons() const
{
	if (not GetIsValidLeftIconWidget() || not GetIsValidRightIconWidget())
	{
		return;
	}
	M_LeftIconWidget->SetIconSize(M_IconSize);
	M_LeftIconWidget->SetupTacticalIcon(M_LeftIcon);
	M_RightIconWidget->SetIconSize(M_IconSize);
	M_RightIconWidget->SetupTacticalIcon(M_RightIcon);
}

bool UW_TacticalUnitIconCorners::GetIsValidLeftIconWidget() const
{
	if (IsValid(M_LeftIconWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_LeftIconWidget",
		"GetIsValidLeftIconWidget",
		this);
	return false;
}

bool UW_TacticalUnitIconCorners::GetIsValidRightIconWidget() const
{
	if (IsValid(M_RightIconWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_RightIconWidget",
		"GetIsValidRightIconWidget",
		this);
	return false;
}
