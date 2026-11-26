// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_LocationIntro.h"

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
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/SubtitleStyleDefaults.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace LocationIntroConstants
{
	// Above the HUD and the cinematic overlay (5); the HUD is hidden during the intro anyway.
	constexpr int32 ViewportZOrder = 6;
	constexpr int32 LocationFontSize = 44;
	constexpr int32 SubtitleFontSize = 15;
	constexpr int32 SubtitleLetterSpacing = 260;
	constexpr int32 DateFontSize = 23;
	constexpr int32 TimeFontSize = 15;
	inline const TCHAR* TypewriterFontPath = TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono");

	// Bar position used for the layout when the bars are off, so the texts keep clear of the screen edges.
	constexpr float NoLetterboxLayoutFraction = 0.035f;
	constexpr float BarsInSeconds = 0.9f;

	constexpr float RuleGrowSeconds = 0.9f;
	constexpr float RuleThickness = 2.f;
	constexpr float RuleTitleGap = 24.f;
	constexpr float LocationFadeDelaySeconds = 0.15f;
	constexpr float LocationFadeSeconds = 1.2f;
	constexpr float LocationTrackingSeconds = 1.9f;
	// The location rises into place by this many units while it fades in.
	constexpr float LocationRiseDistance = 10.f;
	constexpr float SubtitleDelaySeconds = 0.9f;
	constexpr float SubtitleFadeSeconds = 0.8f;
	constexpr float SubtitleTopGap = 8.f;

	constexpr float AccentLeadSeconds = 0.35f;
	constexpr float AccentGrowSeconds = 0.5f;
	constexpr float AccentWidth = 3.f;
	constexpr float AccentTextGap = 16.f;
	constexpr float TimeTopGap = 4.f;
	// Pause between the date and the time line.
	constexpr float TimeLineDelaySeconds = 0.25f;
	constexpr float CursorBlinkPeriodSeconds = 0.9f;
	inline const TCHAR* TypewriterCursor = TEXT("_");

	const FVector2D ShadowOffset(1.5f, 1.5f);
}

namespace LocationIntroEasing
{
	float GetPhase(const float ElapsedSeconds, const float StartSeconds, const float LengthSeconds)
	{
		if (LengthSeconds <= KINDA_SMALL_NUMBER)
		{
			return ElapsedSeconds >= StartSeconds ? 1.f : 0.f;
		}
		return FMath::Clamp((ElapsedSeconds - StartSeconds) / LengthSeconds, 0.f, 1.f);
	}

	float EaseOutCubic(const float Alpha)
	{
		return 1.f - FMath::Pow(1.f - Alpha, 3.f);
	}

	float EaseInOutSine(const float Alpha)
	{
		return 0.5f - 0.5f * FMath::Cos(PI * Alpha);
	}
}

UW_LocationIntro::UW_LocationIntro(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace LocationIntroConstants;
	static ConstructorHelpers::FObjectFinder<UFont> MainFont(SubtitleStyleDefaults::MainFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> ReadableFont(SubtitleStyleDefaults::ReadableFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> TypewriterFont(TypewriterFontPath);
	const UObject* const MainFontObject = MainFont.Succeeded() ? MainFont.Object : nullptr;
	const UObject* const ReadableFontObject = ReadableFont.Succeeded() ? ReadableFont.Object : nullptr;
	const UObject* const TypewriterFontObject = TypewriterFont.Succeeded() ? TypewriterFont.Object : nullptr;

	Style.LocationFont = MainFontObject != nullptr
		                     ? FSlateFontInfo(MainFontObject, LocationFontSize)
		                     : FCoreStyle::GetDefaultFontStyle("Bold", LocationFontSize);
	Style.SubtitleFont = ReadableFontObject != nullptr
		                     ? FSlateFontInfo(ReadableFontObject, SubtitleFontSize,
		                                      FName(SubtitleStyleDefaults::ReadableTypeface))
		                     : FCoreStyle::GetDefaultFontStyle("Regular", SubtitleFontSize);
	Style.SubtitleFont.LetterSpacing = SubtitleLetterSpacing;
	Style.DateFont = TypewriterFontObject != nullptr
		                 ? FSlateFontInfo(TypewriterFontObject, DateFontSize)
		                 : FCoreStyle::GetDefaultFontStyle("Mono", DateFontSize);
	Style.TimeFont = TypewriterFontObject != nullptr
		                 ? FSlateFontInfo(TypewriterFontObject, TimeFontSize)
		                 : FCoreStyle::GetDefaultFontStyle("Mono", TimeFontSize);
}

UW_LocationIntro* UW_LocationIntro::ShowIntro(APlayerController* OwningPlayer,
                                              const FLocationIntroTexts& Texts,
                                              const FLocationIntroPresentation& Presentation,
                                              const float DurationSeconds)
{
	if (not IsValid(OwningPlayer))
	{
		RTSFunctionLibrary::ReportError("UW_LocationIntro::ShowIntro needs a player controller.");
		return nullptr;
	}
	UW_LocationIntro* const Intro = CreateWidget<UW_LocationIntro>(OwningPlayer, StaticClass());
	if (not IsValid(Intro))
	{
		RTSFunctionLibrary::ReportError("UW_LocationIntro::ShowIntro could not create the widget.");
		return nullptr;
	}
	Intro->ApplyTexts(Texts, Presentation, DurationSeconds);
	Intro->AddToViewport(LocationIntroConstants::ViewportZOrder);
	Intro->SetElapsedSeconds(0.f);
	return Intro;
}

float UW_LocationIntro::GetMinDurationSeconds(const FLocationIntroTexts& Texts,
                                              const FLocationIntroPresentation& Presentation)
{
	// The texts stay fully shown at least this long between typing out and the fade.
	constexpr float MinFullyShownSeconds = 1.f;
	// Matches the pause between the date and the time line.
	constexpr float TimeLineDelaySeconds = 0.25f;
	const float LettersPerSecond = FMath::Max(1.f, Presentation.TypewriterLettersPerSecond);
	const int32 TypedLetters = Texts.Date.ToString().Len() + Texts.TimeOfDay.ToString().Len();
	const float TypingSeconds = TypedLetters / LettersPerSecond + TimeLineDelaySeconds;
	const float TypedSeconds = Presentation.DateStartSeconds + TypingSeconds;
	const float LocationSeconds = Presentation.LocationStartSeconds + MinFullyShownSeconds;
	return FMath::Max(TypedSeconds, LocationSeconds) + MinFullyShownSeconds + Presentation.FadeOutSeconds;
}

void UW_LocationIntro::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_LocationIntro::CloseIntro()
{
	RemoveFromParent();
}

void UW_LocationIntro::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;
	BuildLetterboxAndFade(Canvas);
	BuildLocationBlock(Canvas);
	BuildDateBlock(Canvas);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_LocationIntro::BuildLetterboxAndFade(UCanvasPanel* Canvas)
{
	// The default image brush is a plain white quad; tinted black it is a bar or the fade layer.
	M_FadeBlack = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_FadeBlack->SetColorAndOpacity(FLinearColor::Black);
	UCanvasPanelSlot* const FadeSlot = Canvas->AddChildToCanvas(M_FadeBlack);
	FadeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FadeSlot->SetOffsets(FMargin(0.f));

	UImage* const TopBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	TopBar->SetColorAndOpacity(FLinearColor::Black);
	M_TopBarSlot = Canvas->AddChildToCanvas(TopBar);
	M_TopBarSlot->SetOffsets(FMargin(0.f));

	UImage* const BottomBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	BottomBar->SetColorAndOpacity(FLinearColor::Black);
	M_BottomBarSlot = Canvas->AddChildToCanvas(BottomBar);
	M_BottomBarSlot->SetOffsets(FMargin(0.f));
}

void UW_LocationIntro::BuildLocationBlock(UCanvasPanel* Canvas)
{
	using namespace LocationIntroConstants;
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_LocationBlock = Column;
	M_LocationBlockSlot = Canvas->AddChildToCanvas(Column);
	M_LocationBlockSlot->SetAlignment(FVector2D(0.5f, 0.f));
	M_LocationBlockSlot->SetAutoSize(true);

	// [rule] LOCATION [rule]
	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* const TitleRowSlot = Column->AddChildToVerticalBox(TitleRow);
	TitleRowSlot->SetHorizontalAlignment(HAlign_Center);

	M_LeftRuleBox = MakeRule(Style.AccentColor);
	UHorizontalBoxSlot* const LeftRuleSlot = TitleRow->AddChildToHorizontalBox(M_LeftRuleBox);
	LeftRuleSlot->SetVerticalAlignment(VAlign_Center);
	LeftRuleSlot->SetHorizontalAlignment(HAlign_Right);

	M_LocationText = MakeText(Style.LocationFont, Style.TextColor);
	M_LocationText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* const LocationSlot = TitleRow->AddChildToHorizontalBox(M_LocationText);
	LocationSlot->SetVerticalAlignment(VAlign_Center);
	LocationSlot->SetPadding(FMargin(RuleTitleGap, 0.f));

	M_RightRuleBox = MakeRule(Style.AccentColor);
	UHorizontalBoxSlot* const RightRuleSlot = TitleRow->AddChildToHorizontalBox(M_RightRuleBox);
	RightRuleSlot->SetVerticalAlignment(VAlign_Center);
	RightRuleSlot->SetHorizontalAlignment(HAlign_Left);

	M_SubtitleText = MakeText(Style.SubtitleFont, Style.SubtitleColor);
	M_SubtitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const SubtitleSlot = Column->AddChildToVerticalBox(M_SubtitleText);
	SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
	SubtitleSlot->SetPadding(FMargin(0.f, SubtitleTopGap, 0.f, 0.f));
}

void UW_LocationIntro::BuildDateBlock(UCanvasPanel* Canvas)
{
	using namespace LocationIntroConstants;
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_DateBlock = Row;
	M_DateBlockSlot = Canvas->AddChildToCanvas(Row);
	M_DateBlockSlot->SetAlignment(FVector2D(0.f, 1.f));
	M_DateBlockSlot->SetAutoSize(true);

	USizeBox* const AccentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AccentBox->SetWidthOverride(AccentWidth);
	M_DateAccentBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_DateAccentBar->SetColorAndOpacity(Style.AccentColor);
	// Grows upward from its foot.
	M_DateAccentBar->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	AccentBox->SetContent(M_DateAccentBar);
	UHorizontalBoxSlot* const AccentSlot = Row->AddChildToHorizontalBox(AccentBox);
	AccentSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* const TextColumnSlot = Row->AddChildToHorizontalBox(TextColumn);
	TextColumnSlot->SetPadding(FMargin(AccentTextGap, 0.f, 0.f, 0.f));
	TextColumnSlot->SetVerticalAlignment(VAlign_Bottom);

	M_DateText = MakeText(Style.DateFont, Style.TextColor);
	TextColumn->AddChildToVerticalBox(M_DateText);
	M_TimeText = MakeText(Style.TimeFont, Style.AccentColor);
	TextColumn->AddChildToVerticalBox(M_TimeText)->SetPadding(FMargin(0.f, TimeTopGap, 0.f, 0.f));
}

UTextBlock* UW_LocationIntro::MakeText(const FSlateFontInfo& Font, const FLinearColor& Color) const
{
	UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	// The game shows behind the texts, so a soft shadow keeps them readable on bright ground.
	Text->SetShadowOffset(LocationIntroConstants::ShadowOffset);
	Text->SetShadowColorAndOpacity(Style.ShadowColor);
	return Text;
}

USizeBox* UW_LocationIntro::MakeRule(const FLinearColor& Color) const
{
	USizeBox* const RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RuleBox->SetHeightOverride(LocationIntroConstants::RuleThickness);
	RuleBox->SetWidthOverride(0.f);
	UImage* const Rule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Rule->SetColorAndOpacity(Color);
	RuleBox->SetContent(Rule);
	return RuleBox;
}

void UW_LocationIntro::ApplyTexts(const FLocationIntroTexts& Texts, const FLocationIntroPresentation& Presentation,
                                  const float DurationSeconds)
{
	M_Presentation = Presentation;
	M_DurationSeconds = DurationSeconds;
	M_FullDate = Texts.Date.ToString();
	M_FullTime = Texts.TimeOfDay.ToString();
	if (not IsValid(M_LocationText) || not IsValid(M_SubtitleText))
	{
		RTSFunctionLibrary::ReportError("UW_LocationIntro::ApplyTexts: the widget tree was not built.");
		return;
	}
	M_LocationText->SetText(Texts.Location.ToUpper());
	M_SubtitleText->SetText(Texts.LocationSubtitle);
	M_SubtitleText->SetVisibility(Texts.LocationSubtitle.IsEmpty()
		                              ? ESlateVisibility::Collapsed
		                              : ESlateVisibility::HitTestInvisible);
	M_TimeText->SetVisibility(M_FullTime.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	M_FadeBlack->SetVisibility(Presentation.bFadeFromBlack
		                           ? ESlateVisibility::HitTestInvisible
		                           : ESlateVisibility::Collapsed);

	const float BarFraction = GetLayoutBarFraction();
	M_LocationBlockSlot->SetAnchors(FAnchors(0.5f, BarFraction));
	M_LocationBlockSlot->SetPosition(FVector2D(0.f, Style.LocationTopGap));
	M_DateBlockSlot->SetAnchors(FAnchors(0.f, 1.f - BarFraction));
	M_DateBlockSlot->SetPosition(FVector2D(Style.DateLeftMargin, -Style.DateBottomGap));
}

float UW_LocationIntro::GetLayoutBarFraction() const
{
	return M_Presentation.bShowLetterbox
		       ? M_Presentation.LetterboxHeightFraction
		       : LocationIntroConstants::NoLetterboxLayoutFraction;
}

float UW_LocationIntro::GetFadeOutVisibility(const float ElapsedSeconds) const
{
	const float FadeOutStart = M_DurationSeconds - M_Presentation.FadeOutSeconds;
	const float FadeOutPhase = LocationIntroEasing::GetPhase(ElapsedSeconds, FadeOutStart,
	                                                         M_Presentation.FadeOutSeconds);
	return 1.f - LocationIntroEasing::EaseInOutSine(FadeOutPhase);
}

void UW_LocationIntro::SetElapsedSeconds(const float ElapsedSeconds)
{
	if (not IsValid(M_TopBarSlot) || not IsValid(M_LocationText) || not IsValid(M_DateText))
	{
		return;
	}
	const float FadeOutVisibility = GetFadeOutVisibility(ElapsedSeconds);
	UpdateLetterbox(ElapsedSeconds, FadeOutVisibility);
	UpdateFadeFromBlack(ElapsedSeconds);
	UpdateLocation(ElapsedSeconds, FadeOutVisibility);
	UpdateDate(ElapsedSeconds, FadeOutVisibility);
}

void UW_LocationIntro::UpdateLetterbox(const float ElapsedSeconds, const float FadeOutVisibility) const
{
	if (not M_Presentation.bShowLetterbox)
	{
		M_TopBarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
		M_BottomBarSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		return;
	}
	const float SlideIn = LocationIntroEasing::EaseOutCubic(
		LocationIntroEasing::GetPhase(ElapsedSeconds, 0.f, LocationIntroConstants::BarsInSeconds));
	const float BarHeight = M_Presentation.LetterboxHeightFraction * SlideIn * FadeOutVisibility;
	M_TopBarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, BarHeight));
	M_BottomBarSlot->SetAnchors(FAnchors(0.f, 1.f - BarHeight, 1.f, 1.f));
}

void UW_LocationIntro::UpdateFadeFromBlack(const float ElapsedSeconds) const
{
	if (not M_Presentation.bFadeFromBlack)
	{
		return;
	}
	const float FadePhase = LocationIntroEasing::GetPhase(ElapsedSeconds, 0.f, M_Presentation.FadeFromBlackSeconds);
	M_FadeBlack->SetRenderOpacity(1.f - LocationIntroEasing::EaseInOutSine(FadePhase));
}

void UW_LocationIntro::UpdateLocation(const float ElapsedSeconds, const float FadeOutVisibility)
{
	using namespace LocationIntroConstants;
	using namespace LocationIntroEasing;
	const float Start = M_Presentation.LocationStartSeconds;

	const float RuleGrowth = EaseOutCubic(GetPhase(ElapsedSeconds, Start, RuleGrowSeconds));
	M_LeftRuleBox->SetWidthOverride(Style.RuleWidth * RuleGrowth);
	M_RightRuleBox->SetWidthOverride(Style.RuleWidth * RuleGrowth);

	const float TitleFade = EaseInOutSine(GetPhase(ElapsedSeconds, Start + LocationFadeDelaySeconds,
	                                               LocationFadeSeconds));
	M_LocationText->SetRenderOpacity(TitleFade);
	M_LocationText->SetRenderTranslation(FVector2D(0.f, LocationRiseDistance * (1.f - TitleFade)));

	// The letters close in from wide tracking, the slowest part of the reveal.
	const float Tracking = EaseOutCubic(GetPhase(ElapsedSeconds, Start, LocationTrackingSeconds));
	const int32 LetterSpacing = FMath::RoundToInt(FMath::Lerp(
		static_cast<float>(Style.LocationLetterSpacingStart),
		static_cast<float>(Style.LocationLetterSpacingEnd), Tracking));
	if (LetterSpacing != M_ShownLetterSpacing)
	{
		M_ShownLetterSpacing = LetterSpacing;
		FSlateFontInfo TrackedFont = Style.LocationFont;
		TrackedFont.LetterSpacing = LetterSpacing;
		M_LocationText->SetFont(TrackedFont);
	}

	const float SubtitleFade = EaseInOutSine(GetPhase(ElapsedSeconds, Start + SubtitleDelaySeconds,
	                                                  SubtitleFadeSeconds));
	M_SubtitleText->SetRenderOpacity(SubtitleFade);
	M_LocationBlock->SetRenderOpacity(FadeOutVisibility);
}

void UW_LocationIntro::UpdateDate(const float ElapsedSeconds, const float FadeOutVisibility)
{
	using namespace LocationIntroConstants;
	using namespace LocationIntroEasing;
	const float AccentGrowth = EaseOutCubic(GetPhase(ElapsedSeconds,
	                                                 M_Presentation.DateStartSeconds - AccentLeadSeconds,
	                                                 AccentGrowSeconds));
	M_DateAccentBar->SetRenderScale(FVector2D(1.f, AccentGrowth));
	UpdateTypedText(ElapsedSeconds);
	M_DateBlock->SetRenderOpacity(FadeOutVisibility);
}

void UW_LocationIntro::UpdateTypedText(const float ElapsedSeconds)
{
	using namespace LocationIntroConstants;
	const float LettersPerSecond = FMath::Max(1.f, M_Presentation.TypewriterLettersPerSecond);
	const int32 DateLength = M_FullDate.Len();
	const int32 TimeLength = M_FullTime.Len();
	const float TypingSeconds = FMath::Max(0.f, ElapsedSeconds - M_Presentation.DateStartSeconds);
	const int32 DateLetters = FMath::Min(DateLength, FMath::FloorToInt(TypingSeconds * LettersPerSecond));
	const float DateDoneSeconds = static_cast<float>(DateLength) / LettersPerSecond + TimeLineDelaySeconds;
	const int32 TimeLetters = FMath::Clamp(
		FMath::FloorToInt((TypingSeconds - DateDoneSeconds) * LettersPerSecond), 0, TimeLength);
	const bool bHasStarted = ElapsedSeconds >= M_Presentation.DateStartSeconds;
	const bool bIsTypingTime = DateLetters >= DateLength && TimeLength > 0;
	const bool bIsDoneTyping = DateLetters >= DateLength && TimeLetters >= TimeLength;

	// The cursor stays on while typing and blinks once the line is complete.
	const float BlinkPhase = FMath::Fmod(ElapsedSeconds, CursorBlinkPeriodSeconds) / CursorBlinkPeriodSeconds;
	const bool bShowCursor = bHasStarted && (not bIsDoneTyping || BlinkPhase < 0.5f);

	const int32 TypedLetters = bHasStarted ? DateLetters + TimeLetters : INDEX_NONE;
	if (TypedLetters == M_ShownTypedLetters && bShowCursor == bM_ShownCursor)
	{
		return;
	}
	if (TypedLetters > M_ShownTypedLetters && M_ShownTypedLetters != INDEX_NONE)
	{
		PlayTypewriterClick();
	}
	M_ShownTypedLetters = TypedLetters;
	bM_ShownCursor = bShowCursor;

	const bool bCursorOnDate = bShowCursor && not bIsTypingTime;
	const bool bCursorOnTime = bShowCursor && bIsTypingTime;
	const FString DateShown = M_FullDate.Left(DateLetters) + (bCursorOnDate ? TypewriterCursor : TEXT(""));
	const FString TimeShown = M_FullTime.Left(TimeLetters) + (bCursorOnTime ? TypewriterCursor : TEXT(""));
	M_DateText->SetText(FText::FromString(DateShown));
	M_TimeText->SetText(FText::FromString(TimeShown));
}

void UW_LocationIntro::PlayTypewriterClick() const
{
	if (M_Presentation.TypewriterSound == nullptr)
	{
		return;
	}
	UGameplayStatics::PlaySound2D(this, M_Presentation.TypewriterSound);
}
