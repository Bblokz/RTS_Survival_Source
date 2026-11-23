// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurrenderingTruckCountdown.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace SurrenderingTruckCountdownLayout
{
	// Screen pixels: readable at RTS camera distance without covering the truck.
	constexpr float BarWidth = 150.f;
	constexpr float BarHeight = 12.f;
	constexpr int32 TextSize = 18;
	constexpr float PlatePadding = 6.f;
	constexpr float TextBottomGap = 3.f;
	const FLinearColor PlateColour(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor BarFillColour(0.9f, 0.12f, 0.05f, 1.f);
	const FLinearColor TextColour(1.f, 0.85f, 0.75f, 1.f);
}

bool UW_SurrenderingTruckCountdown::Initialize()
{
	const bool bInitializedNow = Super::Initialize();
	// Drawn over the world: clicks must reach the units and ground underneath.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// A Blueprint child brings its own tree; the native class builds one.
	if (bInitializedNow and IsValid(WidgetTree) and WidgetTree->RootWidget == nullptr)
	{
		BuildNativeLayout();
	}
	return bInitializedNow;
}

void UW_SurrenderingTruckCountdown::BuildNativeLayout()
{
	using namespace SurrenderingTruckCountdownLayout;
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CountdownPlate"));
	Plate->SetBrushColor(PlateColour);
	Plate->SetPadding(FMargin(PlatePadding));
	WidgetTree->RootWidget = Plate;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CountdownColumn"));
	Plate->SetContent(Column);

	CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountdownText"));
	FSlateFontInfo Font = CountdownText->GetFont();
	Font.Size = TextSize;
	CountdownText->SetFont(Font);
	CountdownText->SetColorAndOpacity(FSlateColor(TextColour));
	CountdownText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(CountdownText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, TextBottomGap));
	}

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CountdownBarBox"));
	BarBox->SetWidthOverride(BarWidth);
	BarBox->SetHeightOverride(BarHeight);
	Column->AddChildToVerticalBox(BarBox);

	CountdownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CountdownBar"));
	CountdownBar->SetFillColorAndOpacity(BarFillColour);
	CountdownBar->SetPercent(1.f);
	BarBox->SetContent(CountdownBar);
}

void UW_SurrenderingTruckCountdown::SetCountdown(const float SecondsLeft, const float FractionLeft,
                                                 const FText& TextFormat)
{
	const float ClampedFraction = FMath::Clamp(FractionLeft, 0.f, 1.f);
	if (IsValid(CountdownBar))
	{
		CountdownBar->SetPercent(ClampedFraction);
	}
	if (IsValid(CountdownText))
	{
		const int32 WholeSecondsLeft = FMath::Max(0, FMath::CeilToInt(SecondsLeft));
		CountdownText->SetText(FText::Format(TextFormat, FText::AsNumber(WholeSecondsLeft)));
	}
	BP_OnCountdownUpdated(SecondsLeft, ClampedFraction);
}
