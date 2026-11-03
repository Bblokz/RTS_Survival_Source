// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalMapPicker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeLauncher.h"
#include "SurvivalUiStyle.h"

namespace SurvivalMapPickerConstants
{
	const TCHAR* StyleAssetPath = TEXT("/Game/RTS_Survival/SurvivalMode/UI/DA_SurvivalUiStyle.DA_SurvivalUiStyle");
	constexpr float CardWidth = 520.f;
	constexpr float CardHeight = 110.f;
	constexpr float CardGap = 14.f;
	constexpr float PanelPadding = 24.f;
	constexpr float CardTextPadding = 12.f;
	constexpr float BackButtonWidth = 200.f;
	constexpr float BackButtonHeight = 54.f;
	constexpr int32 TitleFontSize = 22;
	constexpr int32 CardTitleFontSize = 17;
	constexpr int32 CardTextFontSize = 13;
}

void USurvivalMapChoiceButton::InitChoiceButton(UW_SurvivalMapPicker* Picker, const int32 ChoiceIndex)
{
	M_Picker = Picker;
	M_ChoiceIndex = ChoiceIndex;
	OnClicked.AddDynamic(this, &USurvivalMapChoiceButton::HandleClicked);
}

void USurvivalMapChoiceButton::HandleClicked()
{
	if (M_Picker.IsValid())
	{
		M_Picker->ChooseMap(M_ChoiceIndex);
	}
}

void UW_SurvivalMapPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (not IsValid(M_StyleAsset))
	{
		M_StyleAsset = LoadObject<USurvivalUiStyleAsset>(nullptr, SurvivalMapPickerConstants::StyleAssetPath);
	}
	BuildTree();
}

const FSurvivalUiStyle* UW_SurvivalMapPicker::GetStyle() const
{
	return IsValid(M_StyleAsset) ? &M_StyleAsset->Style : nullptr;
}

void UW_SurvivalMapPicker::StyleText(UTextBlock* Text, const bool bHeading, const int32 Size, const FSlateColor& Color) const
{
	if (not IsValid(Text))
	{
		return;
	}
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateFontInfo BaseFont = Style ? (bHeading ? Style->HeadingFont : Style->BodyFont) : FSlateFontInfo();
	UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, UnitVariantSelectionWidgets::MakeFont(BaseFont, Size), Color);
}

void UW_SurvivalMapPicker::BuildTree()
{
	using namespace SurvivalMapPickerConstants;
	if (not WidgetTree)
	{
		return;
	}
	const FSurvivalUiStyle* Style = GetStyle();
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	if (Style and IsValid(Style->DialogPanel.GetResourceObject()))
	{
		Panel->SetBrush(Style->DialogPanel);
	}
	else
	{
		Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.92f));
	}
	Panel->SetPadding(FMargin(PanelPadding));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(NSLOCTEXT("SurvivalMapPicker", "Title", "SURVIVAL  -  CHOOSE A MAP"));
	StyleText(Title, true, TitleFontSize, Style ? Style->TextColor : FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, CardGap));

	M_CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Cards"));
	Column->AddChildToVerticalBox(M_CardColumn)->SetHorizontalAlignment(HAlign_Center);

	USizeBox* BackBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BackBox"));
	BackBox->SetWidthOverride(BackButtonWidth);
	BackBox->SetHeightOverride(BackButtonHeight);
	UButton* Back = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Back"));
	if (Style and IsValid(Style->BackButton.Normal.GetResourceObject()))
	{
		Back->SetStyle(Style->BackButton);
	}
	Back->OnClicked.AddDynamic(this, &UW_SurvivalMapPicker::OnBackClicked);
	BackBox->SetContent(Back);
	UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackText"));
	BackText->SetText(NSLOCTEXT("SurvivalMapPicker", "Back", "BACK"));
	StyleText(BackText, false, CardTextFontSize, Style ? Style->TextColor : FSlateColor(FLinearColor::White));
	Back->SetContent(BackText);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(BackBox);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0.f, CardGap, 0.f, 0.f));
}

void UW_SurvivalMapPicker::InitMapPicker(const TArray<FSurvivalMapChoice>& Choices)
{
	M_Choices = Choices;
	if (not IsValid(M_CardColumn))
	{
		return;
	}
	M_CardColumn->ClearChildren();
	for (int32 Index = 0; Index < M_Choices.Num(); ++Index)
	{
		UVerticalBoxSlot* CardSlot = M_CardColumn->AddChildToVerticalBox(BuildCard(M_Choices[Index], Index));
		CardSlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : SurvivalMapPickerConstants::CardGap, 0.f, 0.f));
	}
}

UWidget* UW_SurvivalMapPicker::BuildCard(const FSurvivalMapChoice& Choice, const int32 ChoiceIndex)
{
	using namespace SurvivalMapPickerConstants;
	const FSurvivalUiStyle* Style = GetStyle();
	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardBox->SetWidthOverride(CardWidth);
	CardBox->SetMinDesiredHeight(CardHeight);

	USurvivalMapChoiceButton* Button = WidgetTree->ConstructWidget<USurvivalMapChoiceButton>(USurvivalMapChoiceButton::StaticClass());
	if (Style and IsValid(Style->RosterEntry.Normal.GetResourceObject()))
	{
		Button->SetStyle(Style->RosterEntry);
	}
	Button->InitChoiceButton(this, ChoiceIndex);
	CardBox->SetContent(Button);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Button->SetContent(Content);
	// Without this the button centres each card's text on its own width and the titles do not line up.
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Content->Slot))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(Choice.Title);
	StyleText(Title, true, CardTitleFontSize, Style ? Style->AccentColor : FSlateColor(FLinearColor::White));
	Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(CardTextPadding, CardTextPadding, CardTextPadding, 0.f));

	UTextBlock* Description = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Description->SetText(Choice.Description);
	Description->SetAutoWrapText(true);
	StyleText(Description, false, CardTextFontSize, Style ? Style->TextColor : FSlateColor(FLinearColor::White));
	Content->AddChildToVerticalBox(Description)->SetPadding(FMargin(CardTextPadding, 4.f, CardTextPadding, CardTextPadding));
	return CardBox;
}

void UW_SurvivalMapPicker::ChooseMap(const int32 ChoiceIndex)
{
	if (not M_Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}
	const TSoftObjectPtr<UWorld> ChosenMap = M_Choices[ChoiceIndex].Map;
	RemoveFromParent();
	USurvivalModeLauncher::LaunchSurvivalOnMap(this, ChosenMap);
}

void UW_SurvivalMapPicker::OnBackClicked()
{
	RemoveFromParent();
}
