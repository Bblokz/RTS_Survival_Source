// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SpacedArmourTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SpacedArmourTakeoverLayout
{
	// Sizes are for the 1080p reference height, inside the base frame's 1240 wide panel; paddings and fonts match the
	// cover takeover's cards so the field manuals read alike.
	constexpr float CardGap = 20.f;
	const FMargin GlassPanePadding(14.f, 14.f, 14.f, 18.f);
	// The render is imported at twice this size, so it stays crisp on 4K screens.
	const FVector2D RenderSize(640.f, 360.f);
	const FMargin RenderWellPadding(6.f);
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 22.f);
	const FMargin CaptionPadding(0.f, 12.f, 0.f, 0.f);
	const FMargin FirstStateLabelPadding(0.f, 0.f, 0.f, 2.f);
	const FMargin StateLabelPadding(0.f, 18.f, 0.f, 2.f);
	const FMargin StateTitlePadding(0.f, 0.f, 0.f, 4.f);
	constexpr float CaptionWrapWidth = 640.f;
	constexpr float StateTextWrapWidth = 440.f;
	constexpr int32 StateLabelFontSize = 12;
	constexpr int32 StateLabelLetterSpacing = 100;
	constexpr int32 StateTitleFontSize = 19;
	constexpr int32 BodyFontSize = 15;

	// The cover takeover's green, yellow and red, here for an intact, damaged and stripped side.
	const FLinearColor IntactColor(0.f, 1.f, 0.f);
	const FLinearColor DamagedColor(0.54f, 0.547f, 0.f);
	const FLinearColor StrippedColor(1.f, 0.f, 0.f);

	const TCHAR* const RenderAssetPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/SpacedArmour/T_SpacedArmour_PlateShotOff."
		"T_SpacedArmour_PlateShotOff");
}

UW_SpacedArmourTakeover::UW_SpacedArmourTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_PlateShotOffRender = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(SpacedArmourTakeoverLayout::RenderAssetPath));
}

FText UW_SpacedArmourTakeover::GetSpacedArmourTitle()
{
	return NSLOCTEXT("SpacedArmourTakeover", "Title", "Spaced Armour");
}

FText UW_SpacedArmourTakeover::GetSpacedArmourSummary()
{
	return NSLOCTEXT("SpacedArmourTakeover", "ArchiveSummary",
	                 "Some tanks carry thin plates on their sides. They add to the side armour, but hits knock them "
	                 "off, even shots that bounce off the hull without harming it.\n\n"
	                 "Intact: every plate in place, the side gets the plates' full armour on top of the hull's own.\n\n"
	                 "Damaged: some plates are gone and the side keeps half the bonus. Explosions nearby also shake "
	                 "plates loose; rifle and machine gun fire cannot.\n\n"
	                 "Stripped: no plates left, only the bare hull. Repair the tank to put them back.");
}

void UW_SpacedArmourTakeover::SetupSpacedArmour()
{
	HideKindLabel();
	SetTitleText(GetSpacedArmourTitle());
	SetDescriptionText(NSLOCTEXT("SpacedArmourTakeover", "Description",
	                             "Some tanks carry thin plates on their sides. They add to the side armour, but hits "
	                             "knock them off, even shots that bounce off the hull without harming it."));
	BuildBody();
}

TArray<FSpacedArmourTakeoverState> UW_SpacedArmourTakeover::MakeStates() const
{
	// The add-on armour zone's states (UVehicleModuleAddOnArmorBehaviour): healthy gives the full add-on armour, yellow
	// half, red none. Every side plate rule rolls add-on damage on non-penetrating hits too (EModuleNonPenPolicy::
	// External); hits under 20 mm calibre never roll it.
	using namespace SpacedArmourTakeoverLayout;
	TArray<FSpacedArmourTakeoverState> States;
	FSpacedArmourTakeoverState& IntactState = States.AddDefaulted_GetRef();
	IntactState.StateLabel = NSLOCTEXT("SpacedArmourTakeover", "IntactLabel", "INTACT");
	IntactState.StateLabelColor = IntactColor;
	IntactState.Title = NSLOCTEXT("SpacedArmourTakeover", "IntactTitle", "Extra side armour");
	IntactState.Body = NSLOCTEXT("SpacedArmourTakeover", "IntactBody",
	                             "The plates' armour counts on top of the hull's own side armour.");

	FSpacedArmourTakeoverState& DamagedState = States.AddDefaulted_GetRef();
	DamagedState.StateLabel = NSLOCTEXT("SpacedArmourTakeover", "DamagedLabel", "DAMAGED");
	DamagedState.StateLabelColor = DamagedColor;
	DamagedState.Title = NSLOCTEXT("SpacedArmourTakeover", "DamagedTitle", "Half the bonus");
	DamagedState.Body = NSLOCTEXT("SpacedArmourTakeover", "DamagedBody",
	                              "Any hit can knock plates loose, even one that bounces off. Explosions nearby do it "
	                              "too; rifle and machine gun fire cannot.");

	FSpacedArmourTakeoverState& StrippedState = States.AddDefaulted_GetRef();
	StrippedState.StateLabel = NSLOCTEXT("SpacedArmourTakeover", "StrippedLabel", "STRIPPED");
	StrippedState.StateLabelColor = StrippedColor;
	StrippedState.Title = NSLOCTEXT("SpacedArmourTakeover", "StrippedTitle", "No bonus");
	StrippedState.Body = NSLOCTEXT("SpacedArmourTakeover", "StrippedBody",
	                               "Only the bare hull is left on that side. Repair the tank to put the plates back.");
	return States;
}

void UW_SpacedArmourTakeover::BuildBody()
{
	using namespace SpacedArmourTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_LoadedRender = nullptr;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BodyColumn->AddChildToVerticalBox(CardRow);
	UHorizontalBoxSlot* const RenderSlot = CardRow->AddChildToHorizontalBox(BuildRenderCard());
	RenderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	RenderSlot->SetPadding(FMargin(0.f, 0.f, CardGap, 0.f));
	UHorizontalBoxSlot* const StatesSlot = CardRow->AddChildToHorizontalBox(BuildStatesCard());
	StatesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

UWidget* UW_SpacedArmourTakeover::BuildRenderCard()
{
	using namespace SpacedArmourTakeoverLayout;
	UVerticalBox* const CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder* const RenderWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RenderWell->SetBrush(M_Style.Skin.CardIconWell);
	RenderWell->SetPadding(RenderWellPadding);
	USizeBox* const RenderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RenderBox->SetWidthOverride(RenderSize.X);
	RenderBox->SetHeightOverride(RenderSize.Y);
	RenderWell->SetContent(RenderBox);
	CardColumn->AddChildToVerticalBox(RenderWell)->SetHorizontalAlignment(HAlign_Center);

	UTexture2D* const LoadedRender = M_PlateShotOffRender.LoadSynchronous();
	if (IsValid(LoadedRender))
	{
		M_LoadedRender = LoadedRender;
		UImage* const RenderImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		RenderImage->SetBrushFromTexture(LoadedRender, false);
		RenderBox->SetContent(RenderImage);
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Spaced armour takeover: render %s could not be loaded; the card shows an empty well."),
			*M_PlateShotOffRender.ToString()));
	}

	UTextBlock* const CaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CaptionText->SetText(NSLOCTEXT("SpacedArmourTakeover", "RenderCaption",
	                               "A hit tears a side plate off a Panzer II Luchs. The gap stays until the tank is "
	                               "repaired."));
	CaptionText->SetWrapTextAt(CaptionWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CaptionText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn->AddChildToVerticalBox(CaptionText)->SetPadding(CaptionPadding);
	return WrapInGlassPane(CardColumn);
}

UWidget* UW_SpacedArmourTakeover::BuildStatesCard()
{
	UVerticalBox* const StatesColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const TArray<FSpacedArmourTakeoverState> States = MakeStates();
	for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
	{
		AddStateText(*StatesColumn, States[StateIndex], StateIndex == 0);
	}
	return WrapInGlassPane(StatesColumn);
}

UWidget* UW_SpacedArmourTakeover::WrapInGlassPane(UWidget* Content) const
{
	// The vehicle takeover's facts pane: blurred glass over the plate.
	UBackgroundBlur* const GlassBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
	GlassBlur->SetBlurStrength(M_Style.Skin.VariantSectionBlurStrength);
	GlassBlur->SetLowQualityFallbackBrush(FSlateNoResource());
	GlassBlur->SetPadding(FMargin(0.f));
	GlassBlur->SetHorizontalAlignment(HAlign_Fill);
	GlassBlur->SetVerticalAlignment(VAlign_Fill);
	UBorder* const GlassPane = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GlassPane->SetBrush(M_Style.Skin.VariantSectionGlass);
	GlassPane->SetPadding(SpacedArmourTakeoverLayout::GlassPanePadding);
	GlassPane->SetContent(Content);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

void UW_SpacedArmourTakeover::AddStateText(UVerticalBox& StatesColumn, const FSpacedArmourTakeoverState& State,
                                           const bool bIsFirst) const
{
	using namespace SpacedArmourTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(State.StateLabel);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StateLabelFontSize,
		                                                 StateLabelLetterSpacing), FSlateColor(State.StateLabelColor));
	StatesColumn.AddChildToVerticalBox(LabelText)->SetPadding(bIsFirst ? FirstStateLabelPadding : StateLabelPadding);

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(State.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StateTitleFontSize),
		M_Style.Skin.TextColor);
	StatesColumn.AddChildToVerticalBox(TitleText)->SetPadding(StateTitlePadding);

	UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(State.Body);
	BodyText->SetWrapTextAt(StateTextWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		BodyText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	StatesColumn.AddChildToVerticalBox(BodyText);
}
