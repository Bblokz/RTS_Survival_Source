// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UnitVariantDetailPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "UnitVariantProbes.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "UnitVariantStatComparison.h"

namespace UnitVariantDetailPanelLayout
{
	constexpr float PanelWidth = 380.f;
	// The variant panel's height; a longer list scrolls inside the plate.
	constexpr float PanelMaxHeight = 848.f;
	const FMargin PanelPadding(24.f, 20.f, 24.f, 24.f);
	const FMargin SectionLabelPadding(0.f, 18.f, 0.f, 8.f);
	constexpr float RowSpacing = 12.f;
	const FVector2D AbilityIconSize(64.f, 64.f);
	const FVector2D WeaponIconSize(56.f, 56.f);
	const FMargin IconWellPadding(4.f);
	const FMargin RowTextPadding(14.f, 0.f, 0.f, 0.f);
	const FMargin TitlePadding(0.f, 0.f, 0.f, 4.f);
	const FMargin ChangeTagPadding(8.f, 3.f, 0.f, 0.f);
	// Weapon stat bars: a pictogram, then one pip per level.
	constexpr float StatIconSize = 16.f;
	const FMargin StatIconPadding(0.f, 0.f, 6.f, 0.f);
	const FVector2D StatPipSize(9.f, 10.f);
	constexpr float StatPipGap = 2.f;
	constexpr float StatBarSpacing = 3.f;
	const FMargin StatBarsPadding(0.f, 2.f, 0.f, 0.f);
	const FLinearColor EmptyPipColor(1.f, 1.f, 1.f, 0.12f);
	const FMargin ChangeArrowsPadding(10.f, 0.f, 0.f, 0.f);
	// A lost ability or weapon stays readable but clearly secondary to what the variant has.
	constexpr float RemovedRowOpacity = 0.45f;
	const TCHAR* const WeaponIconColumn = TEXT("Slate");
	constexpr int32 LabelFontSize = 12;
	constexpr int32 LabelLetterSpacing = 100;
	constexpr int32 VariantNameFontSize = 20;
	constexpr int32 RowTitleFontSize = 16;
	constexpr int32 ChangeTagFontSize = 11;
	constexpr int32 BodyFontSize = 14;
	constexpr int32 CalibreFallbackFontSize = 14;
}

void UW_UnitVariantDetailPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_UnitVariantDetailPanel::BuildWidgetTree()
{
	using namespace UnitVariantDetailPanelLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelWidth);
	PanelBox->SetMaxDesiredHeight(PanelMaxHeight);
	WidgetTree->RootWidget = PanelBox;
	M_Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Plate->SetPadding(PanelPadding);
	PanelBox->SetContent(M_Plate);
	UScrollBox* const ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	M_Plate->SetContent(ScrollBox);

	UVerticalBox* const PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ScrollBox->AddChild(PanelColumn);
	M_VariantNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_VariantNameText->SetAutoWrapText(true);
	PanelColumn->AddChildToVerticalBox(M_VariantNameText);
	M_SectionColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelColumn->AddChildToVerticalBox(M_SectionColumn);
	// Never takes the mouse from the hangar or the cards it sits next to.
	SetVisibility(ESlateVisibility::Hidden);
}

void UW_UnitVariantDetailPanel::SetupPanel(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
                                           const UDataTable* WeaponCardTable)
{
	using namespace UnitVariantDetailPanelLayout;
	M_Style = Style;
	M_RichTextStyle = RichTextStyle;
	M_WeaponCardTable = WeaponCardTable;
	M_DamageStatIcon = Style.DamageStatIcon.LoadSynchronous();
	M_PenetrationStatIcon = Style.PenetrationStatIcon.LoadSynchronous();
	M_ReloadStatIcon = Style.ReloadStatIcon.LoadSynchronous();
	if (not GetIsValidPlate())
	{
		return;
	}
	// The card art is drawn to stretch, so it frames a panel of any height.
	M_Plate->SetBrush(Style.Card.Normal);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_VariantNameText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, VariantNameFontSize),
		Style.SelectedTextColor);
}

void UW_UnitVariantDetailPanel::ShowVariant(const FUnitVariantDetailContent& Content)
{
	if (not GetIsValidPlate() || Content.AbilityCards == nullptr)
	{
		return;
	}
	M_VariantNameText->SetText(Content.VariantName.ToUpper());
	M_SectionColumn->ClearChildren();
	AddAbilitySection(Content);
	AddWeaponSection(Content);
	AddComparisonSection(Content);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_UnitVariantDetailPanel::HidePanel()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UW_UnitVariantDetailPanel::AddAbilitySection(const FUnitVariantDetailContent& Content)
{
	AddSectionLabel(NSLOCTEXT("UnitVariantSelection", "AbilitiesLabel", "ABILITIES"));
	const TArray<FUnitVariantAbilityCard>& AbilityCards = *Content.AbilityCards;
	const TArray<FUnitVariantAbilityCard> NoCards;
	const TArray<FUnitVariantAbilityCard>& BaseCards = Content.BaseAbilityCards != nullptr
		                                                   ? *Content.BaseAbilityCards
		                                                   : NoCards;
	for (const FUnitVariantAbilityCard& AbilityCard : AbilityCards)
	{
		const bool bIsGained = Content.BaseAbilityCards != nullptr && not BaseCards.ContainsByPredicate(
			[&AbilityCard](const FUnitVariantAbilityCard& BaseCard) { return BaseCard.GetIsSameAbility(AbilityCard); });
		AddAbilityRow(AbilityCard, bIsGained ? EUnitVariantChange::Gained : EUnitVariantChange::Unchanged);
	}
	int32 RemovedCount = 0;
	for (const FUnitVariantAbilityCard& BaseCard : BaseCards)
	{
		if (not AbilityCards.ContainsByPredicate(
			[&BaseCard](const FUnitVariantAbilityCard& AbilityCard) { return AbilityCard.GetIsSameAbility(BaseCard); }))
		{
			AddAbilityRow(BaseCard, EUnitVariantChange::Removed);
			++RemovedCount;
		}
	}
	if (AbilityCards.IsEmpty() && RemovedCount == 0)
	{
		AddMutedLine(NSLOCTEXT("UnitVariantSelection", "NoAbilities", "No special abilities."));
	}
}

void UW_UnitVariantDetailPanel::AddWeaponSection(const FUnitVariantDetailContent& Content)
{
	AddSectionLabel(NSLOCTEXT("UnitVariantSelection", "WeaponsLabel", "WEAPONS"));
	if (Content.Probe == nullptr)
	{
		AddMutedLine(NSLOCTEXT("UnitVariantSelection", "ReadingWeapons", "Reading the loadout..."));
		return;
	}
	const auto HasWeapon = [](const FUnitVariantProbeResult& Result, const FName WeaponRowName)
	{
		return Result.Weapons.ContainsByPredicate([WeaponRowName](const FUnitVariantWeaponStats& Weapon)
		{
			return Weapon.WeaponRowName == WeaponRowName;
		});
	};
	for (const FUnitVariantWeaponStats& Weapon : Content.Probe->Weapons)
	{
		const bool bIsGained = Content.BaseProbe != nullptr && not HasWeapon(*Content.BaseProbe, Weapon.WeaponRowName);
		const FUnitVariantWeaponStats* const BaseWeapon = Content.BaseProbe != nullptr
			                                                  ? UnitVariantStatComparison::FindComparableBaseWeapon(
				                                                  Weapon, *Content.Probe, *Content.BaseProbe)
			                                                  : nullptr;
		AddWeaponRow(Weapon, bIsGained ? EUnitVariantChange::Gained : EUnitVariantChange::Unchanged, BaseWeapon);
	}
	if (Content.BaseProbe != nullptr)
	{
		for (const FUnitVariantWeaponStats& BaseWeapon : Content.BaseProbe->Weapons)
		{
			if (not HasWeapon(*Content.Probe, BaseWeapon.WeaponRowName))
			{
				AddWeaponRow(BaseWeapon, EUnitVariantChange::Removed, nullptr);
			}
		}
	}
	if (Content.Probe->Weapons.IsEmpty() && (Content.BaseProbe == nullptr || Content.BaseProbe->Weapons.IsEmpty()))
	{
		AddMutedLine(NSLOCTEXT("UnitVariantSelection", "NoWeapons", "No weapons."));
	}
}

void UW_UnitVariantDetailPanel::AddComparisonSection(const FUnitVariantDetailContent& Content)
{
	if (Content.Probe == nullptr || Content.BaseProbe == nullptr)
	{
		return;
	}
	// Gun changes already show in the weapon bars; this section only exists when health or speed differ.
	const TArray<FUnitVariantStatRow> StatRows = UnitVariantStatComparison::CompareWithBase(
		*Content.Probe, *Content.BaseProbe);
	if (StatRows.IsEmpty())
	{
		return;
	}
	AddSectionLabel(NSLOCTEXT("UnitVariantSelection", "ComparedLabel", "AGAINST BASE"));
	for (const FUnitVariantStatRow& StatRow : StatRows)
	{
		AddStatRow(StatRow);
	}
}

void UW_UnitVariantDetailPanel::AddAbilityRow(const FUnitVariantAbilityCard& AbilityCard,
                                              const EUnitVariantChange Change)
{
	using namespace UnitVariantDetailPanelLayout;
	UImage* Icon = nullptr;
	if (IsValid(AbilityCard.Icon))
	{
		Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(AbilityCard.Icon);
	}
	URichTextBlock* const DescriptionText = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass());
	DescriptionText->SetTextStyleSet(M_RichTextStyle);
	DescriptionText->SetAutoWrapText(true);
	DescriptionText->SetText(AbilityCard.Description);
	AddIconRow(Icon, AbilityIconSize, AbilityCard.Title, Change, DescriptionText);
}

void UW_UnitVariantDetailPanel::AddWeaponRow(const FUnitVariantWeaponStats& Weapon, const EUnitVariantChange Change,
                                             const FUnitVariantWeaponStats* BaseWeapon)
{
	using namespace UnitVariantDetailPanelLayout;
	UWidget* Icon = nullptr;
	if (USlateBrushAsset* const IconBrush = Cast<USlateBrushAsset>(UnitVariantAbilityCards::FindRowObject(
		M_WeaponCardTable, Weapon.WeaponRowName, WeaponIconColumn)))
	{
		// Weapon icons are not all square; the scale box keeps their aspect inside the well.
		UScaleBox* const IconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		IconScale->SetStretch(EStretch::ScaleToFit);
		UImage* const IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromAsset(IconBrush);
		IconScale->SetContent(IconImage);
		Icon = IconScale;
	}
	else
	{
		// Not every weapon has a WeaponUI_Data row (the weapon bar shows no icon for those either): the calibre
		// keeps the well from looking broken until the art exists.
		UTextBlock* const CalibreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CalibreText->SetText(FText::Format(NSLOCTEXT("UnitVariantSelection", "CalibreFallback", "{0}\nmm"),
		                                   FText::AsNumber(FMath::RoundToInt(Weapon.CalibreMm))));
		CalibreText->SetJustification(ETextJustify::Center);
		UnitVariantSelectionWidgets::ApplyTextOnMetal(
			CalibreText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, CalibreFallbackFontSize),
			M_Style.MutedTextColor);
		UScaleBox* const CalibreScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		CalibreScale->SetStretch(EStretch::ScaleToFit);
		CalibreScale->SetStretchDirection(EStretchDirection::DownOnly);
		CalibreScale->SetContent(CalibreText);
		Icon = CalibreScale;
	}
	const FText Title = Weapon.Count > 1
		                    ? FText::Format(NSLOCTEXT("UnitVariantSelection", "WeaponCount", "{0}\u00D7 {1}"),
		                                    FText::AsNumber(Weapon.Count), Weapon.DisplayName)
		                    : Weapon.DisplayName;
	// Pips turn green or red where the weapon beats or trails the Base weapon it replaces.
	const auto TrendAgainstBase = [BaseWeapon](const float Value, const float BaseValue, const bool bHigherIsBetter)
	{
		return BaseWeapon != nullptr
			       ? UnitVariantStatComparison::GetTrend(Value, BaseValue, bHigherIsBetter)
			       : EUnitVariantStatTrend::Same;
	};
	const float NoBaseValue = 0.f;
	UVerticalBox* const StatBars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	StatBars->AddChildToVerticalBox(BuildStatBar(
		M_DamageStatIcon, UnitVariantStatComparison::GetDamageLevel(Weapon.Damage),
		TrendAgainstBase(Weapon.Damage, BaseWeapon != nullptr ? BaseWeapon->Damage : NoBaseValue, true)))->SetPadding(
		StatBarsPadding);
	StatBars->AddChildToVerticalBox(BuildStatBar(
		M_PenetrationStatIcon, UnitVariantStatComparison::GetPenetrationLevel(Weapon.ArmorPen),
		TrendAgainstBase(Weapon.ArmorPen, BaseWeapon != nullptr ? BaseWeapon->ArmorPen : NoBaseValue, true)))->
		SetPadding(FMargin(0.f, StatBarSpacing, 0.f, 0.f));
	StatBars->AddChildToVerticalBox(BuildStatBar(
		M_ReloadStatIcon, UnitVariantStatComparison::GetReloadLevel(Weapon.ReloadSeconds),
		TrendAgainstBase(Weapon.ReloadSeconds, BaseWeapon != nullptr ? BaseWeapon->ReloadSeconds : NoBaseValue,
		                 false)))->SetPadding(FMargin(0.f, StatBarSpacing, 0.f, 0.f));
	AddIconRow(Icon, WeaponIconSize, Title, Change, StatBars);
}

UWidget* UW_UnitVariantDetailPanel::BuildStatBar(UTexture2D* StatIcon, const int32 Level,
                                                 const EUnitVariantStatTrend Trend)
{
	using namespace UnitVariantDetailPanelLayout;
	UHorizontalBox* const Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(StatIconSize);
	IconBox->SetHeightOverride(StatIconSize);
	if (IsValid(StatIcon))
	{
		UImage* const IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(StatIcon);
		IconImage->SetColorAndOpacity(M_Style.DescriptionColor.GetSpecifiedColor());
		IconBox->SetContent(IconImage);
	}
	UHorizontalBoxSlot* const IconSlot = Bar->AddChildToHorizontalBox(IconBox);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(StatIconPadding);
	const FSlateColor& FilledPipStyleColor = Trend == EUnitVariantStatTrend::Better
		                                         ? M_Style.BetterColor
		                                         : Trend == EUnitVariantStatTrend::Worse
		                                         ? M_Style.RemovedColor
		                                         : M_Style.AccentColor;
	const FLinearColor FilledPipColor = FilledPipStyleColor.GetSpecifiedColor();
	for (int32 PipIndex = 0; PipIndex < UnitVariantStatComparison::MaxStatLevel; ++PipIndex)
	{
		USizeBox* const PipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PipBox->SetWidthOverride(StatPipSize.X);
		PipBox->SetHeightOverride(StatPipSize.Y);
		// The default brush is a plain white quad; the tint makes it a filled or an empty pip.
		UImage* const Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetColorAndOpacity(PipIndex < Level ? FilledPipColor : EmptyPipColor);
		PipBox->SetContent(Pip);
		UHorizontalBoxSlot* const PipSlot = Bar->AddChildToHorizontalBox(PipBox);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(0.f, 0.f, StatPipGap, 0.f));
	}
	return Bar;
}

void UW_UnitVariantDetailPanel::AddStatRow(const FUnitVariantStatRow& StatRow)
{
	using namespace UnitVariantDetailPanelLayout;
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(StatRow.Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.BodyFont, BodyFontSize), M_Style.DescriptionColor);
	Row->AddChildToHorizontalBox(LabelText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// One arrow per step of change, pointing the way the stat moved; green when that helps the player.
	const FString Arrow = StatRow.bIsIncrease ? TEXT("\u25B2") : TEXT("\u25BC");
	UTextBlock* const ArrowsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ArrowsText->SetText(FText::FromString(FString::ChrN(StatRow.Steps, Arrow[0])));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		ArrowsText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, BodyFontSize),
		StatRow.bIsBetter ? M_Style.BetterColor : M_Style.RemovedColor);
	Row->AddChildToHorizontalBox(ArrowsText)->SetPadding(ChangeArrowsPadding);
	AddToSection(Row);
}

void UW_UnitVariantDetailPanel::AddIconRow(UWidget* Icon, const FVector2D& IconSize, const FText& Title,
                                           const EUnitVariantChange Change, UWidget* Detail)
{
	using namespace UnitVariantDetailPanelLayout;
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(IconSize.X);
	IconBox->SetHeightOverride(IconSize.Y);
	UBorder* const IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconWell->SetBrush(M_Style.UnitIconWell);
	IconWell->SetPadding(IconWellPadding);
	IconWell->SetContent(Icon);
	IconBox->SetContent(IconWell);
	Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* const TextSlot = Row->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(RowTextPadding);
	TextColumn->AddChildToVerticalBox(BuildTitleRow(Title, Change))->SetPadding(TitlePadding);
	TextColumn->AddChildToVerticalBox(Detail);
	if (Change == EUnitVariantChange::Removed)
	{
		Row->SetRenderOpacity(RemovedRowOpacity);
	}
	AddToSection(Row);
}

UWidget* UW_UnitVariantDetailPanel::BuildTitleRow(const FText& Title, const EUnitVariantChange Change)
{
	using namespace UnitVariantDetailPanelLayout;
	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Title.ToUpper());
	TitleText->SetAutoWrapText(true);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, RowTitleFontSize), M_Style.TextColor);
	TitleRow->AddChildToHorizontalBox(TitleText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	if (Change == EUnitVariantChange::Unchanged)
	{
		return TitleRow;
	}
	const bool bIsGained = Change == EUnitVariantChange::Gained;
	UTextBlock* const ChangeTag = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ChangeTag->SetText(bIsGained
		                   ? NSLOCTEXT("UnitVariantSelection", "Gained", "NEW")
		                   : NSLOCTEXT("UnitVariantSelection", "Removed", "REMOVED"));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		ChangeTag, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, ChangeTagFontSize, LabelLetterSpacing),
		bIsGained ? M_Style.AccentColor : M_Style.RemovedColor);
	UHorizontalBoxSlot* const TagSlot = TitleRow->AddChildToHorizontalBox(ChangeTag);
	TagSlot->SetVerticalAlignment(VAlign_Top);
	TagSlot->SetPadding(ChangeTagPadding);
	return TitleRow;
}

void UW_UnitVariantDetailPanel::AddSectionLabel(const FText& Label)
{
	using namespace UnitVariantDetailPanelLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, LabelFontSize, LabelLetterSpacing),
		M_Style.LabelColor);
	M_SectionColumn->AddChildToVerticalBox(LabelText)->SetPadding(SectionLabelPadding);
	bM_NextRowFollowsLabel = true;
}

void UW_UnitVariantDetailPanel::AddMutedLine(const FText& Text)
{
	using namespace UnitVariantDetailPanelLayout;
	UTextBlock* const MutedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MutedText->SetText(Text);
	MutedText->SetAutoWrapText(true);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		MutedText, UnitVariantSelectionWidgets::MakeFont(M_Style.BodyFont, BodyFontSize), M_Style.MutedTextColor);
	M_SectionColumn->AddChildToVerticalBox(MutedText);
}

void UW_UnitVariantDetailPanel::AddToSection(UWidget* Row)
{
	using namespace UnitVariantDetailPanelLayout;
	// The first row of a section sits right under its label; later rows keep the row spacing.
	M_SectionColumn->AddChildToVerticalBox(Row)->SetPadding(
		FMargin(0.f, bM_NextRowFollowsLabel ? 0.f : RowSpacing, 0.f, 0.f));
	bM_NextRowFollowsLabel = false;
}

bool UW_UnitVariantDetailPanel::GetIsValidPlate() const
{
	if (IsValid(M_Plate) && IsValid(M_SectionColumn) && IsValid(M_VariantNameText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Plate", "GetIsValidPlate", this);
	return false;
}
