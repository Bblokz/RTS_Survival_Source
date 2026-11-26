// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_PlacementPanel.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace PlacementPanelStyle
{
	// The HUD's small-panel plates (Docs/UI/Skins/ShippingReadyUI.md): the thin panel is the hover info box and cost
	// tooltip plate, the resource counter plate is the recessed label field the key caps sit in.
	const TCHAR* const PanelPlatePath = TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/"
		"Instances/MI9_Custom_ThinPanel_32x32_u_n.MI9_Custom_ThinPanel_32x32_u_n");
	const TCHAR* const KeyCapPlatePath = TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/"
		"Instances/MI9_Custom_ResourceCounter_32x32_u_n.MI9_Custom_ResourceCounter_32x32_u_n");
	// The size the slice instances were made for (their MI9_<name>_<W>x<H> name).
	const FVector2D PlateBrushSize(32.f, 32.f);

	// Colours of the survival / variant menus (FSurvivalUiStyle) so the HUD panels read as one set.
	const FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("ECE8DC")));
	const FLinearColor MutedTextColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9FA09A")));
	const FLinearColor AccentColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FF0FF")));
	const FLinearColor WarningColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E07A5F")));
	const FLinearColor AcceptColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FD46B")));
	// sRGB #08090A: the dark groove of the mission objective plates.
	const FLinearColor GrooveColor(0.0024f, 0.0027f, 0.003f, 1.f);
	const FVector2D TextShadowOffset(1.f, 1.f);
	const FLinearColor TextShadowColor(0.f, 0.f, 0.f, 0.8f);

	// Whole-number font sizes; letter spacing in 1/1000 em (the objective plates' small caps label).
	constexpr int32 TitleFontSize = 11;
	constexpr int32 TitleLetterSpacing = 160;
	constexpr int32 StatusFontSize = 15;
	constexpr int32 StatFontSize = 12;
	constexpr int32 KeyCapFontSize = 10;
	constexpr int32 HintFontSize = 11;
	const FName BoldTypeface(TEXT("Bold"));
	const FName RegularTypeface(TEXT("Regular"));

	constexpr float PanelMinWidth = 260.f;
	const FMargin PanelPadding(14.f, 10.f, 14.f, 12.f);
	const FMargin StatusRowPadding(0.f, 2.f, 0.f, 0.f);
	const FVector2D StatusLampSize(8.f, 8.f);
	const FMargin StatusLampPadding(0.f, 0.f, 8.f, 0.f);
	const FVector2D GrooveSize(1.f, 2.f);
	const FMargin GroovePadding(0.f, 7.f, 0.f, 7.f);
	const FMargin StatRowPadding(0.f, 1.f, 0.f, 1.f);
	constexpr float StatColumnGap = 16.f;
	const FMargin HintRowPadding(0.f, 2.f, 0.f, 2.f);
	constexpr int32 HintsPerRow = 2;
	constexpr float HintGap = 14.f;
	const FMargin KeyCapPadding(7.f, 2.f, 7.f, 3.f);
	constexpr float KeyCapMinWidth = 26.f;
	const FMargin KeyCapGap(0.f, 0.f, 3.f, 0.f);
	const FMargin HintActionPadding(4.f, 0.f, 0.f, 0.f);
}

namespace PlacementPanelLayout
{
	// Draws above the HUD so a panel pushed over a HUD band stays readable; below the camera tour skip prompt (8).
	constexpr int32 ViewportZOrder = 3;
	// Screen bands kept clear while the panel can avoid the placed object without them: the bottom band is the
	// HUD's selection and action panel (about 310 units of the 1080 unit tall widget space), the right band the
	// minimap and training menu column (about 305 units wide; both are anchored to the screen edge).
	const FMargin SafeInsets(16.f, 16.f, 320.f, 312.f);
	constexpr float GapToPlacedObject = 28.f;
	constexpr float FollowInterpSpeed = 18.f;
	constexpr float FadeInSeconds = 0.12f;
}

namespace PlacementPanelHelpers
{
	FSlateBrush MakePlateBrush(const TCHAR* const MaterialPath)
	{
		FSlateBrush Brush;
		UMaterialInterface* const PlateMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
		if (not IsValid(PlateMaterial))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("The placement panel could not load its plate %s.\nAt function: MakePlateBrush"), MaterialPath));
			Brush.TintColor = FSlateColor(FLinearColor(0.02f, 0.022f, 0.025f, 0.92f));
			return Brush;
		}
		Brush.SetResourceObject(PlateMaterial);
		Brush.ImageSize = PlacementPanelStyle::PlateBrushSize;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::Both;
		Brush.Margin = FMargin(0.f);
		return Brush;
	}

	// A plain white quad of this size, tinted by the image's colour. The image's own default brush is 32 units square
	// and SetDesiredSizeOverride does not shrink it, so the lamp and the grooves drew as large blocks.
	FSlateBrush MakeSolidBrush(const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = Size;
		return Brush;
	}

	void SetTextIfChanged(UTextBlock* const TextBlock, const FText& NewText)
	{
		if (not IsValid(TextBlock) || TextBlock->GetText().EqualTo(NewText))
		{
			return;
		}
		TextBlock->SetText(NewText);
	}

	FString MakeHintsSignature(const TArray<FPlacementPanelKeyHint>& KeyHints)
	{
		FString Signature;
		for (const FPlacementPanelKeyHint& KeyHint : KeyHints)
		{
			for (const FText& Key : KeyHint.Keys)
			{
				Signature += Key.ToString() + TEXT("+");
			}
			Signature += KeyHint.Action.ToString() + TEXT("|");
		}
		return Signature;
	}

	FLinearColor GetStatusColor(const EPlacementPanelStatus Status)
	{
		switch (Status)
		{
		case EPlacementPanelStatus::Ready:
			return PlacementPanelStyle::AcceptColor;
		case EPlacementPanelStatus::Blocked:
			return PlacementPanelStyle::WarningColor;
		case EPlacementPanelStatus::Info:
			return PlacementPanelStyle::AccentColor;
		}
		return PlacementPanelStyle::TextColor;
	}
}

void UW_PlacementPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	// Never takes a click: the player places with the left mouse button wherever the panel is.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_PlacementPanel::ShowPanel()
{
	if (not IsInViewport())
	{
		AddToViewport(PlacementPanelLayout::ViewportZOrder);
		SetAlignmentInViewport(FVector2D::ZeroVector);
	}
	if (bM_IsShown)
	{
		return;
	}
	bM_IsShown = true;
	bM_HasDockedSinceShown = false;
	M_FadeAlpha = 0.f;
	SetRenderOpacity(0.f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_PlacementPanel::HidePanel()
{
	bM_IsShown = false;
	M_AvoidWorldBounds = FBox(ForceInit);
	M_LastAvoidScreenRect = FBox2D(ForceInit);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_PlacementPanel::SetContent(const FPlacementPanelContent& Content)
{
	using namespace PlacementPanelHelpers;
	SetTextIfChanged(M_TitleText, Content.Title);
	SetTextIfChanged(M_StatusText, Content.StatusText);
	const FLinearColor StatusColor = GetStatusColor(Content.Status);
	if (IsValid(M_StatusText))
	{
		M_StatusText->SetColorAndOpacity(FSlateColor(StatusColor));
	}
	if (IsValid(M_StatusLamp))
	{
		M_StatusLamp->SetColorAndOpacity(StatusColor);
	}
	SetStats(Content.Stats);
	SetKeyHints(Content.KeyHints);
}

FBox2D UW_PlacementPanel::GetPanelScreenRect() const
{
	return PlacementPanelDocking::MakePanelRect(M_CurrentTopLeft, GetDesiredSize());
}

void UW_PlacementPanel::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (not bM_IsShown)
	{
		return;
	}
	UpdateFade(InDeltaTime);
}

void UW_PlacementPanel::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UVerticalBox* const Content = BuildPlate();
	BuildHeader(*Content);
	AddGroove(*Content);
	M_StatsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlacementStats"));
	Content->AddChildToVerticalBox(M_StatsBox);
	M_HintsGroove = AddGroove(*Content);
	M_HintsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlacementKeyHints"));
	Content->AddChildToVerticalBox(M_HintsBox);
}

UVerticalBox* UW_PlacementPanel::BuildPlate()
{
	using namespace PlacementPanelStyle;
	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlacementPanel"));
	Root->SetMinDesiredWidth(PanelMinWidth);
	WidgetTree->RootWidget = Root;

	UBorder* const Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlacementPlate"));
	Plate->SetBrush(PlacementPanelHelpers::MakePlateBrush(PanelPlatePath));
	Plate->SetPadding(PanelPadding);
	Root->AddChild(Plate);

	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PlacementContent"));
	Plate->SetContent(Content);
	return Content;
}

void UW_PlacementPanel::BuildHeader(UVerticalBox& Content)
{
	using namespace PlacementPanelStyle;
	M_TitleText = MakeText(TitleFontSize, BoldTypeface, AccentColor, TitleLetterSpacing);
	Content.AddChildToVerticalBox(M_TitleText);

	UHorizontalBox* const StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* const StatusRowSlot = Content.AddChildToVerticalBox(StatusRow);
	StatusRowSlot->SetPadding(StatusRowPadding);

	M_StatusLamp = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PlacementStatusLamp"));
	M_StatusLamp->SetBrush(PlacementPanelHelpers::MakeSolidBrush(StatusLampSize));
	UHorizontalBoxSlot* const LampSlot = StatusRow->AddChildToHorizontalBox(M_StatusLamp);
	LampSlot->SetVerticalAlignment(VAlign_Center);
	LampSlot->SetPadding(StatusLampPadding);

	M_StatusText = MakeText(StatusFontSize, BoldTypeface, TextColor);
	UHorizontalBoxSlot* const StatusSlot = StatusRow->AddChildToHorizontalBox(M_StatusText);
	StatusSlot->SetVerticalAlignment(VAlign_Center);
}

UImage* UW_PlacementPanel::AddGroove(UVerticalBox& Content)
{
	using namespace PlacementPanelStyle;
	UImage* const Groove = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Groove->SetBrush(PlacementPanelHelpers::MakeSolidBrush(GrooveSize));
	Groove->SetColorAndOpacity(GrooveColor);
	UVerticalBoxSlot* const GrooveSlot = Content.AddChildToVerticalBox(Groove);
	GrooveSlot->SetHorizontalAlignment(HAlign_Fill);
	GrooveSlot->SetPadding(GroovePadding);
	return Groove;
}

void UW_PlacementPanel::AddStatRow()
{
	using namespace PlacementPanelStyle;
	if (not IsValid(M_StatsBox))
	{
		return;
	}
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* const RowSlot = M_StatsBox->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(StatRowPadding);

	UTextBlock* const Label = MakeText(StatFontSize, RegularTypeface, MutedTextColor);
	Row->AddChildToHorizontalBox(Label);

	USpacer* const Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Spacer->SetSize(FVector2D(StatColumnGap, 0.f));
	UHorizontalBoxSlot* const SpacerSlot = Row->AddChildToHorizontalBox(Spacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UTextBlock* const Value = MakeText(StatFontSize, BoldTypeface, TextColor);
	Row->AddChildToHorizontalBox(Value);

	M_StatRows.Add(Row);
	M_StatLabels.Add(Label);
	M_StatValues.Add(Value);
}

void UW_PlacementPanel::SetStats(const TArray<FPlacementPanelStatRow>& Stats)
{
	using namespace PlacementPanelStyle;
	while (M_StatRows.Num() < Stats.Num())
	{
		const int32 RowCountBefore = M_StatRows.Num();
		AddStatRow();
		if (M_StatRows.Num() == RowCountBefore)
		{
			// The stats box is missing; AddStatRow cannot add rows.
			return;
		}
	}
	for (int32 RowIndex = 0; RowIndex < M_StatRows.Num(); ++RowIndex)
	{
		const bool bIsShownRow = Stats.IsValidIndex(RowIndex);
		M_StatRows[RowIndex]->SetVisibility(bIsShownRow ? ESlateVisibility::HitTestInvisible
			                                    : ESlateVisibility::Collapsed);
		if (not bIsShownRow)
		{
			continue;
		}
		PlacementPanelHelpers::SetTextIfChanged(M_StatLabels[RowIndex], Stats[RowIndex].Label);
		PlacementPanelHelpers::SetTextIfChanged(M_StatValues[RowIndex], Stats[RowIndex].Value);
		M_StatValues[RowIndex]->SetColorAndOpacity(FSlateColor(Stats[RowIndex].bIsWarning ? WarningColor : TextColor));
	}
}

void UW_PlacementPanel::SetKeyHints(const TArray<FPlacementPanelKeyHint>& KeyHints)
{
	const FString Signature = PlacementPanelHelpers::MakeHintsSignature(KeyHints);
	if (Signature == M_ShownHintsSignature || not IsValid(M_HintsBox))
	{
		return;
	}
	M_ShownHintsSignature = Signature;
	M_HintsBox->ClearChildren();
	if (IsValid(M_HintsGroove))
	{
		M_HintsGroove->SetVisibility(KeyHints.IsEmpty() ? ESlateVisibility::Collapsed
			                             : ESlateVisibility::HitTestInvisible);
	}
	UHorizontalBox* HintRow = nullptr;
	for (int32 HintIndex = 0; HintIndex < KeyHints.Num(); ++HintIndex)
	{
		const bool bStartsRow = HintIndex % PlacementPanelStyle::HintsPerRow == 0;
		if (bStartsRow)
		{
			HintRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			M_HintsBox->AddChildToVerticalBox(HintRow)->SetPadding(PlacementPanelStyle::HintRowPadding);
		}
		UHorizontalBoxSlot* const HintSlot = HintRow->AddChildToHorizontalBox(MakeKeyHint(KeyHints[HintIndex]));
		HintSlot->SetPadding(FMargin(bStartsRow ? 0.f : PlacementPanelStyle::HintGap, 0.f, 0.f, 0.f));
		HintSlot->SetVerticalAlignment(VAlign_Center);
	}
}

UHorizontalBox* UW_PlacementPanel::MakeKeyHint(const FPlacementPanelKeyHint& KeyHint)
{
	using namespace PlacementPanelStyle;
	UHorizontalBox* const Hint = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FText& Key : KeyHint.Keys)
	{
		UHorizontalBoxSlot* const KeySlot = Hint->AddChildToHorizontalBox(MakeKeyCap(Key));
		KeySlot->SetPadding(KeyCapGap);
		KeySlot->SetVerticalAlignment(VAlign_Center);
	}
	UTextBlock* const Action = MakeText(HintFontSize, RegularTypeface, MutedTextColor);
	Action->SetText(KeyHint.Action);
	UHorizontalBoxSlot* const ActionSlot = Hint->AddChildToHorizontalBox(Action);
	ActionSlot->SetPadding(HintActionPadding);
	ActionSlot->SetVerticalAlignment(VAlign_Center);
	return Hint;
}

UBorder* UW_PlacementPanel::MakeKeyCap(const FText& KeyText)
{
	using namespace PlacementPanelStyle;
	UBorder* const KeyCap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KeyCap->SetBrush(PlacementPanelHelpers::MakePlateBrush(KeyCapPlatePath));
	KeyCap->SetPadding(KeyCapPadding);
	KeyCap->SetHorizontalAlignment(HAlign_Center);
	KeyCap->SetVerticalAlignment(VAlign_Center);

	USizeBox* const KeyWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	KeyWidth->SetMinDesiredWidth(KeyCapMinWidth - KeyCapPadding.Left - KeyCapPadding.Right);
	KeyCap->SetContent(KeyWidth);

	UTextBlock* const KeyLabel = MakeText(KeyCapFontSize, BoldTypeface, TextColor);
	KeyLabel->SetText(KeyText);
	KeyLabel->SetJustification(ETextJustify::Center);
	KeyWidth->AddChild(KeyLabel);
	return KeyCap;
}

UTextBlock* UW_PlacementPanel::MakeText(const int32 FontSize, const FName Typeface, const FLinearColor& Color,
                                        const int32 LetterSpacing)
{
	UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, FontSize);
	Font.LetterSpacing = LetterSpacing;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowOffset(PlacementPanelStyle::TextShadowOffset);
	Text->SetShadowColorAndOpacity(PlacementPanelStyle::TextShadowColor);
	return Text;
}

void UW_PlacementPanel::UpdateDocking(const FBox& AvoidWorldBounds, const float DeltaTime)
{
	using namespace PlacementPanelLayout;
	if (not bM_IsShown)
	{
		return;
	}
	M_AvoidWorldBounds = AvoidWorldBounds;
	FVector2D PanelSize = GetDesiredSize();
	if (PanelSize.IsNearlyZero())
	{
		ForceLayoutPrepass();
		PanelSize = GetDesiredSize();
	}
	FBox2D AvoidRect(ForceInit);
	if (not TryProjectAvoidRect(AvoidRect))
	{
		AvoidRect = FBox2D(ForceInit);
	}
	M_LastAvoidScreenRect = AvoidRect;

	FPlacementPanelDockRequest Request;
	Request.AvoidRect = AvoidRect;
	Request.PanelSize = PanelSize;
	Request.ViewportSize = GetViewportSizeInWidgetUnits();
	Request.SafeInsets = SafeInsets;
	Request.Gap = GapToPlacedObject;
	Request.PreferredDock = M_CurrentDock;
	const FPlacementPanelDockResult Dock = PlacementPanelDocking::ChooseDock(Request);

	const FVector2D FollowedTopLeft = FMath::Vector2DInterpTo(M_CurrentTopLeft, Dock.TopLeft, DeltaTime,
	                                                          FollowInterpSpeed);
	// Slides after the object, but never across it: a new side, or a slide that would cover it, jumps.
	const bool bFollowWouldCover = AvoidRect.bIsValid && not Dock.bOverlapsAvoidRect &&
		PlacementPanelDocking::GetDoRectsOverlap(PlacementPanelDocking::MakePanelRect(FollowedTopLeft, PanelSize),
		                                         AvoidRect);
	const bool bJump = not bM_HasDockedSinceShown || Dock.Dock != M_CurrentDock || bFollowWouldCover;
	M_CurrentTopLeft = bJump ? Dock.TopLeft : FollowedTopLeft;
	M_CurrentDock = Dock.Dock;
	bM_HasDockedSinceShown = true;
	SetPositionInViewport(M_CurrentTopLeft.RoundToVector(), /*bRemoveDPIScale=*/ false);
}

bool UW_PlacementPanel::TryProjectAvoidRect(FBox2D& OutAvoidRect) const
{
	APlayerController* const OwningPlayer = GetOwningPlayer();
	if (not M_AvoidWorldBounds.IsValid || not IsValid(OwningPlayer))
	{
		return false;
	}
	FVector Corners[8];
	M_AvoidWorldBounds.GetVertices(Corners);
	OutAvoidRect = FBox2D(ForceInit);
	for (const FVector& Corner : Corners)
	{
		FVector2D ScreenPosition;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(OwningPlayer, Corner, ScreenPosition, false))
		{
			OutAvoidRect += ScreenPosition;
		}
	}
	return OutAvoidRect.bIsValid;
}

FVector2D UW_PlacementPanel::GetViewportSizeInWidgetUnits() const
{
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	// GetViewportSize only reads the world from its context object; it just isn't declared const.
	const FVector2D ViewportPixels = UWidgetLayoutLibrary::GetViewportSize(const_cast<UW_PlacementPanel*>(this));
	return ViewportScale > KINDA_SMALL_NUMBER ? ViewportPixels / ViewportScale : ViewportPixels;
}

void UW_PlacementPanel::UpdateFade(const float DeltaTime)
{
	if (M_FadeAlpha >= 1.f)
	{
		return;
	}
	M_FadeAlpha = FMath::Min(1.f, M_FadeAlpha + DeltaTime / PlacementPanelLayout::FadeInSeconds);
	SetRenderOpacity(M_FadeAlpha);
}
