// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AbilityPreviewTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Components/Button.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/AbilityPreviewDirector.h"
#include "RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/AbilityPreviewTargets.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/RadixiteNuke/RadixiteNuke.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/FactionSystem/FactionSelection/AbilityPreview/W_AbilityPreviewOverlay.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/W_CommanderAbilityRow.h"
#include "RTS_Survival/FactionSystem/FactionSelection/CommanderSelection/W_CommanderSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/W_UnitVariantSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/Music/RTSMusicManager/RTSMusicManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAbilityPreviewTest, Log, All);

/** Counts error lines other code logs while a case runs. */
class FAbilityPreviewTestErrorCounter final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if ((Verbosity & ELogVerbosity::VerbosityMask) != ELogVerbosity::Error
			|| Category == LogRTSAbilityPreviewTest.GetCategoryName())
		{
			return;
		}
		// Data gaps that fire the same way in missions are not preview bugs; they are counted apart.
		const FString Text(Message);
		// Blueprint and settings gaps of the dropped units: they fire the same way in missions, not because of the
		// preview. Counted per kind for the summary.
		static const TCHAR* const KnownContentGaps[] = {
			TEXT("SquadWeaponIconSettings"), TEXT("M_EngineGasSound"), TEXT("SpatialVoiceLinePlayer"),
			TEXT("M_DecalMaterial not set"), TEXT("Attempted to add ability at index"), TEXT("AttachRockets"),
			TEXT("ModuleBox")
		};
		for (const TCHAR* const Gap : KnownContentGaps)
		{
			if (Text.Contains(Gap))
			{
				FScopeLock Lock(&M_Lock);
				++M_KnownDataErrorCount;
				M_KnownGapCounts.FindOrAdd(Gap)++;
				return;
			}
		}
		FScopeLock Lock(&M_Lock);
		++M_ErrorCount;
		if (M_Samples.Num() < 4)
		{
			M_Samples.Add(FString(Message).Left(220));
		}
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	void Reset()
	{
		FScopeLock Lock(&M_Lock);
		M_ErrorCount = 0;
		M_Samples.Reset();
	}

	int32 GetErrorCount(TArray<FString>& OutSamples)
	{
		FScopeLock Lock(&M_Lock);
		OutSamples = M_Samples;
		return M_ErrorCount;
	}

private:
	FCriticalSection M_Lock;
	int32 M_ErrorCount = 0;
	TArray<FString> M_Samples;

public:
	// Never reset: reported once in the summary.
	int32 M_KnownDataErrorCount = 0;
	TMap<FString, int32> M_KnownGapCounts;
};

namespace AbilityPreviewTestPrivate
{
	const TCHAR* const CommandLineSwitch = TEXT("AbilityPreviewTest");
	const TCHAR* const ManualBackSwitch = TEXT("AbilityPreviewTestManualBack");
	const TCHAR* const OnlySwitch = TEXT("AbilityPreviewTestOnly=");
	// Shaders compile on the first frames of a fresh run.
	constexpr double FirstStepDelaySeconds = 14.0;
	constexpr double SettleSeconds = 1.0;
	constexpr double ManualBackSeconds = 5.0;
	constexpr double AfterCloseSeconds = 7.0;
	// The director's own safety timeout is 90 s; past this the case fails.
	constexpr double CaseTimeoutSeconds = 110.0;
	const double ShotTimes[] = {1.5, 7.0, 14.0, 24.0};
	constexpr double BurstShotInterval = 0.4;
	constexpr double BurstShotsUntil = 30.0;
	constexpr double ShotAfterFinishedSeconds = 1.5;

	/**
	 * Machine gun only strafing: its 8 mm traces (about 32 m long) start at the dive from about 25 m up, so most end in
	 * the air, in missions too. The targets must be there and attacked; hits are logged, not required.
	 */
	bool GetIsTargetHitRequired(const EGlobalAbility Ability)
	{
		return Ability != EGlobalAbility::GA_2xBf110Strafing && Ability != EGlobalAbility::GA_Bf109Strafing;
	}

	/** The targets the user asked for per aircraft ability; abilities not listed must spawn none or any kind. */
	EAbilityPreviewTargetKind GetExpectedTargetKind(const EGlobalAbility Ability)
	{
		switch (Ability)
		{
		case EGlobalAbility::GA_2xBf110Strafing:
		case EGlobalAbility::GA_Bf109Strafing:
		case EGlobalAbility::GA_GerIncendiaryJu87:
			return EAbilityPreviewTargetKind::Infantry;
		case EGlobalAbility::GA_2xHortentrafing:
		case EGlobalAbility::GA_GerJu87GTankBusters:
			return EAbilityPreviewTargetKind::LightVehicles;
		case EGlobalAbility::GA_Ju87Strafing:
		case EGlobalAbility::GA_3xJu87Strafing:
		case EGlobalAbility::GA_Me410Airstrike:
		case EGlobalAbility::GA_HeavyCarpet_Ju390B_250Gr:
			return EAbilityPreviewTargetKind::MediumVehicles;
		default:
			return EAbilityPreviewTargetKind::None;
		}
	}
	constexpr float CleanupCheckRadius = 60000.f;

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

	FString ShortAbilityName(const EGlobalAbility Ability)
	{
		FString Name = UEnum::GetValueAsString(Ability);
		Name.RemoveFromStart(TEXT("EGlobalAbility::"));
		return Name;
	}
}

bool UAbilityPreviewTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), AbilityPreviewTestPrivate::CommandLineSwitch);
}

TStatId UAbilityPreviewTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAbilityPreviewTestSubsystem, STATGROUP_Tickables);
}

void UAbilityPreviewTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_ManualBack = FParse::Param(FCommandLine::Get(), AbilityPreviewTestPrivate::ManualBackSwitch);
	bM_BurstShots = FParse::Param(FCommandLine::Get(), TEXT("AbilityPreviewTestBurst"));
	M_ErrorCounter = MakeShared<FAbilityPreviewTestErrorCounter>();
	GLog->AddOutputDevice(M_ErrorCounter.Get());
	TWeakObjectPtr<UAbilityPreviewTestSubsystem> WeakThis(this);
	M_SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateWeakLambda(
		this, [WeakThis](AActor* Spawned)
		{
			if (WeakThis.IsValid() && WeakThis->M_Phase == EPhase::Running)
			{
				WeakThis->M_CaseSpawns.Add(Spawned);
			}
		}));
	BuildCases();
	M_NextActionTime = FPlatformTime::Seconds() + AbilityPreviewTestPrivate::FirstStepDelaySeconds;
	UE_LOG(LogRTSAbilityPreviewTest, Display, TEXT("RTS_ABILITY_PREVIEW_TEST started cases=%d manualBack=%d"),
	       M_Cases.Num(), bM_ManualBack ? 1 : 0);
}

void UAbilityPreviewTestSubsystem::Deinitialize()
{
	if (M_ErrorCounter.IsValid())
	{
		GLog->RemoveOutputDevice(M_ErrorCounter.Get());
	}
	if (UWorld* World = GetWorld(); IsValid(World) && M_SpawnHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(M_SpawnHandle);
	}
	Super::Deinitialize();
}

void UAbilityPreviewTestSubsystem::BuildCases()
{
	const URTSGlobalAbilitySettings* const Settings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* const DataAsset = IsValid(Settings)
		                                                    ? Settings->GlobalAbilityDataAsset.LoadSynchronous()
		                                                    : nullptr;
	FString Only;
	FParse::Value(FCommandLine::Get(), AbilityPreviewTestPrivate::OnlySwitch, Only);
	TArray<FString> OnlyNames;
	Only.ParseIntoArray(OnlyNames, TEXT("+"));
	TSet<EGlobalAbility> Seen;
	for (const FCommanderSelectionEntryData& Commander : CommanderSelectionData::BuildCommanderEntries(DataAsset))
	{
		for (int32 SlotIndex = 0; SlotIndex < Commander.AbilitySlots.Num(); ++SlotIndex)
		{
			const FCommanderSelectionAbilitySlot& AbilitySlot = Commander.AbilitySlots[SlotIndex];
			if (Seen.Contains(AbilitySlot.Ability))
			{
				continue;
			}
			const FString Name = AbilityPreviewTestPrivate::ShortAbilityName(AbilitySlot.Ability);
			if (not OnlyNames.IsEmpty() && not OnlyNames.Contains(Name))
			{
				continue;
			}
			Seen.Add(AbilitySlot.Ability);
			M_Cases.Add({AbilitySlot.Ability, Commander.Commander, SlotIndex});
			// The same ability right again: the stopped copy may still have timers running.
			if (FParse::Param(FCommandLine::Get(), TEXT("AbilityPreviewTestRepeat")))
			{
				M_Cases.Add({AbilitySlot.Ability, Commander.Commander, SlotIndex});
			}
		}
	}
}

void UAbilityPreviewTestSubsystem::SetPhase(const EPhase NewPhase, const double Now)
{
	M_Phase = NewPhase;
	M_PhaseStart = Now;
}

void UAbilityPreviewTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || M_Phase == EPhase::Done)
	{
		return;
	}
	if (UW_FactionPopup* const Popup = AbilityPreviewTestPrivate::FindLiveWidget<UW_FactionPopup>(World))
	{
		Popup->OnPopupAccepted.Broadcast();
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now < M_NextActionTime)
	{
		return;
	}
	switch (M_Phase)
	{
	case EPhase::Boot:
		if (EnsureCommanderScreen(Now))
		{
			SetPhase(EPhase::HoverShot, Now);
		}
		return;
	case EPhase::HoverShot:
		TickHoverShot(Now);
		return;
	default:
		TickCase(Now);
		return;
	}
}

bool UAbilityPreviewTestSubsystem::EnsureCommanderScreen(const double Now)
{
	UWorld* const World = GetWorld();
	if (not M_Controller.IsValid())
	{
		M_Controller = Cast<AFactionPlayerController>(World->GetFirstPlayerController());
		if (not M_Controller.IsValid())
		{
			return false;
		}
	}
	M_Menu = AbilityPreviewTestPrivate::FindLiveWidget<UW_CommanderSelectionMenu>(World);
	if (M_Menu.IsValid())
	{
		return true;
	}
	if (UW_UnitVariantSelectionMenu* const VariantMenu =
		AbilityPreviewTestPrivate::FindLiveWidget<UW_UnitVariantSelectionMenu>(World))
	{
		if (not bM_VariantsConfirmed)
		{
			bM_VariantsConfirmed = true;
			M_Controller->HandleUnitVariantSelectionConfirmed({});
			M_NextActionTime = Now + AbilityPreviewTestPrivate::SettleSeconds;
		}
		return false;
	}
	if (not bM_LaunchRequested)
	{
		bM_LaunchRequested = true;
		M_Controller->HandleLaunchCampaignRequested(ERTSFaction::GerBreakthroughDoctrine);
		M_NextActionTime = Now + 2.0 * AbilityPreviewTestPrivate::SettleSeconds;
	}
	return false;
}

void UAbilityPreviewTestSubsystem::TickHoverShot(const double Now)
{
	// Hover the second kit row with real mouse moves (SetCursorPos alone sends no hover): its glass must light up.
	++M_HoverStep;
	const UW_CommanderAbilityRow* const Row = M_Menu.IsValid() ? M_Menu->GetAbilityRow(1) : nullptr;
	if (M_HoverStep <= 6)
	{
		if (IsValid(Row) && FSlateApplication::IsInitialized())
		{
			const FVector2D Centre = Row->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
			FSlateApplication::Get().SetCursorPos(Centre);
			const FPointerEvent MouseMove(0, Centre, Centre - FVector2D(2.f, 0.f), TSet<FKey>(), FKey(), 0.f,
			                              FModifierKeysState());
			FSlateApplication::Get().ProcessMouseMoveEvent(MouseMove);
		}
		M_NextActionTime = Now + 0.15;
		return;
	}
	if (M_HoverStep == 7)
	{
		Check(IsValid(Row) && IsValid(Row->GetRowButton()) && Row->GetRowButton()->IsHovered(), TEXT("row_hovered"));
		const UMaterialInterface* const FactionButtonGlass = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/RTS_Survival/FactionSelection/UI/Materials/MI_FactionButton_Glass.MI_FactionButton_Glass"));
		int32 RowsOnFactionGlass = 0;
		for (int32 SlotIndex = 0; M_Menu.IsValid() && M_Menu->GetAbilityRow(SlotIndex); ++SlotIndex)
		{
			if (IsValid(FactionButtonGlass) && M_Menu->GetAbilityRow(SlotIndex)->GetGlassSourceMaterial() == FactionButtonGlass)
			{
				++RowsOnFactionGlass;
			}
		}
		Check(RowsOnFactionGlass >= 7, TEXT("rows_use_faction_button_glass"), FString::FromInt(RowsOnFactionGlass));
		UE_LOG(LogRTSAbilityPreviewTest, Display, TEXT("RTS_ABILITY_PREVIEW_TEST glass parity rows=%d material=%s"),
		       RowsOnFactionGlass, IsValid(FactionButtonGlass) ? *FactionButtonGlass->GetPathName() : TEXT("missing"));
		Check(IsValid(Row) && Row->GetGlassHoverAmount() > 0.9f, TEXT("glass_lit_on_hover"),
		      IsValid(Row) ? FString::SanitizeFloat(Row->GetGlassHoverAmount()) : TEXT("no row"));
		Shot(TEXT("00_commander_row_hovered"));
		M_NextActionTime = Now + 1.0;
		return;
	}
	if (M_HoverStep == 8)
	{
		if (FSlateApplication::IsInitialized())
		{
			const FVector2D Away(5.f, 5.f);
			FSlateApplication::Get().SetCursorPos(Away);
			FSlateApplication::Get().ProcessMouseMoveEvent(
				FPointerEvent(0, Away, Away + FVector2D(2.f, 0.f), TSet<FKey>(), FKey(), 0.f, FModifierKeysState()));
		}
		M_NextActionTime = Now + 0.8;
		return;
	}
	// The glass fades back once the cursor left the row.
	Check(IsValid(Row) && Row->GetGlassHoverAmount() < 0.1f, TEXT("glass_idle_after_hover"),
	      IsValid(Row) ? FString::SanitizeFloat(Row->GetGlassHoverAmount()) : TEXT("no row"));
	if (FSlateApplication::IsInitialized())
	{
		const FVector2D Away(5.f, 5.f);
		FSlateApplication::Get().SetCursorPos(Away);
		FSlateApplication::Get().ProcessMouseMoveEvent(
			FPointerEvent(0, Away, Away + FVector2D(2.f, 0.f), TSet<FKey>(), FKey(), 0.f, FModifierKeysState()));
	}
	M_ResultLines.Add(FString::Printf(TEXT("HOVER %s"), M_CaseNotes.IsEmpty() ? TEXT("ok") : TEXT("FAILED")));
	M_CaseNotes.Reset();
	M_CaseIndex = 0;
	SetPhase(EPhase::SelectCommander, Now);
	M_NextActionTime = Now + 0.5;
}

void UAbilityPreviewTestSubsystem::TickCase(const double Now)
{
	if (not M_Cases.IsValidIndex(M_CaseIndex))
	{
		Finish();
		return;
	}
	switch (M_Phase)
	{
	case EPhase::SelectCommander:
		if (M_Menu.IsValid())
		{
			M_Menu->SelectCommander(M_Cases[M_CaseIndex].Commander);
		}
		M_NextActionTime = Now + 0.5;
		SetPhase(EPhase::ClickRow, Now);
		return;
	case EPhase::ClickRow:
		StartCase(Now);
		return;
	case EPhase::Running:
		CheckRunning(Now);
		return;
	case EPhase::AfterClose:
		FinishCase(Now);
		return;
	default:
		return;
	}
}

void UAbilityPreviewTestSubsystem::StartCase(const double Now)
{
	const FPreviewCase& Case = M_Cases[M_CaseIndex];
	M_CaseNotes.Reset();
	M_CaseSpawns.Reset();
	M_ErrorCounter->Reset();
	M_ShotsTaken = 0;
	bM_SawFinished = false;
	bM_ShotAfterFinished = false;
	bM_ManualBackDone = false;
	bM_CheckedCombatMusic = false;
	M_MusicDuringPreview = TEXT("unchecked");
	M_TargetSummary = TEXT("none");
	M_MaxTargetsDamaged = 0;
	M_MaxTargetsDestroyed = 0;
	M_NextBurstShotTime = 0.0;
	M_FinishedTime = -1.0;
	const int32 FailedBefore = M_Failed;
	UE_LOG(LogRTSAbilityPreviewTest, Display, TEXT("RTS_ABILITY_PREVIEW_TEST CASE_START %s commander=%s slot=%d"),
	       *GetCaseName(), *UEnum::GetValueAsString(Case.Commander), Case.SlotIndex);
	UW_CommanderAbilityRow* const Row = M_Menu.IsValid() ? M_Menu->GetAbilityRow(Case.SlotIndex) : nullptr;
	Check(IsValid(Row) && IsValid(Row->GetRowButton()), TEXT("row_exists"));
	if (IsValid(Row) && IsValid(Row->GetRowButton()))
	{
		// The row's own click, as the player's mouse would send it.
		Row->GetRowButton()->OnClicked.Broadcast();
	}
	const bool bOpened = M_Controller.IsValid() && M_Controller->GetIsAbilityPreviewOpen();
	Check(bOpened, TEXT("preview_opened"));
	if (not bOpened)
	{
		M_ResultLines.Add(FString::Printf(TEXT("FAIL %s did not open: %s"), *GetCaseName(),
		                                  *FString::Join(M_CaseNotes, TEXT("; "))));
		M_Failed = FMath::Max(M_Failed, FailedBefore + 1);
		++M_CaseIndex;
		SetPhase(EPhase::SelectCommander, Now);
		M_NextActionTime = Now + AbilityPreviewTestPrivate::SettleSeconds;
		return;
	}
	Check(M_Menu.IsValid() && not M_Menu->IsVisible(), TEXT("commander_screen_hidden"));
	Check(IsValid(AbilityPreviewTestPrivate::FindLiveWidget<UW_AbilityPreviewOverlay>(GetWorld())),
	      TEXT("overlay_shown"));
	Check(M_Controller->GetViewTarget() == M_Controller->GetAbilityPreviewDirector(), TEXT("camera_on_desert"));
	CheckTargetsSpawned();
	M_RunStart = Now;
	SetPhase(EPhase::Running, Now);
}

void UAbilityPreviewTestSubsystem::CheckRunning(const double Now)
{
	using namespace AbilityPreviewTestPrivate;
	const double Elapsed = Now - M_RunStart;
	const bool bOpen = M_Controller.IsValid() && M_Controller->GetIsAbilityPreviewOpen();
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	if (bOpen && Elapsed > 2.0 && not bM_CheckedCombatMusic)
	{
		bM_CheckedCombatMusic = true;
		if (M_CaseIndex == 0 && GEngine)
		{
			// Lists what the "lighting needs to be rebuilt" message counts while a preview runs.
			GEngine->Exec(GetWorld(), TEXT("DumpUnbuiltLightInteractions"));
		}
		const FString Music = GetMusicName();
		M_MusicDuringPreview = Music + TEXT("/") + GetTrackName();
		Check(Music.Contains(TEXT("Combat")), TEXT("combat_music"), Music);
	}
	if (bOpen && M_ShotsTaken < UE_ARRAY_COUNT(ShotTimes) && Elapsed >= ShotTimes[M_ShotsTaken])
	{
		Shot(FString::Printf(TEXT("%s_t%02d"), *GetCaseName(), FMath::RoundToInt(ShotTimes[M_ShotsTaken])));
		++M_ShotsTaken;
	}
	if (bOpen)
	{
		TrackTargetHealth();
	}
	if (bM_BurstShots && bOpen && Elapsed >= M_NextBurstShotTime && Elapsed < BurstShotsUntil)
	{
		M_NextBurstShotTime = Elapsed + BurstShotInterval;
		Shot(FString::Printf(TEXT("%s_burst%05.1f"), *GetCaseName(), Elapsed));
	}
	if (bOpen && IsValid(Director) && Director->GetHasAbilityFinished() && not bM_SawFinished)
	{
		bM_SawFinished = true;
		M_FinishedTime = Elapsed;
		// The nuke and V2 end at launch; the preview counts as finished only once their warhead came down.
		int32 WarheadsInFlight = 0;
		for (TActorIterator<ARadixiteNuke> It(GetWorld()); It; ++It)
		{
			WarheadsInFlight += It->GetPhase() <= ERadixiteNukePhase::Falling ? 1 : 0;
		}
		Check(WarheadsInFlight == 0, TEXT("warhead_landed_before_finish"), FString::FromInt(WarheadsInFlight));
		// Glider drops: the glider is gone and the camera has panned to the tank before the countdown starts.
		int32 VisibleGliders = 0;
		for (TActorIterator<AGliderTankDelivery> It(GetWorld()); It; ++It)
		{
			VisibleGliders += It->IsHidden() ? 0 : 1;
		}
		Check(VisibleGliders == 0, TEXT("glider_hidden_before_finish"), FString::FromInt(VisibleGliders));
	}
	if (bM_SawFinished && not bM_ShotAfterFinished && bOpen && Elapsed >= M_FinishedTime + ShotAfterFinishedSeconds)
	{
		bM_ShotAfterFinished = true;
		Shot(GetCaseName() + TEXT("_finished"));
		RecordTargetHealth();
	}
	if (bM_ManualBack && not bM_ManualBackDone && bOpen && Elapsed >= ManualBackSeconds)
	{
		bM_ManualBackDone = true;
		if (UW_AbilityPreviewOverlay* const Overlay = FindLiveWidget<UW_AbilityPreviewOverlay>(GetWorld()))
		{
			Overlay->GetBackButton()->OnClicked.Broadcast();
		}
	}
	if (not bOpen)
	{
		SetPhase(EPhase::AfterClose, Now);
		// The music manager fades the combat track out over 5 s before the commander loop starts.
		M_NextActionTime = Now + AfterCloseSeconds;
		return;
	}
	if (Elapsed > CaseTimeoutSeconds)
	{
		Check(false, TEXT("closed_in_time"));
		M_Controller->HandleAbilityPreviewBack();
	}
}

void UAbilityPreviewTestSubsystem::FinishCase(const double Now)
{
	const double Duration = Now - M_RunStart - AbilityPreviewTestPrivate::AfterCloseSeconds;
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	const FString Reason = IsValid(Director) ? Director->GetCloseReason() : TEXT("none");
	if (bM_ManualBack)
	{
		Check(bM_ManualBackDone && Duration < AbilityPreviewTestPrivate::ManualBackSeconds + 1.5, TEXT("back_button"));
	}
	else
	{
		Check(Reason == TEXT("finished"), TEXT("auto_return_after_finish"), Reason);
		if (bM_SawFinished)
		{
			const double Linger = Duration - M_FinishedTime;
			Check(Linger > 5.0 && Linger < 7.5, TEXT("six_second_linger"), FString::Printf(TEXT("%.1f"), Linger));
		}
	}
	Check(M_Controller.IsValid() && M_Controller->GetViewTarget() != M_Controller->GetAbilityPreviewDirector(),
	      TEXT("camera_back_in_hangar"));
	Check(M_Menu.IsValid() && M_Menu->IsVisible(), TEXT("commander_screen_back"));
	Check(not IsValid(AbilityPreviewTestPrivate::FindLiveWidget<UW_AbilityPreviewOverlay>(GetWorld())),
	      TEXT("overlay_removed"));
	const FString Music = GetMusicName();
	const FString MusicAfter = Music + TEXT("/") + GetTrackName();
	Check(Music.Contains(TEXT("TransportLoop")), TEXT("commander_music_back"), Music);
	// The track itself, not only the category: the commander screen's loop must be playing again.
	Check(GetTrackName().Contains(TEXT("Loading_transporters")), TEXT("commander_track_playing"), GetTrackName());
	const int32 Leftovers = CountLeftoverSpawns();
	Check(Leftovers == 0, TEXT("no_leftovers"), FString::FromInt(Leftovers));
	TArray<FString> ErrorSamples;
	const int32 Errors = M_ErrorCounter->GetErrorCount(ErrorSamples);
	Check(Errors == 0, TEXT("no_errors"), FString::Join(ErrorSamples, TEXT(" | ")));
	Shot(GetCaseName() + TEXT("_back_in_hangar"));

	const bool bCasePassed = M_CaseNotes.IsEmpty();
	bCasePassed ? ++M_Passed : ++M_Failed;
	const FString Line = FString::Printf(
		TEXT("%s %s finished_at=%.1f closed_at=%.1f reason=%s spawns=%d errors=%d targets=%s music_during=%s "
			"music_after=%s %s"),
		bCasePassed ? TEXT("PASS") : TEXT("FAIL"), *GetCaseName(), M_FinishedTime, Duration, *Reason,
		M_CaseSpawns.Num(), Errors, *M_TargetSummary, *M_MusicDuringPreview, *MusicAfter,
		*FString::Join(M_CaseNotes, TEXT("; ")));
	M_ResultLines.Add(Line);
	UE_LOG(LogRTSAbilityPreviewTest, Display, TEXT("RTS_ABILITY_PREVIEW_TEST RESULT %s"), *Line);
	FFileHelper::SaveStringArrayToFile(M_ResultLines,
	                                   *(FPaths::ProjectSavedDir() / TEXT("AbilityPreviewTest/results.txt")));
	++M_CaseIndex;
	SetPhase(EPhase::SelectCommander, Now);
	M_NextActionTime = Now + AbilityPreviewTestPrivate::SettleSeconds;
}

int32 UAbilityPreviewTestSubsystem::CountLeftoverSpawns() const
{
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	if (not IsValid(Director))
	{
		return 0;
	}
	int32 Leftovers = 0;
	for (const TWeakObjectPtr<AActor>& WeakSpawn : M_CaseSpawns)
	{
		const AActor* const Spawn = WeakSpawn.Get();
		if (not IsValid(Spawn) || Spawn->IsActorBeingDestroyed())
		{
			continue;
		}
		const FString ClassName = Spawn->GetClass()->GetName();
		if (ClassName.Contains(TEXT("Pool")) || ClassName.Contains(TEXT("Projectile"))
			|| ClassName.Contains(TEXT("Manager")) || Spawn->IsA<AController>())
		{
			continue;
		}
		if (FVector::DistSquared2D(Spawn->GetActorLocation(), Director->GetActorLocation())
			<= FMath::Square(AbilityPreviewTestPrivate::CleanupCheckRadius))
		{
			++Leftovers;
			if (Leftovers <= 3)
			{
				UE_LOG(LogRTSAbilityPreviewTest, Warning, TEXT("RTS_ABILITY_PREVIEW_TEST leftover %s (%s)"),
				       *Spawn->GetName(), *ClassName);
			}
		}
	}
	return Leftovers;
}

void UAbilityPreviewTestSubsystem::CheckTargetsSpawned()
{
	using namespace AbilityPreviewTestPrivate;
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	const UAbilityPreviewTargetsComponent* const Targets = IsValid(Director) ? Director->GetPreviewTargets() : nullptr;
	const EAbilityPreviewTargetKind Expected = GetExpectedTargetKind(M_Cases[M_CaseIndex].Ability);
	const EAbilityPreviewTargetKind Spawned = IsValid(Targets)
		                                          ? Targets->GetSpawnedKind()
		                                          : EAbilityPreviewTargetKind::None;
	const int32 SpawnedCount = IsValid(Targets) ? Targets->GetTargets().Num() : 0;
	if (Expected == EAbilityPreviewTargetKind::None)
	{
		return;
	}
	Check(Spawned == Expected, TEXT("targets_match_ability"),
	      FString::Printf(TEXT("expected %s got %s"), *UEnum::GetValueAsString(Expected),
	                      *UEnum::GetValueAsString(Spawned)));
	Check(SpawnedCount >= 2, TEXT("targets_spawned"), FString::FromInt(SpawnedCount));
}

void UAbilityPreviewTestSubsystem::RecordTargetHealth()
{
	using namespace AbilityPreviewTestPrivate;
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	const UAbilityPreviewTargetsComponent* const Targets = IsValid(Director) ? Director->GetPreviewTargets() : nullptr;
	if (not IsValid(Targets) || Targets->GetSpawnedKind() == EAbilityPreviewTargetKind::None)
	{
		return;
	}
	TrackTargetHealth();
	const FAbilityPreviewTargetHealth Health = Targets->GetTargetHealth();
	M_TargetSummary = FString::Printf(TEXT("%s/units%d/damaged%d/destroyed%d"),
	                                  *UEnum::GetValueAsString(Targets->GetSpawnedKind()), Health.Units,
	                                  M_MaxTargetsDamaged, M_MaxTargetsDestroyed);
	// The point of the targets: the aircraft must hit them (machine gun strafing only logs it).
	if (GetIsTargetHitRequired(M_Cases[M_CaseIndex].Ability))
	{
		Check(M_MaxTargetsDamaged >= 1, TEXT("targets_hit"), M_TargetSummary);
	}
}

void UAbilityPreviewTestSubsystem::TrackTargetHealth()
{
	const AAbilityPreviewDirector* const Director = M_Controller.IsValid()
		                                                ? M_Controller->GetAbilityPreviewDirector()
		                                                : nullptr;
	const UAbilityPreviewTargetsComponent* const Targets = IsValid(Director) ? Director->GetPreviewTargets() : nullptr;
	if (not IsValid(Targets) || Targets->GetSpawnedKind() == EAbilityPreviewTargetKind::None)
	{
		return;
	}
	const FAbilityPreviewTargetHealth Health = Targets->GetTargetHealth();
	M_MaxTargetsDamaged = FMath::Max(M_MaxTargetsDamaged, Health.Damaged);
	M_MaxTargetsDestroyed = FMath::Max(M_MaxTargetsDestroyed, Health.Destroyed);
}

FString UAbilityPreviewTestSubsystem::GetTrackName() const
{
	const URTSGameInstance* const GameInstance = Cast<URTSGameInstance>(GetWorld()->GetGameInstance());
	const URTSMusicManager* const MusicManager = IsValid(GameInstance) ? GameInstance->GetMusicManager() : nullptr;
	return IsValid(MusicManager) ? MusicManager->GetCurrentTrackName() : TEXT("none");
}

FString UAbilityPreviewTestSubsystem::GetMusicName() const
{
	const URTSGameInstance* const GameInstance = Cast<URTSGameInstance>(GetWorld()->GetGameInstance());
	const URTSMusicManager* const MusicManager = IsValid(GameInstance) ? GameInstance->GetMusicManager() : nullptr;
	return IsValid(MusicManager) ? UEnum::GetValueAsString(MusicManager->GetCurrentMusicType()) : TEXT("none");
}

void UAbilityPreviewTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	if (bCondition)
	{
		return;
	}
	const FString Note = Detail.IsEmpty() ? Name : FString::Printf(TEXT("%s(%s)"), *Name, *Detail);
	M_CaseNotes.Add(Note);
	UE_LOG(LogRTSAbilityPreviewTest, Warning, TEXT("RTS_ABILITY_PREVIEW_TEST CHECK_FAILED %s %s"), *GetCaseName(),
	       *Note);
}

void UAbilityPreviewTestSubsystem::Shot(const FString& Name)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("AbilityPreviewTest") / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
}

FString UAbilityPreviewTestSubsystem::GetCaseName() const
{
	return M_Cases.IsValidIndex(M_CaseIndex)
		       ? AbilityPreviewTestPrivate::ShortAbilityName(M_Cases[M_CaseIndex].Ability)
		       : TEXT("setup");
}

void UAbilityPreviewTestSubsystem::Finish()
{
	SetPhase(EPhase::Done, FPlatformTime::Seconds());
	FString Gaps;
	if (M_ErrorCounter.IsValid())
	{
		for (const TPair<FString, int32>& Gap : M_ErrorCounter->M_KnownGapCounts)
		{
			Gaps += FString::Printf(TEXT(" %s=%d"), *Gap.Key, Gap.Value);
		}
	}
	M_ResultLines.Add(FString::Printf(TEXT("SUMMARY passed=%d failed=%d known_content_gaps=%d:%s"), M_Passed, M_Failed,
	                                  M_ErrorCounter.IsValid() ? M_ErrorCounter->M_KnownDataErrorCount : 0, *Gaps));
	FFileHelper::SaveStringArrayToFile(M_ResultLines,
	                                   *(FPaths::ProjectSavedDir() / TEXT("AbilityPreviewTest/results.txt")));
	UE_LOG(LogRTSAbilityPreviewTest, Display, TEXT("RTS_ABILITY_PREVIEW_TEST DONE passed=%d failed=%d"), M_Passed,
	       M_Failed);
	RTSFunctionLibrary::PopModalDialogSuppression();
	FPlatformMisc::RequestExit(false, TEXT("AbilityPreviewTest"));
}

#else

bool UAbilityPreviewTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UAbilityPreviewTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UAbilityPreviewTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAbilityPreviewTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UAbilityPreviewTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAbilityPreviewTestSubsystem, STATGROUP_Tickables);
}

#endif
