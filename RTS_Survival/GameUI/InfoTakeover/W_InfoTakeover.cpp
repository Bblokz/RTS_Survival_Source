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
	return Header;
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
	UWidget* const UnderstoodButtonBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style.Skin, NSLOCTEXT("InfoTakeover", "Understood", "UNDERSTOOD"), M_Style.Skin.ConfirmButton,
		UnderstoodButtonSize, M_UnderstoodButton);
	Footer->AddChildToHorizontalBox(UnderstoodButtonBox)->SetVerticalAlignment(VAlign_Center);
	M_UnderstoodButton->OnClicked.AddDynamic(this, &UW_InfoTakeover::HandleUnderstoodClicked);
	return Footer;
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
