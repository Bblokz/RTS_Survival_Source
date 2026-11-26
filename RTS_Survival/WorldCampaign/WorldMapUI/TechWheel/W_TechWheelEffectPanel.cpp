// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/W_TechWheelEffectPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantStatComparison.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantWeaponWidgets.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelEffectProbes.h"

#define LOCTEXT_NAMESPACE "WorldTechWheelEffect"

namespace TechWheelEffectPanelLayout
{
	constexpr int32 LabelFontSize = 8;
	constexpr int32 LabelLetterSpacing = 200;
	constexpr int32 UnitNameFontSize = 9;
	constexpr int32 WeaponNameFontSize = 10;
	constexpr int32 StatusFontSize = 10;
	const FVector2D UnitIconSize(30.f, 30.f);
	const FMargin UnitChipPadding(4.f, 3.f, 9.f, 3.f);
	const FMargin UnitChipGap(0.f, 0.f, 6.f, 6.f);
	const FMargin UnitNamePadding(7.f, 0.f, 0.f, 0.f);
	const FVector2D WeaponIconSize(46.f, 46.f);
	const FMargin WeaponBlockPadding(0.f, 0.f, 0.f, 12.f);
	const FMargin WeaponRowsPadding(12.f, 0.f, 0.f, 0.f);
	constexpr float SectionTopPadding = 14.f;
	constexpr float LabelBottomPadding = 6.f;
	// The pips of the unit variant screen (UnitVariantWeaponWidgets).
	constexpr float StatIconSize = 16.f;
	const FMargin StatIconPadding(0.f, 0.f, 6.f, 0.f);
	const FVector2D StatPipSize(9.f, 10.f);
	constexpr float StatPipGap = 2.f;
	const FMargin StatBarPadding(0.f, 4.f, 0.f, 0.f);
	const FLinearColor EmptyPipColor(1.f, 1.f, 1.f, 0.12f);
	const FLinearColor UnitChipColor(1.f, 1.f, 1.f, 0.05f);
	// Lists longer than this only repeat near-identical guns.
	constexpr int32 MaxWeaponBlocks = 5;
}

void UW_TechWheelEffectPanel::SetupPanel(const FWorldCampaignUILook& Look)
{
	M_Look = &Look;
	M_Probes = NewObject<UTechWheelEffectProbes>(this);
	LoadSharedAssets();
	BuildWidgetTree();
}

void UW_TechWheelEffectPanel::LoadSharedAssets()
{
	M_WeaponStyle.HeadingFont = M_Look->Deck.HeadingFont;
	M_WeaponStyle.BodyFont = M_Look->Deck.BodyFont;
	M_WeaponStyle.MutedTextColor = M_Look->Deck.Style.MutedTextColor;
	M_WeaponStyle.DescriptionColor = M_Look->Deck.Style.DescriptionColor;
	M_WeaponStyle.AccentColor = FSlateColor(WorldCampaignUIColors::Accent);

	M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	M_WeaponCardTable = M_WeaponStyle.WeaponCardTable.LoadSynchronous();
	const UnitVariantWeaponWidgets::FStatIcons StatIcons = UnitVariantWeaponWidgets::LoadStatIcons(M_WeaponStyle);
	M_DamageStatIcon = StatIcons.Damage;
	M_PenetrationStatIcon = StatIcons.Penetration;
	M_ReloadStatIcon = StatIcons.Reload;
	M_MagazineStatIcon = StatIcons.Magazine;
}

void UW_TechWheelEffectPanel::BuildWidgetTree()
{
	using namespace TechWheelEffectPanelLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	WidgetTree->RootWidget = Scroll;
	M_Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(M_Column);

	AddSectionLabel(LOCTEXT("Affects", "AFFECTS"));
	M_UnitsBox = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	M_Column->AddChildToVerticalBox(M_UnitsBox);

	M_WeaponsLabel = AddSectionLabel(LOCTEXT("Weapons", "WEAPONS WITH THIS TECHNOLOGY"));
	M_StatusText = DeckBuilderWidgets::MakeText(
		*WidgetTree, LOCTEXT("Reading", "Reading weapon data..."),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, StatusFontSize), M_Look->Deck.Style.MutedTextColor);
	M_Column->AddChildToVerticalBox(M_StatusText);
	M_WeaponsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_Column->AddChildToVerticalBox(M_WeaponsBox);
	M_Column->SetVisibility(ESlateVisibility::Collapsed);
}

UTextBlock* UW_TechWheelEffectPanel::AddSectionLabel(const FText& Label)
{
	using namespace TechWheelEffectPanelLayout;
	UTextBlock* const LabelText = DeckBuilderWidgets::MakeText(
		*WidgetTree, Label, DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, LabelFontSize, LabelLetterSpacing),
		M_Look->Deck.Style.MutedTextColor);
	M_Column->AddChildToVerticalBox(LabelText)->SetPadding(
		FMargin(0.f, SectionTopPadding, 0.f, LabelBottomPadding));
	return LabelText;
}

void UW_TechWheelEffectPanel::ShowTechnology(const ETechnology Technology)
{
	if (Technology == M_ShownTechnology || not GetIsValidColumn() || not GetIsValidProbes())
	{
		return;
	}

	ClearPanel();
	M_ShownTechnology = Technology;
	UClass* const EffectClass = TechWheelEffects::LoadEffectClass(Technology);
	if (not IsValid(EffectClass))
	{
		return;
	}

	const UTechnologyEffect& Effect = *EffectClass->GetDefaultObject<UTechnologyEffect>();
	const TArray<FTrainingOption> Units = TechWheelEffects::GetAffectedUnits(Effect);
	if (Units.IsEmpty())
	{
		return;
	}

	for (const FTrainingOption& Unit : Units)
	{
		AddUnitChip(Unit);
	}

	M_Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UWorld* const World = GetWorld();
	if (not IsValid(World) || not TechWheelEffects::GetCanChangeWeapons(Effect))
	{
		return;
	}

	M_WeaponsLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	TWeakObjectPtr<UW_TechWheelEffectPanel> WeakThis(this);
	M_Probes->ProbeTechnology(*World, Technology, [WeakThis, Technology]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HandleProbesRead(Technology);
		}
	});
}

void UW_TechWheelEffectPanel::ClearPanel()
{
	M_ShownTechnology = ETechnology::Tech_NONE;
	if (IsValid(M_Probes))
	{
		M_Probes->CancelProbes();
	}

	if (not GetIsValidColumn())
	{
		return;
	}

	M_UnitsBox->ClearChildren();
	M_WeaponsBox->ClearChildren();
	M_WeaponsLabel->SetVisibility(ESlateVisibility::Collapsed);
	M_StatusText->SetVisibility(ESlateVisibility::Collapsed);
	M_Column->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_TechWheelEffectPanel::NativeDestruct()
{
	if (IsValid(M_Probes))
	{
		M_Probes->CancelProbes();
	}

	// A probe cancelled here is read again when the page comes back.
	M_ShownTechnology = ETechnology::Tech_NONE;
	Super::NativeDestruct();
}

void UW_TechWheelEffectPanel::AddUnitChip(const FTrainingOption& Unit)
{
	using namespace TechWheelEffectPanelLayout;
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (const USlateBrushAsset* const IconBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit))
	{
		USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(UnitIconSize.X);
		IconBox->SetHeightOverride(UnitIconSize.Y);
		IconBox->SetContent(DeckBuilderWidgets::MakeImage(
			*WidgetTree, DeckBuilderWidgets::MakePictureBrush(IconBrush->Brush, UnitIconSize)));
		Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
	}

	FText UnitName = UUnitVariantPresentationLibrary::FindTrainingTableTitle(M_TrainingOptionsTable, Unit);
	if (UnitName.IsEmpty())
	{
		UnitName = UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(Unit);
	}

	UTextBlock* const NameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, UnitName, DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, UnitNameFontSize),
		M_Look->Deck.Style.TextColor);
	UHorizontalBoxSlot* const NameSlot = Row->AddChildToHorizontalBox(NameText);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(UnitNamePadding);

	UBorder* const Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Chip->SetBrush(FWorldCampaignUILook::MakeFlatBrush(UnitChipColor));
	Chip->SetPadding(UnitChipPadding);
	Chip->SetContent(Row);
	M_UnitsBox->AddChildToWrapBox(Chip)->SetPadding(UnitChipGap);
}

void UW_TechWheelEffectPanel::HandleProbesRead(const ETechnology Technology)
{
	using namespace TechWheelEffectPanelLayout;
	if (Technology != M_ShownTechnology || not GetIsValidColumn() || not GetIsValidProbes())
	{
		return;
	}

	M_StatusText->SetVisibility(ESlateVisibility::Collapsed);
	const FTechWheelEffectResult* const Result = M_Probes->FindResult(Technology);
	if (Result == nullptr || Result->WeaponChanges.IsEmpty())
	{
		M_WeaponsLabel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Vehicles often carry the same gun under their own weapon entry; one block per gun name is enough.
	TArray<FString> ShownWeaponNames;
	for (const FTechWheelWeaponChange& Change : Result->WeaponChanges)
	{
		const FString WeaponName = Change.After.DisplayName.ToString();
		if (ShownWeaponNames.Contains(WeaponName) || ShownWeaponNames.Num() >= MaxWeaponBlocks)
		{
			continue;
		}

		ShownWeaponNames.Add(WeaponName);
		AddWeaponBlock(Change);
	}
}

void UW_TechWheelEffectPanel::AddWeaponBlock(const FTechWheelWeaponChange& Change)
{
	using namespace TechWheelEffectPanelLayout;
	UHorizontalBox* const Block = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(WeaponIconSize.X);
	IconBox->SetHeightOverride(WeaponIconSize.Y);
	IconBox->SetContent(UnitVariantWeaponWidgets::BuildWeaponIcon(
		*WidgetTree, M_WeaponStyle, M_WeaponCardTable, Change.After));
	Block->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Rows->AddChildToVerticalBox(DeckBuilderWidgets::MakeText(
		*WidgetTree, UnitVariantWeaponWidgets::GetWeaponTitle(Change.After),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, WeaponNameFontSize), M_Look->Deck.Style.TextColor));
	AddChangedStatBars(*Rows, Change);
	Block->AddChildToHorizontalBox(Rows)->SetPadding(WeaponRowsPadding);
	M_WeaponsBox->AddChildToVerticalBox(Block)->SetPadding(WeaponBlockPadding);
}

void UW_TechWheelEffectPanel::AddChangedStatBars(UVerticalBox& Rows, const FTechWheelWeaponChange& Change)
{
	using namespace TechWheelEffectPanelLayout;
	using namespace UnitVariantStatComparison;
	struct FStatRow
	{
		UTexture2D* Icon;
		int32 BeforeLevel;
		int32 AfterLevel;
		EUnitVariantStatTrend Trend;
	};
	const FUnitVariantWeaponStats& Before = Change.Before;
	const FUnitVariantWeaponStats& After = Change.After;
	const FStatRow StatRows[] = {
		{M_DamageStatIcon, GetDamageLevel(Before.Damage), GetDamageLevel(After.Damage),
		 GetTrend(After.Damage, Before.Damage, true)},
		{M_PenetrationStatIcon, GetPenetrationLevel(Before.ArmorPen), GetPenetrationLevel(After.ArmorPen),
		 GetTrend(After.ArmorPen, Before.ArmorPen, true)},
		{M_ReloadStatIcon, GetReloadLevel(Before.ReloadSeconds), GetReloadLevel(After.ReloadSeconds),
		 GetTrend(After.ReloadSeconds, Before.ReloadSeconds, false)},
		{M_MagazineStatIcon, GetMagazineLevel(Before.MagCapacity), GetMagazineLevel(After.MagCapacity),
		 GetTrend(After.MagCapacity, Before.MagCapacity, true)}
	};
	for (const FStatRow& StatRow : StatRows)
	{
		if (StatRow.Trend == EUnitVariantStatTrend::Same)
		{
			continue;
		}

		Rows.AddChildToVerticalBox(BuildChangeBar(StatRow.Icon, StatRow.BeforeLevel, StatRow.AfterLevel,
		                                          StatRow.Trend))->SetPadding(StatBarPadding);
	}
}

UWidget* UW_TechWheelEffectPanel::BuildChangeBar(UTexture2D* StatIcon, const int32 BeforeLevel,
                                                 const int32 AfterLevel, const EUnitVariantStatTrend Trend)
{
	using namespace TechWheelEffectPanelLayout;
	const int32 MaxLevel = UnitVariantStatComparison::MaxStatLevel;
	// The 1-10 scale is coarse: a change too small to cross a level still moves one pip here.
	const int32 ShownAfterLevel = Trend == EUnitVariantStatTrend::Better
		                              ? FMath::Min(FMath::Max(AfterLevel, BeforeLevel + 1), MaxLevel)
		                              : FMath::Max(FMath::Min(AfterLevel, BeforeLevel - 1), 0);
	const int32 SharedLevel = FMath::Min(BeforeLevel, ShownAfterLevel);
	const FLinearColor SharedColor = M_WeaponStyle.AccentColor.GetSpecifiedColor();
	const FLinearColor GainedColor = M_WeaponStyle.BetterColor.GetSpecifiedColor();
	const FLinearColor LostColor = M_WeaponStyle.RemovedColor.GetSpecifiedColor();

	UHorizontalBox* const Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(StatIconSize);
	IconBox->SetHeightOverride(StatIconSize);
	if (IsValid(StatIcon))
	{
		UImage* const IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(StatIcon);
		IconImage->SetColorAndOpacity(M_WeaponStyle.DescriptionColor.GetSpecifiedColor());
		IconBox->SetContent(IconImage);
	}

	UHorizontalBoxSlot* const IconSlot = Bar->AddChildToHorizontalBox(IconBox);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(StatIconPadding);
	for (int32 PipIndex = 0; PipIndex < MaxLevel; ++PipIndex)
	{
		FLinearColor PipColor = EmptyPipColor;
		if (PipIndex < SharedLevel)
		{
			PipColor = SharedColor;
		}
		else if (PipIndex < ShownAfterLevel)
		{
			PipColor = GainedColor;
		}
		else if (PipIndex < BeforeLevel)
		{
			PipColor = LostColor;
		}

		USizeBox* const PipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PipBox->SetWidthOverride(StatPipSize.X);
		PipBox->SetHeightOverride(StatPipSize.Y);
		// The default brush is a plain white quad; the tint makes it a pip.
		UImage* const Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetColorAndOpacity(PipColor);
		PipBox->SetContent(Pip);
		UHorizontalBoxSlot* const PipSlot = Bar->AddChildToHorizontalBox(PipBox);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(0.f, 0.f, StatPipGap, 0.f));
	}

	return Bar;
}

bool UW_TechWheelEffectPanel::GetIsValidColumn() const
{
	if (IsValid(M_Column))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_Column"), TEXT("GetIsValidColumn"), this);
	return false;
}

bool UW_TechWheelEffectPanel::GetIsValidProbes() const
{
	if (IsValid(M_Probes))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_Probes"), TEXT("GetIsValidProbes"), this);
	return false;
}

#undef LOCTEXT_NAMESPACE
