// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalStartLocationPicker.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Materials/MaterialInterface.h"
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
	// Children after the location index text: the resources row, then the special line.
	constexpr int32 ResourcesInsertOffset = 1;
	constexpr int32 SpecialInsertOffset = 2;

	// Same glass dome icons as the top resource bar (UW_PlayerResource).
	const TCHAR* const RadixiteIconPath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/ResourceUI/Materials/MI_GlassResourceIcon_Radixite.MI_GlassResourceIcon_Radixite");
	const TCHAR* const MetalIconPath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/ResourceUI/Materials/MI_GlassResourceIcon_Metal.MI_GlassResourceIcon_Metal");
	constexpr float ResourceIconSize = 26.f;
	constexpr float LabelToIconGap = 8.f;
	constexpr float IconToCountGap = 4.f;
	constexpr float CountToIconGap = 14.f;

	void ApplyText(UTextBlock* Text, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		if (IsValid(Text))
		{
			UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, Font, Color);
		}
	}

	void ApplySummarySlot(UPanelSlot* const InsertedSlot)
	{
		UVerticalBoxSlot* const BoxSlot = Cast<UVerticalBoxSlot>(InsertedSlot);
		if (not IsValid(BoxSlot))
		{
			return;
		}
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(SummaryPadding);
	}

	UMaterialInterface* LoadIconMaterial(const TCHAR* const MaterialPath)
	{
		UMaterialInterface* const Material =
			TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath)).LoadSynchronous();
		if (not IsValid(Material))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Survival start location picker: could not load resource icon material %s; the drop-off "
					"resources are shown as words."), MaterialPath));
		}
		return Material;
	}
}

void UW_SurvivalStartLocationPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	LoadResourceIconMaterials();
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
	using namespace SurvivalUiLayout;
	using namespace SurvivalStartLocationPickerPrivate;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_ResourcesRow))
	{
		CreateResourcesRow();
	}
	// Without the icon row the resources are one line of words.
	if (not IsValid(M_ResourcesText))
	{
		M_ResourcesText = CreateSummaryText(ResourcesInsertOffset);
	}
	if (not IsValid(M_SpecialText))
	{
		M_SpecialText = CreateSummaryText(SpecialInsertOffset);
	}
	if (not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	const FSlateFontInfo BodyFont = MakeFont(Style.BodyFont, BodyFontSize);
	ApplyText(M_ResourcesText, BodyFont, Style.TextColor);
	ApplyText(M_RadixiteCountText, BodyFont, Style.TextColor);
	ApplyText(M_MetalCountText, BodyFont, Style.TextColor);
	ApplyText(M_SpecialText, BodyFont, Style.AccentColor);
}

UTextBlock* UW_SurvivalStartLocationPicker::CreateSummaryText(const int32 InsertIndexOffset)
{
	if (not IsValid(WidgetTree))
	{
		return nullptr;
	}
	UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	if (not InsertSummaryWidget(Text, InsertIndexOffset))
	{
		return nullptr;
	}
	return Text;
}

bool UW_SurvivalStartLocationPicker::InsertSummaryWidget(UWidget* const Widget, const int32 InsertIndexOffset)
{
	using namespace SurvivalStartLocationPickerPrivate;
	UWidget* const Anchor = IsValid(M_LocationIndexText)
		                        ? static_cast<UWidget*>(M_LocationIndexText.Get())
		                        : static_cast<UWidget*>(M_LocationNameText.Get());
	UPanelWidget* const Panel = IsValid(Anchor) ? Anchor->GetParent() : nullptr;
	if (not IsValid(Panel) or not IsValid(Widget))
	{
		return false;
	}
	ApplySummarySlot(Panel->InsertChildAt(Panel->GetChildIndex(Anchor) + InsertIndexOffset, Widget));
	return true;
}

void UW_SurvivalStartLocationPicker::LoadResourceIconMaterials()
{
	using namespace SurvivalStartLocationPickerPrivate;
	M_RadixiteIconMaterial = LoadIconMaterial(RadixiteIconPath);
	M_MetalIconMaterial = LoadIconMaterial(MetalIconPath);
}

void UW_SurvivalStartLocationPicker::CreateResourcesRow()
{
	using namespace SurvivalStartLocationPickerPrivate;
	// A missing material was reported on load; the words fallback is used then.
	if (not IsValid(M_RadixiteIconMaterial) or not IsValid(M_MetalIconMaterial) or not IsValid(WidgetTree))
	{
		return;
	}
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (not PlaceResourcesRow(Row))
	{
		return;
	}
	M_ResourcesRow = Row;
	// Hidden until RefreshSummaryTexts has counts, so no icons show without numbers.
	M_ResourcesRow->SetVisibility(ESlateVisibility::Collapsed);
	if (not IsValid(M_ResourcesText))
	{
		M_ResourcesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	}
	M_ResourcesText->SetAutoWrapText(false);
	M_ResourcesText->SetText(NSLOCTEXT("SurvivalStartLocation", "CloseNodesLabel", "Near the drop-off:"));
	M_ResourcesRow->AddChildToHorizontalBox(M_ResourcesText)->SetVerticalAlignment(VAlign_Center);
	M_RadixiteCountText = AddResourceIconAndCount(M_RadixiteIconMaterial, LabelToIconGap);
	M_MetalCountText = AddResourceIconAndCount(M_MetalIconMaterial, CountToIconGap);
}

bool UW_SurvivalStartLocationPicker::PlaceResourcesRow(UHorizontalBox* const Row)
{
	using namespace SurvivalStartLocationPickerPrivate;
	if (not IsValid(M_ResourcesText))
	{
		return InsertSummaryWidget(Row, ResourcesInsertOffset);
	}
	// A Blueprint label moves into the row so label, icons and counts read as one line.
	UPanelWidget* const Panel = M_ResourcesText->GetParent();
	if (not IsValid(Panel) or not IsValid(Row))
	{
		return false;
	}
	const int32 LabelIndex = Panel->GetChildIndex(M_ResourcesText);
	M_ResourcesText->RemoveFromParent();
	ApplySummarySlot(Panel->InsertChildAt(LabelIndex, Row));
	return true;
}

UTextBlock* UW_SurvivalStartLocationPicker::AddResourceIconAndCount(UMaterialInterface* const IconMaterial,
                                                                    const float LeadingGap)
{
	using namespace SurvivalStartLocationPickerPrivate;
	if (not IsValid(M_ResourcesRow) or not IsValid(WidgetTree))
	{
		return nullptr;
	}
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(ResourceIconSize);
	IconBox->SetHeightOverride(ResourceIconSize);
	UImage* const Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Icon->SetBrushFromMaterial(IconMaterial);
	IconBox->SetContent(Icon);
	UHorizontalBoxSlot* const IconSlot = M_ResourcesRow->AddChildToHorizontalBox(IconBox);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(LeadingGap, 0.f, 0.f, 0.f));

	UTextBlock* const CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UHorizontalBoxSlot* const CountSlot = M_ResourcesRow->AddChildToHorizontalBox(CountText);
	CountSlot->SetVerticalAlignment(VAlign_Center);
	CountSlot->SetPadding(FMargin(IconToCountGap, 0.f, 0.f, 0.f));
	return CountText;
}

bool UW_SurvivalStartLocationPicker::GetHasResourceIconRow() const
{
	return IsValid(M_ResourcesRow) and IsValid(M_RadixiteCountText) and IsValid(M_MetalCountText);
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
	if (GetHasResourceIconRow())
	{
		M_RadixiteCountText->SetText(FText::AsNumber(Radixite));
		M_MetalCountText->SetText(FText::AsNumber(Metal));
		M_ResourcesRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else if (IsValid(M_ResourcesText))
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
