// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_VehicleInfoTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantWeaponWidgets.h"

namespace VehicleInfoTakeoverLayout
{
	constexpr float FactsColumnWidth = 470.f;
	constexpr float ColumnGap = 36.f;
	const FVector2D PictureSize(470.f, 224.f);
	const FMargin PictureWellPadding(6.f);
	constexpr float GlassPaneSidePadding = 14.f;
	const FMargin GlassPanePadding(GlassPaneSidePadding, 12.f, GlassPaneSidePadding, 14.f);
	const FMargin GlassPaneTopGap(0.f, 14.f, 0.f, 0.f);
	const FMargin FirstSectionLabelPadding(0.f, 0.f, 0.f, 8.f);
	const FMargin SectionLabelPadding(0.f, 16.f, 0.f, 8.f);
	const FMargin DescriptionScrollPadding(0.f, 0.f, 12.f, 0.f);
	constexpr float SpecLabelWidth = 188.f;
	const FMargin SpecRowPadding(0.f, 0.f, 0.f, 6.f);
	constexpr float SpecValueGap = 10.f;
	const FMargin SpecValuePadding(SpecValueGap, 0.f, 0.f, 0.f);
	constexpr float WeaponIconWidth = 56.f;
	const FVector2D WeaponIconSize(WeaponIconWidth, WeaponIconWidth);
	const FMargin WeaponIconWellPadding(4.f);
	const FMargin WeaponRowPadding(0.f, 0.f, 0.f, 12.f);
	constexpr float WeaponTextGap = 14.f;
	const FMargin WeaponTextPadding(WeaponTextGap, 0.f, 0.f, 0.f);
	const FMargin WeaponTitlePadding(0.f, 0.f, 0.f, 4.f);
	constexpr int32 LabelFontSize = 12;
	constexpr int32 LabelLetterSpacing = 100;
	constexpr int32 SpecFontSize = 15;
	constexpr int32 WeaponTitleFontSize = 16;
	constexpr int32 MutedFontSize = 14;
	// The value column's width inside the glass pane; text wraps at a fixed width so a row has its full height on the
	// first frame (auto wrap measures with the previous frame's width).
	constexpr float SpecValueWrapWidth = FactsColumnWidth - 2.f * GlassPaneSidePadding - SpecLabelWidth - SpecValueGap;
	constexpr float WeaponTitleWrapWidth = FactsColumnWidth - 2.f * GlassPaneSidePadding - WeaponIconWidth - WeaponTextGap;
}

void UW_VehicleInfoTakeover::SetupVehicle(const FVehicleInfoTakeoverContent& Content)
{
	M_Content = Content;
	M_WeaponCardTable = M_Style.Skin.WeaponCardTable.LoadSynchronous();
	SetKindLabel(NSLOCTEXT("InfoTakeover", "VehicleKindLabel", "VEHICLE INTEL"));
	SetTitleText(Content.DisplayName);
	SetDescriptionText(Content.Description);
	BuildBody();
}

void UW_VehicleInfoTakeover::BuildBody()
{
	using namespace VehicleInfoTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	UHorizontalBox* const Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BodyColumn->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBoxSlot* const DescriptionSlot = Columns->AddChildToHorizontalBox(BuildDescriptionColumn());
	DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DescriptionSlot->SetPadding(FMargin(0.f, 0.f, ColumnGap, 0.f));
	Columns->AddChildToHorizontalBox(BuildFactsColumn());
}

UWidget* UW_VehicleInfoTakeover::BuildDescriptionColumn()
{
	using namespace VehicleInfoTakeoverLayout;
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddSectionLabel(*Column, NSLOCTEXT("InfoTakeover", "BriefingLabel", "BRIEFING"), true);
	// However much text the designer writes, it wraps to the column and scrolls inside the panel.
	UScrollBox* const DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	DescriptionScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock();
	if (IsValid(DescriptionBlock))
	{
		if (UScrollBoxSlot* const DescriptionSlot = Cast<UScrollBoxSlot>(DescriptionScroll->AddChild(DescriptionBlock)))
		{
			// Keeps the text clear of the scroll bar.
			DescriptionSlot->SetPadding(DescriptionScrollPadding);
		}
	}
	Column->AddChildToVerticalBox(DescriptionScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Column;
}

UWidget* UW_VehicleInfoTakeover::BuildFactsColumn()
{
	using namespace VehicleInfoTakeoverLayout;
	USizeBox* const ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetWidthOverride(FactsColumnWidth);
	UScrollBox* const FactsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ColumnBox->SetContent(FactsScroll);
	UVerticalBox* const FactsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	FactsScroll->AddChild(FactsColumn);
	if (M_Content.bHasImage)
	{
		FactsColumn->AddChildToVerticalBox(BuildPicture());
	}

	UVerticalBox* const PaneColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_SpecColumn = PaneColumn;
	AddSectionLabel(*PaneColumn, NSLOCTEXT("InfoTakeover", "SpecificationsLabel", "SPECIFICATIONS"), true);
	AddSpecRow(NSLOCTEXT("InfoTakeover", "ProductionYear", "Production year:"),
	           M_Content.ProductionYear.IsEmpty() ? NSLOCTEXT("InfoTakeover", "UnknownYear", "????")
	                                              : M_Content.ProductionYear);
	AddArmorRows();
	M_WeaponColumn = PaneColumn;
	AddWeaponRows();
	UVerticalBoxSlot* const PaneSlot = FactsColumn->AddChildToVerticalBox(BuildGlassPane(PaneColumn));
	PaneSlot->SetPadding(M_Content.bHasImage ? GlassPaneTopGap : FMargin(0.f));
	return ColumnBox;
}

UWidget* UW_VehicleInfoTakeover::BuildPicture()
{
	using namespace VehicleInfoTakeoverLayout;
	USizeBox* const PictureBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PictureBox->SetWidthOverride(PictureSize.X);
	PictureBox->SetHeightOverride(PictureSize.Y);
	UBorder* const PictureWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PictureWell->SetBrush(M_Style.Skin.CardIconWell);
	PictureWell->SetPadding(PictureWellPadding);
	PictureWell->SetHorizontalAlignment(HAlign_Center);
	PictureWell->SetVerticalAlignment(VAlign_Center);
	PictureBox->SetContent(PictureWell);
	// Square training icons and wide renders both fit without stretching.
	UScaleBox* const PictureScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	PictureScale->SetStretch(EStretch::ScaleToFit);
	UImage* const Picture = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (IsValid(M_Content.ImageTexture))
	{
		Picture->SetBrushFromTexture(M_Content.ImageTexture, true);
	}
	else
	{
		Picture->SetBrush(M_Content.ImageBrush);
	}
	PictureScale->SetContent(Picture);
	PictureWell->SetContent(MakePictureWithGlass(PictureScale));
	return PictureBox;
}

UWidget* UW_VehicleInfoTakeover::BuildGlassPane(UWidget* Content)
{
	using namespace VehicleInfoTakeoverLayout;
	UBackgroundBlur* const GlassBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
	GlassBlur->SetBlurStrength(M_Style.Skin.VariantSectionBlurStrength);
	GlassBlur->SetLowQualityFallbackBrush(FSlateNoResource());
	GlassBlur->SetPadding(FMargin(0.f));
	GlassBlur->SetHorizontalAlignment(HAlign_Fill);
	GlassBlur->SetVerticalAlignment(VAlign_Fill);
	UBorder* const GlassPane = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GlassPane->SetBrush(M_Style.Skin.VariantSectionGlass);
	GlassPane->SetPadding(GlassPanePadding);
	GlassPane->SetContent(Content);
	RegisterGlassPane(*GlassPane);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

void UW_VehicleInfoTakeover::AddSectionLabel(UVerticalBox& Column, const FText& Label, const bool bIsFirstInColumn)
{
	using namespace VehicleInfoTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, LabelFontSize, LabelLetterSpacing),
		M_Style.Skin.LabelColor);
	Column.AddChildToVerticalBox(LabelText)->SetPadding(bIsFirstInColumn
		                                                    ? FirstSectionLabelPadding
		                                                    : SectionLabelPadding);
}

void UW_VehicleInfoTakeover::AddSpecRow(const FText& Label, const FText& Value)
{
	using namespace VehicleInfoTakeoverLayout;
	if (not IsValid(M_SpecColumn))
	{
		return;
	}
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(SpecLabelWidth);
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetWrapTextAt(SpecLabelWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, SpecFontSize),
		M_Style.Skin.DescriptionColor);
	LabelBox->SetContent(LabelText);
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Top);

	UTextBlock* const ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ValueText->SetText(Value);
	ValueText->SetWrapTextAt(SpecValueWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		ValueText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, SpecFontSize),
		M_Style.Skin.TextColor);
	UHorizontalBoxSlot* const ValueSlot = Row->AddChildToHorizontalBox(ValueText);
	ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ValueSlot->SetPadding(SpecValuePadding);
	M_SpecColumn->AddChildToVerticalBox(Row)->SetPadding(SpecRowPadding);
}

void UW_VehicleInfoTakeover::AddArmorRows()
{
	const FInfoTakeoverArmorSummary& Armor = M_Content.Armor;
	const FText Unknown = NSLOCTEXT("InfoTakeover", "UnknownArmor", "-");
	FText FrontValue = Unknown;
	if (Armor.bHasFront)
	{
		const int32 FrontMin = FMath::RoundToInt(Armor.FrontMinMm);
		const int32 FrontMax = FMath::RoundToInt(Armor.FrontMaxMm);
		FrontValue = FrontMin == FrontMax
			             ? FormatMm(Armor.FrontMinMm)
			             : FText::Format(NSLOCTEXT("InfoTakeover", "FrontRange", "{0}-{1} mm"),
			                             FText::AsNumber(FrontMin), FText::AsNumber(FrontMax));
		if (Armor.FrontAddOnMaxMm > 0.f)
		{
			FrontValue = FText::Format(NSLOCTEXT("InfoTakeover", "FrontWithAddOn", "{0} (+{1} add-on)"),
			                           FrontValue, FormatMm(Armor.FrontAddOnMaxMm));
		}
	}
	AddSpecRow(NSLOCTEXT("InfoTakeover", "FrontalArmor", "Frontal armor:"), FrontValue);
	AddSpecRow(NSLOCTEXT("InfoTakeover", "WeakestSideArmor", "Weakest side armor:"),
	           Armor.WeakestSide.bIsValid ? FormatPlate(Armor.WeakestSide) : Unknown);
	AddSpecRow(NSLOCTEXT("InfoTakeover", "WeakestRearArmor", "Weakest rear armor:"),
	           Armor.WeakestRear.bIsValid ? FormatPlate(Armor.WeakestRear) : Unknown);
}

void UW_VehicleInfoTakeover::AddWeaponRows()
{
	using namespace VehicleInfoTakeoverLayout;
	if (not IsValid(M_WeaponColumn))
	{
		return;
	}
	AddSectionLabel(*M_WeaponColumn, NSLOCTEXT("InfoTakeover", "WeaponsLabel", "WEAPONS"), false);
	if (M_Content.Weapons.IsEmpty())
	{
		UTextBlock* const NoWeaponsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NoWeaponsText->SetText(NSLOCTEXT("InfoTakeover", "NoWeapons", "Unarmed."));
		UnitVariantSelectionWidgets::ApplyTextOnMetal(
			NoWeaponsText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, MutedFontSize),
			M_Style.Skin.MutedTextColor);
		M_WeaponColumn->AddChildToVerticalBox(NoWeaponsText);
		return;
	}
	const UnitVariantWeaponWidgets::FStatIcons StatIcons = UnitVariantWeaponWidgets::LoadStatIcons(M_Style.Skin);
	M_StatIcons = {StatIcons.Damage, StatIcons.Penetration, StatIcons.Reload, StatIcons.Magazine};
	for (const FUnitVariantWeaponStats& Weapon : M_Content.Weapons)
	{
		AddWeaponRow(Weapon);
	}
}

void UW_VehicleInfoTakeover::AddWeaponRow(const FUnitVariantWeaponStats& Weapon)
{
	using namespace VehicleInfoTakeoverLayout;
	UnitVariantWeaponWidgets::FStatIcons StatIcons;
	StatIcons.Damage = M_StatIcons.IsValidIndex(0) ? M_StatIcons[0].Get() : nullptr;
	StatIcons.Penetration = M_StatIcons.IsValidIndex(1) ? M_StatIcons[1].Get() : nullptr;
	StatIcons.Reload = M_StatIcons.IsValidIndex(2) ? M_StatIcons[2].Get() : nullptr;
	StatIcons.Magazine = M_StatIcons.IsValidIndex(3) ? M_StatIcons[3].Get() : nullptr;

	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(WeaponIconSize.X);
	IconBox->SetHeightOverride(WeaponIconSize.Y);
	UBorder* const IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconWell->SetBrush(M_Style.Skin.UnitIconWell);
	IconWell->SetPadding(WeaponIconWellPadding);
	IconWell->SetContent(UnitVariantWeaponWidgets::BuildWeaponIcon(*WidgetTree, M_Style.Skin, M_WeaponCardTable,
	                                                               Weapon));
	IconBox->SetContent(IconWell);
	Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(UnitVariantWeaponWidgets::GetWeaponTitle(Weapon).ToUpper());
	TitleText->SetWrapTextAt(WeaponTitleWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, WeaponTitleFontSize),
		M_Style.Skin.TextColor);
	TextColumn->AddChildToVerticalBox(TitleText)->SetPadding(WeaponTitlePadding);
	TextColumn->AddChildToVerticalBox(UnitVariantWeaponWidgets::BuildStatBars(
		*WidgetTree, M_Style.Skin, StatIcons, Weapon, nullptr));
	UHorizontalBoxSlot* const TextSlot = Row->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(WeaponTextPadding);
	M_WeaponColumn->AddChildToVerticalBox(Row)->SetPadding(WeaponRowPadding);
}

FText UW_VehicleInfoTakeover::FormatMm(const float Millimetres)
{
	return FText::Format(NSLOCTEXT("InfoTakeover", "Millimetres", "{0} mm"),
	                     FText::AsNumber(FMath::RoundToInt(Millimetres)));
}

FText UW_VehicleInfoTakeover::FormatPlate(const FInfoTakeoverWeakestPlate& Plate)
{
	const FText PlateMm = FText::Format(NSLOCTEXT("InfoTakeover", "PlateMm", "{0}, {1}"), Plate.PlateName,
	                                    FormatMm(Plate.ArmorMm));
	if (Plate.AddOnArmorMm <= 0.f)
	{
		return PlateMm;
	}
	return FText::Format(NSLOCTEXT("InfoTakeover", "PlateWithAddOn", "{0} (+{1} add-on)"), PlateMm,
	                     FormatMm(Plate.AddOnArmorMm));
}
