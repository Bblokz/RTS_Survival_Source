// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MissionEnd.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/SubtitleStyleDefaults.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace MissionEndConstants
{
	constexpr int32 KickerFontSize = 17;
	constexpr int32 ReasonFontSize = 21;
	constexpr int32 ButtonFontSize = 20;
	constexpr int32 KickerLetterSpacing = 320;
	constexpr int32 ButtonLetterSpacing = 160;
	constexpr float FadeInSeconds = 0.8f;
	constexpr float RevealSeconds = 0.7f;
	constexpr float RevealRise = 24.f;
	constexpr float BottomGap = 64.f;
	constexpr float ShadeHeightFraction = 0.34f;
	constexpr float ButtonGap = 22.f;
	constexpr float ButtonMinWidth = 230.f;
	constexpr int32 ShadeGradientHeight = 64;
	constexpr float ShadeMaxAlpha = 0.88f;

	float EaseOutCubic(const float X)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(X, 0.f, 1.f), 3.f);
	}
}

FMissionEndStyle::FMissionEndStyle()
{
	// Gunmetal plates with a thin lighter rim; the red strip under the label is part of the button content.
	const FLinearColor Plate = FLinearColor(FColor::FromHex(TEXT("2A2D30F2")));
	const FLinearColor PlateHover = FLinearColor(FColor::FromHex(TEXT("3A3E42F5")));
	const FLinearColor PlatePressed = FLinearColor(FColor::FromHex(TEXT("1D1F22F5")));
	const FLinearColor Rim = FLinearColor(FColor::FromHex(TEXT("55595EFF")));
	constexpr float Radius = 4.f;
	constexpr float RimWidth = 1.f;
	ButtonStyle.SetNormal(FSlateRoundedBoxBrush(Plate, Radius, Rim, RimWidth));
	ButtonStyle.SetHovered(FSlateRoundedBoxBrush(PlateHover, Radius, FLinearColor(FColor::FromHex(TEXT("8A8F94FF"))), RimWidth));
	ButtonStyle.SetPressed(FSlateRoundedBoxBrush(PlatePressed, Radius, Rim, RimWidth));
	ButtonStyle.SetNormalPadding(FMargin(0.f));
	ButtonStyle.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 0.f));
}

UW_MissionEnd::UW_MissionEnd(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace MissionEndConstants;
	static ConstructorHelpers::FObjectFinder<UFont> MainFont(SubtitleStyleDefaults::MainFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> ReadableFont(SubtitleStyleDefaults::ReadableFontPath);
	const UObject* const MainFontObject = MainFont.Succeeded() ? MainFont.Object : nullptr;
	const UObject* const ReadableFontObject = ReadableFont.Succeeded() ? ReadableFont.Object : nullptr;
	Style.KickerFont = MainFontObject != nullptr
		                   ? FSlateFontInfo(MainFontObject, KickerFontSize)
		                   : FCoreStyle::GetDefaultFontStyle("Bold", KickerFontSize);
	Style.KickerFont.LetterSpacing = KickerLetterSpacing;
	Style.ButtonFont = MainFontObject != nullptr
		                   ? FSlateFontInfo(MainFontObject, ButtonFontSize)
		                   : FCoreStyle::GetDefaultFontStyle("Bold", ButtonFontSize);
	Style.ButtonFont.LetterSpacing = ButtonLetterSpacing;
	Style.ReasonFont = ReadableFontObject != nullptr
		                   ? FSlateFontInfo(ReadableFontObject, ReasonFontSize, FName(SubtitleStyleDefaults::ReadableTypeface))
		                   : FCoreStyle::GetDefaultFontStyle("Regular", ReasonFontSize);
}

void UW_MissionEnd::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_MissionEnd::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	M_BottomShade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_ShadeGradient = CreateShadeGradient();
	if (M_ShadeGradient != nullptr)
	{
		M_BottomShade->SetBrushFromTexture(M_ShadeGradient);
	}
	UCanvasPanelSlot* const ShadeSlot = Canvas->AddChildToCanvas(M_BottomShade);
	ShadeSlot->SetAnchors(FAnchors(0.f, 1.f - MissionEndConstants::ShadeHeightFraction, 1.f, 1.f));
	ShadeSlot->SetOffsets(FMargin(0.f));

	BuildWidgetTree_EndBar(Canvas);

	// The plain image brush is a white quad; tinted black it covers the cut to the stage and its first frames.
	M_FadeBlack = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_FadeBlack->SetColorAndOpacity(FLinearColor::Black);
	M_FadeBlack->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const FadeSlot = Canvas->AddChildToCanvas(M_FadeBlack);
	FadeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FadeSlot->SetOffsets(FMargin(0.f));
	ApplyRevealProgress(0.f);
}

void UW_MissionEnd::BuildWidgetTree_EndBar(UCanvasPanel* Canvas)
{
	M_EndBar = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* const BarSlot = Canvas->AddChildToCanvas(M_EndBar);
	BarSlot->SetAnchors(FAnchors(0.5f, 1.f));
	BarSlot->SetAlignment(FVector2D(0.5f, 1.f));
	BarSlot->SetAutoSize(true);
	BarSlot->SetPosition(FVector2D(0.f, -MissionEndConstants::BottomGap));

	auto AddCentredText = [this](const FSlateFontInfo& Font, const FLinearColor& Colour, const float BottomPadding)
	{
		UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Colour));
		Text->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* const TextSlot = M_EndBar->AddChildToVerticalBox(Text);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		return Text;
	};
	M_KickerText = AddCentredText(Style.KickerFont, Style.SignalColor, 10.f);
	M_ReasonText = AddCentredText(Style.ReasonFont, Style.BoneDimColor, 18.f);

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* const ButtonsSlot = M_EndBar->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* PrimaryLabel = nullptr;
	UTextBlock* SecondaryLabel = nullptr;
	M_PrimaryButton = BuildButton(PrimaryLabel, true);
	M_SecondaryButton = BuildButton(SecondaryLabel, false);
	M_PrimaryLabel = PrimaryLabel;
	M_SecondaryLabel = SecondaryLabel;
	UHorizontalBoxSlot* const PrimarySlot = Buttons->AddChildToHorizontalBox(M_PrimaryButton);
	PrimarySlot->SetPadding(FMargin(0.f, 0.f, MissionEndConstants::ButtonGap * 0.5f, 0.f));
	UHorizontalBoxSlot* const SecondarySlot = Buttons->AddChildToHorizontalBox(M_SecondaryButton);
	SecondarySlot->SetPadding(FMargin(MissionEndConstants::ButtonGap * 0.5f, 0.f, 0.f, 0.f));
	M_PrimaryButton->OnClicked.AddDynamic(this, &UW_MissionEnd::HandlePrimaryClicked);
	M_SecondaryButton->OnClicked.AddDynamic(this, &UW_MissionEnd::HandleSecondaryClicked);
}

UButton* UW_MissionEnd::BuildButton(UTextBlock*& OutLabel, const bool bPrimary)
{
	UButton* const Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(Style.ButtonStyle);
	USizeBox* const Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetMinDesiredWidth(MissionEndConstants::ButtonMinWidth);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Size->AddChild(Column);
	OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OutLabel->SetFont(Style.ButtonFont);
	OutLabel->SetColorAndOpacity(FSlateColor(Style.BoneColor));
	OutLabel->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const LabelSlot = Column->AddChildToVerticalBox(OutLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetPadding(FMargin(28.f, 13.f, 28.f, 7.f));
	// The signal-red strip under the label: bright on the primary button, faint on the other.
	USizeBox* const StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetHeightOverride(2.f);
	StripBox->SetWidthOverride(MissionEndConstants::ButtonMinWidth * 0.62f);
	UImage* const Strip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FLinearColor StripColour = Style.SignalColor;
	StripColour.A = bPrimary ? 0.95f : 0.28f;
	Strip->SetColorAndOpacity(StripColour);
	StripBox->AddChild(Strip);
	UVerticalBoxSlot* const StripSlot = Column->AddChildToVerticalBox(StripBox);
	StripSlot->SetHorizontalAlignment(HAlign_Center);
	StripSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
	Button->AddChild(Size);
	return Button;
}

UTexture2D* UW_MissionEnd::CreateShadeGradient()
{
	using namespace MissionEndConstants;
	UTexture2D* const Gradient = UTexture2D::CreateTransient(1, ShadeGradientHeight, PF_B8G8R8A8);
	if (Gradient == nullptr)
	{
		return nullptr;
	}
	FTexture2DMipMap& Mip = Gradient->GetPlatformData()->Mips[0];
	FColor* const Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Row = 0; Row < ShadeGradientHeight; ++Row)
	{
		const float Alpha = static_cast<float>(Row) / static_cast<float>(ShadeGradientHeight - 1);
		Pixels[Row] = FColor(4, 5, 6, static_cast<uint8>(Alpha * ShadeMaxAlpha * 255.f));
	}
	Mip.BulkData.Unlock();
	Gradient->SRGB = false;
	Gradient->UpdateResource();
	return Gradient;
}

void UW_MissionEnd::SetupEndScreen(const EMissionDebriefOutcome Outcome, const FText& ReasonText,
                                   const TSoftObjectPtr<UWorld>& ContinueLevel, const TSoftObjectPtr<UWorld>& MenuLevel,
                                   const bool bShowBackToMenu)
{
	M_Outcome = Outcome;
	M_ContinueLevel = ContinueLevel;
	M_MenuLevel = MenuLevel;
	const bool bVictory = Outcome == EMissionDebriefOutcome::Victory;
	if (not IsValid(M_KickerText))
	{
		RTSFunctionLibrary::ReportError("UW_MissionEnd::SetupEndScreen ran before the widget tree was built.");
		return;
	}
	M_KickerText->SetText(bVictory ? NSLOCTEXT("MissionEnd", "Accomplished", "MISSION ACCOMPLISHED")
		                      : NSLOCTEXT("MissionEnd", "Failed", "MISSION FAILED"));
	M_ReasonText->SetText(ReasonText);
	M_PrimaryLabel->SetText(bVictory ? NSLOCTEXT("MissionEnd", "Continue", "CONTINUE")
		                        : NSLOCTEXT("MissionEnd", "Restart", "RESTART"));
	M_SecondaryLabel->SetText(bVictory ? NSLOCTEXT("MissionEnd", "MainMenu", "MAIN MENU")
		                          : NSLOCTEXT("MissionEnd", "Quit", "QUIT"));
	const URTSGameInstance* const CampaignGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not bVictory && IsValid(CampaignGameInstance) && CampaignGameInstance->GetIsCampaignOperation())
	{
		// In the campaign, giving a lost mission up returns to the world map and hands the enemy a turn.
		M_SecondaryLabel->SetText(NSLOCTEXT("MissionEnd", "Withdraw", "WITHDRAW"));
	}
	const bool bShowSecondary = not bVictory || bShowBackToMenu;
	M_SecondaryButton->SetVisibility(bShowSecondary ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_MissionEnd::RevealButtons()
{
	bM_AreButtonsRevealed = true;
	M_RevealSeconds = 0.f;
}

void UW_MissionEnd::SkipFadeIn()
{
	M_FadeSeconds = MissionEndConstants::FadeInSeconds;
	if (IsValid(M_FadeBlack))
	{
		M_FadeBlack->SetRenderOpacity(0.f);
	}
}

void UW_MissionEnd::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace MissionEndConstants;
	// Widgets tick on real time while the world is paused.
	M_FadeSeconds = FMath::Min(M_FadeSeconds + InDeltaTime, FadeInSeconds);
	if (IsValid(M_FadeBlack))
	{
		M_FadeBlack->SetRenderOpacity(1.f - EaseOutCubic(M_FadeSeconds / FadeInSeconds));
	}
	if (not bM_AreButtonsRevealed)
	{
		return;
	}
	M_RevealSeconds = FMath::Min(M_RevealSeconds + InDeltaTime, RevealSeconds);
	ApplyRevealProgress(EaseOutCubic(M_RevealSeconds / RevealSeconds));
}

void UW_MissionEnd::ApplyRevealProgress(const float Progress) const
{
	if (IsValid(M_EndBar))
	{
		M_EndBar->SetRenderOpacity(Progress);
		M_EndBar->SetRenderTranslation(FVector2D(0.f, (1.f - Progress) * MissionEndConstants::RevealRise));
		M_EndBar->SetVisibility(Progress > 0.f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IsValid(M_BottomShade))
	{
		M_BottomShade->SetRenderOpacity(Progress);
	}
}

void UW_MissionEnd::HandlePrimaryClicked()
{
	if (M_Outcome == EMissionDebriefOutcome::Victory)
	{
		OpenLevel(M_ContinueLevel, TEXT("continue after victory"));
		return;
	}
	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(this, true));
	UGameplayStatics::OpenLevel(this, CurrentLevel);
}

void UW_MissionEnd::HandleSecondaryClicked()
{
	if (M_Outcome == EMissionDebriefOutcome::Victory)
	{
		OpenLevel(M_MenuLevel, TEXT("return to the main menu"));
		return;
	}
	URTSGameInstance* const CampaignGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (IsValid(CampaignGameInstance) && CampaignGameInstance->GetIsCampaignOperation())
	{
		CampaignGameInstance->ReportWorldMissionResult(EWorldMissionOutcome::Defeat);
		OpenLevel(URTSGameInstance::GetWorldCampaignMap(), TEXT("withdraw to the world map"));
		return;
	}
	APlayerController* const OwningPlayer = GetOwningPlayer();
	if (not IsValid(OwningPlayer))
	{
		RTSFunctionLibrary::ReportError("Mission end widget could not quit: no owning player controller.");
		return;
	}
	UKismetSystemLibrary::QuitGame(this, OwningPlayer, EQuitPreference::Quit, true);
}

void UW_MissionEnd::OpenLevel(const TSoftObjectPtr<UWorld>& Level, const TCHAR* const ActionName) const
{
	if (Level.IsNull())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Mission end widget could not %s: no level set."), ActionName));
		return;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level, true, FString());
}
