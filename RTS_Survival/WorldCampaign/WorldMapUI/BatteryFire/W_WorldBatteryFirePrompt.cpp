// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/BatteryFire/W_WorldBatteryFirePrompt.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireDirector.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

// Same sizes as the commander panel's pick prompt, in UI units at the 1920x1080 reference.
namespace WorldBatteryFirePromptLayout
{
	constexpr float PlateWidth = 400.f;
	const FMargin PlatePadding(14.f, 8.f, 14.f, 9.f);
	const FVector2D CancelButtonSize(92.f, 28.f);
	constexpr float LineGap = 4.f;
	constexpr float CornerRadius = 2.f;
	constexpr float BorderWidth = 1.f;
	constexpr float PlateOpacity = 0.94f;
	constexpr int32 PromptFontSize = 12;
	constexpr int32 PromptLetterSpacing = 50;
	constexpr int32 CancelFontSize = 11;
	constexpr int32 CancelLetterSpacing = 100;
	constexpr int32 FailFontSize = 11;
	// A refused click keeps its reason on the plate this long.
	constexpr float FailReasonSeconds = 2.5f;
}

void UW_WorldBatteryFirePrompt::InitBatteryFirePrompt(AWorldPlayerController* WorldPlayerController,
                                                      const FWorldCampaignUILook* Look)
{
	if (not IsValid(WorldPlayerController) || Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_WorldBatteryFirePrompt::InitBatteryFirePrompt was given no controller or no look."));
		return;
	}

	BuildWidgetTree(*Look);
	SetVisibility(ESlateVisibility::Collapsed);
	M_Director = WorldPlayerController->GetWorldBatteryFireDirector();
	if (not GetIsValidDirector())
	{
		return;
	}

	M_Director->OnPickingChanged.AddUObject(this, &UW_WorldBatteryFirePrompt::HandlePickingChanged);
	M_Director->OnPickRefused.AddUObject(this, &UW_WorldBatteryFirePrompt::HandlePickRefused);
}

void UW_WorldBatteryFirePrompt::CancelPicking()
{
	// Quiet check: the HUD calls this whenever it hides, also before the prompt was ever used.
	if (M_Director.IsValid())
	{
		M_Director->CancelTargetPicking();
	}
}

void UW_WorldBatteryFirePrompt::NativeDestruct()
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_HideFailReasonTimer);
	}
	// The director delegates stay bound: NativeDestruct also runs when the HUD is only taken off the screen, and a
	// UObject delegate skips a destroyed prompt.
	Super::NativeDestruct();
}

FReply UW_WorldBatteryFirePrompt::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                          const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CancelPicking();
	}
	return FReply::Handled();
}

void UW_WorldBatteryFirePrompt::BuildWidgetTree(const FWorldCampaignUILook& Look)
{
	using namespace WorldBatteryFirePromptLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	USizeBox* const PlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlateBox->SetWidthOverride(PlateWidth);
	WidgetTree->RootWidget = PlateBox;

	FSlateBrush PlateBrush;
	PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PlateBrush.TintColor = FSlateColor(WorldCampaignUIColors::PlateFill.CopyWithNewOpacity(PlateOpacity));
	PlateBrush.OutlineSettings = FSlateBrushOutlineSettings(CornerRadius, FSlateColor(WorldCampaignUIColors::Cream),
	                                                        BorderWidth);
	M_Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Plate->SetBrush(PlateBrush);
	M_Plate->SetPadding(PlatePadding);
	PlateBox->SetContent(M_Plate);

	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_Plate->SetContent(Lines);
	UHorizontalBox* const PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Lines->AddChildToVerticalBox(PromptRow);
	M_PromptText = DeckBuilderWidgets::MakeText(
		*WidgetTree, NSLOCTEXT("WorldBatteryFire", "PickPrompt", "SELECT AN ENEMY DIVISION IN RANGE"),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PromptFontSize, PromptLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Cream));
	UHorizontalBoxSlot* const PromptSlot = PromptRow->AddChildToHorizontalBox(M_PromptText);
	PromptSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PromptSlot->SetVerticalAlignment(VAlign_Center);

	UWidget* const CancelBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("WorldBatteryFire", "CancelPicking", "CANCEL"), Look.Deck.Button,
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, CancelFontSize, CancelLetterSpacing),
		Look.Deck.Style.TextColor, CancelButtonSize, M_CancelButton, M_CancelLabel);
	M_CancelButton->OnClicked.AddUniqueDynamic(this, &UW_WorldBatteryFirePrompt::HandleCancelClicked);
	PromptRow->AddChildToHorizontalBox(CancelBox)->SetVerticalAlignment(VAlign_Center);

	M_FailText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, FailFontSize),
		FSlateColor(WorldCampaignUIColors::Bad));
	M_FailText->SetVisibility(ESlateVisibility::Collapsed);
	Lines->AddChildToVerticalBox(M_FailText)->SetPadding(FMargin(0.f, LineGap, 0.f, 0.f));
}

void UW_WorldBatteryFirePrompt::HandlePickingChanged()
{
	const bool bIsPicking = M_Director.IsValid() && M_Director->GetIsPickingTarget();
	HideFailReason();
	// Visible, not hit-test invisible: a click on the plate must not reach the map as a pick.
	SetVisibility(bIsPicking ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_WorldBatteryFirePrompt::HandlePickRefused(const FText& Reason)
{
	if (not IsValid(M_FailText) || Reason.IsEmpty())
	{
		return;
	}

	M_FailText->SetText(Reason);
	M_FailText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_HideFailReasonTimer, this, &UW_WorldBatteryFirePrompt::HideFailReason,
		                                  WorldBatteryFirePromptLayout::FailReasonSeconds, false);
	}
}

void UW_WorldBatteryFirePrompt::HideFailReason()
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_HideFailReasonTimer);
	}

	if (IsValid(M_FailText))
	{
		M_FailText->SetText(FText::GetEmpty());
		M_FailText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UW_WorldBatteryFirePrompt::HandleCancelClicked()
{
	CancelPicking();
}

bool UW_WorldBatteryFirePrompt::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_Director"), TEXT("UW_WorldBatteryFirePrompt::GetIsValidDirector"), this);
	return false;
}
