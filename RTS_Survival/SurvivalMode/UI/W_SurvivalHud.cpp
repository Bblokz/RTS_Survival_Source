// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalHud.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SurvivalArmyUiTypes.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"
#include "TimerManager.h"

namespace SurvivalHudPrivate
{
	void ApplyText(UTextBlock* Text, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		if (IsValid(Text))
		{
			UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, Font, Color);
		}
	}

	void SetText(UTextBlock* Text, const FText& Value)
	{
		if (IsValid(Text))
		{
			Text->SetText(Value);
		}
	}

	void SetShown(UWidget* Widget, const bool bShown, const ESlateVisibility ShownVisibility)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ShownVisibility : ESlateVisibility::Collapsed);
		}
	}

	void ApplyBrushIfSet(UBorder* Border, const FSlateBrush& Brush)
	{
		if (IsValid(Border) and IsValid(Brush.GetResourceObject()))
		{
			Border->SetBrush(Brush);
		}
	}

	void ApplyButtonStyleIfSet(UButton* Button, const FButtonStyle& ButtonStyle)
	{
		if (IsValid(Button) and IsValid(ButtonStyle.Normal.GetResourceObject()))
		{
			Button->SetStyle(ButtonStyle);
		}
	}
}

void UW_SurvivalHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The overlay never blocks the RTS camera and selection; only the defeat panel takes the mouse.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	EnsureEnemiesText();
	ApplyStyleToBoundWidgets();
	ApplyStaticTexts();
	BindButtons();
	SurvivalHudPrivate::SetShown(M_DefeatPanel, false, ESlateVisibility::Visible);
	SurvivalHudPrivate::SetShown(M_BannerPanel, false, ESlateVisibility::HitTestInvisible);
}

void UW_SurvivalHud::NativeDestruct()
{
	if (M_Director.IsValid())
	{
		M_Director->OnPhaseChanged.Remove(M_PhaseChangedHandle);
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RefreshTimerHandle);
		World->GetTimerManager().ClearTimer(M_BannerTimerHandle);
	}
	Super::NativeDestruct();
}

void UW_SurvivalHud::InitHud(ASurvivalModeDirector* Director)
{
	if (M_Director.IsValid())
	{
		M_Director->OnPhaseChanged.Remove(M_PhaseChangedHandle);
	}
	M_Director = Director;
	if (not GetIsValidDirector())
	{
		return;
	}
	M_PhaseChangedHandle = Director->OnPhaseChanged.AddUObject(this, &UW_SurvivalHud::OnPhaseChanged);
	StartRefreshTimer();
	RefreshStatus();
}

void UW_SurvivalHud::ShowDefeat(const float SecondsSurvived, const int32 WavesLaunched, const float BestSeconds,
                                const bool bIsNewBest)
{
	using namespace SurvivalHudPrivate;
	bM_IsDefeatShown = true;
	HideIncomingWaveBanner();
	// The result panel shows the final time; the status plate would only crowd it on small screens.
	SetShown(M_StatusPanel, false, ESlateVisibility::HitTestInvisible);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RefreshTimerHandle);
	}
	SetText(M_TimerText, SurvivalArmyUi::FormatClock(SecondsSurvived));
	SetText(M_TimeSurvivedText, SurvivalArmyUi::FormatClock(SecondsSurvived));
	SetText(M_WavesText, FText::AsNumber(WavesLaunched));
	// A new best is this run's time; the director may pass the stored best from before the save.
	const float ShownBest = bIsNewBest ? FMath::Max(BestSeconds, SecondsSurvived) : BestSeconds;
	SetText(M_BestTimeText, ShownBest > 0.f
		                        ? SurvivalArmyUi::FormatClock(ShownBest)
		                        : NSLOCTEXT("SurvivalHud", "NoBest", "-"));
	SetShown(M_NewRecordText, bIsNewBest, ESlateVisibility::HitTestInvisible);
	SetShown(M_DefeatPanel, true, ESlateVisibility::Visible);
	SetDefeatInputMode();
}

void UW_SurvivalHud::ShowIncomingWaveBanner(const int32 WaveNumber, const FText& EdgeLabel, const float DisplaySeconds)
{
	using namespace SurvivalHudPrivate;
	if (bM_IsDefeatShown)
	{
		return;
	}
	SetText(M_BannerTitleText, FText::Format(NSLOCTEXT("SurvivalHud", "BannerTitle", "WAVE {0} INCOMING"),
	                                         FText::AsNumber(WaveNumber)));
	SetText(M_BannerText, EdgeLabel.IsEmpty()
		                      ? FText::GetEmpty()
		                      : FText::Format(NSLOCTEXT("SurvivalHud", "BannerFrom", "from the {0}"), EdgeLabel));
	SetShown(M_BannerText, not EdgeLabel.IsEmpty(), ESlateVisibility::HitTestInvisible);
	SetShown(M_BannerPanel, true, ESlateVisibility::HitTestInvisible);
	UWorld* const World = GetWorld();
	if (not World)
	{
		return;
	}
	const float Seconds = DisplaySeconds > 0.f ? DisplaySeconds : SurvivalUiLayout::DefaultBannerSeconds;
	TWeakObjectPtr<UW_SurvivalHud> WeakThis(this);
	World->GetTimerManager().SetTimer(M_BannerTimerHandle, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HideIncomingWaveBanner();
		}
	}, Seconds, false);
}

void UW_SurvivalHud::HideIncomingWaveBanner()
{
	SurvivalHudPrivate::SetShown(M_BannerPanel, false, ESlateVisibility::HitTestInvisible);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_BannerTimerHandle);
	}
}

FText UW_SurvivalHud::GetCompassDirectionText(const FVector& WaveEntryLocation, const FVector& BaseLocation)
{
	return SurvivalArmyUi::GetCompassDirectionText(WaveEntryLocation, BaseLocation);
}

void UW_SurvivalHud::RetrySurvival()
{
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival HUD: cannot retry, the current level name is empty."));
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
}

void UW_SurvivalHud::ReturnToMainMenu()
{
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString DefaultMapPath = GameMapsSettings ? GameMapsSettings->GetGameDefaultMap() : FString();
	if (DefaultMapPath.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival HUD: cannot return to the main menu, no default map set."));
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*DefaultMapPath));
}

void UW_SurvivalHud::ApplyStyleToBoundWidgets()
{
	using namespace SurvivalUiLayout;
	using namespace SurvivalHudPrivate;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	ApplyBrushIfSet(M_StatusPlate, Style.HudPlate);
	ApplyBrushIfSet(M_BannerPlate, Style.BannerPlate);
	ApplyBrushIfSet(M_ResultPanel, Style.ResultPanel);
	ApplyButtonStyleIfSet(M_RetryButton, Style.ConfirmButton);
	ApplyButtonStyleIfSet(M_MainMenuButton, Style.BackButton);
	ApplyText(M_TimerText, MakeFont(Style.HeadingFont, HudTimerFontSize, LabelLetterSpacing), Style.TextColor);
	ApplyText(M_PhaseText, MakeFont(Style.HeadingFont, HudLabelFontSize, TitleLetterSpacing), Style.MutedTextColor);
	ApplyText(M_WaveText, MakeFont(Style.HeadingFont, HudLabelFontSize, LabelLetterSpacing), Style.AccentColor);
	ApplyText(M_EnemiesText, MakeFont(Style.HeadingFont, HudLabelFontSize, LabelLetterSpacing), Style.WarningColor);
	ApplyText(M_BannerTitleText, MakeFont(Style.HeadingFont, BannerTitleFontSize, TitleLetterSpacing),
	          Style.WarningColor);
	ApplyText(M_BannerText, MakeFont(Style.BodyFont, LabelFontSize), Style.TextColor);
	ApplyText(M_DefeatTitleText, MakeFont(Style.HeadingFont, ResultTitleFontSize, TitleLetterSpacing),
	          Style.WarningColor);
	for (UTextBlock* const Label : {M_TimeSurvivedLabel.Get(), M_WavesLabel.Get(), M_BestTimeLabel.Get()})
	{
		ApplyText(Label, MakeFont(Style.HeadingFont, LabelFontSize, LabelLetterSpacing), Style.MutedTextColor);
	}
	for (UTextBlock* const Value : {M_TimeSurvivedText.Get(), M_WavesText.Get(), M_BestTimeText.Get()})
	{
		ApplyText(Value, MakeFont(Style.HeadingFont, ResultValueFontSize), Style.TextColor);
	}
	ApplyText(M_NewRecordText, MakeFont(Style.HeadingFont, LabelFontSize, TitleLetterSpacing), Style.AccentColor);
	ApplyText(M_RetryText, MakeFont(Style.HeadingFont, ActionButtonFontSize, ActionButtonLetterSpacing),
	          Style.TextColor);
	ApplyText(M_MainMenuText, MakeFont(Style.HeadingFont, ActionButtonFontSize, ActionButtonLetterSpacing),
	          Style.TextColor);
}

void UW_SurvivalHud::EnsureEnemiesText()
{
	if (IsValid(M_EnemiesText) or not IsValid(M_WaveText) or not WidgetTree)
	{
		return;
	}
	UPanelWidget* StatusColumn = M_WaveText->GetParent();
	if (not IsValid(StatusColumn))
	{
		return;
	}
	M_EnemiesText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("M_EnemiesText"));
	UPanelSlot* EnemiesSlot = StatusColumn->AddChild(M_EnemiesText);
	if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(EnemiesSlot))
	{
		const UVerticalBoxSlot* WaveSlot = Cast<UVerticalBoxSlot>(M_WaveText->Slot);
		VerticalSlot->SetHorizontalAlignment(WaveSlot ? WaveSlot->GetHorizontalAlignment() : HAlign_Center);
	}
	M_EnemiesText->SetJustification(ETextJustify::Center);
	// The status plate was laid out for three lines; let it grow for the fourth.
	if (USizeBox* StatusBox = Cast<USizeBox>(M_StatusPanel))
	{
		StatusBox->ClearHeightOverride();
	}
}

void UW_SurvivalHud::ApplyStaticTexts()
{
	using namespace SurvivalHudPrivate;
	SetText(M_DefeatTitleText, NSLOCTEXT("SurvivalHud", "DefeatTitle", "YOUR LINES HAVE FALLEN"));
	SetText(M_TimeSurvivedLabel, NSLOCTEXT("SurvivalHud", "TimeSurvivedLabel", "TIME SURVIVED"));
	SetText(M_WavesLabel, NSLOCTEXT("SurvivalHud", "WavesLabel", "WAVES SURVIVED"));
	SetText(M_BestTimeLabel, NSLOCTEXT("SurvivalHud", "BestTimeLabel", "BEST ON THIS MAP"));
	SetText(M_NewRecordText, NSLOCTEXT("SurvivalHud", "NewRecord", "NEW RECORD"));
	SetText(M_RetryText, NSLOCTEXT("SurvivalHud", "Retry", "RETRY"));
	SetText(M_MainMenuText, NSLOCTEXT("SurvivalHud", "MainMenu", "MAIN MENU"));
}

void UW_SurvivalHud::BindButtons()
{
	if (IsValid(M_RetryButton))
	{
		M_RetryButton->OnClicked.RemoveAll(this);
		M_RetryButton->OnClicked.AddDynamic(this, &UW_SurvivalHud::OnRetryClicked);
	}
	if (IsValid(M_MainMenuButton))
	{
		M_MainMenuButton->OnClicked.RemoveAll(this);
		M_MainMenuButton->OnClicked.AddDynamic(this, &UW_SurvivalHud::OnMainMenuClicked);
	}
}

void UW_SurvivalHud::StartRefreshTimer()
{
	UWorld* const World = GetWorld();
	if (not World)
	{
		return;
	}
	TWeakObjectPtr<UW_SurvivalHud> WeakThis(this);
	World->GetTimerManager().SetTimer(M_RefreshTimerHandle, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RefreshStatus();
		}
	}, SurvivalUiLayout::HudRefreshSeconds, true);
}

void UW_SurvivalHud::RefreshStatus()
{
	if (bM_IsDefeatShown || not M_Director.IsValid())
	{
		return;
	}
	RefreshStatusForPhase(*M_Director, M_Director->GetPhase());
}

void UW_SurvivalHud::RefreshStatusForPhase(const ASurvivalModeDirector& Director, const ESurvivalPhase Phase)
{
	using namespace SurvivalHudPrivate;
	const bool bShowStatus = Phase == ESurvivalPhase::Deploying || Phase == ESurvivalPhase::GracePeriod
		|| Phase == ESurvivalPhase::Waves;
	SetShown(M_StatusPanel, bShowStatus, ESlateVisibility::HitTestInvisible);
	const bool bShowWaves = Phase == ESurvivalPhase::Waves;
	SetShown(M_WaveText, bShowWaves, ESlateVisibility::HitTestInvisible);
	SetShown(M_EnemiesText, bShowWaves, ESlateVisibility::HitTestInvisible);
	switch (Phase)
	{
	case ESurvivalPhase::Deploying:
		SetText(M_PhaseText, NSLOCTEXT("SurvivalHud", "PhaseDeploying", "DEPLOYING"));
		SetText(M_TimerText, NSLOCTEXT("SurvivalHud", "TimerDeploying", "--:--"));
		break;
	case ESurvivalPhase::GracePeriod:
		SetText(M_PhaseText, NSLOCTEXT("SurvivalHud", "PhaseGrace", "FIRST WAVE IN"));
		SetText(M_TimerText, SurvivalArmyUi::FormatClock(Director.GetGraceSecondsRemaining()));
		break;
	case ESurvivalPhase::Waves:
		if (Director.GetIsInIntermission())
		{
			SetText(M_PhaseText, NSLOCTEXT("SurvivalHud", "PhaseIntermission", "NEXT WAVE IN"));
			SetText(M_TimerText, SurvivalArmyUi::FormatClock(Director.GetIntermissionSecondsRemaining()));
			SetText(M_WaveText, FText::Format(NSLOCTEXT("SurvivalHud", "WaveSurvived", "WAVE {0} SURVIVED"),
			                                  FText::AsNumber(Director.GetRoundsSurvived())));
			SetText(M_EnemiesText, Director.GetHasChosenReward()
				                       ? NSLOCTEXT("SurvivalHud", "RewardInbound", "REINFORCEMENTS INBOUND")
				                       : NSLOCTEXT("SurvivalHud", "PickReward", "CHOOSE YOUR REINFORCEMENTS"));
			break;
		}
		SetText(M_PhaseText, NSLOCTEXT("SurvivalHud", "PhaseWaves", "SURVIVED"));
		SetText(M_TimerText, SurvivalArmyUi::FormatClock(Director.GetElapsedSurvivalSeconds()));
		SetText(M_WaveText, FText::Format(NSLOCTEXT("SurvivalHud", "WaveCounter", "WAVE {0}"),
		                                  FText::AsNumber(Director.GetWavesLaunched())));
		SetText(M_EnemiesText, FText::Format(NSLOCTEXT("SurvivalHud", "EnemiesRemaining", "{0} ENEMIES REMAINING"),
		                                     FText::AsNumber(Director.GetEnemiesRemaining())));
		break;
	default:
		break;
	}
}

void UW_SurvivalHud::OnPhaseChanged(const ESurvivalPhase OldPhase, const ESurvivalPhase NewPhase)
{
	if (bM_IsDefeatShown or not M_Director.IsValid())
	{
		return;
	}
	RefreshStatusForPhase(*M_Director, NewPhase);
}

void UW_SurvivalHud::SetDefeatInputMode()
{
	APlayerController* const PlayerController = GetOwningPlayer();
	if (not IsValid(PlayerController))
	{
		return;
	}
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(TakeWidget());
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
}

void UW_SurvivalHud::OnRetryClicked()
{
	RetrySurvival();
}

void UW_SurvivalHud::OnMainMenuClicked()
{
	ReturnToMainMenu();
}

bool UW_SurvivalHud::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}
