// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantSelectionCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/W_UnitVariantSelectionMenu.h"
#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitVariantSelectionCapture, Log, All);

namespace UnitVariantSelectionCapturePrivate
{
	const TCHAR* const CommandLineSwitch = TEXT("UnitVariantSelectionCapture");
	const TCHAR* const FactionSwitch = TEXT("UnitVariantSelectionFaction=");
	const TCHAR* const EachUnitSwitch = TEXT("UnitVariantSelectionEachUnit");
	// The variant screen spawns hidden probe units far below the hangar to read their weapons; not previews.
	const FName ProbeActorTag = TEXT("UnitVariantProbe");
	constexpr float BootSeconds = 3.f;
	constexpr float MenuTimeoutSeconds = 30.f;
	// The hangar preview is spawned async; give it time to load and settle before the picture.
	constexpr float SettleSeconds = 6.f;
	constexpr float CaptureWriteSeconds = 2.f;
	// Only the cards and the ability panel matter on the per-unit pictures, so the hangar preview is not waited
	// for; the panel needs the unit's variant classes, which load async.
	constexpr float EachUnitSettleSeconds = 2.5f;

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

	// The variant screen opens on the profile's saved choice, so the first preview is that variant, not Base.
	EUnitVariant GetSavedVariant(const FTrainingOption& Unit)
	{
		const USavePlayerProfile* PlayerProfile = FRTS_Profile::LoadPlayerProfile();
		if (not IsValid(PlayerProfile))
		{
			return EUnitVariant::Base;
		}
		const FTrainingOption SpawnerKey = UPlayerUnitVariantManager::GetSpawnerKey(Unit);
		for (const FUnitVariantSelection& Selection : PlayerProfile->M_UnitVariantSelections)
		{
			if (UPlayerUnitVariantManager::GetSpawnerKey(Selection.Unit) == SpawnerKey)
			{
				return Selection.Variant;
			}
		}
		return EUnitVariant::Base;
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
		TickWaitingForLastCapture();
		return;
	case EUnitVariantSelectionCapturePhase::SettlingOnEachUnit:
		TickSettlingOnEachUnit();
		return;
	case EUnitVariantSelectionCapturePhase::WaitingForEachUnitCapture:
		TickWaitingForEachUnitCapture();
		return;
	case EUnitVariantSelectionCapturePhase::SettlingOnDiscardDialog:
		TickSettlingOnDiscardDialog();
		return;
	case EUnitVariantSelectionCapturePhase::WaitingForDiscardDialogCapture:
		TickWaitingForDiscardDialogCapture();
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

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForLastCapture()
{
	using namespace UnitVariantSelectionCapturePrivate;
	if (GetPhaseSeconds() < CaptureWriteSeconds)
	{
		return;
	}
	if (not FParse::Param(FCommandLine::Get(), EachUnitSwitch))
	{
		Finish();
		return;
	}
	M_EachUnitIndex = 0;
	SelectEachUnitOrFinish();
}

void UUnitVariantSelectionCaptureSubsystem::TickSettlingOnEachUnit()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::EachUnitSettleSeconds)
	{
		return;
	}
	if (not bM_EachUnitScrolledToEnd)
	{
		// The preview may still be loading here, but an earlier unit's preview must never be left behind.
		const int32 PreviewCount = LogPreviewAbilities();
		Check(PreviewCount <= 1, TEXT("hangar_at_most_one_preview"),
		      FString::Printf(TEXT("unit_index=%d previews=%d"), M_EachUnitIndex, PreviewCount));
	}
	Capture(FString::Printf(TEXT("variant_cards_unit_%02d%s"), M_EachUnitIndex,
	                        bM_EachUnitScrolledToEnd ? TEXT("_end") : TEXT("")));
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForEachUnitCapture);
}

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForEachUnitCapture()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::CaptureWriteSeconds)
	{
		return;
	}
	if (not bM_EachUnitScrolledToEnd && M_Menu.IsValid())
	{
		// Second picture of the same unit: the bottom of the card list, where overflowing cards end up, with
		// Variant A hovered so the ability panel shows the hovered card instead of the equipped one.
		bM_EachUnitScrolledToEnd = true;
		M_Menu->ScrollCardsToEnd();
		M_Menu->HandleVariantCardHovered(EUnitVariant::VariantA, true);
		EnterPhase(EUnitVariantSelectionCapturePhase::SettlingOnEachUnit);
		return;
	}
	++M_EachUnitIndex;
	SelectEachUnitOrFinish();
}

void UUnitVariantSelectionCaptureSubsystem::SelectEachUnitOrFinish()
{
	if (not M_Menu.IsValid())
	{
		Finish();
		return;
	}
	if (M_EachUnitIndex >= M_Menu->GetUnitCount())
	{
		StartKeyboardCheck();
		return;
	}
	M_Menu->HandleUnitEntryClicked(M_EachUnitIndex);
	bM_EachUnitScrolledToEnd = false;
	EnterPhase(EUnitVariantSelectionCapturePhase::SettlingOnEachUnit);
}

void UUnitVariantSelectionCaptureSubsystem::StartKeyboardCheck()
{
	const EUnitVariant VariantBefore = M_Menu->Debug_GetChosenVariantOfSelectedUnit();
	M_Menu->Debug_PressNavigationKey(EKeys::Right);
	M_Menu->Debug_PressNavigationKey(EKeys::Enter);
	const EUnitVariant VariantAfter = M_Menu->Debug_GetChosenVariantOfSelectedUnit();
	Check(VariantAfter != VariantBefore, TEXT("keyboard_equips_variant"),
	      FString::Printf(TEXT("before=%s after=%s"), *UEnum::GetValueAsString(VariantBefore),
	                      *UEnum::GetValueAsString(VariantAfter)));
	Check(M_Menu->Debug_GetUnconfirmedChangeCount() > 0, TEXT("keyboard_change_is_unconfirmed"),
	      FString::Printf(TEXT("changes=%d"), M_Menu->Debug_GetUnconfirmedChangeCount()));
	M_Menu->Debug_PressNavigationKey(EKeys::Escape);
	Check(M_Menu->Debug_GetIsDiscardDialogOpen(), TEXT("back_with_changes_opens_discard_dialog"));
	EnterPhase(EUnitVariantSelectionCapturePhase::SettlingOnDiscardDialog);
}

void UUnitVariantSelectionCaptureSubsystem::TickSettlingOnDiscardDialog()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::EachUnitSettleSeconds)
	{
		return;
	}
	Capture(TEXT("variant_discard_dialog"));
	EnterPhase(EUnitVariantSelectionCapturePhase::WaitingForDiscardDialogCapture);
}

void UUnitVariantSelectionCaptureSubsystem::TickWaitingForDiscardDialogCapture()
{
	if (GetPhaseSeconds() < UnitVariantSelectionCapturePrivate::CaptureWriteSeconds)
	{
		return;
	}
	if (M_Menu.IsValid())
	{
		// Escape answers the dialog with "keep editing"; the screen must stay open with the choice intact.
		M_Menu->Debug_PressNavigationKey(EKeys::Escape);
		Check(not M_Menu->Debug_GetIsDiscardDialogOpen() && M_Menu->Debug_GetUnconfirmedChangeCount() > 0,
		      TEXT("escape_keeps_editing"));
	}
	Finish();
}

int32 UUnitVariantSelectionCaptureSubsystem::LogPreviewAbilities() const
{
	int32 PreviewCount = 0;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		ICommands* const Commands = Cast<ICommands>(*ActorIt);
		if (Commands == nullptr || ActorIt->IsActorBeingDestroyed()
			|| ActorIt->ActorHasTag(UnitVariantSelectionCapturePrivate::ProbeActorTag))
		{
			continue;
		}
		FString AbilityList;
		for (const FUnitAbilityEntry& Entry : Commands->GetUnitAbilityEntries())
		{
			if (Entry.AbilityId != EAbilityID::IdNoAbility)
			{
				AbilityList += FString::Printf(TEXT("%s:%d "), *UEnum::GetValueAsString(Entry.AbilityId), Entry.CustomType);
			}
		}
		UE_LOG(LogRTSUnitVariantSelectionCapture, Display, TEXT("RTS_VARIANT_SELECTION_CAPTURE preview=%s abilities=%s"),
		       *ActorIt->GetClass()->GetName(), *AbilityList);
		++PreviewCount;
	}
	return PreviewCount;
}

void UUnitVariantSelectionCaptureSubsystem::CheckPreviewClass(const bool bExpectVariant)
{
	int32 PreviewUnitCount = 0;
	int32 ExpectedVariantUnitCount = 0;
	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		// Only the commandable actor counts, so a squad preview is one unit and not one per squad member.
		const URTSComponent* RTSComponent = ActorIt->FindComponentByClass<URTSComponent>();
		if (Cast<ICommands>(*ActorIt) == nullptr || not IsValid(RTSComponent) || ActorIt->IsActorBeingDestroyed()
			|| ActorIt->ActorHasTag(UnitVariantSelectionCapturePrivate::ProbeActorTag))
		{
			continue;
		}
		const EUnitVariant ExpectedVariant = bExpectVariant
			                                     ? EUnitVariant::VariantA
			                                     : UnitVariantSelectionCapturePrivate::GetSavedVariant(
				                                     RTSComponent->GetUnitTrainingOption());
		++PreviewUnitCount;
		ExpectedVariantUnitCount += RTSComponent->GetUnitVariant() == ExpectedVariant ? 1 : 0;
		UE_LOG(LogRTSUnitVariantSelectionCapture, Display,
		       TEXT("RTS_VARIANT_SELECTION_CAPTURE unit=%s variant=%d expected_variant=%d"),
		       *ActorIt->GetClass()->GetName(), static_cast<int32>(RTSComponent->GetUnitVariant()),
		       static_cast<int32>(ExpectedVariant));
	}
	const FString Details = FString::Printf(TEXT("units=%d expected_variant_units=%d"), PreviewUnitCount,
	                                        ExpectedVariantUnitCount);
	const bool bShowsOnlyExpectedUnit = PreviewUnitCount == 1 && ExpectedVariantUnitCount == 1;
	Check(bShowsOnlyExpectedUnit, bExpectVariant ? TEXT("hangar_shows_variant_a") : TEXT("hangar_shows_saved_variant"),
	      Details);
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
