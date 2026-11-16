// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MissionCameraTour.h"

#include "Blueprint/WidgetTree.h"
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
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/SubtitleStyleDefaults.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionCameraTourWidgetConstants
{
	// Same layer as the location intro: above the HUD (hidden during the tour anyway) and the cinematic overlay.
	constexpr int32 ViewportZOrder = 6;
	constexpr int32 TitleFontSize = 30;
	constexpr int32 TitleLetterSpacing = 280;
	constexpr int32 SubtitleFontSize = 15;
	constexpr int32 SubtitleLetterSpacing = 120;
	constexpr float RuleThickness = 2.f;
	constexpr float RuleTitleGap = 18.f;
	constexpr float SubtitleTopGap = 6.f;
	// Caption layout distance from the screen bottom when the bars are off.
	constexpr float NoLetterboxLayoutFraction = 0.035f;
	const FVector2D ShadowOffset(1.5f, 1.5f);
}

UW_MissionCameraTour::UW_MissionCameraTour(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace MissionCameraTourWidgetConstants;
	static ConstructorHelpers::FObjectFinder<UFont> MainFont(SubtitleStyleDefaults::MainFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> ReadableFont(SubtitleStyleDefaults::ReadableFontPath);
	const UObject* const MainFontObject = MainFont.Succeeded() ? MainFont.Object : nullptr;
	const UObject* const ReadableFontObject = ReadableFont.Succeeded() ? ReadableFont.Object : nullptr;
	TitleFont = MainFontObject != nullptr
		            ? FSlateFontInfo(MainFontObject, TitleFontSize)
		            : FCoreStyle::GetDefaultFontStyle("Bold", TitleFontSize);
	TitleFont.LetterSpacing = TitleLetterSpacing;
	SubtitleFont = ReadableFontObject != nullptr
		               ? FSlateFontInfo(ReadableFontObject, SubtitleFontSize,
		                                FName(SubtitleStyleDefaults::ReadableTypeface))
		               : FCoreStyle::GetDefaultFontStyle("Regular", SubtitleFontSize);
	SubtitleFont.LetterSpacing = SubtitleLetterSpacing;
}

UW_MissionCameraTour* UW_MissionCameraTour::ShowTour(APlayerController* OwningPlayer, const bool bShowLetterbox,
                                                     const float LetterboxHeightFraction)
{
	if (not IsValid(OwningPlayer))
	{
		RTSFunctionLibrary::ReportError("UW_MissionCameraTour::ShowTour needs a player controller.");
		return nullptr;
	}
	UW_MissionCameraTour* const Tour = CreateWidget<UW_MissionCameraTour>(OwningPlayer, StaticClass());
	if (not IsValid(Tour))
	{
		RTSFunctionLibrary::ReportError("UW_MissionCameraTour::ShowTour could not create the widget.");
		return nullptr;
	}
	Tour->bM_ShowLetterbox = bShowLetterbox;
	Tour->M_LetterboxHeightFraction = FMath::Clamp(LetterboxHeightFraction, 0.f, 0.2f);
	Tour->PlaceCaption();
	Tour->SetBars(0.f);
	Tour->SetCaptionVisibility(0.f);
	Tour->AddToViewport(MissionCameraTourWidgetConstants::ViewportZOrder);
	return Tour;
}

void UW_MissionCameraTour::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_MissionCameraTour::CloseTour()
{
	RemoveFromParent();
}

void UW_MissionCameraTour::SetBars(const float BarsShown) const
{
	if (not GetIsTreeBuilt())
	{
		return;
	}
	const float BarHeight = bM_ShowLetterbox ? M_LetterboxHeightFraction * FMath::Clamp(BarsShown, 0.f, 1.f) : 0.f;
	M_TopBarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, BarHeight));
	M_BottomBarSlot->SetAnchors(FAnchors(0.f, 1.f - BarHeight, 1.f, 1.f));
}

void UW_MissionCameraTour::SetCaption(const FText& Title, const FText& Subtitle)
{
	if (not GetIsTreeBuilt())
	{
		return;
	}
	M_TitleText->SetText(Title.ToUpper());
	M_SubtitleText->SetText(Subtitle);
	M_SubtitleText->SetVisibility(Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	M_CaptionBlock->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UW_MissionCameraTour::SetCaptionVisibility(const float CaptionShown) const
{
	if (not GetIsTreeBuilt())
	{
		return;
	}
	const float Clamped = FMath::Clamp(CaptionShown, 0.f, 1.f);
	M_CaptionBlock->SetRenderOpacity(Clamped);
	// The caption settles upwards into place while it fades in.
	M_CaptionBlock->SetRenderTranslation(FVector2D(0.f, 8.f * (1.f - Clamped)));
}

void UW_MissionCameraTour::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;
	BuildBars(Canvas);
	BuildCaption(Canvas);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_MissionCameraTour::BuildBars(UCanvasPanel* Canvas)
{
	// The default image brush is a plain white quad; tinted black it is a cinema bar.
	UImage* const TopBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TopBar->SetColorAndOpacity(FLinearColor::Black);
	M_TopBarSlot = Canvas->AddChildToCanvas(TopBar);
	M_TopBarSlot->SetOffsets(FMargin(0.f));

	UImage* const BottomBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	BottomBar->SetColorAndOpacity(FLinearColor::Black);
	M_BottomBarSlot = Canvas->AddChildToCanvas(BottomBar);
	M_BottomBarSlot->SetOffsets(FMargin(0.f));
}

void UW_MissionCameraTour::BuildCaption(UCanvasPanel* Canvas)
{
	using namespace MissionCameraTourWidgetConstants;
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_CaptionBlock = Column;
	M_CaptionSlot = Canvas->AddChildToCanvas(Column);
	M_CaptionSlot->SetAlignment(FVector2D(0.5f, 1.f));
	M_CaptionSlot->SetAutoSize(true);

	// [rule] TITLE [rule]
	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* const TitleRowSlot = Column->AddChildToVerticalBox(TitleRow);
	TitleRowSlot->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBoxSlot* const LeftRuleSlot = TitleRow->AddChildToHorizontalBox(MakeRule());
	LeftRuleSlot->SetVerticalAlignment(VAlign_Center);

	M_TitleText = MakeText(TitleFont, TextColor);
	M_TitleText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* const TitleSlot = TitleRow->AddChildToHorizontalBox(M_TitleText);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(FMargin(RuleTitleGap, 0.f));

	UHorizontalBoxSlot* const RightRuleSlot = TitleRow->AddChildToHorizontalBox(MakeRule());
	RightRuleSlot->SetVerticalAlignment(VAlign_Center);

	M_SubtitleText = MakeText(SubtitleFont, SubtitleColor);
	M_SubtitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const SubtitleSlot = Column->AddChildToVerticalBox(M_SubtitleText);
	SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
	SubtitleSlot->SetPadding(FMargin(0.f, SubtitleTopGap, 0.f, 0.f));
}

UTextBlock* UW_MissionCameraTour::MakeText(const FSlateFontInfo& Font, const FLinearColor& Color) const
{
	UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowColorAndOpacity(ShadowColor);
	Text->SetShadowOffset(MissionCameraTourWidgetConstants::ShadowOffset);
	return Text;
}

USizeBox* UW_MissionCameraTour::MakeRule() const
{
	USizeBox* const RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RuleBox->SetHeightOverride(MissionCameraTourWidgetConstants::RuleThickness);
	RuleBox->SetWidthOverride(RuleWidth);
	UImage* const Rule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Rule->SetColorAndOpacity(AccentColor);
	RuleBox->SetContent(Rule);
	return RuleBox;
}

void UW_MissionCameraTour::PlaceCaption() const
{
	if (not GetIsTreeBuilt())
	{
		return;
	}
	const float BarFraction = bM_ShowLetterbox
		                          ? M_LetterboxHeightFraction
		                          : MissionCameraTourWidgetConstants::NoLetterboxLayoutFraction;
	M_CaptionSlot->SetAnchors(FAnchors(0.5f, 1.f - BarFraction));
	M_CaptionSlot->SetPosition(FVector2D(0.f, -CaptionBottomGap));
}

bool UW_MissionCameraTour::GetIsTreeBuilt() const
{
	if (IsValid(M_TopBarSlot) && IsValid(M_BottomBarSlot) && IsValid(M_CaptionSlot) && IsValid(M_TitleText)
		&& IsValid(M_SubtitleText) && IsValid(M_CaptionBlock))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("UW_MissionCameraTour: the widget tree was not built.");
	return false;
}
