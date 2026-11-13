// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_NomadicConversionTakeover.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace NomadicConversionTakeoverLayout
{
	// Sizes are for the 1080p reference height, inside the base frame's 1240 wide panel; the page is laid out like the
	// camera controls takeover's side pages (one large picture, its texts beside it).
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

	const TCHAR* const DeployIllustrationPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion/WBP_NomCnv_Deploy.WBP_NomCnv_Deploy_C");
	const TCHAR* const RadiusIllustrationPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion/WBP_NomCnv_Radius.WBP_NomCnv_Radius_C");
}

UW_NomadicConversionTakeover::UW_NomadicConversionTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_DeployIllustration = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(NomadicConversionTakeoverLayout::DeployIllustrationPath));
	M_RadiusIllustration = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(NomadicConversionTakeoverLayout::RadiusIllustrationPath));
}

FText UW_NomadicConversionTakeover::GetNomadicConversionTitle()
{
	return NSLOCTEXT("NomadicConversionTakeover", "Title", "Nomadic Trucks");
}

FText UW_NomadicConversionTakeover::GetNomadicConversionSummary()
{
	return NSLOCTEXT("NomadicConversionTakeover", "ArchiveSummary",
	                 "Your HQ and other base buildings travel as trucks. A truck converts into its building where you "
	                 "place it, and the building can pack up into a truck again.\n\n"
	                 "Convert: select the truck and click the building picture in the bottom panel (default hotkey "
	                 "Ctrl+Q).\n\n"
	                 "Place: move the preview to a free spot and left click. Comma and period rotate it; right click "
	                 "cancels. The truck drives there and unfolds into the building.\n\n"
	                 "Pack up: select the building and click the truck picture in the same place to fold it back into a "
	                 "truck.\n\n"
	                 "Build radius: your HQ is exempt and can deploy anywhere it fits. Every other truck must deploy "
	                 "inside a build radius, the area around your deployed HQ and some other buildings, such as the "
	                 "Gamma Facility. The radius shows while you place.");
}

void UW_NomadicConversionTakeover::SetupNomadicConversion()
{
	BuildBody();
	ApplyPageTexts(ENomadicConversionTakeoverPage::Conversion);
}

void UW_NomadicConversionTakeover::ApplyPageTexts(const ENomadicConversionTakeoverPage Page)
{
	HideKindLabel();
	const bool bIsLastPage = static_cast<int32>(Page) + 1 >= GetPageCount();
	SetConfirmButtonLabel(bIsLastPage
		                      ? NSLOCTEXT("InfoTakeover", "Understood", "UNDERSTOOD")
		                      : NSLOCTEXT("NomadicConversionTakeover", "Continue", "CONTINUE"));
	if (Page == ENomadicConversionTakeoverPage::BuildRadius)
	{
		SetTitleText(NSLOCTEXT("NomadicConversionTakeover", "RadiusPageTitle", "Build Radius"));
		SetDescriptionText(NSLOCTEXT("NomadicConversionTakeover", "RadiusPageDescription",
		                             "Your HQ may deploy anywhere. Every other truck must deploy close to your base, "
		                             "inside a build radius."));
		return;
	}
	SetTitleText(GetNomadicConversionTitle());
	SetDescriptionText(NSLOCTEXT("NomadicConversionTakeover", "Description",
	                             "Your HQ and other base buildings travel as trucks. Convert a truck into its building "
	                             "where you want it, and pack it up again when you need to move."));
}

bool UW_NomadicConversionTakeover::TryShowPage(const int32 PageIndex)
{
	if (PageIndex < 0 || PageIndex >= GetPageCount() || not IsValid(M_PageSwitcher))
	{
		return false;
	}
	M_ActivePage = static_cast<ENomadicConversionTakeoverPage>(PageIndex);
	M_PageSwitcher->SetActiveWidgetIndex(PageIndex);
	ApplyPageTexts(M_ActivePage);
	// Starts the page's loop at its beginning now that the page shows (also when PREVIOUS returns to it).
	UUserWidget* const PageIllustration = M_ActivePage == ENomadicConversionTakeoverPage::BuildRadius
		                                      ? M_RadiusIllustrationWidget.Get()
		                                      : M_DeployIllustrationWidget.Get();
	if (IsValid(PageIllustration))
	{
		PlayAllAnimationsLooped(*PageIllustration);
	}
	RestartTyping();
	return true;
}

TArray<FNomadicConversionTakeoverStep> UW_NomadicConversionTakeover::MakeConversionSteps() const
{
	// From the game: the truck's convert button is CreateBuildingButton in the bottom panel (W_BuildingUI_ItemPanel,
	// picture and tooltip from DT_TruckButtons), hotkey IA_NomadicExpansion (Ctrl + Q in RTS_IMC_ChordedActions).
	// ACPPController::ConstructBuilding starts the preview; a primary click places it (TryPlaceBuilding), a secondary
	// click cancels; IA_RotateLeft / IA_RotateRight (comma, period) turn it by RotationDegrees. Only the player HQ skips
	// the build radius (bUseBuildRadius = NomadicVehicle != M_PlayerHQ); radii come from deployed nomadics with a
	// UBuildRadiusComp. ConvertToVehicleButton packs a building up (ConvertBackToVehicle).
	TArray<FNomadicConversionTakeoverStep> Steps;
	FNomadicConversionTakeoverStep& ConvertStep = Steps.AddDefaulted_GetRef();
	ConvertStep.Label = NSLOCTEXT("NomadicConversionTakeover", "ConvertLabel", "BOTTOM PANEL  /  CTRL + Q");
	ConvertStep.Title = NSLOCTEXT("NomadicConversionTakeover", "ConvertTitle", "Convert into a building");
	ConvertStep.Body = NSLOCTEXT("NomadicConversionTakeover", "ConvertBody",
	                             "Select the truck and click the building picture in the bottom panel, or press "
	                             "Ctrl+Q.");

	FNomadicConversionTakeoverStep& PlaceStep = Steps.AddDefaulted_GetRef();
	PlaceStep.Label = NSLOCTEXT("NomadicConversionTakeover", "PlaceLabel", "LEFT CLICK  /  COMMA, PERIOD");
	PlaceStep.Title = NSLOCTEXT("NomadicConversionTakeover", "PlaceTitle", "Place it");
	PlaceStep.Body = NSLOCTEXT("NomadicConversionTakeover", "PlaceBody",
	                           "Move the preview to a free spot and left click. Comma and period rotate it; right "
	                           "click cancels. The truck drives there and unfolds.");

	FNomadicConversionTakeoverStep& PackStep = Steps.AddDefaulted_GetRef();
	PackStep.Label = NSLOCTEXT("NomadicConversionTakeover", "PackLabel", "PACK UP");
	PackStep.Title = NSLOCTEXT("NomadicConversionTakeover", "PackTitle", "Move it again");
	PackStep.Body = NSLOCTEXT("NomadicConversionTakeover", "PackBody",
	                          "Select the building and click the truck picture in the same place to fold it back "
	                          "into a truck.");
	return Steps;
}

TArray<FNomadicConversionTakeoverStep> UW_NomadicConversionTakeover::MakeBuildRadiusSteps() const
{
	// From the game: ACPPController converts with bUseBuildRadius = NomadicVehicle != M_PlayerHQ, so only the player
	// HQ ignores the radii; ACPPConstructionPreview::GetisWithinBuildRadii refuses other spots outside every radius.
	// The radii are the UBuildRadiusComp of deployed nomadics (the HQ, the Gamma Facility, ...), shown while placing.
	TArray<FNomadicConversionTakeoverStep> Steps;
	FNomadicConversionTakeoverStep& HQStep = Steps.AddDefaulted_GetRef();
	HQStep.Label = NSLOCTEXT("NomadicConversionTakeover", "RadiusHQLabel", "YOUR HQ");
	HQStep.Title = NSLOCTEXT("NomadicConversionTakeover", "RadiusHQTitle", "Exempt: deploys anywhere");
	HQStep.Body = NSLOCTEXT("NomadicConversionTakeover", "RadiusHQBody",
	                        "The HQ truck needs no build radius. It can deploy on any open spot it fits on, also far "
	                        "away from your other buildings.");

	FNomadicConversionTakeoverStep& OtherStep = Steps.AddDefaulted_GetRef();
	OtherStep.Label = NSLOCTEXT("NomadicConversionTakeover", "RadiusOtherLabel", "OTHER TRUCKS");
	OtherStep.Title = NSLOCTEXT("NomadicConversionTakeover", "RadiusOtherTitle", "Inside a build radius");
	OtherStep.Body = NSLOCTEXT("NomadicConversionTakeover", "RadiusOtherBody",
	                           "Every other truck must deploy inside a build radius. Outside it the preview cannot be "
	                           "placed.");

	FNomadicConversionTakeoverStep& SourceStep = Steps.AddDefaulted_GetRef();
	SourceStep.Label = NSLOCTEXT("NomadicConversionTakeover", "RadiusSourceLabel", "BUILD RADIUS");
	SourceStep.Title = NSLOCTEXT("NomadicConversionTakeover", "RadiusSourceTitle", "Where it comes from");
	SourceStep.Body = NSLOCTEXT("NomadicConversionTakeover", "RadiusSourceBody",
	                            "Your deployed HQ and some other buildings, such as the Gamma Facility, give a build "
	                            "radius. It shows while you place a building.");
	return Steps;
}

void UW_NomadicConversionTakeover::BuildBody()
{
	using namespace NomadicConversionTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_DeployIllustrationWidget = nullptr;
	M_RadiusIllustrationWidget = nullptr;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	// Child order follows ENomadicConversionTakeoverPage.
	M_PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	M_PageSwitcher->AddChild(BuildPage(M_DeployIllustration, MakeConversionSteps(), M_DeployIllustrationWidget));
	M_PageSwitcher->AddChild(BuildPage(M_RadiusIllustration, MakeBuildRadiusSteps(), M_RadiusIllustrationWidget));
	M_PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(ENomadicConversionTakeoverPage::Conversion));
	M_ActivePage = ENomadicConversionTakeoverPage::Conversion;
	BodyColumn->AddChildToVerticalBox(M_PageSwitcher);
}

UWidget* UW_NomadicConversionTakeover::BuildPage(const TSoftClassPtr<UUserWidget>& IllustrationClass,
                                                 const TArray<FNomadicConversionTakeoverStep>& Steps,
                                                 TObjectPtr<UUserWidget>& OutIllustration)
{
	using namespace NomadicConversionTakeoverLayout;
	UHorizontalBox* const PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PageRow->AddChildToHorizontalBox(BuildIllustrationWell(IllustrationClass, IllustrationSize, OutIllustration))
	       ->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		AddStepText(*TextColumn, Steps[StepIndex], StepIndex == 0);
	}
	UHorizontalBoxSlot* const TextSlot = PageRow->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(FMargin(SideTextGap, 0.f, 0.f, 0.f));
	return BuildGlassPane(PageRow);
}

UWidget* UW_NomadicConversionTakeover::BuildGlassPane(UWidget* Content)
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
	GlassPane->SetPadding(NomadicConversionTakeoverLayout::GlassPanePadding);
	GlassPane->SetContent(Content);
	RegisterGlassPane(*GlassPane);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

UWidget* UW_NomadicConversionTakeover::BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass,
                                                             const FVector2D& DesignedSize,
                                                             TObjectPtr<UUserWidget>& OutIllustration)
{
	using namespace NomadicConversionTakeoverLayout;
	UBorder* const IllustrationWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IllustrationWell->SetBrush(M_Style.Skin.CardIconWell);
	IllustrationWell->SetPadding(IllustrationWellPadding);
	USizeBox* const IllustrationBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IllustrationBox->SetWidthOverride(DesignedSize.X);
	IllustrationBox->SetHeightOverride(DesignedSize.Y);
	// The truck and the preview move over the picture; only the illustration's own area may show.
	IllustrationBox->SetClipping(EWidgetClipping::ClipToBounds);
	IllustrationWell->SetContent(IllustrationBox);

	UClass* const LoadedClass = IllustrationClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Nomadic conversion takeover: illustration %s could not be loaded; the picture well stays empty."),
			*IllustrationClass.ToString()));
		return IllustrationWell;
	}
	UUserWidget* const Illustration = CreateWidget<UUserWidget>(this, LoadedClass);
	if (not IsValid(Illustration))
	{
		return IllustrationWell;
	}
	IllustrationBox->SetContent(MakePictureWithGlass(Illustration));
	PlayAllAnimationsLooped(*Illustration);
	OutIllustration = Illustration;
	return IllustrationWell;
}

void UW_NomadicConversionTakeover::AddStepText(UVerticalBox& TextColumn, const FNomadicConversionTakeoverStep& Step,
                                               const bool bIsFirst) const
{
	using namespace NomadicConversionTakeoverLayout;
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

void UW_NomadicConversionTakeover::PlayAllAnimationsLooped(UUserWidget& Illustration)
{
	// The same as the camera controls takeover's illustrations: no graph is needed in the illustration Blueprint.
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
			Illustration.PlayAnimation(Animation, 0.f, NomadicConversionTakeoverLayout::LoopForever);
		}
	}
}
