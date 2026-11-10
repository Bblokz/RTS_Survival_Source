// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_PortraitSubtitle.h"

#include "TranscriptReveal.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace PortraitSubtitleConstants
{
	// How often the reveal is updated; words are ~0.2-0.5 s apart, so 30 ms is well below what the eye notices.
	constexpr float UpdateIntervalSeconds = 0.03f;
	// Text line height relative to the font size, used to reserve room for ReservedLines lines.
	constexpr float LineHeightPerFontSize = 1.35f;
	constexpr float SpeakerToLineGap = 2.0f;
}

void UW_PortraitSubtitle::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_PortraitSubtitle::NativeDestruct()
{
	StopUpdateTimer();
	Super::NativeDestruct();
}

void UW_PortraitSubtitle::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	M_OuterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidgetTree->RootWidget = M_OuterBox;

	M_Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_OuterBox->AddChild(M_Panel);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_Panel->SetContent(Column);

	M_SpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UVerticalBoxSlot* const SpeakerSlot = Column->AddChildToVerticalBox(M_SpeakerText);
	SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PortraitSubtitleConstants::SpeakerToLineGap));

	M_LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Column->AddChildToVerticalBox(M_LineBox);

	M_LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_LineText->SetAutoWrapText(true);
	M_LineText->SetJustification(ETextJustify::Left);
	M_LineBox->AddChild(M_LineText);

	ApplyStyle(M_Style);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_PortraitSubtitle::ApplyStyle(const FPortraitSubtitleStyle& Style)
{
	M_Style = Style;
	if (not IsValid(M_OuterBox) || not IsValid(M_Panel) || not IsValid(M_SpeakerText) || not IsValid(M_LineBox)
		|| not IsValid(M_LineText))
	{
		// Tree not built yet; NativeOnInitialized applies the stored style.
		return;
	}

	M_OuterBox->SetWidthOverride(M_Style.BoxWidth);
	M_Panel->SetBrush(M_Style.PanelBrush);
	M_Panel->SetPadding(M_Style.PanelPadding);

	M_SpeakerText->SetFont(M_Style.SpeakerFont);
	M_SpeakerText->SetColorAndOpacity(M_Style.SpeakerColor);

	M_LineText->SetFont(M_Style.LineFont);
	M_LineText->SetColorAndOpacity(M_Style.LineColor);

	const float LineHeight = M_Style.LineFont.Size * PortraitSubtitleConstants::LineHeightPerFontSize;
	M_LineBox->SetMinDesiredHeight(LineHeight * FMath::Max(1, M_Style.ReservedLines));
}

void UW_PortraitSubtitle::StartLine(const FText& SpeakerName, const TArray<FCinematicTranscriptSentence>& Sentences,
                                    TFunction<float()> GetPlaybackSeconds)
{
	StopUpdateTimer();
	M_Sentences = Sentences;
	M_GetPlaybackSeconds = MoveTemp(GetPlaybackSeconds);
	M_ShownText.Reset();

	if (IsValid(M_SpeakerText))
	{
		M_SpeakerText->SetText(SpeakerName);
	}
	if (IsValid(M_LineText))
	{
		M_LineText->SetText(FText::GetEmpty());
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UWorld* const World = GetWorld();
	if (M_Sentences.Num() == 0 || not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<UW_PortraitSubtitle> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_UpdateTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->UpdateSubtitle();
			}
		}),
		PortraitSubtitleConstants::UpdateIntervalSeconds,
		true);
}

void UW_PortraitSubtitle::StopLine()
{
	StopUpdateTimer();
	M_Sentences.Reset();
	M_GetPlaybackSeconds = nullptr;
	M_ShownText.Reset();
	if (IsValid(M_LineText))
	{
		M_LineText->SetText(FText::GetEmpty());
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_PortraitSubtitle::StopUpdateTimer()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
	}
}

void UW_PortraitSubtitle::UpdateSubtitle()
{
	if (not M_GetPlaybackSeconds)
	{
		StopUpdateTimer();
		return;
	}
	// Before the line is heard and after it ends the position is < 0: keep what is shown (the last sentence).
	const float SecondsIntoLine = M_GetPlaybackSeconds();
	if (SecondsIntoLine < 0.0f)
	{
		return;
	}
	// The last sentence stays until the portrait closes; earlier ones are wiped after the hold time.
	SetLineText(TranscriptReveal::BuildVisibleText(M_Sentences, SecondsIntoLine, M_Style.SentenceHoldSeconds, true));
}

void UW_PortraitSubtitle::SetLineText(const FString& NewText)
{
	if (NewText.Equals(M_ShownText, ESearchCase::CaseSensitive) || not IsValid(M_LineText))
	{
		return;
	}
	M_ShownText = NewText;
	M_LineText->SetText(FText::FromString(NewText));
}
