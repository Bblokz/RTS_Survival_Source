// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_InfoTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace InfoTakeoverLayout
{
	// Sizes are for the 1080p reference height; UMG's DPI curve scales them to the screen.
	constexpr float PanelWidth = 1240.f;
	constexpr float PanelMaxHeight = 920.f;
	const FMargin PanelPadding(40.f, 30.f, 40.f, 30.f);
	const FMargin TitlePadding(0.f, 4.f, 0.f, 0.f);
	constexpr float DividerHeight = 1.f;
	const FMargin DividerPadding(0.f, 14.f, 0.f, 18.f);
	const FMargin FooterPadding(0.f, 22.f, 0.f, 0.f);
	const FMargin CheckBoxLabelPadding(10.f, 0.f, 0.f, 0.f);
	const FVector2D UnderstoodButtonSize(280.f, 48.f);
	constexpr float DividerOpacity = 0.35f;
	constexpr int32 KindLabelFontSize = 13;
	constexpr int32 KindLabelLetterSpacing = 200;
	constexpr int32 CheckBoxLabelFontSize = 15;
	constexpr float MinLetterInterval = 0.004f;
	constexpr float SoundFadeOutSeconds = 0.15f;
	constexpr int32 ViewportZOrder = 900;
	// Page lamps: centred on the title placard's lamp seat (the plate art's placard spans y 16..102 of the panel).
	constexpr float PageLampSize = 24.f;
	constexpr float PageLampGap = 6.f;
	const FMargin PageLampRowPadding(0.f, 17.f, 10.f, 0.f);
	constexpr float PreviousButtonGap = 16.f;
	const FName HoverSweepIntensityParameter(TEXT("HoverSweepIntensity"));
	const FName HoverScratchBoostParameter(TEXT("HoverScratchBoost"));
	const FName HoverEdgeTintParameter(TEXT("HoverEdgeTint"));
	const FName HoverParameter(TEXT("Hover"));
}

void UW_InfoTakeover::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	LoadSounds();
	BuildWidgetTree();
}

void UW_InfoTakeover::BuildWidgetTree()
{
	using namespace InfoTakeoverLayout;
	if (not IsValid(WidgetTree))
	{
		return;
	}
	// A Blueprint child may carry the designer's default empty canvas; the C++ tree replaces it.
	const UPanelWidget* const ExistingRootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	const bool bHasDesignedTree = IsValid(WidgetTree->RootWidget)
		&& (ExistingRootPanel == nullptr || ExistingRootPanel->GetChildrenCount() > 0);
	if (bHasDesignedTree)
	{
		return;
	}
	UOverlay* const ScreenOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = ScreenOverlay;
	// The default brush is a plain white quad; the tint makes it the dim layer over the paused game.
	UImage* const ScreenDim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ScreenDim->SetColorAndOpacity(M_Style.ScreenDimColor);
	UOverlaySlot* const DimSlot = ScreenOverlay->AddChildToOverlay(ScreenDim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* const PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelWidth);
	// A long description makes the body scroll instead of growing the panel past the screen.
	PanelBox->SetMaxDesiredHeight(PanelMaxHeight);
	UOverlaySlot* const PanelSlot = ScreenOverlay->AddChildToOverlay(PanelBox);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	UBorder* const Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// The card art is drawn to stretch, so it frames a panel of any height.
	Plate->SetBrush(M_Style.Skin.Card.Normal);
	Plate->SetPadding(PanelPadding);
	PanelBox->SetContent(Plate);

	UVerticalBox* const PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Plate->SetContent(PanelColumn);
	PanelColumn->AddChildToVerticalBox(BuildHeader());
	M_BodyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelColumn->AddChildToVerticalBox(M_BodyColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PanelColumn->AddChildToVerticalBox(BuildFooter())->SetPadding(FooterPadding);
	M_DescriptionText = BuildTypewriterBlock(M_Style.BodyTextStyleSet.LoadSynchronous());
	M_DescriptionText->SetAutoWrapText(true);
}

UWidget* UW_InfoTakeover::BuildHeader()
{
	using namespace InfoTakeoverLayout;
	UVerticalBox* const Header = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_KindLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_KindLabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, KindLabelFontSize,
		                                                       KindLabelLetterSpacing), M_Style.Skin.LabelColor);
	Header->AddChildToVerticalBox(M_KindLabelText);
	M_TitleText = BuildTypewriterBlock(M_Style.TitleTextStyleSet.LoadSynchronous());
	M_TitleText->SetAutoWrapText(true);
	Header->AddChildToVerticalBox(M_TitleText)->SetPadding(TitlePadding);

	USizeBox* const DividerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DividerBox->SetHeightOverride(DividerHeight);
	UImage* const Divider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FLinearColor DividerColor = M_Style.Skin.LabelColor.GetSpecifiedColor();
	DividerColor.A = DividerOpacity;
	Divider->SetColorAndOpacity(DividerColor);
	DividerBox->SetContent(Divider);
	Header->AddChildToVerticalBox(DividerBox)->SetPadding(DividerPadding);

	// The page lamps sit over the header's top right, in the title placard of the plate art.
	UOverlay* const HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* const HeaderSlot = HeaderOverlay->AddChildToOverlay(Header);
	HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	HeaderSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlaySlot* const LampSlot = HeaderOverlay->AddChildToOverlay(BuildPageLampRow());
	LampSlot->SetHorizontalAlignment(HAlign_Right);
	LampSlot->SetVerticalAlignment(VAlign_Top);
	LampSlot->SetPadding(PageLampRowPadding);
	return HeaderOverlay;
}

UWidget* UW_InfoTakeover::BuildPageLampRow()
{
	M_PageLampRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_PageLampRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	RebuildPageLamps(1);
	return M_PageLampRow;
}

void UW_InfoTakeover::RebuildPageLamps(const int32 LampCount)
{
	using namespace InfoTakeoverLayout;
	if (not IsValid(M_PageLampRow))
	{
		return;
	}
	M_PageLampRow->ClearChildren();
	M_PageLamps.Reset();
	for (int32 LampIndex = 0; LampIndex < LampCount; ++LampIndex)
	{
		USizeBox* const LampBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LampBox->SetWidthOverride(PageLampSize);
		LampBox->SetHeightOverride(PageLampSize);
		UImage* const Lamp = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Lamp->SetBrush(M_Style.PageLampUnlit);
		LampBox->SetContent(Lamp);
		UHorizontalBoxSlot* const LampSlot = M_PageLampRow->AddChildToHorizontalBox(LampBox);
		LampSlot->SetPadding(FMargin(LampIndex == 0 ? 0.f : PageLampGap, 0.f, 0.f, 0.f));
		M_PageLamps.Add(Lamp);
	}
}

UWidget* UW_InfoTakeover::BuildFooter()
{
	using namespace InfoTakeoverLayout;
	UHorizontalBox* const Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_NeverShowAgainCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	M_NeverShowAgainCheckBox->SetWidgetStyle(M_Style.NeverShowAgainCheckBox);
	M_NeverShowAgainCheckBox->SetIsChecked(false);
	// The label is the box's content, so clicking the words ticks it too.
	UTextBlock* const CheckBoxLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CheckBoxLabel->SetText(NSLOCTEXT("InfoTakeover", "NeverShowAgain", "Never show these info widgets again"));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CheckBoxLabel, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, CheckBoxLabelFontSize),
		M_Style.Skin.MutedTextColor);
	UHorizontalBox* const CheckBoxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CheckBoxRow->AddChildToHorizontalBox(M_NeverShowAgainCheckBox)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* const LabelSlot = CheckBoxRow->AddChildToHorizontalBox(CheckBoxLabel);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(CheckBoxLabelPadding);
	Footer->AddChildToHorizontalBox(CheckBoxRow)->SetVerticalAlignment(VAlign_Center);

	USpacer* const FooterSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Footer->AddChildToHorizontalBox(FooterSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	M_PreviousButtonBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style.Skin, NSLOCTEXT("InfoTakeover", "Previous", "PREVIOUS"), M_Style.Skin.ConfirmButton,
		UnderstoodButtonSize, M_PreviousButton);
	UHorizontalBoxSlot* const PreviousSlot = Footer->AddChildToHorizontalBox(M_PreviousButtonBox);
	PreviousSlot->SetVerticalAlignment(VAlign_Center);
	PreviousSlot->SetPadding(FMargin(0.f, 0.f, PreviousButtonGap, 0.f));
	M_PreviousButtonBox->SetVisibility(ESlateVisibility::Collapsed);
	M_PreviousButton->OnClicked.AddDynamic(this, &UW_InfoTakeover::HandlePreviousClicked);
	RegisterGlassButton(*M_PreviousButton, TEXT("Button_Previous"));
	UWidget* const UnderstoodButtonBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style.Skin, NSLOCTEXT("InfoTakeover", "Understood", "UNDERSTOOD"), M_Style.Skin.ConfirmButton,
		UnderstoodButtonSize, M_UnderstoodButton);
	Footer->AddChildToHorizontalBox(UnderstoodButtonBox)->SetVerticalAlignment(VAlign_Center);
	M_UnderstoodButton->OnClicked.AddDynamic(this, &UW_InfoTakeover::HandleUnderstoodClicked);
	RegisterGlassButton(*M_UnderstoodButton, TEXT("Button_Confirm"));
	return Footer;
}

UWidget* UW_InfoTakeover::MakePictureWithGlass(UWidget* Picture)
{
	const FSlateBrush& GlassBrush = M_Style.PictureGlass;
	const bool bHasGlass = GlassBrush.DrawAs != ESlateBrushDrawType::NoDrawType
		&& GlassBrush.GetResourceObject() != nullptr;
	if (not IsValid(Picture) || not bHasGlass)
	{
		return Picture;
	}
	UOverlay* const PictureOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* const PictureSlot = PictureOverlay->AddChildToOverlay(Picture);
	PictureSlot->SetHorizontalAlignment(HAlign_Fill);
	PictureSlot->SetVerticalAlignment(VAlign_Fill);
	// Hit-testable, so hovering the picture alone lights its glass.
	PictureOverlay->SetVisibility(ESlateVisibility::Visible);
	FSlateBrush InstanceBrush = GlassBrush;
	UMaterialInstanceDynamic* const GlassMaterial = MakeDynamicGlassBrush(InstanceBrush);
	UImage* const Glass = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Glass->SetBrush(InstanceBrush);
	// The glass never takes clicks or hovers from the picture underneath.
	Glass->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const GlassSlot = PictureOverlay->AddChildToOverlay(Glass);
	GlassSlot->SetHorizontalAlignment(HAlign_Fill);
	GlassSlot->SetVerticalAlignment(VAlign_Fill);
	if (IsValid(GlassMaterial))
	{
		FInfoTakeoverGlassElement& Element = M_GlassElements.AddDefaulted_GetRef();
		Element.Name = FName(TEXT("Picture"), M_GlassElements.Num() - 1);
		Element.HoverSource = PictureOverlay;
		Element.Materials.Add(GlassMaterial);
	}
	return PictureOverlay;
}

UMaterialInstanceDynamic* UW_InfoTakeover::MakeDynamicGlassBrush(FSlateBrush& Brush)
{
	UMaterialInterface* const GlassMaterial = Cast<UMaterialInterface>(Brush.GetResourceObject());
	if (GlassMaterial == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* const DynamicMaterial = UMaterialInstanceDynamic::Create(GlassMaterial, this);
	Brush.SetResourceObject(DynamicMaterial);
	return DynamicMaterial;
}

void UW_InfoTakeover::RegisterGlassPane(UBorder& GlassPane)
{
	using namespace InfoTakeoverLayout;
	FSlateBrush PaneBrush = GlassPane.Background;
	UMaterialInstanceDynamic* const PaneMaterial = MakeDynamicGlassBrush(PaneBrush);
	if (not IsValid(PaneMaterial))
	{
		return;
	}
	// The pane carries text: a quieter light band, scratches and edge tint than on a button.
	PaneMaterial->SetScalarParameterValue(HoverSweepIntensityParameter, M_Style.PaneHoverSweepIntensity);
	PaneMaterial->SetScalarParameterValue(HoverScratchBoostParameter, M_Style.PaneHoverScratchBoost);
	PaneMaterial->SetScalarParameterValue(HoverEdgeTintParameter, M_Style.PaneHoverEdgeTint);
	GlassPane.SetBrush(PaneBrush);
	FInfoTakeoverGlassElement& Element = M_GlassElements.AddDefaulted_GetRef();
	Element.Name = FName(TEXT("Pane"), M_GlassElements.Num() - 1);
	Element.HoverSource = &GlassPane;
	Element.Materials.Add(PaneMaterial);
	LinkPicturesToPane(GlassPane);
}

void UW_InfoTakeover::LinkPicturesToPane(UBorder& GlassPane)
{
	for (FInfoTakeoverGlassElement& Element : M_GlassElements)
	{
		const bool bIsUnlinkedPicture = Element.HoverSource.IsValid() && not Element.ParentHoverSource.IsValid()
			&& Element.HoverSource.Get() != &GlassPane && not Element.PressSource.IsValid();
		if (bIsUnlinkedPicture && GetIsWidgetInside(Element.HoverSource.Get(), &GlassPane))
		{
			Element.ParentHoverSource = &GlassPane;
		}
	}
}

bool UW_InfoTakeover::GetIsWidgetInside(const UWidget* Widget, const UWidget* Ancestor)
{
	for (const UWidget* Current = Widget; Current != nullptr; Current = Current->GetParent())
	{
		if (Current == Ancestor)
		{
			return true;
		}
	}
	return false;
}

void UW_InfoTakeover::RegisterGlassButton(UButton& Button, const FName ElementName)
{
	FButtonStyle ButtonStyle = Button.GetStyle();
	FInfoTakeoverGlassElement Element;
	Element.Name = ElementName;
	Element.HoverSource = &Button;
	Element.PressSource = &Button;
	for (FSlateBrush* const StateBrush : {&ButtonStyle.Normal, &ButtonStyle.Hovered, &ButtonStyle.Pressed})
	{
		if (UMaterialInstanceDynamic* const StateMaterial = MakeDynamicGlassBrush(*StateBrush))
		{
			Element.Materials.Add(StateMaterial);
		}
	}
	if (Element.Materials.IsEmpty())
	{
		return;
	}
	Button.SetStyle(ButtonStyle);
	M_GlassElements.Add(Element);
}

void UW_InfoTakeover::TickGlassHover(const float DeltaSeconds)
{
	for (FInfoTakeoverGlassElement& Element : M_GlassElements)
	{
		const UWidget* const Source = Element.HoverSource.Get();
		const UWidget* const ParentSource = Element.ParentHoverSource.Get();
		const bool bIsHovered = Element.bIsForcedHover || (Source != nullptr && Source->IsHovered())
			|| (ParentSource != nullptr && ParentSource->IsHovered());
		const bool bIsPressed = Element.PressSource.IsValid() && Element.PressSource->IsPressed();
		if (not Element.Animator.Tick(DeltaSeconds, bIsHovered, bIsPressed, M_Style.GlassHoverTimings))
		{
			continue;
		}
		for (UMaterialInstanceDynamic* const Material : Element.Materials)
		{
			if (IsValid(Material))
			{
				Element.Animator.ApplyTo(*Material);
			}
		}
	}
}

FName UW_InfoTakeover::GetGlassElementName(const int32 ElementIndex) const
{
	return M_GlassElements.IsValidIndex(ElementIndex) ? M_GlassElements[ElementIndex].Name : NAME_None;
}

float UW_InfoTakeover::GetGlassElementHover(const int32 ElementIndex) const
{
	using namespace InfoTakeoverLayout;
	if (not M_GlassElements.IsValidIndex(ElementIndex) || M_GlassElements[ElementIndex].Materials.IsEmpty())
	{
		return -1.f;
	}
	const UMaterialInstanceDynamic* const Material = M_GlassElements[ElementIndex].Materials[0];
	if (not IsValid(Material))
	{
		return -1.f;
	}
	float HoverValue = -1.f;
	Material->GetScalarParameterValue(FHashedMaterialParameterInfo(HoverParameter), HoverValue);
	return HoverValue;
}

void UW_InfoTakeover::SetGlassElementForcedHover(const int32 ElementIndex, const bool bIsHovered,
                                                 const float FrozenSweepProgress)
{
	if (not M_GlassElements.IsValidIndex(ElementIndex))
	{
		return;
	}
	M_GlassElements[ElementIndex].bIsForcedHover = bIsHovered;
	M_GlassElements[ElementIndex].Animator.FrozenSweepProgress = FrozenSweepProgress;
}

UPauseSafeProtoRichTextBlock* UW_InfoTakeover::BuildTypewriterBlock(UDataTable* TextStyleSet)
{
	UPauseSafeProtoRichTextBlock* const TextBlock = WidgetTree->ConstructWidget<UPauseSafeProtoRichTextBlock>(
		UPauseSafeProtoRichTextBlock::StaticClass());
	if (IsValid(TextStyleSet))
	{
		// Not SetProtoTextStyleSet: it rebuilds only the base rich text style and marks it built, so Proto never
		// reads its Default row. Set before the Slate widget exists, the first build reads both.
		TextBlock->ProtoTextStyleSet = TextStyleSet;
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_InfoTakeover: a Proto text style set of M_Style is missing; the "
			"typewriter text uses the engine default style."));
	}
	TextBlock->OverridePrintLetterInterval = true;
	return TextBlock;
}

void UW_InfoTakeover::LoadSounds()
{
	M_TitleTypingSound = M_Style.TitleTypingSound.LoadSynchronous();
	M_DescriptionOngoingSound = M_Style.DescriptionOngoingSound.LoadSynchronous();
	M_DescriptionStartSounds.Reset();
	for (const TSoftObjectPtr<USoundBase>& StartSound : M_Style.DescriptionStartSounds)
	{
		if (USoundBase* const LoadedSound = StartSound.LoadSynchronous())
		{
			M_DescriptionStartSounds.Add(LoadedSound);
		}
	}
}

void UW_InfoTakeover::OpenTakeover()
{
	using namespace InfoTakeoverLayout;
	if (not IsInViewport())
	{
		AddToViewport(ViewportZOrder);
	}
	OvertakeUI(this);
	SetKeyboardFocus();
	RefreshPageControls();
	// The description waits hidden until the title has typed in.
	if (GetIsValidDescriptionText() && not M_DescriptionText->GetText().IsEmpty())
	{
		M_DescriptionText->HideText();
	}
	StartTitleTyping();
}

void UW_InfoTakeover::SetKindLabel(const FText& KindLabel)
{
	if (IsValid(M_KindLabelText))
	{
		M_KindLabelText->SetText(KindLabel);
	}
}

void UW_InfoTakeover::HideKindLabel()
{
	if (not IsValid(M_KindLabelText))
	{
		return;
	}
	M_KindLabelText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_InfoTakeover::SetTitleText(const FText& Title)
{
	if (not GetIsValidTitleText())
	{
		return;
	}
	M_TitleText->SetText(UPauseSafeProtoRichTextBlock::MakeTypewriterText(Title));
}

void UW_InfoTakeover::SetDescriptionText(const FText& Description)
{
	if (not GetIsValidDescriptionText())
	{
		return;
	}
	M_DescriptionText->SetText(UPauseSafeProtoRichTextBlock::MakeTypewriterText(Description));
}

void UW_InfoTakeover::StartTitleTyping()
{
	if (not GetIsValidTitleText())
	{
		return;
	}
	M_TypingPhase = EInfoTakeoverTypingPhase::Title;
	// Proto stops an empty print without its finish event, which would leave the sequence waiting.
	if (M_TitleText->GetText().IsEmpty())
	{
		StartDescriptionTyping();
		return;
	}
	M_TitleText->PrintLetterInterval = GetLetterIntervalForDuration(
		M_TitleText->GetText(), M_Style.TitleTypingSeconds, M_Style.TitleTypingSeconds);
	M_TitleText->OnFinishLetterByLetterPrint.AddUniqueDynamic(this, &UW_InfoTakeover::HandleTitlePrintFinished);
	PlayTypingSound(M_TitleTypingSound, false);
	M_TitleText->PrintLetterByLetter();
}

void UW_InfoTakeover::HandleTitlePrintFinished(bool bStoppedEarly)
{
	if (M_TypingPhase != EInfoTakeoverTypingPhase::Title)
	{
		return;
	}
	StartDescriptionTyping();
}

void UW_InfoTakeover::StartDescriptionTyping()
{
	if (not GetIsValidDescriptionText())
	{
		FinishTyping();
		return;
	}
	M_TypingPhase = EInfoTakeoverTypingPhase::Description;
	if (M_DescriptionText->GetText().IsEmpty())
	{
		FinishTyping();
		return;
	}
	M_DescriptionText->PrintLetterInterval = GetLetterIntervalForDuration(
		M_DescriptionText->GetText(), M_Style.DescriptionMaxTypingSeconds, M_Style.DescriptionLetterInterval);
	M_DescriptionText->OnFinishLetterByLetterPrint.AddUniqueDynamic(
		this, &UW_InfoTakeover::HandleDescriptionPrintFinished);
	USoundBase* const StartSound = M_DescriptionStartSounds.IsEmpty()
		                               ? nullptr
		                               : M_DescriptionStartSounds[FMath::RandRange(
			                               0, M_DescriptionStartSounds.Num() - 1)].Get();
	PlayTypingSound(StartSound, false);
	M_DescriptionText->PrintLetterByLetter();
}

void UW_InfoTakeover::HandleDescriptionPrintFinished(bool bStoppedEarly)
{
	if (M_TypingPhase != EInfoTakeoverTypingPhase::Description)
	{
		return;
	}
	FinishTyping();
}

void UW_InfoTakeover::FinishTyping()
{
	M_TypingPhase = EInfoTakeoverTypingPhase::Done;
	StopTypingSound();
}

void UW_InfoTakeover::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slate ticks this while the game is paused; InDeltaTime is real time.
	if (M_TypingPhase == EInfoTakeoverTypingPhase::Title && IsValid(M_TitleText))
	{
		M_TitleText->TickLetterPrint(InDeltaTime);
	}
	else if (M_TypingPhase == EInfoTakeoverTypingPhase::Description && IsValid(M_DescriptionText))
	{
		M_DescriptionText->TickLetterPrint(InDeltaTime);
	}
	TickTypingSound(InDeltaTime);
	TickGlassHover(InDeltaTime);
}

void UW_InfoTakeover::TickTypingSound(const float RealDeltaSeconds)
{
	if (M_TypingSound.SecondsLeft <= 0.f)
	{
		return;
	}
	M_TypingSound.SecondsLeft -= RealDeltaSeconds;
	if (M_TypingSound.SecondsLeft > 0.f || M_TypingPhase != EInfoTakeoverTypingPhase::Description)
	{
		return;
	}
	// The description outlasted its start sound: keep the typing audible with the ongoing loop.
	PlayTypingSound(M_DescriptionOngoingSound, true);
}

void UW_InfoTakeover::PlayTypingSound(USoundBase* Sound, const bool bIsOngoing)
{
	StopTypingSound();
	if (not IsValid(Sound))
	{
		return;
	}
	// SpawnSound2D marks the component as a UI sound, so it plays while the game is paused.
	M_TypingSound.ActiveSound = UGameplayStatics::SpawnSound2D(this, Sound);
	M_TypingSound.SecondsLeft = Sound->GetDuration();
	M_TypingSound.bIsOngoing = bIsOngoing;
	UE_LOG(LogTemp, Verbose, TEXT("INFO_TAKEOVER sound %s (%.1f s)"), *Sound->GetName(), M_TypingSound.SecondsLeft);
}

void UW_InfoTakeover::StopTypingSound()
{
	using namespace InfoTakeoverLayout;
	if (IsValid(M_TypingSound.ActiveSound))
	{
		M_TypingSound.ActiveSound->FadeOut(SoundFadeOutSeconds, 0.f);
	}
	M_TypingSound.ActiveSound = nullptr;
	M_TypingSound.SecondsLeft = 0.f;
}

float UW_InfoTakeover::GetLetterIntervalForDuration(const FText& Text, const float TargetSeconds,
                                                    const float MaxInterval)
{
	using namespace InfoTakeoverLayout;
	const int32 LetterCount = FMath::Max(
		UUnitVariantPresentationLibrary::StripRichTextMarkup(Text).ToString().Len(), 1);
	return FMath::Clamp(TargetSeconds / LetterCount, MinLetterInterval, MaxInterval);
}

FReply UW_InfoTakeover::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey != EKeys::Enter && PressedKey != EKeys::SpaceBar && PressedKey != EKeys::Escape)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	// The first press finishes the typing; the next one is Understood.
	if (M_TypingPhase == EInfoTakeoverTypingPhase::Title || M_TypingPhase == EInfoTakeoverTypingPhase::Description)
	{
		if (IsValid(M_TitleText))
		{
			M_TitleText->FinishPrintNow();
		}
		if (IsValid(M_DescriptionText))
		{
			M_DescriptionText->FinishPrintNow();
		}
		FinishTyping();
		return FReply::Handled();
	}
	PressConfirmButton();
	return FReply::Handled();
}

void UW_InfoTakeover::HandleUnderstoodClicked()
{
	PressConfirmButton();
}

void UW_InfoTakeover::HandlePreviousClicked()
{
	PressPreviousButton();
}

void UW_InfoTakeover::PressPreviousButton()
{
	if (bM_IsClosing)
	{
		return;
	}
	// Never closes the takeover: on the first page there is nothing to go back to.
	TryShowPreviousPage();
}

bool UW_InfoTakeover::TryShowNextPage()
{
	const int32 NextPageIndex = GetCurrentPageIndex() + 1;
	if (NextPageIndex >= GetPageCount() || not TryShowPage(NextPageIndex))
	{
		return false;
	}
	RefreshPageControls();
	return true;
}

bool UW_InfoTakeover::TryShowPreviousPage()
{
	const int32 PreviousPageIndex = GetCurrentPageIndex() - 1;
	if (PreviousPageIndex < 0 || not TryShowPage(PreviousPageIndex))
	{
		return false;
	}
	RefreshPageControls();
	return true;
}

void UW_InfoTakeover::RefreshPageControls()
{
	const int32 PageCount = FMath::Max(GetPageCount(), 1);
	if (M_PageLamps.Num() != PageCount)
	{
		RebuildPageLamps(PageCount);
	}
	const int32 CurrentPageIndex = GetCurrentPageIndex();
	for (int32 LampIndex = 0; LampIndex < M_PageLamps.Num(); ++LampIndex)
	{
		if (IsValid(M_PageLamps[LampIndex]))
		{
			M_PageLamps[LampIndex]->SetBrush(LampIndex == CurrentPageIndex ? M_Style.PageLampLit
			                                                                : M_Style.PageLampUnlit);
		}
	}
	if (IsValid(M_PreviousButtonBox))
	{
		M_PreviousButtonBox->SetVisibility(CurrentPageIndex > 0 ? ESlateVisibility::Visible
		                                                        : ESlateVisibility::Collapsed);
	}
}

bool UW_InfoTakeover::GetIsPreviousButtonVisible() const
{
	return IsValid(M_PreviousButtonBox) && M_PreviousButtonBox->GetVisibility() == ESlateVisibility::Visible;
}

int32 UW_InfoTakeover::GetLitPageLampIndex() const
{
	int32 LitIndex = INDEX_NONE;
	for (int32 LampIndex = 0; LampIndex < M_PageLamps.Num(); ++LampIndex)
	{
		const bool bIsLit = IsValid(M_PageLamps[LampIndex]) && M_PageLamps[LampIndex]->GetBrush() == M_Style.PageLampLit;
		if (not bIsLit)
		{
			continue;
		}
		if (LitIndex != INDEX_NONE)
		{
			return INDEX_NONE;
		}
		LitIndex = LampIndex;
	}
	return LitIndex;
}

void UW_InfoTakeover::PressConfirmButton()
{
	if (bM_IsClosing || TryShowNextPage())
	{
		return;
	}
	OnClickedContinue();
}

void UW_InfoTakeover::SetConfirmButtonLabel(const FText& Label) const
{
	if (not IsValid(M_UnderstoodButton))
	{
		return;
	}
	// BuildTextButton makes the label the button's only content.
	if (UTextBlock* const LabelText = Cast<UTextBlock>(M_UnderstoodButton->GetContent()))
	{
		LabelText->SetText(Label);
	}
}

void UW_InfoTakeover::RestartTyping()
{
	if (bM_IsClosing)
	{
		return;
	}
	StopTypingSound();
	if (GetIsValidDescriptionText() && not M_DescriptionText->GetText().IsEmpty())
	{
		M_DescriptionText->HideText();
	}
	StartTitleTyping();
}

void UW_InfoTakeover::OnClickedContinue()
{
	if (bM_IsClosing)
	{
		return;
	}
	bM_IsClosing = true;
	M_TypingPhase = EInfoTakeoverTypingPhase::Done;
	StopTypingSound();
	ApplyNeverShowAgainChoice();
	Super::OnClickedContinue();
	OnTakeoverClosed.Broadcast(this);
}

void UW_InfoTakeover::SetNeverShowAgainChecked(const bool bIsChecked) const
{
	if (not GetIsValidNeverShowAgainCheckBox())
	{
		return;
	}
	M_NeverShowAgainCheckBox->SetIsChecked(bIsChecked);
}

FString UW_InfoTakeover::GetActiveTypingSoundName() const
{
	if (not IsValid(M_TypingSound.ActiveSound) || not M_TypingSound.ActiveSound->IsPlaying())
	{
		return FString();
	}
	const USoundBase* const Sound = M_TypingSound.ActiveSound->Sound;
	return IsValid(Sound) ? Sound->GetName() : FString();
}

void UW_InfoTakeover::ApplyNeverShowAgainChoice() const
{
	if (not GetIsValidNeverShowAgainCheckBox() || not M_NeverShowAgainCheckBox->IsChecked())
	{
		return;
	}
	const UGameInstance* const GameInstance = GetGameInstance();
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GameInstance != nullptr
		                                                     ? GameInstance->GetSubsystem<URTSSettingsMenuSubsystem>()
		                                                     : nullptr;
	if (SettingsSubsystem == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_InfoTakeover: no settings subsystem to store \"never show again\"."));
		return;
	}
	SettingsSubsystem->SetShowInfoTakeoversImmediately(false);
}

bool UW_InfoTakeover::GetIsValidTitleText() const
{
	if (IsValid(M_TitleText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TitleText", "GetIsValidTitleText", this);
	return false;
}

bool UW_InfoTakeover::GetIsValidDescriptionText() const
{
	if (IsValid(M_DescriptionText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DescriptionText",
	                                                             "GetIsValidDescriptionText", this);
	return false;
}

bool UW_InfoTakeover::GetIsValidNeverShowAgainCheckBox() const
{
	if (IsValid(M_NeverShowAgainCheckBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_NeverShowAgainCheckBox",
	                                                             "GetIsValidNeverShowAgainCheckBox", this);
	return false;
}
