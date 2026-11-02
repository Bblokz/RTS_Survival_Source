// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalStartLocationPicker.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"

namespace SurvivalStartLocationPickerPrivate
{
	// The arrow art points right; the previous button draws it mirrored.
	const FVector2D MirroredScale(-1.f, 1.f);
	// Summary lines under the location name.
	const FMargin SummaryPadding(0.f, 4.f, 0.f, 0.f);

	void ApplyText(UTextBlock* Text, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		if (IsValid(Text))
		{
			UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, Font, Color);
		}
	}
}

void UW_SurvivalStartLocationPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	ApplyStyleToBoundWidgets();
	BindButtons();
	if (IsValid(M_TitleText))
	{
		M_TitleText->SetText(NSLOCTEXT("SurvivalStartLocation", "Title", "CHOOSE START LOCATION"));
	}
	if (IsValid(M_HintText))
	{
		M_HintText->SetText(NSLOCTEXT("SurvivalStartLocation", "Hint", "Left / Right to cycle, Enter to deploy"));
	}
	if (IsValid(M_ConfirmText))
	{
		M_ConfirmText->SetText(NSLOCTEXT("SurvivalStartLocation", "Confirm", "DEPLOY HERE"));
	}
}

void UW_SurvivalStartLocationPicker::InitStartLocationPicker(ASurvivalModeDirector* Director)
{
	M_Director = Director;
	if (not GetIsValidDirector())
	{
		return;
	}
	M_Locations.Reset();
	for (const TWeakObjectPtr<ASurvivalStartLocation>& Location : Director->GetStartLocations())
	{
		if (Location.IsValid())
		{
			M_Locations.Add(Location);
		}
	}
	EnsureSummaryTexts();
	if (M_Locations.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival start location picker: the map has no start locations."));
		if (IsValid(M_LocationNameText))
		{
			M_LocationNameText->SetText(NSLOCTEXT("SurvivalStartLocation", "None", "NO START LOCATIONS"));
		}
		if (IsValid(M_ConfirmButton))
		{
			M_ConfirmButton->SetIsEnabled(false);
		}
		return;
	}
	SelectLocationByIndex(0);
}

void UW_SurvivalStartLocationPicker::SelectNextLocation()
{
	SelectLocationByIndex(M_SelectedIndex + 1);
}

void UW_SurvivalStartLocationPicker::SelectPreviousLocation()
{
	SelectLocationByIndex(M_SelectedIndex - 1);
}

void UW_SurvivalStartLocationPicker::SelectLocationByIndex(const int32 LocationIndex)
{
	if (M_Locations.IsEmpty() || bM_IsConfirmed || not GetIsValidDirector())
	{
		return;
	}
	const int32 Count = M_Locations.Num();
	M_SelectedIndex = ((LocationIndex % Count) + Count) % Count;
	RefreshTexts();
	M_Director->PreviewStartLocation(M_Locations[M_SelectedIndex].Get());
}

bool UW_SurvivalStartLocationPicker::SelectLocationActor(ASurvivalStartLocation* StartLocation)
{
	if (not IsValid(StartLocation))
	{
		return false;
	}
	const int32 LocationIndex = M_Locations.IndexOfByPredicate(
		[StartLocation](const TWeakObjectPtr<ASurvivalStartLocation>& Location)
		{
			return Location.Get() == StartLocation;
		});
	if (LocationIndex == INDEX_NONE)
	{
		return false;
	}
	SelectLocationByIndex(LocationIndex);
	return true;
}

void UW_SurvivalStartLocationPicker::ConfirmSelectedLocation()
{
	if (bM_IsConfirmed or not M_Locations.IsValidIndex(M_SelectedIndex) or not GetIsValidDirector())
	{
		return;
	}
	ASurvivalStartLocation* const Location = M_Locations[M_SelectedIndex].Get();
	if (not IsValid(Location))
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival start location picker: the selected location was destroyed."));
		return;
	}
	// The director removes this widget and starts the deployment; ignore input until it does.
	bM_IsConfirmed = true;
	if (IsValid(M_ConfirmButton))
	{
		M_ConfirmButton->SetIsEnabled(false);
	}
	M_Director->ConfirmStartLocation(Location);
}

FReply UW_SurvivalStartLocationPicker::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left)
	{
		SelectPreviousLocation();
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right)
	{
		SelectNextLocation();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		ConfirmSelectedLocation();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UW_SurvivalStartLocationPicker::ApplyStyleToBoundWidgets()
{
	using namespace SurvivalUiLayout;
	using namespace SurvivalStartLocationPickerPrivate;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	if (IsValid(M_PanelBorder) and IsValid(Style.DialogPanel.GetResourceObject()))
	{
		M_PanelBorder->SetBrush(Style.DialogPanel);
	}
	if (IsValid(Style.ArrowButton.Normal.GetResourceObject()))
	{
		for (UButton* const Arrow : {M_PreviousButton.Get(), M_NextButton.Get()})
		{
			if (IsValid(Arrow))
			{
				Arrow->SetStyle(Style.ArrowButton);
			}
		}
		if (IsValid(M_PreviousButton))
		{
			M_PreviousButton->SetRenderScale(MirroredScale);
		}
	}
	if (IsValid(M_ConfirmButton) and IsValid(Style.ConfirmButton.Normal.GetResourceObject()))
	{
		M_ConfirmButton->SetStyle(Style.ConfirmButton);
	}
	ApplyText(M_TitleText, MakeFont(Style.HeadingFont, LabelFontSize, TitleLetterSpacing), Style.TextColor);
	ApplyText(M_LocationNameText, MakeFont(Style.HeadingFont, LocationNameFontSize, LabelLetterSpacing),
	          Style.AccentColor);
	ApplyText(M_LocationIndexText, MakeFont(Style.BodyFont, BodyFontSize), Style.MutedTextColor);
	ApplyText(M_HintText, MakeFont(Style.BodyFont, BodyFontSize), Style.MutedTextColor);
	ApplyText(M_ConfirmText, MakeFont(Style.HeadingFont, ActionButtonFontSize, ActionButtonLetterSpacing),
	          Style.TextColor);
}

void UW_SurvivalStartLocationPicker::BindButtons()
{
	if (IsValid(M_PreviousButton))
	{
		M_PreviousButton->OnClicked.RemoveAll(this);
		M_PreviousButton->OnClicked.AddDynamic(this, &UW_SurvivalStartLocationPicker::OnPreviousClicked);
	}
	if (IsValid(M_NextButton))
	{
		M_NextButton->OnClicked.RemoveAll(this);
		M_NextButton->OnClicked.AddDynamic(this, &UW_SurvivalStartLocationPicker::OnNextClicked);
	}
	if (IsValid(M_ConfirmButton))
	{
		M_ConfirmButton->OnClicked.RemoveAll(this);
		M_ConfirmButton->OnClicked.AddDynamic(this, &UW_SurvivalStartLocationPicker::OnConfirmClicked);
	}
}

void UW_SurvivalStartLocationPicker::RefreshTexts()
{
	if (IsValid(M_LocationNameText))
	{
		M_LocationNameText->SetText(GetLocationName(M_SelectedIndex).ToUpper());
	}
	if (IsValid(M_LocationIndexText))
	{
		M_LocationIndexText->SetText(FText::Format(NSLOCTEXT("SurvivalStartLocation", "Index", "{0} / {1}"),
		                                           FText::AsNumber(M_SelectedIndex + 1),
		                                           FText::AsNumber(M_Locations.Num())));
	}
	RefreshSummaryTexts();
	const bool bCanCycle = M_Locations.Num() > 1;
	for (UButton* const Arrow : {M_PreviousButton.Get(), M_NextButton.Get()})
	{
		if (IsValid(Arrow))
		{
			Arrow->SetIsEnabled(bCanCycle);
		}
	}
}

void UW_SurvivalStartLocationPicker::EnsureSummaryTexts()
{
	if (not IsValid(M_ResourcesText))
	{
		M_ResourcesText = CreateSummaryText(1);
	}
	if (not IsValid(M_SpecialText))
	{
		M_SpecialText = CreateSummaryText(2);
	}
	using namespace SurvivalUiLayout;
	using namespace SurvivalStartLocationPickerPrivate;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	ApplyText(M_ResourcesText, MakeFont(Style.BodyFont, BodyFontSize), Style.TextColor);
	ApplyText(M_SpecialText, MakeFont(Style.BodyFont, BodyFontSize), Style.AccentColor);
}

UTextBlock* UW_SurvivalStartLocationPicker::CreateSummaryText(const int32 InsertIndexOffset)
{
	using namespace SurvivalStartLocationPickerPrivate;
	UWidget* Anchor = IsValid(M_LocationIndexText) ? static_cast<UWidget*>(M_LocationIndexText) : static_cast<UWidget*>(M_LocationNameText);
	UPanelWidget* Panel = IsValid(Anchor) ? Anchor->GetParent() : nullptr;
	if (not IsValid(Panel) or not IsValid(WidgetTree))
	{
		return nullptr;
	}
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	UPanelSlot* InsertedSlot = Panel->InsertChildAt(Panel->GetChildIndex(Anchor) + InsertIndexOffset, Text);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(InsertedSlot))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(SummaryPadding);
	}
	return Text;
}

void UW_SurvivalStartLocationPicker::RefreshSummaryTexts()
{
	const ASurvivalStartLocation* const Location = M_Locations.IsValidIndex(M_SelectedIndex) ? M_Locations[M_SelectedIndex].Get() : nullptr;
	const ASurvivalModeDirector* const Director = M_Director.Get();
	const USurvivalModeSettings* const ModeSettings = IsValid(Director) ? Director->GetSettings() : nullptr;
	const USurvivalStartContentSettings* const ContentSettings = IsValid(ModeSettings) ? ModeSettings->StartContent.Get() : nullptr;
	if (not IsValid(Location) or not IsValid(ContentSettings))
	{
		return;
	}
	int32 Radixite = 0;
	int32 Metal = 0;
	FText SpecialTitle;
	FText SpecialLine;
	Location->GetPickerSummary(*ContentSettings, Radixite, Metal, SpecialTitle, SpecialLine);
	if (IsValid(M_ResourcesText))
	{
		M_ResourcesText->SetText(FText::Format(
			NSLOCTEXT("SurvivalStartLocation", "CloseNodes", "Near the drop-off: {0} radixite, {1} metal"),
			FText::AsNumber(Radixite), FText::AsNumber(Metal)));
	}
	if (IsValid(M_SpecialText))
	{
		M_SpecialText->SetText(SpecialTitle.IsEmpty()
			                       ? FText::GetEmpty()
			                       : FText::Format(NSLOCTEXT("SurvivalStartLocation", "Special", "{0}: {1}"), SpecialTitle, SpecialLine));
	}
}

FText UW_SurvivalStartLocationPicker::GetLocationName(const int32 LocationIndex) const
{
	const ASurvivalStartLocation* const Location = M_Locations.IsValidIndex(LocationIndex)
		                                               ? M_Locations[LocationIndex].Get()
		                                               : nullptr;
	if (IsValid(Location) and not Location->GetDisplayName().IsEmpty())
	{
		return Location->GetDisplayName();
	}
	return FText::Format(NSLOCTEXT("SurvivalStartLocation", "Fallback", "Start Location {0}"),
	                     FText::AsNumber(LocationIndex + 1));
}

void UW_SurvivalStartLocationPicker::OnPreviousClicked()
{
	SelectPreviousLocation();
}

void UW_SurvivalStartLocationPicker::OnNextClicked()
{
	SelectNextLocation();
}

void UW_SurvivalStartLocationPicker::OnConfirmClicked()
{
	ConfirmSelectedLocation();
}

bool UW_SurvivalStartLocationPicker::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}
