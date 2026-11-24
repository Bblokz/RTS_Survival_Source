// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_IntroCinematicHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace IntroCinematicHudConstants
{
	const TCHAR* PortraitBrushPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope.SB_Portrait_AnnouncerScope");
	const TCHAR* NoticeBurnMaterialPath = TEXT(
		"/Game/RTS_Survival/Cinematics/Intro1976/UI/M_Intro_NoticeBurn.M_Intro_NoticeBurn");
	const TCHAR* TypewriterFontPath = TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono");
	const TCHAR* DisplayFontPath = TEXT("/Game/RTS_Survival/Fonts/RR_mainFont.RR_mainFont");
	const FName BurnParameterName = TEXT("Burn");

	constexpr float TypedCharactersPerSecond = 26.0f;
	constexpr float NoticeCharactersPerSecond = 34.0f;
	constexpr float FadeSeconds = 0.45f;

	constexpr int32 CardLine1FontSize = 40;
	constexpr int32 CardLine2FontSize = 19;
	constexpr float CardLeftMargin = 84.0f;
	constexpr float CardBottomMargin = 150.0f;
	constexpr float CardLineGap = 6.0f;

	constexpr int32 StampFontSize = 46;
	constexpr float StampSlamSeconds = 0.14f;
	constexpr float StampSlamStartScale = 2.6f;
	constexpr float StampOutlineWidth = 5.0f;
	constexpr float StampCornerRadius = 6.0f;

	// The notice text is gone when the burn is a third of the way: it catches the heat before the backdrop does.
	constexpr float NoticeTextBurnRate = 3.0f;
	// Second card line sits this many font heights below the first line's baseline.
	constexpr float CardLineHeightFactor = 1.6f;
	const FVector2D TextShadowOffset(2.0f, 2.0f);
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FMargin StampPadding(22.0f, 6.0f, 22.0f, 8.0f);
	constexpr float StampLetterSpacing = 120.0f;
	constexpr float LabelLetterSpacing = 160.0f;
	constexpr float StampSlamEaseExponent = 2.0f;
	constexpr int32 NoticeFontSize = 21;
	constexpr float NoticeBlockWidth = 760.0f;
	constexpr float NoticeBlockHeight = 210.0f;
	constexpr int32 LabelFontSize = 22;

	// Dull red of the "Rotsignal" takeover look.
	const FLinearColor StampColor = FLinearColor(FColor(194, 74, 58, 235));
	const FLinearColor PaperColor = FLinearColor(FColor(236, 232, 220, 255));
	const FLinearColor BurnTextColor = FLinearColor(FColor(255, 150, 60, 255));

	FSlateFontInfo MakeFont(const TCHAR* FontPath, const int32 Size, const float LetterSpacing = 0.0f)
	{
		const UObject* const FontObject = LoadObject<UFont>(nullptr, FontPath);
		FSlateFontInfo Font = FontObject != nullptr
			                      ? FSlateFontInfo(FontObject, Size)
			                      : FCoreStyle::GetDefaultFontStyle("Regular", Size);
		Font.LetterSpacing = static_cast<int32>(LetterSpacing);
		return Font;
	}

	const FIntroCinematicCueRow* FindActiveCue(const TArray<FIntroCinematicCueRow>& Cues,
	                                           const EIntroCinematicCueType Type, const float SequenceSeconds)
	{
		// Overlapping cues of one type: the one that started last wins.
		const FIntroCinematicCueRow* Active = nullptr;
		for (const FIntroCinematicCueRow& Cue : Cues)
		{
			if (Cue.Type != Type || SequenceSeconds < Cue.StartSeconds || SequenceSeconds >= Cue.EndSeconds)
			{
				continue;
			}
			if (Active == nullptr || Cue.StartSeconds >= Active->StartSeconds)
			{
				Active = &Cue;
			}
		}
		return Active;
	}
}

void IntroCinematicHudLayout::WrapRootInDesignFrame(UWidgetTree* WidgetTree)
{
	if (not IsValid(WidgetTree) || not IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UWidget* const Content = WidgetTree->RootWidget;
	UScaleBox* const ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	ScaleBox->SetStretch(EStretch::ScaleToFit);
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(DesignWidth);
	SizeBox->SetHeightOverride(DesignHeight);
	WidgetTree->RootWidget = ScaleBox;
	ScaleBox->AddChild(SizeBox);
	SizeBox->AddChild(Content);
}

void UW_IntroCinematicSubtitles::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

UW_IntroCinematicSubtitles::UW_IntroCinematicSubtitles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace IntroCinematicHudLayout;
	Style.StripTopMargin = PortraitTopMargin + PortraitHeight + SubtitleGap;
	Style.StripWidth = SubtitleStripWidth;
	bShowCountdown = false;
	DefaultSpeaker = NSLOCTEXT("IntroCinematic", "Narrator", "Advisor");
	// The scratch voice lines have their transcripts in a table of the cinematic's own; the final lines go into
	// DT_CinematicTranscripts and this override is then removed.
	TranscriptTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Cinematics/Intro1976/Data/DT_IntroTranscripts.DT_IntroTranscripts")));
}

void UW_IntroCinematicHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_IntroCinematicHud::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = M_Canvas;
	BuildNotice();
	BuildPortrait();
	BuildCard();
	BuildStamp();
	BuildLabel();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_IntroCinematicHud::BuildNotice()
{
	using namespace IntroCinematicHudConstants;
	M_NoticeBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	UCanvasPanelSlot* const BackgroundSlot = M_Canvas->AddChildToCanvas(M_NoticeBackground);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));
	if (UMaterialInterface* const BurnMaterial = LoadObject<UMaterialInterface>(nullptr, NoticeBurnMaterialPath))
	{
		M_NoticeBurnMaterial = UMaterialInstanceDynamic::Create(BurnMaterial, this);
		M_NoticeBackground->SetBrushFromMaterial(M_NoticeBurnMaterial);
	}
	else
	{
		M_NoticeBackground->SetColorAndOpacity(FLinearColor::Black);
	}
	M_NoticeBackground->SetVisibility(ESlateVisibility::Collapsed);

	M_NoticeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NoticeText->SetFont(MakeFont(TypewriterFontPath, NoticeFontSize));
	M_NoticeText->SetJustification(ETextJustify::Left);
	// A fixed block, so the lines stay put while they are typed.
	UCanvasPanelSlot* const TextSlot = M_Canvas->AddChildToCanvas(M_NoticeText);
	TextSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TextSlot->SetAutoSize(false);
	TextSlot->SetSize(FVector2D(NoticeBlockWidth, NoticeBlockHeight));
	TextSlot->SetPosition(FVector2D::ZeroVector);
	M_NoticeText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_IntroCinematicHud::BuildPortrait()
{
	using namespace IntroCinematicHudConstants;
	using namespace IntroCinematicHudLayout;
	M_PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (USlateBrushAsset* const BrushAsset = LoadObject<USlateBrushAsset>(nullptr, PortraitBrushPath))
	{
		M_PortraitImage->SetBrush(BrushAsset->Brush);
	}
	UCanvasPanelSlot* const PortraitSlot = M_Canvas->AddChildToCanvas(M_PortraitImage);
	PortraitSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	PortraitSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	PortraitSlot->SetPosition(FVector2D(0.0f, PortraitTopMargin));
	PortraitSlot->SetSize(FVector2D(PortraitWidth, PortraitHeight));
	M_PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_IntroCinematicHud::BuildCard()
{
	using namespace IntroCinematicHudConstants;
	const auto AddLine = [this](const int32 FontSize, const float BottomOffset) -> UTextBlock*
	{
		UTextBlock* const Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Line->SetFont(MakeFont(TypewriterFontPath, FontSize));
		Line->SetColorAndOpacity(FSlateColor(PaperColor));
		Line->SetShadowOffset(TextShadowOffset);
		Line->SetShadowColorAndOpacity(TextShadowColor);
		UCanvasPanelSlot* const LineSlot = M_Canvas->AddChildToCanvas(Line);
		LineSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		LineSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		LineSlot->SetAutoSize(true);
		LineSlot->SetPosition(FVector2D(CardLeftMargin, -BottomOffset));
		Line->SetVisibility(ESlateVisibility::Collapsed);
		return Line;
	};
	M_CardLine2 = AddLine(CardLine2FontSize, CardBottomMargin);
	M_CardLine1 = AddLine(CardLine1FontSize, CardBottomMargin + CardLine2FontSize * CardLineHeightFactor + CardLineGap);
}

void UW_IntroCinematicHud::BuildStamp()
{
	using namespace IntroCinematicHudConstants;
	M_StampBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_StampBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, StampCornerRadius, StampColor,
	                                              StampOutlineWidth));
	M_StampBorder->SetPadding(StampPadding);
	M_StampText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_StampText->SetFont(MakeFont(DisplayFontPath, StampFontSize, StampLetterSpacing));
	M_StampText->SetColorAndOpacity(FSlateColor(StampColor));
	M_StampBorder->SetContent(M_StampText);
	UCanvasPanelSlot* const StampSlot = M_Canvas->AddChildToCanvas(M_StampBorder);
	StampSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	StampSlot->SetAutoSize(true);
	M_StampBorder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	M_StampBorder->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_IntroCinematicHud::BuildLabel()
{
	using namespace IntroCinematicHudConstants;
	M_LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_LabelText->SetFont(MakeFont(TypewriterFontPath, LabelFontSize, LabelLetterSpacing));
	M_LabelText->SetColorAndOpacity(FSlateColor(PaperColor));
	M_LabelText->SetShadowOffset(TextShadowOffset);
	M_LabelText->SetShadowColorAndOpacity(TextShadowColor);
	UCanvasPanelSlot* const LabelSlot = M_Canvas->AddChildToCanvas(M_LabelText);
	LabelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
	LabelSlot->SetAutoSize(true);
	M_LabelText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_IntroCinematicHud::ShowCuesAtTime(const TArray<FIntroCinematicCueRow>& Cues, const float SequenceSeconds)
{
	using namespace IntroCinematicHudConstants;
	if (not GetIsValidCanvas())
	{
		return;
	}
	// The notice owns the screen from the first frame until it has burnt away, so look for it by its end only.
	const FIntroCinematicCueRow* Notice = nullptr;
	for (const FIntroCinematicCueRow& Cue : Cues)
	{
		if (Cue.Type == EIntroCinematicCueType::Notice && SequenceSeconds < Cue.EndSeconds)
		{
			Notice = &Cue;
			break;
		}
	}
	ShowNotice(Notice, SequenceSeconds);
	ShowPortrait(FindActiveCue(Cues, EIntroCinematicCueType::Portrait, SequenceSeconds), SequenceSeconds);
	ShowCard(FindActiveCue(Cues, EIntroCinematicCueType::Card, SequenceSeconds), SequenceSeconds);
	ShowStamp(FindActiveCue(Cues, EIntroCinematicCueType::Stamp, SequenceSeconds), SequenceSeconds);
	ShowLabel(FindActiveCue(Cues, EIntroCinematicCueType::Label, SequenceSeconds), SequenceSeconds);
}

void UW_IntroCinematicHud::ShowPortrait(const FIntroCinematicCueRow* Cue, const float SequenceSeconds)
{
	if (Cue == nullptr)
	{
		M_PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float FadeIn = (SequenceSeconds - Cue->StartSeconds) / IntroCinematicHudConstants::FadeSeconds;
	M_PortraitImage->SetRenderOpacity(FMath::Clamp(FadeIn, 0.0f, 1.0f));
}

void UW_IntroCinematicHud::ShowCard(const FIntroCinematicCueRow* Cue, const float SequenceSeconds)
{
	using namespace IntroCinematicHudConstants;
	if (Cue == nullptr)
	{
		M_CardLine1->SetVisibility(ESlateVisibility::Collapsed);
		M_CardLine2->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const float SinceStart = SequenceSeconds - Cue->StartSeconds;
	// The second line starts typing when the first is complete.
	const float Line1Seconds = Cue->Line1.Len() / TypedCharactersPerSecond;
	const float FadeOut = FMath::Clamp((Cue->EndSeconds - SequenceSeconds) / FadeSeconds, 0.0f, 1.0f);
	M_CardLine1->SetText(GetTypedText(Cue->Line1, SinceStart, TypedCharactersPerSecond));
	M_CardLine2->SetText(GetTypedText(Cue->Line2, SinceStart - Line1Seconds, TypedCharactersPerSecond));
	M_CardLine1->SetRenderOpacity(FadeOut);
	M_CardLine2->SetRenderOpacity(FadeOut);
	M_CardLine1->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_CardLine2->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_IntroCinematicHud::ShowStamp(const FIntroCinematicCueRow* Cue, const float SequenceSeconds)
{
	using namespace IntroCinematicHudConstants;
	if (Cue == nullptr)
	{
		M_StampBorder->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_StampText->SetText(FText::FromString(Cue->Line1));
	if (UCanvasPanelSlot* const StampSlot = Cast<UCanvasPanelSlot>(M_StampBorder->Slot))
	{
		StampSlot->SetAnchors(FAnchors(Cue->Position.X, Cue->Position.Y));
		StampSlot->SetPosition(FVector2D::ZeroVector);
	}
	// Slam: large and faint, down to size within a few frames.
	const float SlamAlpha = FMath::Clamp((SequenceSeconds - Cue->StartSeconds) / StampSlamSeconds, 0.0f, 1.0f);
	const float Scale = FMath::Lerp(StampSlamStartScale, 1.0f, FMath::InterpEaseIn(0.0f, 1.0f, SlamAlpha, StampSlamEaseExponent));
	M_StampBorder->SetRenderTransformAngle(Cue->AngleDegrees);
	M_StampBorder->SetRenderScale(FVector2D(Scale, Scale));
	// No fade-out: a stamp is on the picture or it is not (a fade left the outline behind the word).
	M_StampBorder->SetRenderOpacity(SlamAlpha);
	M_StampBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_IntroCinematicHud::ShowNotice(const FIntroCinematicCueRow* Cue, const float SequenceSeconds)
{
	using namespace IntroCinematicHudConstants;
	if (Cue == nullptr)
	{
		M_NoticeBackground->SetVisibility(ESlateVisibility::Collapsed);
		M_NoticeText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const float BurnSeconds = FMath::Max(Cue->Param, KINDA_SMALL_NUMBER);
	const float Burn = FMath::Clamp((SequenceSeconds - (Cue->EndSeconds - BurnSeconds)) / BurnSeconds, 0.0f, 1.0f);
	if (IsValid(M_NoticeBurnMaterial))
	{
		M_NoticeBurnMaterial->SetScalarParameterValue(BurnParameterName, Burn);
	}
	else
	{
		M_NoticeBackground->SetRenderOpacity(1.0f - Burn);
	}
	M_NoticeBackground->SetVisibility(ESlateVisibility::HitTestInvisible);

	// The text catches the heat first: it turns orange and is gone in the first third of the burn.
	const float TextBurn = FMath::Clamp(Burn * NoticeTextBurnRate, 0.0f, 1.0f);
	const FString NoticeLines = Cue->Line1.Replace(TEXT("\\n"), TEXT("\n"));
	M_NoticeText->SetText(GetTypedText(NoticeLines, SequenceSeconds - Cue->StartSeconds, NoticeCharactersPerSecond));
	M_NoticeText->SetColorAndOpacity(FSlateColor(FMath::Lerp(PaperColor, BurnTextColor, TextBurn)));
	M_NoticeText->SetRenderOpacity(1.0f - TextBurn);
	M_NoticeText->SetVisibility(SequenceSeconds >= Cue->StartSeconds
		                            ? ESlateVisibility::HitTestInvisible
		                            : ESlateVisibility::Collapsed);
}

void UW_IntroCinematicHud::ShowLabel(const FIntroCinematicCueRow* Cue, const float SequenceSeconds)
{
	using namespace IntroCinematicHudConstants;
	if (Cue == nullptr)
	{
		M_LabelText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (UCanvasPanelSlot* const LabelSlot = Cast<UCanvasPanelSlot>(M_LabelText->Slot))
	{
		LabelSlot->SetAnchors(FAnchors(Cue->Position.X, Cue->Position.Y));
		LabelSlot->SetPosition(FVector2D::ZeroVector);
	}
	M_LabelText->SetText(GetTypedText(Cue->Line1, SequenceSeconds - Cue->StartSeconds, TypedCharactersPerSecond));
	M_LabelText->SetRenderOpacity(FMath::Clamp((Cue->EndSeconds - SequenceSeconds) / FadeSeconds, 0.0f, 1.0f));
	M_LabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UW_IntroCinematicHud::GetIsValidCanvas() const
{
	if (IsValid(M_Canvas))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Canvas", "GetIsValidCanvas", this);
	return false;
}

FText UW_IntroCinematicHud::GetTypedText(const FString& Text, const float SecondsSinceStart,
                                         const float CharactersPerSecond)
{
	const int32 Count = FMath::Clamp(FMath::FloorToInt32(SecondsSinceStart * CharactersPerSecond), 0, Text.Len());
	return FText::FromString(Text.Left(Count));
}

float UW_IntroCinematicHud::GetCueFade(const FIntroCinematicCueRow& Cue, const float SequenceSeconds,
                                       const float FadeSeconds)
{
	const float FadeIn = (SequenceSeconds - Cue.StartSeconds) / FadeSeconds;
	const float FadeOut = (Cue.EndSeconds - SequenceSeconds) / FadeSeconds;
	return FMath::Clamp(FMath::Min(FadeIn, FadeOut), 0.0f, 1.0f);
}
