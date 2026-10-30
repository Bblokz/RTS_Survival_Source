// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalRewardPicker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Slate/SlateBrushAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "SurvivalArmyUiTypes.h"
#include "SurvivalUiStyle.h"

namespace SurvivalRewardPickerConstants
{
	const TCHAR* StyleAssetPath = TEXT("/Game/RTS_Survival/SurvivalMode/UI/DA_SurvivalUiStyle.DA_SurvivalUiStyle");
	// Below the HUD status plate at the top centre, clear of the main UI's bottom bar.
	constexpr float PanelTopOffset = 260.f;
	constexpr float CardWidth = 330.f;
	constexpr float CardMinHeight = 260.f;
	constexpr float CardGap = 18.f;
	constexpr float PanelPadding = 18.f;
	constexpr float IconSize = 44.f;
	constexpr int32 TitleFontSize = 20;
	constexpr int32 TimerFontSize = 14;
	constexpr int32 CardTitleFontSize = 15;
	constexpr int32 SupplyFontSize = 12;
	constexpr int32 LineFontSize = 13;
	constexpr int32 VariantFontSize = 10;
	constexpr float TimerRefreshSeconds = 0.25f;
}

void UW_SurvivalRewardPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (not IsValid(M_StyleAsset))
	{
		M_StyleAsset = LoadObject<USurvivalUiStyleAsset>(nullptr, SurvivalRewardPickerConstants::StyleAssetPath);
	}
	M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	M_VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
	// The game keeps running underneath; only the cards take the mouse.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildTree();
}

void UW_SurvivalRewardPicker::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerRefreshHandle);
	}
	Super::NativeDestruct();
}

const FSurvivalUiStyle* UW_SurvivalRewardPicker::GetStyle() const
{
	return IsValid(M_StyleAsset) ? &M_StyleAsset->Style : nullptr;
}

void UW_SurvivalRewardPicker::StyleText(UTextBlock* Text, const bool bHeading, const int32 Size,
                                        const FSlateColor& Color) const
{
	if (not IsValid(Text))
	{
		return;
	}
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateFontInfo BaseFont = Style ? (bHeading ? Style->HeadingFont : Style->BodyFont) : FSlateFontInfo();
	UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, UnitVariantSelectionWidgets::MakeFont(BaseFont, Size), Color);
}

void UW_SurvivalRewardPicker::BuildTree()
{
	using namespace SurvivalRewardPickerConstants;
	if (not WidgetTree)
	{
		return;
	}
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateColor TextColor = Style ? Style->TextColor : FSlateColor(FLinearColor::White);
	const FSlateColor AccentColor = Style ? Style->AccentColor : FSlateColor(FLinearColor(0.25f, 0.94f, 1.f));

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	if (Style and IsValid(Style->DialogPanel.GetResourceObject()))
	{
		Panel->SetBrush(Style->DialogPanel);
	}
	else
	{
		Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.85f));
	}
	Panel->SetPadding(FMargin(PanelPadding));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.f));
	PanelSlot->SetPosition(FVector2D(0.f, PanelTopOffset));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	StyleText(M_TitleText, true, TitleFontSize, TextColor);
	Column->AddChildToVerticalBox(M_TitleText)->SetHorizontalAlignment(HAlign_Center);

	M_TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Timer"));
	StyleText(M_TimerText, false, TimerFontSize, AccentColor);
	UVerticalBoxSlot* TimerSlot = Column->AddChildToVerticalBox(M_TimerText);
	TimerSlot->SetHorizontalAlignment(HAlign_Center);
	TimerSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 12.f));

	M_CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Cards"));
	Column->AddChildToVerticalBox(M_CardRow)->SetHorizontalAlignment(HAlign_Center);
}

void UW_SurvivalRewardPicker::InitRewardPicker(ASurvivalModeDirector* Director, const int32 RoundNumber,
                                               const TArray<FSurvivalRewardOption>& Options)
{
	M_Director = Director;
	bM_HasChosen = false;
	if (IsValid(M_TitleText))
	{
		M_TitleText->SetText(FText::Format(
			NSLOCTEXT("SurvivalReward", "Title", "WAVE {0} SURVIVED  -  CHOOSE YOUR REINFORCEMENTS"),
			FText::AsNumber(RoundNumber)));
	}
	if (IsValid(M_CardRow))
	{
		M_CardRow->ClearChildren();
		M_CardButtons.Reset();
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			UWidget* Card = BuildCard(Options[Index], Index);
			UHorizontalBoxSlot* CardSlot = M_CardRow->AddChildToHorizontalBox(Card);
			CardSlot->SetPadding(FMargin(Index == 0 ? 0.f : SurvivalRewardPickerConstants::CardGap, 0.f, 0.f, 0.f));
			CardSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	RefreshTimer();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_TimerRefreshHandle, this, &UW_SurvivalRewardPicker::RefreshTimer,
		                                  SurvivalRewardPickerConstants::TimerRefreshSeconds, true);
	}
}

UWidget* UW_SurvivalRewardPicker::BuildCard(const FSurvivalRewardOption& Option, const int32 OptionIndex)
{
	using namespace SurvivalRewardPickerConstants;
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateColor AccentColor = Style ? Style->AccentColor : FSlateColor(FLinearColor(0.25f, 0.94f, 1.f));
	const FSlateColor MutedColor = Style ? Style->MutedTextColor : FSlateColor(FLinearColor::Gray);

	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardBox->SetWidthOverride(CardWidth);
	CardBox->SetMinDesiredHeight(CardMinHeight);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	if (Style and IsValid(Style->RosterEntry.Normal.GetResourceObject()))
	{
		Button->SetStyle(Style->RosterEntry);
	}
	CardBox->SetContent(Button);
	M_CardButtons.Add(Button);
	switch (OptionIndex)
	{
	case 0: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard0Clicked);
		break;
	case 1: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard1Clicked);
		break;
	case 2: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard2Clicked);
		break;
	case 3: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard3Clicked);
		break;
	case 4: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard4Clicked);
		break;
	default: Button->OnClicked.AddDynamic(this, &UW_SurvivalRewardPicker::OnCard5Clicked);
		break;
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Button->SetContent(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(Option.Title);
	StyleText(Title, true, CardTitleFontSize, AccentColor);
	Content->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* Supply = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Supply->SetText(FText::Format(NSLOCTEXT("SurvivalReward", "Supply", "{0} SUPPLY WORTH  |  {1} UNITS"),
	                              FText::AsNumber(Option.TotalSupply), FText::AsNumber(Option.Units.Num())));
	StyleText(Supply, false, SupplyFontSize, MutedColor);
	UVerticalBoxSlot* SupplySlot = Content->AddChildToVerticalBox(Supply);
	SupplySlot->SetHorizontalAlignment(HAlign_Center);
	SupplySlot->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));

	// Equal units are listed once with a count.
	TArray<TPair<FSurvivalArmyEntry, int32>> Lines;
	for (const FSurvivalArmyEntry& Entry : Option.Units)
	{
		TPair<FSurvivalArmyEntry, int32>* Existing = Lines.FindByPredicate(
			[&Entry](const TPair<FSurvivalArmyEntry, int32>& Line)
			{
				return Line.Key.Unit == Entry.Unit and Line.Key.Variant == Entry.Variant;
			});
		if (Existing)
		{
			++Existing->Value;
			continue;
		}
		Lines.Add(TPair<FSurvivalArmyEntry, int32>(Entry, 1));
	}
	for (const TPair<FSurvivalArmyEntry, int32>& Line : Lines)
	{
		UVerticalBoxSlot* LineSlot = Content->AddChildToVerticalBox(BuildUnitLine(Line.Key, Line.Value));
		LineSlot->SetPadding(FMargin(6.f, 2.f));
	}
	return CardBox;
}

UWidget* UW_SurvivalRewardPicker::BuildUnitLine(const FSurvivalArmyEntry& Entry, const int32 Count)
{
	using namespace SurvivalRewardPickerConstants;
	const FSurvivalUiStyle* Style = GetStyle();
	const FSlateColor TextColor = Style ? Style->TextColor : FSlateColor(FLinearColor::White);
	const FSlateColor AccentColor = Style ? Style->AccentColor : FSlateColor(FLinearColor(0.25f, 0.94f, 1.f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(IconSize);
	IconBox->SetHeightOverride(IconSize);
	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	const FSlateBrush IconBrush = MakeIconBrush(Entry.Unit, Entry.Variant);
	if (IsValid(IconBrush.GetResourceObject()))
	{
		Icon->SetBrush(IconBrush);
	}
	else
	{
		Icon->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f));
	}
	IconBox->SetContent(Icon);
	Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	const FText NameText = Count > 1
		                       ? FText::Format(NSLOCTEXT("SurvivalReward", "LineCount", "{0} x {1}"),
		                                       FText::AsNumber(Count), Entry.DisplayName)
		                       : Entry.DisplayName;
	Name->SetText(NameText);
	Name->SetAutoWrapText(true);
	StyleText(Name, false, LineFontSize, TextColor);
	Texts->AddChildToVerticalBox(Name);
	if (Entry.Variant != EUnitVariant::Base)
	{
		UTextBlock* Variant = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
			                                           ? M_VariantDataAsset->FindVariantDefinition(
				                                           UPlayerUnitVariantManager::GetSpawnerKey(Entry.Unit),
				                                           Entry.Variant)
			                                           : nullptr;
		Variant->SetText(Definition and not Definition->DisplayName.IsEmpty()
			                 ? Definition->DisplayName
			                 : UEnum::GetDisplayValueAsText(Entry.Variant));
		StyleText(Variant, false, VariantFontSize, AccentColor);
		Texts->AddChildToVerticalBox(Variant);
	}
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Texts);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Row;
}

FSlateBrush UW_SurvivalRewardPicker::MakeIconBrush(const FTrainingOption& Unit, const EUnitVariant Variant)
{
	// Same order as the army selection: variant training brush, variant icon, then the training table picture.
	const FUnitVariantDefinition* Definition = nullptr;
	if (Variant != EUnitVariant::Base and IsValid(M_VariantDataAsset))
	{
		Definition = M_VariantDataAsset->FindVariantDefinition(UPlayerUnitVariantManager::GetSpawnerKey(Unit),
		                                                        Variant);
	}
	if (Definition and Definition->bOverrideTrainingImageBrush and IsValid(Definition->TrainingImageBrush))
	{
		return Definition->TrainingImageBrush->Brush;
	}
	if (Definition and not Definition->Icon.IsNull())
	{
		UTexture2D* const IconTexture = Definition->Icon.LoadSynchronous();
		if (IsValid(IconTexture))
		{
			M_LoadedIcons.AddUnique(IconTexture);
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTexture);
			IconBrush.ImageSize = FVector2D(SurvivalRewardPickerConstants::IconSize);
			return IconBrush;
		}
	}
	const USlateBrushAsset* TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit);
	return IsValid(TableBrush) ? TableBrush->Brush : FSlateBrush();
}

void UW_SurvivalRewardPicker::RefreshTimer()
{
	if (not IsValid(M_TimerText) or not M_Director.IsValid())
	{
		return;
	}
	const FText Clock = SurvivalArmyUi::FormatClock(M_Director->GetIntermissionSecondsRemaining());
	M_TimerText->SetText(bM_HasChosen
		                     ? FText::Format(NSLOCTEXT("SurvivalReward", "Inbound", "REINFORCEMENTS INBOUND  -  NEXT WAVE IN {0}"),
		                                     Clock)
		                     : FText::Format(NSLOCTEXT("SurvivalReward", "Timer", "NEXT WAVE IN {0}"), Clock));
}

void UW_SurvivalRewardPicker::ShowChosen(const int32 OptionIndex)
{
	bM_HasChosen = true;
	for (int32 Index = 0; Index < M_CardButtons.Num(); ++Index)
	{
		if (not IsValid(M_CardButtons[Index]))
		{
			continue;
		}
		// The chosen card stays bright (OnCardClicked ignores further clicks); the others fade and lock.
		M_CardButtons[Index]->SetIsEnabled(Index == OptionIndex);
		M_CardButtons[Index]->SetRenderOpacity(Index == OptionIndex ? 1.f : 0.35f);
	}
	RefreshTimer();
}

void UW_SurvivalRewardPicker::OnCardClicked(const int32 OptionIndex)
{
	if (bM_HasChosen or not M_Director.IsValid())
	{
		return;
	}
	M_Director->ChooseReward(OptionIndex);
}

void UW_SurvivalRewardPicker::OnCard0Clicked() { OnCardClicked(0); }
void UW_SurvivalRewardPicker::OnCard1Clicked() { OnCardClicked(1); }
void UW_SurvivalRewardPicker::OnCard2Clicked() { OnCardClicked(2); }
void UW_SurvivalRewardPicker::OnCard3Clicked() { OnCardClicked(3); }
void UW_SurvivalRewardPicker::OnCard4Clicked() { OnCardClicked(4); }
void UW_SurvivalRewardPicker::OnCard5Clicked() { OnCardClicked(5); }
