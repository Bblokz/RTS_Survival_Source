// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantSelectionCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/W_UnitVariantSelectionMenu.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitVariantSelectionCapture, Log, All);

namespace UnitVariantSelectionCapturePrivate
{
	const TCHAR* const CommandLineSwitch = TEXT("UnitVariantSelectionCapture");
	const TCHAR* const FactionSwitch = TEXT("UnitVariantSelectionFaction=");
	constexpr float BootSeconds = 3.f;
	constexpr float MenuTimeoutSeconds = 30.f;
	// The hangar preview is spawned async; give it time to load and settle before the picture.
	constexpr float SettleSeconds = 6.f;
	constexpr float CaptureWriteSeconds = 2.f;

	template <typename WidgetType>
	WidgetType* FindLiveWidget(const UWorld* World)
	{
		for (TObjectIterator<WidgetType> WidgetIt; WidgetIt; ++WidgetIt)
		{
			if (WidgetIt->GetWorld() == World && WidgetIt->IsInViewport())
			{
				return *WidgetIt;
			}
		}
		return nullptr;
	}
}

bool UUnitVariantSelectionCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), UnitVariantSelectionCapturePrivate::CommandLineSwitch);
}

TStatId UUnitVariantSelectionCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitVariantSelectionCaptureSubsystem, STATGROUP_Tickables);
}

void UUnitVariantSelectionCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	if (URTSGameInstance* GameInstance = Cast<URTSGameInstance>(InWorld.GetGameInstance()))
	{
		GameInstance->SetAllowUnitVariantSelection(true);
	}
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForFactionMenu);
	UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE started"));
}

void UUnitVariantSelectionCaptureSubsystem::Tick(const float DeltaTime)
{
	switch (M_Phase)
	{
	case EUnitVariantSelectionCapturePhase::WaitingForFactionMenu:
		TickWaitingForFactionMenu();
		return;
	case EUnitVariantSelectionCapturePhase::WaitingForVariantMenu:
		TickWaitingForVariantMenu();
		return;
	case EUnitVariantSelectionCapturePhase::SettlingOnBase:
		TickSettlingOnBase();
		return;
	case EUnitVariantSelectionCapturePhase::WaitingForBaseCapture:
		TickWaitingForBaseCapture();
		return;
	case EUnitVariantSelectionCapturePhase::SettlingOnVariant:
		TickSettlingOnVariant();
		return;
	case EUnitVariantSelectionCapturePhase::WaitingForLastCapture:
		if (GetPhaseSeconds() > UnitVariantSelectionCapturePrivate::CaptureWriteSeconds)
		{
			Finish();
		}
		return;
	default:
		return;
	}
}

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForFactionMenu()
{
	using namespace UnitVariantSelectionCapturePrivate;
	if (GetPhaseSeconds() < BootSeconds)
	{
		return;
	}
	if (UW_FactionPopup* Popup = FindLiveWidget<UW_FactionPopup>(GetWorld()))
	{
		Popup->OnPopupAccepted.Broadcast();
		return;
	}
	AFactionPlayerController* Controller = GetController();
	if (not IsValid(Controller) || not FindLiveWidget<UW_FactionSelectionMenu>(GetWorld()))
	{
		if (GetPhaseSeconds() > MenuTimeoutSeconds)
		{
			Check(false, TEXT("faction_menu_shown"));
			Finish();
		}
		return;
	}
	Capture(TEXT("faction_menu"));
	int32 FactionValue = static_cast<int32>(ERTSFaction::GerBreakthroughDoctrine);
	FParse::Value(FCommandLine::Get(), FactionSwitch, FactionValue);
	Controller->HandleLaunchCampaignRequested(static_cast<ERTSFaction>(FactionValue));
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForVariantMenu);
}

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForVariantMenu()
{
	using namespace UnitVariantSelectionCapturePrivate;
	UW_UnitVariantSelectionMenu* Menu = FindLiveWidget<UW_UnitVariantSelectionMenu>(GetWorld());
	if (not IsValid(Menu))
	{
		if (GetPhaseSeconds() > MenuTimeoutSeconds)
		{
			Check(false, TEXT("variant_menu_shown"), TEXT("faction has no unit with variants in its menu setting?"));
			Finish();
		}
		return;
	}
	M_Menu = Menu;
	Check(true, TEXT("variant_menu_shown"));
	EnterPhase(EUnitVariantSelectionCapturePhase::SettlingOnBase);
}

void UUnitVariantSelectionCaptureSubsystem::TickSettlingOnBase()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::SettleSeconds)
	{
		return;
	}
	CheckPreviewClass(false);
	Capture(TEXT("variant_menu_base"));
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForBaseCapture);
}

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForBaseCapture()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::CaptureWriteSeconds)
	{
		return;
	}
	if (M_Menu.IsValid())
	{
		M_Menu->HandleVariantCardClicked(EUnitVariant::VariantA);
	}
	EnterPhase(EUnitVariantSelectionCapturePhase::SettlingOnVariant);
}

void UUnitVariantSelectionCaptureSubsystem::TickSettlingOnVariant()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::SettleSeconds)
	{
		return;
	}
	CheckPreviewClass(true);
	Capture(TEXT("variant_menu_variant_a"));
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForLastCapture);
}

void UUnitVariantSelectionCaptureSubsystem::CheckPreviewClass(const bool bExpectVariant)
{
	int32 PreviewUnitCount = 0;
	int32 VariantUnitCount = 0;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		const URTSComponent* RTSComponent = ActorIt->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent) || ActorIt->IsActorBeingDestroyed())
		{
			continue;
		}
		++PreviewUnitCount;
		VariantUnitCount += RTSComponent->GetUnitVariant() == EUnitVariant::VariantA ? 1 : 0;
		UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE unit=%s variant=%d"),
		       *ActorIt->GetClass()->GetName(), static_cast<int32>(RTSComponent->GetUnitVariant()));
	}
	const FString Details = FString::Printf(TEXT("units=%d variant_a_units=%d"), PreviewUnitCount, VariantUnitCount);
	if (bExpectVariant)
	{
		Check(VariantUnitCount == 1, TEXT("hangar_shows_variant_a"), Details);
		return;
	}
	Check(PreviewUnitCount > 0 && VariantUnitCount == 0, TEXT("hangar_shows_base"), Details);
}

void UUnitVariantSelectionCaptureSubsystem::EnterPhase(const EUnitVariantSelectionCapturePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetRealTimeSeconds() : 0.f;
}

float UUnitVariantSelectionCaptureSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetRealTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UUnitVariantSelectionCaptureSubsystem::Check(const bool bCondition, const FString& Description,
                                                  const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UUnitVariantSelectionCaptureSubsystem::Capture(const FString& CaptureName) const
{
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("UnitVariantSelection") / (CaptureName + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE capture %s"),
	       *ScreenshotPath);
}

void UUnitVariantSelectionCaptureSubsystem::Finish()
{
	M_Phase = EUnitVariantSelectionCapturePhase::Inactive;
	UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	RTSFunctionLibrary::PopModalDialogSuppression();
	FPlatformMisc::RequestExit(false, TEXT("UnitVariantSelectionCapture"));
}

AFactionPlayerController* UUnitVariantSelectionCaptureSubsystem::GetController() const
{
	return IsValid(GetWorld()) ? Cast<AFactionPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
}

#else

bool UUnitVariantSelectionCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UUnitVariantSelectionCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UUnitVariantSelectionCaptureSubsystem::Tick(const float DeltaTime)
{
}

TStatId UUnitVariantSelectionCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUnitVariantSelectionCaptureSubsystem, STATGROUP_Tickables);
}

#endif
