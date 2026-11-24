// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_BxpPackUpTakeover.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
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
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BxpPackUpTakeoverLayout
{
	// Sizes are for the 1080p reference height, inside the base frame's 1240 wide panel; laid out like the nomadic
	// conversion takeover (one large picture, its steps beside it).
	const FMargin GlassPanePadding(14.f, 14.f, 14.f, 18.f);
	// The illustration Blueprint is designed at this size; the well adds its frame around it.
	const FVector2D IllustrationSize(520.f, 292.f);
	const FMargin IllustrationWellPadding(6.f);
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 22.f);
	const FMargin FirstStepLabelPadding(0.f, 4.f, 0.f, 2.f);
	const FMargin StepLabelPadding(0.f, 12.f, 0.f, 2.f);
	const FMargin StepTitlePadding(0.f, 0.f, 0.f, 4.f);
	constexpr float SideTextGap = 28.f;
	// Text wraps at a fixed width so the page has its full height on the first frame.
	constexpr float StepTextWrapWidth = 560.f;
	constexpr int32 StepLabelFontSize = 12;
	constexpr int32 StepLabelLetterSpacing = 100;
	constexpr int32 StepTitleFontSize = 19;
	constexpr int32 StepBodyFontSize = 15;
	constexpr int32 LoopForever = 0;
	// The slot legend under the illustration.
	const FMargin SlotLegendPadding(0.f, 14.f, 0.f, 0.f);
	const FMargin SlotLegendLabelPadding(0.f, 0.f, 0.f, 6.f);
	const FMargin SlotLegendRowPadding(0.f, 0.f, 0.f, 6.f);
	const FMargin SlotLegendPictureWellPadding(3.f);
	constexpr float SlotLegendPictureSize = 40.f;
	constexpr float SlotLegendTextGap = 12.f;
	constexpr float SlotLegendTextWrapWidth = 460.f;

	const TCHAR* const PackUpIllustrationPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/BxpPackUp/WBP_BxpPack_Loop.WBP_BxpPack_Loop_C");
	// W_BuildingItem's pictures for BXS_BeingPackedUp, BXS_PackedUp and BXS_BeingUnpacked /
	// BXS_LookingForUnpackingLocation.
	const TCHAR* const SlotPackingPicturePath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/BuildingButtons/BuildingPlacement/T_IsPackingBXP_v002_0000."
		"T_IsPackingBXP_v002_0000");
	const TCHAR* const SlotPackedPicturePath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/BuildingButtons/BuildingPlacement/T_PlacePackedUpBuilding_v001_0000."
		"T_PlacePackedUpBuilding_v001_0000");
	const TCHAR* const SlotUnpackingPicturePath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/BuildingButtons/BuildingPlacement/T_CancelPackedUpBuilding_v001_0000."
		"T_CancelPackedUpBuilding_v001_0000");
}

UW_BxpPackUpTakeover::UW_BxpPackUpTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_PackUpIllustration = TSoftClassPtr<UUserWidget>(FSoftObjectPath(BxpPackUpTakeoverLayout::PackUpIllustrationPath));
}

FText UW_BxpPackUpTakeover::GetBxpPackUpTitle()
{
	return NSLOCTEXT("BxpPackUpTakeover", "Title", "Packing Up Expansions");
}

FText UW_BxpPackUpTakeover::GetBxpPackUpSummary()
{
	return NSLOCTEXT("BxpPackUpTakeover", "ArchiveSummary",
	                 "When a building packs up into its truck, its building expansions pack up with it. You keep them, "
	                 "and they go back up after the truck deploys again, at no cost.\n\n"
	                 "Pack up: every expansion folds away with the building, also one that was still being built. Its "
	                 "slot in the bottom panel keeps it and shows it as packed.\n\n"
	                 "On the road: while the building is a truck its expansion slots are greyed out, and packed "
	                 "expansions do nothing. If you cancel the pack-up, the expansions stay where they are.\n\n"
	                 "Attached expansions: expansions built onto the building, such as the HQ radar, rebuild in their "
	                 "old place by themselves as soon as the building has deployed. They take their normal build time "
	                 "and cost nothing.\n\n"
	                 "Free-standing expansions: expansions placed on their own, such as flak guns, wait in their slot. "
	                 "Click the slot (Place Packed Up Building) and place them near the building again, also for free. "
	                 "If you cancel, they stay packed.\n\n"
	                 "Only packed expansions are kept: an expansion that is destroyed is gone.");
}

void UW_BxpPackUpTakeover::SetupBxpPackUp()
{
	HideKindLabel();
	SetTitleText(GetBxpPackUpTitle());
	SetDescriptionText(NSLOCTEXT("BxpPackUpTakeover", "Description",
	                             "When a building packs up into its truck, its expansions pack up with it. You keep "
	                             "them: they go back up after the truck deploys again, at no cost."));
	BuildBody();
}

TArray<FBxpPackUpTakeoverStep> UW_BxpPackUpTakeover::MakeSteps() const
{
	// From the game: ANomadicVehicle::ExecuteConvertToVehicleCommand packs every live expansion
	// (StartPackUpAllExpansions), also one being built; OnFinishedConvertingToVehicle destroys them with status
	// BXS_PackedUp, so the slot keeps type, construction rules and socket (ResetEntryAndUpdateUI keeps them for packed).
	// Their storage, discount and cooldown bonuses are removed (FinishPackUpExpansion). While a truck, the slots are
	// disabled (UMainGameUI::OnTruckConverted). On deploy, OnConvertToBuilding_PlacePackedBxps re-places Socket and
	// AtBuildingOrigin expansions at once (BatchAsyncLoadAttachedPackedBxps); they rebuild over M_BuildingTime.
	// Free ones stay BXS_PackedUp until the slot is clicked (RebuildPackedExpansion -> preview); a packed placement is
	// never paid for (PlaceBxpIfAsyncLoaded) and a cancel keeps it packed.
	TArray<FBxpPackUpTakeoverStep> Steps;
	FBxpPackUpTakeoverStep& PackStep = Steps.AddDefaulted_GetRef();
	PackStep.Label = NSLOCTEXT("BxpPackUpTakeover", "PackLabel", "PACK UP");
	PackStep.Title = NSLOCTEXT("BxpPackUpTakeover", "PackTitle", "Expansions pack up too");
	PackStep.Body = NSLOCTEXT("BxpPackUpTakeover", "PackBody",
	                          "Every expansion folds away with the building, also one still being built. Its slot in "
	                          "the bottom panel keeps it, marked as packed.");

	FBxpPackUpTakeoverStep& RoadStep = Steps.AddDefaulted_GetRef();
	RoadStep.Label = NSLOCTEXT("BxpPackUpTakeover", "RoadLabel", "ON THE ROAD");
	RoadStep.Title = NSLOCTEXT("BxpPackUpTakeover", "RoadTitle", "Carried with the truck");
	RoadStep.Body = NSLOCTEXT("BxpPackUpTakeover", "RoadBody",
	                          "While it is a truck, the slots are greyed out and packed expansions do nothing. Cancel "
	                          "the pack-up and they stay where they are.");

	FBxpPackUpTakeoverStep& AutoStep = Steps.AddDefaulted_GetRef();
	AutoStep.Label = NSLOCTEXT("BxpPackUpTakeover", "AutoLabel", "AUTOMATIC");
	AutoStep.Title = NSLOCTEXT("BxpPackUpTakeover", "AutoTitle", "Attached ones come back by themselves");
	AutoStep.Body = NSLOCTEXT("BxpPackUpTakeover", "AutoBody",
	                          "Expansions built onto the building, like the HQ radar, rebuild in their old place as "
	                          "soon as it has deployed. They take their normal build time and cost nothing.");

	FBxpPackUpTakeoverStep& PlaceStep = Steps.AddDefaulted_GetRef();
	PlaceStep.Label = NSLOCTEXT("BxpPackUpTakeover", "PlaceLabel", "SLOT  /  LEFT CLICK");
	PlaceStep.Title = NSLOCTEXT("BxpPackUpTakeover", "PlaceTitle", "Place the others again");
	PlaceStep.Body = NSLOCTEXT("BxpPackUpTakeover", "PlaceBody",
	                           "Free-standing ones, like flak guns, wait in their slot. Click it (Place Packed Up "
	                           "Building) and place them near the building, also for free. Cancel and they stay packed.");
	return Steps;
}

TArray<FBxpPackUpSlotLegendEntry> UW_BxpPackUpTakeover::MakeSlotLegend()
{
	// W_BuildingItem::UpdateVisibleButtonState; the meanings follow its tooltips and UMainGameUI's slot clicks
	// (a packed slot starts the placing, a slot going back up cancels and stays packed).
	using namespace BxpPackUpTakeoverLayout;
	TArray<FBxpPackUpSlotLegendEntry> Legend;
	FBxpPackUpSlotLegendEntry& PackingEntry = Legend.AddDefaulted_GetRef();
	PackingEntry.SlotPicture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(SlotPackingPicturePath));
	PackingEntry.Meaning = NSLOCTEXT("BxpPackUpTakeover", "LegendPacking", "Packing up with the building.");

	FBxpPackUpSlotLegendEntry& PackedEntry = Legend.AddDefaulted_GetRef();
	PackedEntry.SlotPicture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(SlotPackedPicturePath));
	PackedEntry.Meaning = NSLOCTEXT("BxpPackUpTakeover", "LegendPacked",
	                                "Packed. Once deployed, click it to place the expansion again.");

	FBxpPackUpSlotLegendEntry& UnpackingEntry = Legend.AddDefaulted_GetRef();
	UnpackingEntry.SlotPicture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(SlotUnpackingPicturePath));
	UnpackingEntry.Meaning = NSLOCTEXT("BxpPackUpTakeover", "LegendUnpacking",
	                                   "Going back up. Click it to cancel; the expansion stays packed.");
	return Legend;
}

void UW_BxpPackUpTakeover::BuildBody()
{
	using namespace BxpPackUpTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_IllustrationWidget = nullptr;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	BodyColumn->AddChildToVerticalBox(BuildPage());
}

UWidget* UW_BxpPackUpTakeover::BuildPage()
{
	using namespace BxpPackUpTakeoverLayout;
	UHorizontalBox* const PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* const PictureColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PictureColumn->AddChildToVerticalBox(BuildIllustrationWell(M_PackUpIllustration, IllustrationSize));
	PictureColumn->AddChildToVerticalBox(BuildSlotLegend())->SetPadding(SlotLegendPadding);
	PageRow->AddChildToHorizontalBox(PictureColumn)->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const TArray<FBxpPackUpTakeoverStep> Steps = MakeSteps();
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		AddStepText(*TextColumn, Steps[StepIndex], StepIndex == 0);
	}
	UHorizontalBoxSlot* const TextSlot = PageRow->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(FMargin(SideTextGap, 0.f, 0.f, 0.f));
	return BuildGlassPane(PageRow);
}

UWidget* UW_BxpPackUpTakeover::BuildSlotLegend()
{
	using namespace BxpPackUpTakeoverLayout;
	M_SlotLegendPictureCount = 0;
	UVerticalBox* const LegendColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(NSLOCTEXT("BxpPackUpTakeover", "LegendLabel", "EXPANSION SLOT PICTURES"));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StepLabelFontSize,
		                                                 StepLabelLetterSpacing), M_Style.Skin.LabelColor);
	LegendColumn->AddChildToVerticalBox(LabelText)->SetPadding(SlotLegendLabelPadding);
	for (const FBxpPackUpSlotLegendEntry& Entry : MakeSlotLegend())
	{
		LegendColumn->AddChildToVerticalBox(BuildSlotLegendRow(Entry))->SetPadding(SlotLegendRowPadding);
	}
	return LegendColumn;
}

UWidget* UW_BxpPackUpTakeover::BuildSlotLegendRow(const FBxpPackUpSlotLegendEntry& Entry)
{
	using namespace BxpPackUpTakeoverLayout;
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* const PictureWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PictureWell->SetBrush(M_Style.Skin.CardIconWell);
	PictureWell->SetPadding(SlotLegendPictureWellPadding);
	USizeBox* const PictureBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PictureBox->SetWidthOverride(SlotLegendPictureSize);
	PictureBox->SetHeightOverride(SlotLegendPictureSize);
	PictureWell->SetContent(PictureBox);
	if (UTexture2D* const LoadedPicture = Entry.SlotPicture.LoadSynchronous())
	{
		UImage* const PictureImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		PictureImage->SetBrushFromTexture(LoadedPicture, false);
		PictureBox->SetContent(PictureImage);
		++M_SlotLegendPictureCount;
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Building expansion pack up takeover: slot picture %s could not be loaded."),
			*Entry.SlotPicture.ToString()));
	}
	Row->AddChildToHorizontalBox(PictureWell)->SetVerticalAlignment(VAlign_Center);

	UTextBlock* const MeaningText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MeaningText->SetText(Entry.Meaning);
	MeaningText->SetWrapTextAt(SlotLegendTextWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		MeaningText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, StepBodyFontSize),
		M_Style.Skin.DescriptionColor);
	UHorizontalBoxSlot* const TextSlot = Row->AddChildToHorizontalBox(MeaningText);
	TextSlot->SetPadding(FMargin(SlotLegendTextGap, 0.f, 0.f, 0.f));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	return Row;
}

UWidget* UW_BxpPackUpTakeover::BuildGlassPane(UWidget* Content)
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
	GlassPane->SetPadding(BxpPackUpTakeoverLayout::GlassPanePadding);
	GlassPane->SetContent(Content);
	RegisterGlassPane(*GlassPane);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

UWidget* UW_BxpPackUpTakeover::BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass,
                                                     const FVector2D& DesignedSize)
{
	using namespace BxpPackUpTakeoverLayout;
	UBorder* const IllustrationWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IllustrationWell->SetBrush(M_Style.Skin.CardIconWell);
	IllustrationWell->SetPadding(IllustrationWellPadding);
	USizeBox* const IllustrationBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IllustrationBox->SetWidthOverride(DesignedSize.X);
	IllustrationBox->SetHeightOverride(DesignedSize.Y);
	// The truck, the previews and the range disc move over the picture; only the illustration's own area may show.
	IllustrationBox->SetClipping(EWidgetClipping::ClipToBounds);
	IllustrationWell->SetContent(IllustrationBox);

	UClass* const LoadedClass = IllustrationClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Building expansion pack up takeover: illustration %s could not be loaded; the picture well stays "
				"empty."), *IllustrationClass.ToString()));
		return IllustrationWell;
	}
	UUserWidget* const Illustration = CreateWidget<UUserWidget>(this, LoadedClass);
	if (not IsValid(Illustration))
	{
		return IllustrationWell;
	}
	IllustrationBox->SetContent(MakePictureWithGlass(Illustration));
	PlayAllAnimationsLooped(*Illustration);
	M_IllustrationWidget = Illustration;
	return IllustrationWell;
}

void UW_BxpPackUpTakeover::AddStepText(UVerticalBox& TextColumn, const FBxpPackUpTakeoverStep& Step,
                                       const bool bIsFirst) const
{
	using namespace BxpPackUpTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Step.Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StepLabelFontSize,
		                                                 StepLabelLetterSpacing), M_Style.Skin.LabelColor);
	// The first label sits level with the top of the picture.
	TextColumn.AddChildToVerticalBox(LabelText)->SetPadding(bIsFirst ? FirstStepLabelPadding : StepLabelPadding);

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Step.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StepTitleFontSize),
		M_Style.Skin.TextColor);
	TextColumn.AddChildToVerticalBox(TitleText)->SetPadding(StepTitlePadding);

	UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(Step.Body);
	BodyText->SetWrapTextAt(StepTextWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		BodyText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, StepBodyFontSize),
		M_Style.Skin.DescriptionColor);
	TextColumn.AddChildToVerticalBox(BodyText);
}

void UW_BxpPackUpTakeover::PlayAllAnimationsLooped(UUserWidget& Illustration)
{
	// The same as the other field manuals' illustrations: no graph is needed in the illustration Blueprint.
	const UWidgetBlueprintGeneratedClass* const GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(
		Illustration.GetClass());
	if (GeneratedClass == nullptr)
	{
		return;
	}
	for (UWidgetAnimation* const Animation : GeneratedClass->Animations)
	{
		if (IsValid(Animation))
		{
			Illustration.PlayAnimation(Animation, 0.f, BxpPackUpTakeoverLayout::LoopForever);
		}
	}
}
