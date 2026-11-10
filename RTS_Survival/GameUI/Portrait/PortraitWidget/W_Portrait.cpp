// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_Portrait.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Slate/SlateBrushAsset.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/CinematicTranscriptSubsystem.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/SubtitleStyleDefaults.h"
#include "RTS_Survival/GameUI/Portrait/PortraitSubtitle/W_PortraitSubtitle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UW_Portrait::UW_Portrait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultSubtitleSetup();
}

void UW_Portrait::SetDefaultSubtitleSetup()
{
	SpeakerNames.Add(ERTSPortraitTypes::GermanAnnouncer, NSLOCTEXT("Portrait", "SpeakerAdvisor", "Advisor"));
	SpeakerNames.Add(ERTSPortraitTypes::SovietHazmat, NSLOCTEXT("Portrait", "SpeakerHazmat", "Hazmat"));
	SpeakerNames.Add(ERTSPortraitTypes::SovietCortexCommander,
	                 NSLOCTEXT("Portrait", "SpeakerCortex", "Cortex Commander"));
	SpeakerNames.Add(ERTSPortraitTypes::SovietCyborgEmissive, NSLOCTEXT("Portrait", "SpeakerCyborg", "Cyborg"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanScavengers, NSLOCTEXT("Portrait", "SpeakerScavenger", "Scavenger"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanResistanceInfantry,
	                 NSLOCTEXT("Portrait", "SpeakerResistance", "Resistance"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanSniper, NSLOCTEXT("Portrait", "SpeakerSniper", "Sniper"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanVultures, NSLOCTEXT("Portrait", "SpeakerVulture", "Vulture"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanMechanizedPioneers,
	                 NSLOCTEXT("Portrait", "SpeakerPioneer", "Pioneer"));
	SpeakerNames.Add(ERTSPortraitTypes::GermanCommando, NSLOCTEXT("Portrait", "SpeakerCommando", "Commando"));

	static ConstructorHelpers::FObjectFinder<UFont> MainFont(SubtitleStyleDefaults::MainFontPath);
	static ConstructorHelpers::FObjectFinder<UFont> ReadableFont(SubtitleStyleDefaults::ReadableFontPath);
	SubtitleStyle.SpeakerFont = MainFont.Succeeded()
		                            ? FSlateFontInfo(MainFont.Object, SubtitleStyleDefaults::SpeakerFontSize)
		                            : FCoreStyle::GetDefaultFontStyle("Bold", SubtitleStyleDefaults::SpeakerFontSize);
	SubtitleStyle.LineFont = ReadableFont.Succeeded()
		                         ? FSlateFontInfo(ReadableFont.Object, SubtitleStyleDefaults::LineFontSize,
		                                          FName(SubtitleStyleDefaults::ReadableTypeface))
		                         : FCoreStyle::GetDefaultFontStyle("Regular", SubtitleStyleDefaults::LineFontSize);

	SubtitleStyle.PanelBrush = SubtitleStyleDefaults::MakePanelBrush();
}

void UW_Portrait::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CreateSubtitleWidget();
}

void UW_Portrait::CreateSubtitleWidget()
{
	if (M_Subtitle != nullptr || IsDesignTime())
	{
		return;
	}
	M_Subtitle = CreateWidget<UW_PortraitSubtitle>(this, UW_PortraitSubtitle::StaticClass());
	if (not GetIsValidSubtitle())
	{
		return;
	}
	M_Subtitle->ApplyStyle(SubtitleStyle);
	PlaceSubtitleUnderPortrait();
	M_Subtitle->StopLine();
}

void UW_Portrait::PlaceSubtitleUnderPortrait()
{
	if (PortraitImage == nullptr)
	{
		RTSFunctionLibrary::ReportError("UW_Portrait::PlaceSubtitleUnderPortrait - PortraitImage is null.");
		return;
	}

	// Walk up from the image to the widget that sits directly in a canvas (the portrait's frame block).
	UWidget* PortraitBlock = PortraitImage;
	UCanvasPanel* Canvas = nullptr;
	while (PortraitBlock != nullptr && Canvas == nullptr)
	{
		Canvas = Cast<UCanvasPanel>(PortraitBlock->GetParent());
		if (Canvas == nullptr)
		{
			PortraitBlock = PortraitBlock->GetParent();
		}
	}
	const UCanvasPanelSlot* const BlockSlot = PortraitBlock != nullptr
		                                          ? Cast<UCanvasPanelSlot>(PortraitBlock->Slot)
		                                          : nullptr;
	if (Canvas == nullptr || BlockSlot == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::PlaceSubtitleUnderPortrait - the portrait image is not inside a canvas panel; "
			"the subtitle box needs W_BP_Portrait's root canvas.");
		return;
	}

	UCanvasPanelSlot* const SubtitleSlot = Canvas->AddChildToCanvas(M_Subtitle);
	const FAnchors Anchors = BlockSlot->GetAnchors();
	const FVector2D BlockPosition = BlockSlot->GetPosition();
	const FVector2D BlockSize = BlockSlot->GetSize();
	const FVector2D BlockAlignment = BlockSlot->GetAlignment();
	if (Anchors.IsStretchedHorizontal() || Anchors.IsStretchedVertical())
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::PlaceSubtitleUnderPortrait - the portrait block uses stretched anchors; the subtitle "
			"is placed as if they were a point.");
	}

	// Same anchor as the portrait block; top-centre of the subtitle at the block's bottom-centre.
	constexpr float CentreAlignment = 0.5f;
	SubtitleSlot->SetAnchors(Anchors);
	SubtitleSlot->SetAlignment(FVector2D(CentreAlignment, 0.0f));
	SubtitleSlot->SetAutoSize(true);
	SubtitleSlot->SetPosition(FVector2D(
		BlockPosition.X + (CentreAlignment - BlockAlignment.X) * BlockSize.X,
		BlockPosition.Y + (1.0f - BlockAlignment.Y) * BlockSize.Y + SubtitleStyle.GapBelowPortrait));
}

void UW_Portrait::StartSubtitles(const ERTSPortraitTypes PortraitType, const USoundBase* VoiceLine,
                                 TFunction<float()> GetPlaybackSeconds)
{
	if (not GetIsValidSubtitle())
	{
		return;
	}
	const FCinematicTranscriptRow* const Transcript = FindTranscript(VoiceLine);
	static const TArray<FCinematicTranscriptSentence> NoSentences;
	M_Subtitle->StartLine(GetSpeakerName(PortraitType, Transcript),
	                      Transcript != nullptr ? Transcript->Sentences : NoSentences,
	                      MoveTemp(GetPlaybackSeconds));
}

const FCinematicTranscriptRow* UW_Portrait::FindTranscript(const USoundBase* VoiceLine) const
{
	UCinematicTranscriptSubsystem* const Transcripts = UCinematicTranscriptSubsystem::Get(this);
	return Transcripts != nullptr ? Transcripts->FindTranscript(TranscriptTable, VoiceLine) : nullptr;
}

FText UW_Portrait::GetSpeakerName(const ERTSPortraitTypes PortraitType,
                                  const FCinematicTranscriptRow* const Transcript) const
{
	if (Transcript != nullptr && not Transcript->SpeakerOverride.IsEmpty())
	{
		return Transcript->SpeakerOverride;
	}
	const FText* const Name = SpeakerNames.Find(PortraitType);
	return Name != nullptr ? *Name : FText::GetEmpty();
}

bool UW_Portrait::GetIsValidSubtitle() const
{
	if (IsValid(M_Subtitle))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_Subtitle",
		"GetIsValidSubtitle",
		this);
	return false;
}

void UW_Portrait::UpdatePortrait(const ERTSPortraitTypes NewType)
{
	if (NewType == ERTSPortraitTypes::None)
	{
		HidePortrait();
		return;
	}

	TSoftObjectPtr<USlateBrushAsset>* PortraitBrushPtr = PortraitBrushes.Find(NewType);
	if (PortraitBrushPtr == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::UpdatePortrait - No PortraitBrush configured for requested portrait type.");
		HidePortrait();
		return;
	}

	if (PortraitBrushPtr->IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::UpdatePortrait - Portrait brush soft reference is null for requested portrait type.");
		HidePortrait();
		return;
	}

	// We will be showing this widget; opacity is controlled by the fade-in.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Brush already loaded → apply immediately and start fade-in.
	if (PortraitBrushPtr->IsValid())
	{
		USlateBrushAsset* LoadedAsset = PortraitBrushPtr->Get();
		OnBrushLoaded(LoadedAsset);
		return;
	}

	// Async load the brush.
	const FSoftObjectPath AssetPath = PortraitBrushPtr->ToSoftObjectPath();
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<UW_Portrait> WeakThis(this);

	Streamable.RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateLambda([WeakThis, AssetPath]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			UObject* ResolvedObject = AssetPath.ResolveObject();
			if (ResolvedObject == nullptr)
			{
				ResolvedObject = AssetPath.TryLoad();
			}

			USlateBrushAsset* LoadedAsset = Cast<USlateBrushAsset>(ResolvedObject);
			WeakThis->OnBrushLoaded(LoadedAsset);
		}));
}

void UW_Portrait::HidePortrait()
{
	// Stop any active fade-in.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
	}
	bM_IsFadingIn = false;
	M_CurrentFadeTimeSeconds = 0.0f;

	// Hide the widget and clear the brush.
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Hidden);

	if (PortraitImage != nullptr)
	{
		FSlateBrush EmptyBrush;
		PortraitImage->SetBrush(EmptyBrush);
	}

	// HidePortrait also runs from InitPortraitManager before NativeOnInitialized may have built the subtitle.
	if (M_Subtitle != nullptr)
	{
		M_Subtitle->StopLine();
	}
}

void UW_Portrait::OnBrushLoaded(USlateBrushAsset* LoadedAsset)
{
	if (not IsValid(LoadedAsset))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to load portrait brush asset in UW_Portrait::OnBrushLoaded().");
		HidePortrait();
		return;
	}

	if (PortraitImage == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::OnBrushLoaded - PortraitImage is null.");
		return;
	}

	PortraitImage->SetBrush(LoadedAsset->Brush);
	StartFadeIn();
}

void UW_Portrait::StartFadeIn()
{
	// Reset fade parameters and ensure we are visible but fully transparent.
	M_CurrentFadeTimeSeconds = 0.0f;
	bM_IsFadingIn = true;

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.0f);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"UW_Portrait::StartFadeIn - World is null, cannot start fade timer.");
		SetRenderOpacity(1.0f);
		bM_IsFadingIn = false;
		return;
	}

	// Restart the timer if it was already running.
	World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);

	if (M_FadeInDurationSeconds <= 0.0f || M_FadeInTickIntervalSeconds <= 0.0f)
	{
		// Invalid fade parameters: just show instantly.
		SetRenderOpacity(1.0f);
		bM_IsFadingIn = false;
		return;
	}

	World->GetTimerManager().SetTimer(
		M_FadeInTimerHandle,
		this,
		&UW_Portrait::TickFadeIn,
		M_FadeInTickIntervalSeconds,
		true);
}

void UW_Portrait::TickFadeIn()
{
	if (not bM_IsFadingIn)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
		}
		return;
	}

	M_CurrentFadeTimeSeconds += M_FadeInTickIntervalSeconds;

	const float Alpha = FMath::Clamp(
		M_CurrentFadeTimeSeconds / M_FadeInDurationSeconds,
		0.0f,
		1.0f);

	SetRenderOpacity(Alpha);

	if (Alpha >= 1.0f)
	{
		bM_IsFadingIn = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_FadeInTimerHandle);
		}
	}
}
