// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_GlassIconButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_GlassIconButton::SetupGlassButton(UTexture2D* NormalTexture, UTexture2D* HoveredTexture,
                                          UTexture2D* PressedTexture, const float ButtonSize, const FText& Tooltip)
{
	M_NormalTexture = NormalTexture;
	M_HoveredTexture = HoveredTexture;
	M_PressedTexture = PressedTexture;
	M_ButtonSize = ButtonSize;
	M_Tooltip = Tooltip;
	if (USizeBox* const SizeBox = Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget : nullptr))
	{
		SizeBox->SetWidthOverride(M_ButtonSize);
		SizeBox->SetHeightOverride(M_ButtonSize);
	}
	ApplyStyle();
}

void UW_GlassIconButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_GlassIconButton::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(M_ButtonSize);
	SizeBox->SetHeightOverride(M_ButtonSize);
	WidgetTree->RootWidget = SizeBox;

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_GlassIconButton::OnButtonClicked);
	SizeBox->SetContent(M_Button);
	ApplyStyle();
}

void UW_GlassIconButton::ApplyStyle() const
{
	if (not IsValid(M_Button))
	{
		return;
	}
	FButtonStyle Style;
	Style.SetNormal(MakeGlassBrush(M_NormalTexture));
	Style.SetHovered(MakeGlassBrush(M_HoveredTexture ? M_HoveredTexture : M_NormalTexture));
	Style.SetPressed(MakeGlassBrush(M_PressedTexture ? M_PressedTexture : M_NormalTexture));
	Style.SetDisabled(MakeGlassBrush(M_NormalTexture));
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f));
	M_Button->SetStyle(Style);
	M_Button->SetToolTipText(M_Tooltip);
}

FSlateBrush UW_GlassIconButton::MakeGlassBrush(UTexture2D* Texture) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(M_ButtonSize, M_ButtonSize);
	Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
	return Brush;
}

void UW_GlassIconButton::OnButtonClicked()
{
	OnGlassButtonClicked.Broadcast();
	ExecuteAction();
}

void UW_GlassIconButton::ExecuteAction() const
{
	if (M_Action != EGlassIconButtonAction::OpenFullMapView)
	{
		return;
	}
	// Placed in the main game UI's designer tree: the main game UI is an outer of this widget.
	UMainGameUI* const MainGameUI = GetTypedOuter<UMainGameUI>();
	if (not IsValid(MainGameUI))
	{
		RTSFunctionLibrary::ReportError(
			"Glass button with the OpenFullMapView action is not inside the main game UI."
			"\n See UW_GlassIconButton::ExecuteAction");
		return;
	}
	MainGameUI->OpenFullMapView();
}
