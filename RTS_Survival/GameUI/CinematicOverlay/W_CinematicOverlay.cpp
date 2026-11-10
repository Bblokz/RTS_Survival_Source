// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CinematicOverlay.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "MovieSceneSequencePlayer.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/CinematicTranscriptSubsystem.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/SubtitleStyleDefaults.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/TranscriptReveal.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace CinematicOverlayConstants
{
	// Above the regular HUD; the hold-to-skip widget uses 0.
	constexpr int32 ViewportZOrder = 5;
	constexpr int32 SpeakerFontSize = 13;
	constexpr int32 LineFontSize = 16;
	constexpr int32 CountdownFontSize = 13;
	constexpr float MinimumPlayRate = 0.01f;
	constexpr int32 SecondsPerMinute = 60;
}

UW_CinematicOverlay::UW_CinematicOverlay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace CinematicOverlayConstants;
	DefaultSpeaker = NSLOCTEXT("CinematicOverlay", "DefaultSpeaker", "Announcer");
	SpeakerByAssetPath.Add(TEXT("/Portraits/RusHazmat/"), NSLOCTEXT("CinematicOverlay", "SpeakerHazmat", "Hazmat"));

	static ConstructorHelpers::FObjectFinder<UFont> MainFont(SubtitleStyleDefaults::MainFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> ReadableFont(SubtitleStyleDefaults::ReadableFontPath);
	const UObject* const FontObject = MainFont.Succeeded() ? MainFont.Object : nullptr;
	const UObject* const ReadableFontObject = ReadableFont.Succeeded() ? ReadableFont.Object : nullptr;
	Style.SpeakerFont = FontObject != nullptr
		                    ? FSlateFontInfo(FontObject, SpeakerFontSize)
		                    : FCoreStyle::GetDefaultFontStyle("Bold", SpeakerFontSize);
	// Spoken words in a plain sans-serif; the main font stays for the speaker name and the countdown.
	Style.LineFont = ReadableFontObject != nullptr
		                 ? FSlateFontInfo(ReadableFontObject, LineFontSize, FName(SubtitleStyleDefaults::ReadableTypeface))
		                 : FCoreStyle::GetDefaultFontStyle("Regular", LineFontSize);
	Style.CountdownFont = FontObject != nullptr
		                      ? FSlateFontInfo(FontObject, CountdownFontSize)
		                      : FCoreStyle::GetDefaultFontStyle("Bold", CountdownFontSize);
	Style.StripBrush = SubtitleStyleDefaults::MakePanelBrush();
	Style.CountdownBrush = SubtitleStyleDefaults::MakePanelBrush();
}

UW_CinematicOverlay* UW_CinematicOverlay::ShowForSequence(APlayerController* OwningPlayer,
                                                          UMovieSceneSequencePlayer* SequencePlayer,
                                                          TSubclassOf<UW_CinematicOverlay> OverlayClass)
{
	if (not IsValid(OwningPlayer) || not IsValid(SequencePlayer))
	{
		RTSFunctionLibrary::ReportError("UW_CinematicOverlay::ShowForSequence needs a player and a sequence player.");
		return nullptr;
	}
	const TSubclassOf<UW_CinematicOverlay> ClassToCreate = OverlayClass != nullptr
		                                                       ? OverlayClass
		                                                       : TSubclassOf<UW_CinematicOverlay>(StaticClass());
	UW_CinematicOverlay* const Overlay = CreateWidget<UW_CinematicOverlay>(OwningPlayer, ClassToCreate);
	if (not IsValid(Overlay))
	{
		RTSFunctionLibrary::ReportError("UW_CinematicOverlay::ShowForSequence could not create the overlay.");
		return nullptr;
	}
	Overlay->AddToViewport(CinematicOverlayConstants::ViewportZOrder);
	Overlay->StartForSequencePlayer(SequencePlayer);
	return Overlay;
}

void UW_CinematicOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_CinematicOverlay::NativeDestruct()
{
	UnbindSequencePlayer();
	Super::NativeDestruct();
}

void UW_CinematicOverlay::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateOverlay();
}

void UW_CinematicOverlay::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	// Top strip: [speaker][words], centred under the top edge.
	USizeBox* const StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetWidthOverride(Style.StripWidth);
	UCanvasPanelSlot* const StripSlot = Canvas->AddChildToCanvas(StripBox);
	StripSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	StripSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	StripSlot->SetAutoSize(true);
	StripSlot->SetPosition(FVector2D(0.0f, Style.StripTopMargin));

	M_Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	StripBox->AddChild(M_Strip);
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_Strip->SetContent(Row);

	M_SpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UHorizontalBoxSlot* const SpeakerSlot = Row->AddChildToHorizontalBox(M_SpeakerText);
	SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, Style.SpeakerGap, 0.0f));
	SpeakerSlot->SetVerticalAlignment(VAlign_Top);

	M_LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_LineText->SetAutoWrapText(true);
	UHorizontalBoxSlot* const LineSlot = Row->AddChildToHorizontalBox(M_LineText);
	LineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LineSlot->SetVerticalAlignment(VAlign_Top);

	// Countdown in the bottom-right corner.
	M_CountdownPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UCanvasPanelSlot* const CountdownSlot = Canvas->AddChildToCanvas(M_CountdownPanel);
	CountdownSlot->SetAnchors(FAnchors(1.0f, 1.0f));
	CountdownSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	CountdownSlot->SetAutoSize(true);
	CountdownSlot->SetPosition(-Style.CountdownCornerOffset);
	M_CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_CountdownPanel->SetContent(M_CountdownText);

	ApplyStyle();
	M_Strip->SetVisibility(ESlateVisibility::Collapsed);
	M_CountdownPanel->SetVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_CinematicOverlay::ApplyStyle()
{
	M_Strip->SetBrush(Style.StripBrush);
	M_Strip->SetPadding(Style.StripPadding);
	M_SpeakerText->SetFont(Style.SpeakerFont);
	M_SpeakerText->SetColorAndOpacity(Style.SpeakerColor);
	M_LineText->SetFont(Style.LineFont);
	M_LineText->SetColorAndOpacity(Style.LineColor);
	M_CountdownPanel->SetBrush(Style.CountdownBrush);
	M_CountdownPanel->SetPadding(Style.CountdownPadding);
	M_CountdownText->SetFont(Style.CountdownFont);
	M_CountdownText->SetColorAndOpacity(Style.CountdownColor);
}

void UW_CinematicOverlay::StartForSequencePlayer(UMovieSceneSequencePlayer* SequencePlayer)
{
	M_SequencePlayer = SequencePlayer;
	SequencePlayer->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleSequenceEnded);
	SequencePlayer->OnStop.AddUniqueDynamic(this, &ThisClass::HandleSequenceEnded);

	UCinematicTranscriptSubsystem* const Transcripts = UCinematicTranscriptSubsystem::Get(this);
	const TSoftObjectPtr<UDataTable> Table = TranscriptTable;
	M_Cues = CinematicSubtitleTimeline::BuildCues(
		SequencePlayer->GetSequence(),
		[Transcripts, &Table](const USoundBase* Sound) -> const FCinematicTranscriptRow*
		{
			return Transcripts != nullptr ? Transcripts->FindTranscript(Table, Sound) : nullptr;
		},
		[this](const USoundBase* Sound, const FCinematicTranscriptRow& Transcript)
		{
			return GetSpeakerForSound(Sound, Transcript);
		});
	UpdateOverlay();
}

void UW_CinematicOverlay::CloseOverlay()
{
	UnbindSequencePlayer();
	M_Cues.Reset();
	RemoveFromParent();
}

void UW_CinematicOverlay::UpdateOverlay()
{
	// No player once the overlay closed (it may still get one last tick before it leaves the viewport).
	if (not GetIsValidSequencePlayer(false))
	{
		return;
	}
	const float SequenceSeconds = static_cast<float>(M_SequencePlayer->GetCurrentTime().AsSeconds());
	UpdateSubtitle(SequenceSeconds);
	UpdateCountdown(SequenceSeconds);
}

int32 UW_CinematicOverlay::GetActiveCueIndex(const float SequenceSeconds) const
{
	// Overlapping voice lines: the one that started last owns the strip.
	int32 ActiveIndex = INDEX_NONE;
	for (int32 Index = 0; Index < M_Cues.Num(); ++Index)
	{
		const FCinematicSubtitleCue& Cue = M_Cues[Index];
		const float LastWordEnd = Cue.Sentences.Last().Words.Last().EndSeconds;
		const float ActiveUntil = FMath::Max(Cue.EndSeconds, LastWordEnd + Style.SentenceHoldSeconds);
		if (Cue.StartSeconds <= SequenceSeconds && SequenceSeconds <= ActiveUntil)
		{
			ActiveIndex = Index;
		}
	}
	return ActiveIndex;
}

void UW_CinematicOverlay::UpdateSubtitle(const float SequenceSeconds)
{
	const int32 CueIndex = GetActiveCueIndex(SequenceSeconds);
	if (CueIndex != M_ShownCueIndex)
	{
		M_ShownCueIndex = CueIndex;
		M_ShownLine.Reset();
		M_LineText->SetText(FText::GetEmpty());
		const bool bHasCue = M_Cues.IsValidIndex(CueIndex);
		M_Strip->SetVisibility(bHasCue ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		M_SpeakerText->SetText(bHasCue ? M_Cues[CueIndex].Speaker : FText::GetEmpty());
	}
	if (not M_Cues.IsValidIndex(CueIndex))
	{
		return;
	}

	// While the voice line still plays its last sentence stays; after the line the strip clears with the cue.
	const FCinematicSubtitleCue& Cue = M_Cues[CueIndex];
	const FString Line = TranscriptReveal::BuildVisibleText(
		Cue.Sentences, SequenceSeconds, Style.SentenceHoldSeconds, SequenceSeconds <= Cue.EndSeconds);
	if (not Line.Equals(M_ShownLine, ESearchCase::CaseSensitive))
	{
		M_ShownLine = Line;
		M_LineText->SetText(FText::FromString(Line));
	}
}

void UW_CinematicOverlay::UpdateCountdown(const float SequenceSeconds)
{
	if (not bShowCountdown)
	{
		M_CountdownPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const float EndSeconds = static_cast<float>(M_SequencePlayer->GetEndTime().AsSeconds());
	const float PlayRate = FMath::Max(M_SequencePlayer->GetPlayRate(), CinematicOverlayConstants::MinimumPlayRate);
	const int32 SecondsLeft = FMath::Max(0, FMath::CeilToInt((EndSeconds - SequenceSeconds) / PlayRate));
	if (SecondsLeft == M_ShownCountdownSeconds)
	{
		return;
	}
	M_ShownCountdownSeconds = SecondsLeft;
	M_CountdownPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_CountdownText->SetText(FText::FromString(FString::Printf(
		TEXT("%d:%02d"), SecondsLeft / CinematicOverlayConstants::SecondsPerMinute,
		SecondsLeft % CinematicOverlayConstants::SecondsPerMinute)));
}

FText UW_CinematicOverlay::GetSpeakerForSound(const USoundBase* Sound,
                                              const FCinematicTranscriptRow& Transcript) const
{
	if (not Transcript.SpeakerOverride.IsEmpty())
	{
		return Transcript.SpeakerOverride;
	}
	const FString SoundPath = Sound != nullptr ? Sound->GetPathName() : FString();
	for (const TPair<FString, FText>& Rule : SpeakerByAssetPath)
	{
		if (SoundPath.Contains(Rule.Key))
		{
			return Rule.Value;
		}
	}
	return DefaultSpeaker;
}

void UW_CinematicOverlay::HandleSequenceEnded()
{
	CloseOverlay();
}

void UW_CinematicOverlay::UnbindSequencePlayer()
{
	if (not GetIsValidSequencePlayer(false))
	{
		return;
	}
	M_SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::HandleSequenceEnded);
	M_SequencePlayer->OnStop.RemoveDynamic(this, &ThisClass::HandleSequenceEnded);
	M_SequencePlayer.Reset();
}

bool UW_CinematicOverlay::GetIsValidSequencePlayer(const bool bReportError) const
{
	if (M_SequencePlayer.IsValid())
	{
		return true;
	}
	if (bReportError)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, "M_SequencePlayer", "UW_CinematicOverlay::GetIsValidSequencePlayer", this);
	}
	return false;
}
