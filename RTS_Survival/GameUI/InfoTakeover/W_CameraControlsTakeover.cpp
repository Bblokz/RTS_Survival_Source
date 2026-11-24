// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CameraControlsTakeover.h"

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

namespace CameraControlsTakeoverLayout
{
	// Sizes are for the 1080p reference height, inside the base frame's 1240 wide panel.
	constexpr float CardGap = 20.f;
	constexpr float GlassPaneSidePadding = 14.f;
	const FMargin GlassPanePadding(GlassPaneSidePadding, 14.f, GlassPaneSidePadding, 18.f);
	// The illustration Blueprints are designed at these sizes; the well adds its frame around them.
	const FVector2D CameraIllustrationSize(330.f, 186.f);
	// The panning and marquee pages: one large picture with its texts beside it.
	const FVector2D SideIllustrationSize(520.f, 292.f);
	const FMargin IllustrationWellPadding(6.f);
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 22.f);
	const FMargin CardLabelPadding(0.f, 16.f, 0.f, 2.f);
	const FMargin FirstSideLabelPadding(0.f, 4.f, 0.f, 2.f);
	const FMargin CardTitlePadding(0.f, 0.f, 0.f, 6.f);
	constexpr float SideTextGap = 28.f;
	// Text wraps at a fixed width so the cards have their full height on the first frame.
	constexpr float CardTextWrapWidth = 336.f;
	constexpr float SideTextWrapWidth = 560.f;
	constexpr int32 CardLabelFontSize = 12;
	constexpr int32 CardLabelLetterSpacing = 100;
	constexpr int32 CardTitleFontSize = 19;
	constexpr int32 CardBodyFontSize = 15;
	constexpr int32 LoopForever = 0;

	const TCHAR* const IllustrationFolder = TEXT("/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CameraControls/");
	const TCHAR* const KeyboardPanIllustrationName = TEXT("WBP_CamCtl_KeyboardPan");
	const TCHAR* const EdgePanIllustrationName = TEXT("WBP_CamCtl_EdgePan");
	const TCHAR* const ZoomIllustrationName = TEXT("WBP_CamCtl_Zoom");
	const TCHAR* const PanningIllustrationName = TEXT("WBP_CamCtl_Panning");
	const TCHAR* const MarqueeIllustrationName = TEXT("WBP_CamCtl_Marquee");

	TSoftClassPtr<UUserWidget> MakeIllustrationClassPath(const TCHAR* const AssetName)
	{
		return TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			FString::Printf(TEXT("%s%s.%s_C"), IllustrationFolder, AssetName, AssetName)));
	}
}

UW_CameraControlsTakeover::UW_CameraControlsTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace CameraControlsTakeoverLayout;
	M_KeyboardPanIllustration = MakeIllustrationClassPath(KeyboardPanIllustrationName);
	M_EdgePanIllustration = MakeIllustrationClassPath(EdgePanIllustrationName);
	M_ZoomIllustration = MakeIllustrationClassPath(ZoomIllustrationName);
	M_PanningIllustration = MakeIllustrationClassPath(PanningIllustrationName);
	M_MarqueeIllustration = MakeIllustrationClassPath(MarqueeIllustrationName);
}

FText UW_CameraControlsTakeover::GetCameraControlsTitle()
{
	return NSLOCTEXT("CameraControlsTakeover", "Title", "Camera Controls");
}

FText UW_CameraControlsTakeover::GetCameraControlsSummary()
{
	return NSLOCTEXT("CameraControlsTakeover", "ArchiveSummary",
	                 "Hold the arrow keys to move the camera.\n\n"
	                 "Push the cursor against any edge of the screen to scroll that way; the longer you hold it "
	                 "there, the faster the camera goes.\n\n"
	                 "Roll the mouse wheel forward to zoom in and back to zoom out.\n\n"
	                 "Hold the middle mouse button (or P) and move the mouse to rotate the camera: left and right rotate "
	                 "the view around, up and down tilt it a little. Hold Alt and press the middle mouse button to rotate "
	                 "it back to its starting angle.\n\n"
	                 "Hold the left mouse button and drag a box over your units to select them when you let go. Hold "
	                 "Shift while dragging to add them to your selection; if they were all selected already, they are "
	                 "removed instead.");
}

TArray<UUserWidget*> UW_CameraControlsTakeover::GetPageIllustrations(const ECameraControlsTakeoverPage Page) const
{
	TArray<UUserWidget*> Illustrations;
	if (Page == ECameraControlsTakeoverPage::CameraControls)
	{
		for (UUserWidget* const Illustration : M_CameraPageIllustrations)
		{
			Illustrations.Add(Illustration);
		}
		return Illustrations;
	}
	if (UUserWidget* const SideIllustration = GetSidePageIllustration(Page))
	{
		Illustrations.Add(SideIllustration);
	}
	return Illustrations;
}

UUserWidget* UW_CameraControlsTakeover::GetSidePageIllustration(const ECameraControlsTakeoverPage Page) const
{
	switch (Page)
	{
	case ECameraControlsTakeoverPage::CameraPanning:
		return M_PanningPageIllustration;
	case ECameraControlsTakeoverPage::MarqueeSelection:
		return M_MarqueePageIllustration;
	default:
		return nullptr;
	}
}

void UW_CameraControlsTakeover::SetupCameraControls()
{
	BuildBody();
	ApplyPageTexts(ECameraControlsTakeoverPage::CameraControls);
}

void UW_CameraControlsTakeover::ApplyPageTexts(const ECameraControlsTakeoverPage Page)
{
	HideKindLabel();
	const bool bIsLastPage = static_cast<int32>(Page) + 1 >= static_cast<int32>(ECameraControlsTakeoverPage::Count);
	SetConfirmButtonLabel(bIsLastPage
		                      ? NSLOCTEXT("InfoTakeover", "Understood", "UNDERSTOOD")
		                      : NSLOCTEXT("CameraControlsTakeover", "Continue", "CONTINUE"));
	switch (Page)
	{
	case ECameraControlsTakeoverPage::CameraControls:
		SetTitleText(GetCameraControlsTitle());
		SetDescriptionText(NSLOCTEXT("CameraControlsTakeover", "Description",
		                             "Three ways to move your view. Try each one when the battle resumes."));
		return;
	case ECameraControlsTakeoverPage::CameraPanning:
		SetTitleText(NSLOCTEXT("CameraControlsTakeover", "PanningTitle", "Camera Rotation"));
		SetDescriptionText(NSLOCTEXT("CameraControlsTakeover", "PanningDescription",
		                             "Rotate the camera with the middle mouse button to see the battle from another "
		                             "side."));
		return;
	default:
		SetTitleText(NSLOCTEXT("CameraControlsTakeover", "MarqueeTitle", "Selecting Units"));
		SetDescriptionText(NSLOCTEXT("CameraControlsTakeover", "MarqueeDescription",
		                             "Select a whole group of units with one drag of the mouse."));
		return;
	}
}

bool UW_CameraControlsTakeover::TryShowPage(const int32 PageIndex)
{
	if (PageIndex < 0 || PageIndex >= GetPageCount() || not IsValid(M_PageSwitcher))
	{
		return false;
	}
	M_ActivePage = static_cast<ECameraControlsTakeoverPage>(PageIndex);
	M_PageSwitcher->SetActiveWidgetIndex(PageIndex);
	ApplyPageTexts(M_ActivePage);
	// Starts the page's loops at their beginning now that the page shows (also when PREVIOUS returns to it).
	for (UUserWidget* const Illustration : GetPageIllustrations(M_ActivePage))
	{
		if (IsValid(Illustration))
		{
			PlayAllAnimationsLooped(*Illustration);
		}
	}
	RestartTyping();
	return true;
}

TArray<FCameraControlsTakeoverSection> UW_CameraControlsTakeover::MakeCameraSections() const
{
	TArray<FCameraControlsTakeoverSection> Sections;
	FCameraControlsTakeoverSection& KeyboardSection = Sections.AddDefaulted_GetRef();
	KeyboardSection.Label = NSLOCTEXT("CameraControlsTakeover", "KeyboardLabel", "KEYBOARD");
	KeyboardSection.Title = NSLOCTEXT("CameraControlsTakeover", "KeyboardTitle", "Arrow keys");
	KeyboardSection.Body = NSLOCTEXT("CameraControlsTakeover", "KeyboardBody",
	                                 "Hold an arrow key to move the camera that way.");
	KeyboardSection.IllustrationClass = M_KeyboardPanIllustration;

	FCameraControlsTakeoverSection& EdgeSection = Sections.AddDefaulted_GetRef();
	EdgeSection.Label = NSLOCTEXT("CameraControlsTakeover", "EdgeLabel", "SCREEN EDGE");
	EdgeSection.Title = NSLOCTEXT("CameraControlsTakeover", "EdgeTitle", "Edge panning");
	EdgeSection.Body = NSLOCTEXT("CameraControlsTakeover", "EdgeBody",
	                             "Push the cursor against an edge of the screen to scroll. Hold it there to go "
	                             "faster.");
	EdgeSection.IllustrationClass = M_EdgePanIllustration;

	FCameraControlsTakeoverSection& ZoomSection = Sections.AddDefaulted_GetRef();
	ZoomSection.Label = NSLOCTEXT("CameraControlsTakeover", "ZoomLabel", "MOUSE WHEEL");
	ZoomSection.Title = NSLOCTEXT("CameraControlsTakeover", "ZoomTitle", "Zoom");
	ZoomSection.Body = NSLOCTEXT("CameraControlsTakeover", "ZoomBody",
	                             "Roll the wheel forward to zoom in, back to zoom out.");
	ZoomSection.IllustrationClass = M_ZoomIllustration;
	return Sections;
}

TArray<FCameraControlsTakeoverSection> UW_CameraControlsTakeover::MakePanningSections() const
{
	// What RTS_BP_PlayerController does: while IA_PanCamera (middle mouse button or P) is held, Mouse X turns the
	// camera's yaw and Mouse Y its pitch within the pitch limit; IA_PanReset (Alt + middle mouse button) calls PanReset.
	TArray<FCameraControlsTakeoverSection> Sections;
	FCameraControlsTakeoverSection& TurnSection = Sections.AddDefaulted_GetRef();
	TurnSection.Label = NSLOCTEXT("CameraControlsTakeover", "PanLabel", "MIDDLE MOUSE BUTTON");
	TurnSection.Title = NSLOCTEXT("CameraControlsTakeover", "PanTitle", "Rotate the camera");
	TurnSection.Body = NSLOCTEXT("CameraControlsTakeover", "PanBody",
	                             "Hold the middle mouse button (or P) and move the mouse. Left and right rotate the "
	                             "view around; up and down tilt it a little.");

	FCameraControlsTakeoverSection& ResetSection = Sections.AddDefaulted_GetRef();
	ResetSection.Label = NSLOCTEXT("CameraControlsTakeover", "PanResetLabel", "ALT + MIDDLE MOUSE BUTTON");
	ResetSection.Title = NSLOCTEXT("CameraControlsTakeover", "PanResetTitle", "Reset the view");
	ResetSection.Body = NSLOCTEXT("CameraControlsTakeover", "PanResetBody",
	                              "Hold Alt and press the middle mouse button to rotate the camera back to its starting "
	                              "angle.");
	return Sections;
}

TArray<FCameraControlsTakeoverSection> UW_CameraControlsTakeover::MakeMarqueeSections() const
{
	// What ACPPController::SelectUnitsFromMarquee does: own units in the box on release; Shift adds, or removes when
	// every boxed unit was selected already.
	TArray<FCameraControlsTakeoverSection> Sections;
	FCameraControlsTakeoverSection& DragSection = Sections.AddDefaulted_GetRef();
	DragSection.Label = NSLOCTEXT("CameraControlsTakeover", "DragLabel", "LEFT MOUSE BUTTON");
	DragSection.Title = NSLOCTEXT("CameraControlsTakeover", "DragTitle", "Drag to select");
	DragSection.Body = NSLOCTEXT("CameraControlsTakeover", "DragBody",
	                             "Hold the left mouse button and drag a box over your units. The units inside are "
	                             "selected when you let go.");

	FCameraControlsTakeoverSection& ShiftSection = Sections.AddDefaulted_GetRef();
	ShiftSection.Label = NSLOCTEXT("CameraControlsTakeover", "ShiftLabel", "SHIFT");
	ShiftSection.Title = NSLOCTEXT("CameraControlsTakeover", "ShiftTitle", "Add to the selection");
	ShiftSection.Body = NSLOCTEXT("CameraControlsTakeover", "ShiftBody",
	                              "Hold Shift while you drag to add the boxed units to your selection. If they were "
	                              "all selected already, they are removed instead.");
	return Sections;
}

void UW_CameraControlsTakeover::BuildBody()
{
	using namespace CameraControlsTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_CameraPageIllustrations.Reset();
	M_PanningPageIllustration = nullptr;
	M_MarqueePageIllustration = nullptr;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	// Child order follows ECameraControlsTakeoverPage.
	M_PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	M_PageSwitcher->AddChild(BuildCameraPage());
	M_PageSwitcher->AddChild(BuildSideIllustrationPage(M_PanningIllustration, MakePanningSections(),
	                                                   M_PanningPageIllustration));
	M_PageSwitcher->AddChild(BuildSideIllustrationPage(M_MarqueeIllustration, MakeMarqueeSections(),
	                                                   M_MarqueePageIllustration));
	M_PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(ECameraControlsTakeoverPage::CameraControls));
	M_ActivePage = ECameraControlsTakeoverPage::CameraControls;
	BodyColumn->AddChildToVerticalBox(M_PageSwitcher);
}

UWidget* UW_CameraControlsTakeover::BuildCameraPage()
{
	using namespace CameraControlsTakeoverLayout;
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TArray<FCameraControlsTakeoverSection> Sections = MakeCameraSections();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UHorizontalBoxSlot* const CardSlot = CardRow->AddChildToHorizontalBox(BuildSectionCard(Sections[SectionIndex]));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		const bool bIsLastCard = SectionIndex == Sections.Num() - 1;
		CardSlot->SetPadding(FMargin(0.f, 0.f, bIsLastCard ? 0.f : CardGap, 0.f));
	}
	return CardRow;
}

UWidget* UW_CameraControlsTakeover::BuildSideIllustrationPage(const TSoftClassPtr<UUserWidget>& IllustrationClass,
                                                              const TArray<FCameraControlsTakeoverSection>& Sections,
                                                              TObjectPtr<UUserWidget>& OutIllustration)
{
	using namespace CameraControlsTakeoverLayout;
	UHorizontalBox* const PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UUserWidget* Illustration = nullptr;
	PageRow->AddChildToHorizontalBox(BuildIllustrationWell(IllustrationClass, SideIllustrationSize,
	                                                       Illustration))->SetVerticalAlignment(VAlign_Top);
	OutIllustration = Illustration;

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FCameraControlsTakeoverSection& Section : Sections)
	{
		AddCardText(*TextColumn, Section, SideTextWrapWidth);
	}
	if (TextColumn->GetChildrenCount() > 0)
	{
		// The first label sits level with the top of the picture.
		if (UVerticalBoxSlot* const FirstLabelSlot = Cast<UVerticalBoxSlot>(TextColumn->GetChildAt(0)->Slot))
		{
			FirstLabelSlot->SetPadding(FirstSideLabelPadding);
		}
	}
	UHorizontalBoxSlot* const TextSlot = PageRow->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(FMargin(SideTextGap, 0.f, 0.f, 0.f));
	return BuildGlassPane(PageRow);
}

UWidget* UW_CameraControlsTakeover::BuildSectionCard(const FCameraControlsTakeoverSection& Section)
{
	using namespace CameraControlsTakeoverLayout;
	UVerticalBox* const CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UUserWidget* Illustration = nullptr;
	CardColumn->AddChildToVerticalBox(BuildIllustrationWell(Section.IllustrationClass, CameraIllustrationSize,
	                                                        Illustration))->SetHorizontalAlignment(HAlign_Center);
	if (IsValid(Illustration))
	{
		M_CameraPageIllustrations.Add(Illustration);
	}
	AddCardText(*CardColumn, Section, CardTextWrapWidth);
	return BuildGlassPane(CardColumn);
}

UWidget* UW_CameraControlsTakeover::BuildGlassPane(UWidget* Content)
{
	using namespace CameraControlsTakeoverLayout;
	// The vehicle takeover's facts pane: blurred glass over the plate.
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

UWidget* UW_CameraControlsTakeover::BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass,
                                                          const FVector2D& IllustrationSize,
                                                          UUserWidget*& OutIllustration)
{
	using namespace CameraControlsTakeoverLayout;
	OutIllustration = nullptr;
	UBorder* const IllustrationWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IllustrationWell->SetBrush(M_Style.Skin.CardIconWell);
	IllustrationWell->SetPadding(IllustrationWellPadding);
	USizeBox* const IllustrationBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IllustrationBox->SetWidthOverride(IllustrationSize.X);
	IllustrationBox->SetHeightOverride(IllustrationSize.Y);
	// The animations slide and scale their map past its frame; only the illustration's own area may show.
	IllustrationBox->SetClipping(EWidgetClipping::ClipToBounds);
	IllustrationWell->SetContent(IllustrationBox);

	UClass* const LoadedClass = IllustrationClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Camera controls takeover: illustration %s could not be loaded; the card shows an empty well."),
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

void UW_CameraControlsTakeover::AddCardText(UVerticalBox& CardColumn, const FCameraControlsTakeoverSection& Section,
                                            const float WrapWidth) const
{
	using namespace CameraControlsTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Section.Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, CardLabelFontSize,
		                                                 CardLabelLetterSpacing), M_Style.Skin.LabelColor);
	CardColumn.AddChildToVerticalBox(LabelText)->SetPadding(CardLabelPadding);

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Section.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, CardTitleFontSize),
		M_Style.Skin.TextColor);
	CardColumn.AddChildToVerticalBox(TitleText)->SetPadding(CardTitlePadding);

	UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(Section.Body);
	BodyText->SetWrapTextAt(WrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		BodyText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, CardBodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn.AddChildToVerticalBox(BodyText);
}

void UW_CameraControlsTakeover::PlayAllAnimationsLooped(UUserWidget& Illustration)
{
	using namespace CameraControlsTakeoverLayout;
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
			Illustration.PlayAnimation(Animation, 0.f, LoopForever);
		}
	}
}
