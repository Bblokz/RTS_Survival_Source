// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalEventBanner.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/SurvivalMode/UI/SurvivalUiStyle.h"

namespace SurvivalEventBannerConstants
{
	const TCHAR* const StyleAssetPath = TEXT("/Game/RTS_Survival/SurvivalMode/UI/DA_SurvivalUiStyle.DA_SurvivalUiStyle");
	// Left edge, below the top bar, clear of the reward picker (top centre) and the minimap (bottom).
	constexpr float PanelLeft = 24.f;
	constexpr float PanelTop = 150.f;
	constexpr float PanelWidth = 430.f;
	constexpr float PanelPaddingX = 26.f;
	constexpr float PanelPaddingY = 18.f;
	constexpr int32 TitleFontSize = 17;
	constexpr int32 BodyFontSize = 12;
	constexpr int32 CountdownFontSize = 14;
	constexpr float LineGap = 6.f;
}

void UW_SurvivalEventBanner::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	M_StyleAsset = LoadObject<USurvivalUiStyleAsset>(nullptr, SurvivalEventBannerConstants::StyleAssetPath);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BuildTree();
}

const FSurvivalUiStyle* UW_SurvivalEventBanner::GetStyle() const
{
	return IsValid(M_StyleAsset) ? &M_StyleAsset->Style : nullptr;
}

UTextBlock* UW_SurvivalEventBanner::MakeText(const FName Name, const bool bHeading, const int32 Size,
                                             const FSlateColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateFontInfo BaseFont = Style ? (bHeading ? Style->HeadingFont : Style->BodyFont) : FSlateFontInfo();
	UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, UnitVariantSelectionWidgets::MakeFont(BaseFont, Size), Color);
	Text->SetAutoWrapText(true);
	return Text;
}

void UW_SurvivalEventBanner::BuildTree()
{
	using namespace SurvivalEventBannerConstants;
	if (not WidgetTree)
	{
		return;
	}
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateColor TextColor = Style ? Style->TextColor : FSlateColor(FLinearColor::White);
	const FSlateColor WarningColor = Style ? Style->WarningColor : FSlateColor(FLinearColor(0.88f, 0.48f, 0.37f));
	const FSlateColor AccentColor = Style ? Style->AccentColor : FSlateColor(FLinearColor(0.25f, 0.94f, 1.f));

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	if (Style and IsValid(Style->BannerPlate.GetResourceObject()))
	{
		Panel->SetBrush(Style->BannerPlate);
	}
	else
	{
		Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.85f));
	}
	Panel->SetPadding(FMargin(PanelPaddingX, PanelPaddingY));
	// A fixed width so the body lines wrap instead of stretching the plate.
	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WidthBox"));
	WidthBox->SetWidthOverride(PanelWidth);
	WidthBox->SetContent(Panel);
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(WidthBox);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
	PanelSlot->SetPosition(FVector2D(PanelLeft, PanelTop));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);
	M_TitleText = MakeText(TEXT("Title"), true, TitleFontSize, WarningColor);
	M_BodyText = MakeText(TEXT("Body"), false, BodyFontSize, TextColor);
	M_CountdownText = MakeText(TEXT("Countdown"), true, CountdownFontSize, AccentColor);
	M_StatusText = MakeText(TEXT("Status"), false, BodyFontSize, AccentColor);
	for (UTextBlock* Text : {M_TitleText.Get(), M_BodyText.Get(), M_CountdownText.Get(), M_StatusText.Get()})
	{
		UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(Text);
		TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, LineGap));
	}
	M_StatusText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_SurvivalEventBanner::ShowEvent(const FText& Title, const TArray<FText>& Lines)
{
	if (IsValid(M_TitleText))
	{
		M_TitleText->SetText(Title);
	}
	if (IsValid(M_BodyText))
	{
		M_BodyText->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_SurvivalEventBanner::SetCountdownText(const FText& Countdown)
{
	if (IsValid(M_CountdownText))
	{
		M_CountdownText->SetText(Countdown);
	}
}

void UW_SurvivalEventBanner::SetStatusText(const FText& Status)
{
	if (not IsValid(M_StatusText))
	{
		return;
	}
	M_StatusText->SetText(Status);
	M_StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
