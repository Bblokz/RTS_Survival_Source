// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfoTakeoverValidateSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/GameUI/Archive/W_ArchiveItem/W_ArchiveItem.h"
#include "RTS_Survival/GameUI/Archive/W_ArchivePushNotification/W_ArchivePushNotification.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/InfoTakeover/InfoTakeoverSettings.h"
#include "RTS_Survival/GameUI/InfoTakeover/InfoTakeoverSubsystem.h"
#include "RTS_Survival/GameUI/InfoTakeover/VehicleInfoTakeoverContent.h"
#include "RTS_Survival/GameUI/InfoTakeover/W_InfoTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "UnrealClient.h"

namespace InfoTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("InfoTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double StartGameSeconds = 3.0;
	constexpr double VehicleSettleSeconds = 3.0;
	// Real seconds after a takeover opened: one capture while the title types, one while the description types.
	constexpr double TitleShotSeconds = 0.35;
	constexpr double DescriptionShotSeconds = 2.0;
	constexpr double DoneShotDelaySeconds = 0.6;
	constexpr double CloseDelaySeconds = 1.2;
	constexpr double TypingTimeoutSeconds = 20.0;
	constexpr double NextTakeoverTimeoutSeconds = 5.0;
	constexpr int32 TitleShotBit = 1;
	constexpr int32 DescriptionShotBit = 2;
	constexpr int32 DoneShotBit = 4;
	constexpr uint64 FramesBeforeClose = 3;
	constexpr int32 OngoingSoundSampleBit = 8;
	constexpr double OngoingSoundSampleSeconds = 5.0;
	// The enemy player, as missions spawn these vehicles.
	constexpr uint8 VehicleOwningPlayer = 2;
	const FVector VehicleLocation(0.f, 0.f, -50000.f);
	constexpr float VehicleSpacing = 5000.f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const LongTextLabel = TEXT("Layout_LongText");
	const TCHAR* const ShortTextLabel = TEXT("Layout_ShortText");
	const TCHAR* const FieldManualLabel = TEXT("FieldManual_Cover");
}

bool UInfoTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), InfoTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UInfoTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(EInfoTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UInfoTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EInfoTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EInfoTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EInfoTakeoverValidatePhase::SettlingVehicles:
		TickSettlingVehicles();
		break;
	case EInfoTakeoverValidatePhase::WatchingTakeovers:
		TickWatchingTakeovers();
		break;
	case EInfoTakeoverValidatePhase::DeferralLocked:
		TickDeferralLocked();
		break;
	case EInfoTakeoverValidatePhase::DeferralReleased:
		TickDeferralReleased();
		break;
	case EInfoTakeoverValidatePhase::ArchivePopups:
		TickArchivePopups();
		break;
	case EInfoTakeoverValidatePhase::ArchiveOpen:
		TickArchiveOpen();
		break;
	default:
		break;
	}
}

TStatId UInfoTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UInfoTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UInfoTakeoverValidateSubsystem::EnterPhase(const EInfoTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UInfoTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UInfoTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < InfoTakeoverValidateConstants::HudBuildSeconds)
	{
		return;
	}
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return;
	}
	UPlayerStartGameControl* const StartGameControl = M_PlayerController->FindComponentByClass<
		UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EInfoTakeoverValidatePhase::StartingGame);
}

void UInfoTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < InfoTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(true);
	}
	SpawnReferenceVehicles();
	EnterPhase(EInfoTakeoverValidatePhase::SettlingVehicles);
}

void UInfoTakeoverValidateSubsystem::SpawnReferenceVehicles()
{
	using namespace InfoTakeoverValidateConstants;
	const UDataTable* const Library = UInfoTakeoverSettings::Get()->VehicleLibrary.LoadSynchronous();
	if (not IsValid(Library))
	{
		Fail(TEXT("vehicle library did not load"));
		return;
	}
	int32 SpawnIndex = 0;
	for (const FName RowName : Library->GetRowNames())
	{
		const FVehicleInfoTakeoverEntry* const Entry = Library->FindRow<FVehicleInfoTakeoverEntry>(RowName, TEXT(""));
		UClass* const VehicleClass = Entry != nullptr ? Entry->ReferenceVehicleClass.LoadSynchronous() : nullptr;
		if (VehicleClass == nullptr)
		{
			Fail(FString::Printf(TEXT("row %s has no loadable ReferenceVehicleClass"), *RowName.ToString()));
			continue;
		}
		const FVector Location = VehicleLocation + FVector(VehicleSpacing * SpawnIndex++, 0.f, 0.f);
		ATankMaster* const Vehicle = Cast<ATankMaster>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*GetWorld(), VehicleClass, FTransform(Location), VehicleOwningPlayer));
		if (not IsValid(Vehicle))
		{
			Fail(FString::Printf(TEXT("could not spawn %s"), *VehicleClass->GetName()));
			continue;
		}
		Vehicle->SetActorHiddenInGame(true);
		if (UPrimitiveComponent* const RootPrimitive = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
		{
			RootPrimitive->SetSimulatePhysics(false);
		}
		M_Vehicles.Add(Vehicle);
		M_VehicleRowNames.Add(RowName);
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER spawned %d reference vehicles"), M_Vehicles.Num());
}

void UInfoTakeoverValidateSubsystem::TickSettlingVehicles()
{
	if (GetSecondsInPhase() < InfoTakeoverValidateConstants::VehicleSettleSeconds)
	{
		return;
	}
	LogAndQueueTakeovers();
	EnterPhase(EInfoTakeoverValidatePhase::WatchingTakeovers);
}

void UInfoTakeoverValidateSubsystem::LogAndQueueTakeovers()
{
	UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	if (InfoTakeovers == nullptr)
	{
		Fail(TEXT("no UInfoTakeoverSubsystem"));
		return;
	}
	for (int32 VehicleIndex = 0; VehicleIndex < M_Vehicles.Num(); ++VehicleIndex)
	{
		ATankMaster* const Vehicle = M_Vehicles[VehicleIndex];
		if (not IsValid(Vehicle))
		{
			continue;
		}
		const FName RowName = M_VehicleRowNames[VehicleIndex];
		LogVehicleContent(RowName, *Vehicle);
		if (InfoTakeovers->QueueVehicleTakeoverFromLibrary(Vehicle, RowName))
		{
			M_TakeoverLabels.Add(RowName.ToString());
		}
		else
		{
			Fail(FString::Printf(TEXT("queueing row %s returned false"), *RowName.ToString()));
		}
	}
	if (not M_Vehicles.IsEmpty() && IsValid(M_Vehicles[0]))
	{
		QueueCustomLayoutTakeovers(*M_Vehicles[0]);
		M_UnitTakeoverCount = M_TakeoverLabels.Num();
		QueueBehindNeverShowAgain(*InfoTakeovers, *M_Vehicles[0]);
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER queued %d takeovers; %d waiting behind the open one"),
	       M_TakeoverLabels.Num(), InfoTakeovers->GetQueuedTakeoverCount());
}

void UInfoTakeoverValidateSubsystem::LogVehicleContent(const FName RowName, const ATankMaster& Vehicle) const
{
	const FVehicleInfoTakeoverEntry* const Entry = UInfoTakeoverSubsystem::FindVehicleLibraryEntry(RowName);
	if (Entry == nullptr)
	{
		return;
	}
	const FVehicleInfoTakeoverContent Content = VehicleInfoTakeoverContent::BuildContent(
		Vehicle, *Entry, UUnitVariantPresentationLibrary::LoadTrainingOptionsTable());
	const FInfoTakeoverArmorSummary& Armor = Content.Armor;
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER content %s: name='%s' year='%s' image=%d chars=%d"),
	       *RowName.ToString(), *Content.DisplayName.ToString(), *Content.ProductionYear.ToString(),
	       Content.bHasImage ? 1 : 0, Content.Description.ToString().Len());
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER armor %s: front %.0f-%.0f (+%.0f) | side %s %.0f (+%.0f) | rear %s %.0f (+%.0f)"),
	       *RowName.ToString(), Armor.FrontMinMm, Armor.FrontMaxMm, Armor.FrontAddOnMaxMm,
	       *Armor.WeakestSide.PlateName.ToString(), Armor.WeakestSide.ArmorMm, Armor.WeakestSide.AddOnArmorMm,
	       *Armor.WeakestRear.PlateName.ToString(), Armor.WeakestRear.ArmorMm, Armor.WeakestRear.AddOnArmorMm);
	for (const FUnitVariantWeaponStats& Weapon : Content.Weapons)
	{
		UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER weapon %s: %dx %s (%s) cal %.0f dmg %.0f pen %.0f reload %.1f mag %d"),
		       *RowName.ToString(), Weapon.Count, *Weapon.DisplayName.ToString(), *Weapon.WeaponRowName.ToString(),
		       Weapon.CalibreMm, Weapon.Damage, Weapon.ArmorPen, Weapon.ReloadSeconds, Weapon.MagCapacity);
	}
}

void UInfoTakeoverValidateSubsystem::QueueCustomLayoutTakeovers(ATankMaster& Vehicle)
{
	using namespace InfoTakeoverValidateConstants;
	UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	FString LongDescription;
	const FString Paragraph = TEXT("Field report: enemy armour has been sighted along the river crossing, moving in "
		"loose columns with infantry riding on the engine decks. Expect them to stop at the tree line, turn their "
		"hulls toward our positions and fire from hull-down cover. Our scouts counted at least six vehicles, two "
		"of them towing ammunition sledges.\n\n");
	for (int32 RepeatIndex = 0; RepeatIndex < 4; ++RepeatIndex)
	{
		LongDescription += Paragraph;
	}
	FVehicleInfoTakeoverEntry LongEntry;
	LongEntry.DisplayName = FText::FromString(TEXT("A Very Long Vehicle Name To Test Wrapping Of The Title Line"));
	LongEntry.Description = FText::FromString(LongDescription.TrimEnd());
	LongEntry.ProductionYear = FText::FromString(TEXT("????"));
	if (InfoTakeovers->QueueVehicleTakeover(&Vehicle, LongEntry))
	{
		M_TakeoverLabels.Add(LongTextLabel);
	}
	FVehicleInfoTakeoverEntry ShortEntry;
	ShortEntry.Description = FText::FromString(TEXT("Small and fast."));
	if (InfoTakeovers->QueueVehicleTakeover(&Vehicle, ShortEntry))
	{
		M_TakeoverLabels.Add(ShortTextLabel);
	}
}

void UInfoTakeoverValidateSubsystem::QueueBehindNeverShowAgain(UInfoTakeoverSubsystem& InfoTakeovers,
                                                               ATankMaster& Vehicle)
{
	using namespace InfoTakeoverValidateConstants;
	if (InfoTakeovers.QueueCoverTakeover() == INDEX_NONE)
	{
		Fail(TEXT("the cover field manual could not be queued"));
		return;
	}
	M_TakeoverLabels.Add(FieldManualLabel);
	// Not in M_TakeoverLabels: "never show again" on the last unit takeover must drop it.
	FVehicleInfoTakeoverEntry DroppedEntry;
	DroppedEntry.Description = FText::FromString(TEXT("Queued behind never show again; must not open."));
	if (not InfoTakeovers.QueueVehicleTakeover(&Vehicle, DroppedEntry))
	{
		Fail(TEXT("the unit takeover behind the field manual could not be queued"));
	}
}

void UInfoTakeoverValidateSubsystem::TickWatchingTakeovers()
{
	using namespace InfoTakeoverValidateConstants;
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	UW_InfoTakeover* const ActiveTakeover = InfoTakeovers != nullptr ? InfoTakeovers->GetActiveTakeover() : nullptr;
	if (IsValid(ActiveTakeover))
	{
		if (ActiveTakeover != M_WatchedTakeover.Get())
		{
			StartWatching(*ActiveTakeover);
		}
		WatchActiveTakeover(*ActiveTakeover);
		return;
	}
	if (M_OpenedTakeoverCount >= M_TakeoverLabels.Num())
	{
		CheckNeverShowAgainAndStartDeferral();
		return;
	}
	if (GetSecondsInPhase() > NextTakeoverTimeoutSeconds)
	{
		Fail(FString::Printf(TEXT("no takeover opened; %d of %d seen"), M_OpenedTakeoverCount,
		                     M_TakeoverLabels.Num()));
		CheckNeverShowAgainAndStartDeferral();
	}
}

void UInfoTakeoverValidateSubsystem::StartWatching(UW_InfoTakeover& Takeover)
{
	M_WatchedTakeover = &Takeover;
	M_WatchedLabel = M_TakeoverLabels.IsValidIndex(M_OpenedTakeoverCount)
		                 ? M_TakeoverLabels[M_OpenedTakeoverCount]
		                 : FString::Printf(TEXT("Takeover%d"), M_OpenedTakeoverCount);
	++M_OpenedTakeoverCount;
	M_WatchedOpenSeconds = FPlatformTime::Seconds();
	M_WatchedDoneSeconds = 0.0;
	M_WatchedShotMask = 0;
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER opened %s (%d/%d) paused=%d"), *M_WatchedLabel,
	       M_OpenedTakeoverCount, M_TakeoverLabels.Num(), UGameplayStatics::IsGamePaused(this) ? 1 : 0);
	if (not UGameplayStatics::IsGamePaused(this))
	{
		Fail(FString::Printf(TEXT("%s opened without pausing the game"), *M_WatchedLabel));
	}
	const bool bIsUnitTakeover = M_OpenedTakeoverCount <= M_UnitTakeoverCount;
	if (Takeover.GetOffersNeverShowAgain() != bIsUnitTakeover)
	{
		Fail(FString::Printf(TEXT("%s: the never-show box must show on unit takeovers only (shown=%d)"),
		                     *M_WatchedLabel, Takeover.GetOffersNeverShowAgain() ? 1 : 0));
	}
	// The last unit takeover ticks "never show again", as the player would; the field manual behind it must still open.
	if (M_OpenedTakeoverCount == M_UnitTakeoverCount)
	{
		Takeover.SetNeverShowAgainChecked(true);
	}
}

void UInfoTakeoverValidateSubsystem::WatchActiveTakeover(UW_InfoTakeover& Takeover)
{
	using namespace InfoTakeoverValidateConstants;
	const double SecondsOpen = FPlatformTime::Seconds() - M_WatchedOpenSeconds;
	if (not(M_WatchedShotMask & TitleShotBit) && SecondsOpen >= TitleShotSeconds)
	{
		M_WatchedShotMask |= TitleShotBit;
		LogTakeoverState(Takeover, TEXT("title"));
		Capture(M_WatchedLabel + TEXT("_1_title"));
	}
	if (not(M_WatchedShotMask & DescriptionShotBit) && SecondsOpen >= DescriptionShotSeconds)
	{
		M_WatchedShotMask |= DescriptionShotBit;
		LogTakeoverState(Takeover, TEXT("description"));
		Capture(M_WatchedLabel + TEXT("_2_typing"));
	}
	// Longer descriptions chain the ongoing typing sound after the 2-3 s start sound.
	if (not(M_WatchedShotMask & OngoingSoundSampleBit) && SecondsOpen >= OngoingSoundSampleSeconds
		&& Takeover.GetTypingPhase() == EInfoTakeoverTypingPhase::Description)
	{
		M_WatchedShotMask |= OngoingSoundSampleBit;
		LogTakeoverState(Takeover, TEXT("late description"));
	}
	if (Takeover.GetTypingPhase() == EInfoTakeoverTypingPhase::Done && M_WatchedDoneSeconds <= 0.0)
	{
		M_WatchedDoneSeconds = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER %s typed in %.2f s (real time, game paused=%d)"),
		       *M_WatchedLabel, M_WatchedDoneSeconds - M_WatchedOpenSeconds,
		       UGameplayStatics::IsGamePaused(this) ? 1 : 0);
	}
	if (M_WatchedDoneSeconds <= 0.0 && SecondsOpen > TypingTimeoutSeconds)
	{
		Fail(FString::Printf(TEXT("%s never finished typing"), *M_WatchedLabel));
		M_WatchedDoneSeconds = FPlatformTime::Seconds();
	}
	if (M_WatchedDoneSeconds <= 0.0)
	{
		return;
	}
	const double SecondsDone = FPlatformTime::Seconds() - M_WatchedDoneSeconds;
	if (not(M_WatchedShotMask & DoneShotBit) && SecondsDone >= DoneShotDelaySeconds)
	{
		M_WatchedShotMask |= DoneShotBit;
		LogTakeoverState(Takeover, TEXT("done"));
		Capture(M_WatchedLabel + TEXT("_3_done"));
		M_DoneShotFrame = GFrameCounter;
	}
	// Close only once the done capture has rendered: the request is served on a later frame, and closing in the same
	// tick would capture the next takeover instead.
	if (M_WatchedShotMask & DoneShotBit && GFrameCounter > M_DoneShotFrame + FramesBeforeClose
		&& SecondsDone >= CloseDelaySeconds)
	{
		CloseWatchedTakeover(Takeover);
	}
}

void UInfoTakeoverValidateSubsystem::CloseWatchedTakeover(UW_InfoTakeover& Takeover)
{
	// The smoke tests clear overtakes the same way: by the function's name.
	UFunction* const ContinueFunction = Takeover.FindFunction(TEXT("OnClickedContinue"));
	if (ContinueFunction == nullptr)
	{
		Fail(TEXT("OnClickedContinue not found by name"));
		Takeover.OnClickedContinue();
	}
	else
	{
		Takeover.ProcessEvent(ContinueFunction, nullptr);
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER closed %s in viewport=%d paused=%d"), *M_WatchedLabel,
	       Takeover.IsInViewport() ? 1 : 0, UGameplayStatics::IsGamePaused(this) ? 1 : 0);
	M_WatchedTakeover = nullptr;
	EnterPhase(EInfoTakeoverValidatePhase::WatchingTakeovers);
}

void UInfoTakeoverValidateSubsystem::LogTakeoverState(const UW_InfoTakeover& Takeover, const TCHAR* Moment) const
{
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER state %s @%s: phase=%d sound='%s' paused=%d"), *M_WatchedLabel,
	       Moment, static_cast<int32>(Takeover.GetTypingPhase()), *Takeover.GetActiveTypingSoundName(),
	       UGameplayStatics::IsGamePaused(this) ? 1 : 0);
}

void UInfoTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / Folder / (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER capture %s"), *ScreenshotPath);
}

void UInfoTakeoverValidateSubsystem::CheckNeverShowAgainAndStartDeferral()
{
	if (M_Phase != EInfoTakeoverValidatePhase::WatchingTakeovers)
	{
		return;
	}
	if (UGameplayStatics::IsGamePaused(this))
	{
		Fail(TEXT("game still paused after the last takeover closed"));
	}
	if (UInfoTakeoverSubsystem::GetAreInfoTakeoversEnabled())
	{
		Fail(TEXT("\"never show again\" did not turn the setting off"));
	}
	UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	const bool bQueuedWhileOff = InfoTakeovers != nullptr && not M_Vehicles.IsEmpty()
		&& InfoTakeovers->QueueVehicleTakeover(M_Vehicles[0], FVehicleInfoTakeoverEntry());
	if (bQueuedWhileOff)
	{
		Fail(TEXT("a takeover was queued while the setting is off"));
	}
	if (M_OpenedTakeoverCount != M_TakeoverLabels.Num())
	{
		Fail(FString::Printf(TEXT("%d takeovers opened, expected %d: the unit takeover behind \"never show again\" "
			"must be dropped and the field manual kept"), M_OpenedTakeoverCount, M_TakeoverLabels.Num()));
	}
	if (InfoTakeovers != nullptr)
	{
		CheckFieldManualWhileTurnedOff(*InfoTakeovers);
	}
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(true);
		FRTSSettingsSnapshot PendingSettings;
		SettingsSubsystem->GetPendingSettingsValues(PendingSettings);
		if (not PendingSettings.M_GameplaySettings.bM_ShowInfoTakeovers)
		{
			Fail(TEXT("settings menu snapshot did not follow the switch"));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER never-show check done"));
	StartDeferralCheck();
}

void UInfoTakeoverValidateSubsystem::CheckFieldManualWhileTurnedOff(UInfoTakeoverSubsystem& InfoTakeovers)
{
	// Called with the setting off: a field manual queued now must still open, and closing it leaves the setting off.
	const int32 RequestId = InfoTakeovers.QueueCoverTakeover();
	UW_InfoTakeover* const FieldManual = InfoTakeovers.GetActiveTakeover();
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER field manual while off: request=%d opened=%d"), RequestId,
	       IsValid(FieldManual) ? 1 : 0);
	if (RequestId == INDEX_NONE || not IsValid(FieldManual))
	{
		Fail(TEXT("a field manual queued while unit intel is off did not open"));
		return;
	}
	FieldManual->OnClickedContinue();
	if (UInfoTakeoverSubsystem::GetAreInfoTakeoversEnabled() || IsValid(InfoTakeovers.GetActiveTakeover()))
	{
		Fail(TEXT("closing the field manual changed the setting or left a takeover open"));
	}
}

void UInfoTakeoverValidateSubsystem::StartDeferralCheck()
{
	UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	if (InfoTakeovers == nullptr || M_Vehicles.IsEmpty() || not M_PlayerController.IsValid())
	{
		Fail(TEXT("deferral check could not start"));
		RestoreSettingAndFinish();
		return;
	}
	// The lock tutorial overtakes and the tech tree hold while they own the screen.
	M_PlayerController->PauseAndLockGame(true);
	FVehicleInfoTakeoverEntry DeferredEntry;
	DeferredEntry.Description = FText::FromString(TEXT("Queued while the screen was locked."));
	if (not InfoTakeovers->QueueVehicleTakeover(M_Vehicles[0], DeferredEntry))
	{
		Fail(TEXT("deferred takeover was not queued"));
	}
	EnterPhase(EInfoTakeoverValidatePhase::DeferralLocked);
}

void UInfoTakeoverValidateSubsystem::TickDeferralLocked()
{
	constexpr double LockedHoldSeconds = 1.0;
	if (GetSecondsInPhase() < LockedHoldSeconds)
	{
		return;
	}
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	if (InfoTakeovers != nullptr && IsValid(InfoTakeovers->GetActiveTakeover()))
	{
		Fail(TEXT("a takeover opened while the screen was locked"));
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER deferral: nothing opened while locked; %d queued; unlocking"),
	       InfoTakeovers != nullptr ? InfoTakeovers->GetQueuedTakeoverCount() : -1);
	M_PlayerController->PauseAndLockGame(false);
	EnterPhase(EInfoTakeoverValidatePhase::DeferralReleased);
}

void UInfoTakeoverValidateSubsystem::TickDeferralReleased()
{
	constexpr double OpenTimeoutSeconds = 2.0;
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	UW_InfoTakeover* const ActiveTakeover = InfoTakeovers != nullptr ? InfoTakeovers->GetActiveTakeover() : nullptr;
	if (IsValid(ActiveTakeover))
	{
		UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER deferral: opened %.2f s after unlock, paused=%d"),
		       GetSecondsInPhase(), UGameplayStatics::IsGamePaused(this) ? 1 : 0);
		ActiveTakeover->OnClickedContinue();
		EnterPhase(EInfoTakeoverValidatePhase::ArchivePopups);
		return;
	}
	if (GetSecondsInPhase() > OpenTimeoutSeconds)
	{
		Fail(TEXT("deferred takeover did not open after unlock"));
		RestoreSettingAndFinish();
	}
}

void UInfoTakeoverValidateSubsystem::TickArchivePopups()
{
	constexpr double PopupSettleSeconds = 1.5;
	// The capture renders on a later frame; the archive opens only after it.
	constexpr double CaptureRenderSeconds = 1.0;
	if (GetSecondsInPhase() < PopupSettleSeconds)
	{
		return;
	}
	if (not bM_HasCapturedPopups)
	{
		bM_HasCapturedPopups = true;
		Capture(TEXT("Archive_1_popups"));
		return;
	}
	if (GetSecondsInPhase() < PopupSettleSeconds + CaptureRenderSeconds)
	{
		return;
	}
	TArray<UUserWidget*> Notifications;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Notifications, UW_ArchivePushNotification::StaticClass(),
	                                              false);
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER archive pop-ups alive: %d"), Notifications.Num());
	UMainGameUI* const MainGameUI = FRTS_Statics::GetMainGameUI(this);
	if (MainGameUI == nullptr)
	{
		Fail(TEXT("no main game UI to open the archive"));
		RestoreSettingAndFinish();
		return;
	}
	MainGameUI->OnOpenArchive();
	EnterPhase(EInfoTakeoverValidatePhase::ArchiveOpen);
}

void UInfoTakeoverValidateSubsystem::TickArchiveOpen()
{
	constexpr double ArchiveSettleSeconds = 1.5;
	constexpr double CaptureRenderSeconds = 1.0;
	if (GetSecondsInPhase() < ArchiveSettleSeconds)
	{
		return;
	}
	if (not bM_HasCapturedArchive)
	{
		bM_HasCapturedArchive = true;
		Capture(TEXT("Archive_2_open"));
		CheckArchiveEntries();
		return;
	}
	if (GetSecondsInPhase() >= ArchiveSettleSeconds + CaptureRenderSeconds)
	{
		RestoreSettingAndFinish();
	}
}

void UInfoTakeoverValidateSubsystem::CheckArchiveEntries()
{
	// One per distinct vehicle name: the six rows and the long-name layout test; the other custom takeovers fall back
	// to "T-26" and must not be archived twice. Plus one for the cover field manual (opened twice, archived once).
	constexpr int32 ExpectedCustomEntries = 8;
	TArray<UUserWidget*> ArchiveItems;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), ArchiveItems, UW_ArchiveItem::StaticClass(), false);
	int32 CustomEntryCount = 0;
	for (const UUserWidget* const Widget : ArchiveItems)
	{
		const UW_ArchiveItem* const ArchiveItem = Cast<UW_ArchiveItem>(Widget);
		if (not IsValid(ArchiveItem) || ArchiveItem->GetArchiveItem() != ERTSArchiveItem::CustomContent)
		{
			continue;
		}
		++CustomEntryCount;
		UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER archive entry %s"), *ArchiveItem->GetCustomContentKey().ToString());
	}
	if (CustomEntryCount != ExpectedCustomEntries)
	{
		Fail(FString::Printf(TEXT("%d custom archive entries, expected %d"), CustomEntryCount, ExpectedCustomEntries));
	}
}

void UInfoTakeoverValidateSubsystem::RestoreSettingAndFinish()
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bM_OriginalSettingValue);
	}
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER setting restored to %s"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
	Finish();
}

void UInfoTakeoverValidateSubsystem::DismissDifficultyPicker() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(InfoTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UInfoTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("INFO_TAKEOVER FAIL %s"), *Reason);
}

void UInfoTakeoverValidateSubsystem::Finish()
{
	EnterPhase(EInfoTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("INFO_TAKEOVER done: %d takeovers seen, %d failures -> %s"), M_OpenedTakeoverCount,
	       M_Failures, M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("InfoTakeoverValidate"));
}
