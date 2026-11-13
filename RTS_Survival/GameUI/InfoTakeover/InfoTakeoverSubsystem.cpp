// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfoTakeoverSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "InfoTakeoverSettings.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "VehicleInfoTakeoverContent.h"
#include "W_BxpPackUpTakeover.h"
#include "W_CameraControlsTakeover.h"
#include "W_CoverTakeover.h"
#include "W_NomadicConversionTakeover.h"
#include "W_SpacedArmourTakeover.h"
#include "W_VehicleInfoTakeover.h"

namespace InfoTakeoverSubsystemPrivate
{
	// How often a waiting takeover checks whether the screen is free, in real seconds.
	constexpr float RetryIntervalSeconds = 0.25f;
	// Matches the missions' AddArchiveItem default.
	constexpr float ArchiveNotificationSeconds = 20.f;
	constexpr int32 ArchiveSortingPriority = 0;
	const TCHAR* const VehicleArchiveKeyPrefix = TEXT("InfoTakeover.Vehicle.");
	const FName CameraControlsArchiveKey(TEXT("InfoTakeover.CameraControls"));
	const FName CoverArchiveKey(TEXT("InfoTakeover.Cover"));
	const FName SpacedArmourArchiveKey(TEXT("InfoTakeover.SpacedArmour"));
	const FName NomadicConversionArchiveKey(TEXT("InfoTakeover.NomadicConversion"));
	const FName BxpPackUpArchiveKey(TEXT("InfoTakeover.BxpPackUp"));
}

bool UInfoTakeoverSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UInfoTakeoverSubsystem::Deinitialize()
{
	StopRetryTicker();
	// Waiting callers are released without continuing their mission; the world is going away.
	const TArray<FQueuedInfoTakeover> CancelledTakeovers = MoveTemp(M_QueuedTakeovers);
	M_QueuedTakeovers.Reset();
	const bool bHadActiveTakeover = IsValid(M_ActiveTakeover);
	M_ActiveTakeover = nullptr;
	if (bHadActiveTakeover)
	{
		BroadcastRequestFinished(M_ActiveTakeoverContent, EInfoTakeoverRequestResult::Cancelled);
	}
	for (const FQueuedInfoTakeover& CancelledTakeover : CancelledTakeovers)
	{
		BroadcastRequestFinished(CancelledTakeover, EInfoTakeoverRequestResult::Cancelled);
	}
	OnTakeoverRequestFinished.Clear();
	Super::Deinitialize();
}

bool UInfoTakeoverSubsystem::GetAreInfoTakeoversEnabled()
{
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	return GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
}

TArray<FName> UInfoTakeoverSubsystem::GetVehicleLibraryEntryNames()
{
	const UDataTable* const Library = UInfoTakeoverSettings::Get()->VehicleLibrary.LoadSynchronous();
	return IsValid(Library) ? Library->GetRowNames() : TArray<FName>();
}

const FVehicleInfoTakeoverEntry* UInfoTakeoverSubsystem::FindVehicleLibraryEntry(const FName LibraryEntry)
{
	const UDataTable* const Library = UInfoTakeoverSettings::Get()->VehicleLibrary.LoadSynchronous();
	if (not IsValid(Library))
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: the vehicle library DataTable in Project Settings > Info "
			"Takeovers could not be loaded."));
		return nullptr;
	}
	return Library->FindRow<FVehicleInfoTakeoverEntry>(LibraryEntry, TEXT("UInfoTakeoverSubsystem"), false);
}

bool UInfoTakeoverSubsystem::QueueVehicleTakeoverFromLibrary(ATankMaster* Vehicle, const FName LibraryEntry)
{
	const FVehicleInfoTakeoverEntry* const Entry = FindVehicleLibraryEntry(LibraryEntry);
	if (Entry == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Info takeover: no row '%s' in the vehicle library (DT_VehicleInfoTakeovers)."),
			*LibraryEntry.ToString()));
		return false;
	}
	return QueueVehicleTakeover(Vehicle, *Entry);
}

bool UInfoTakeoverSubsystem::QueueVehicleTakeover(ATankMaster* Vehicle, const FVehicleInfoTakeoverEntry& Entry)
{
	if (not IsValid(Vehicle))
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: the vehicle passed to the mission node is not valid."));
		return false;
	}
	FQueuedInfoTakeover QueuedTakeover;
	QueuedTakeover.Kind = EInfoTakeoverKind::Vehicle;
	QueuedTakeover.VehicleContent = VehicleInfoTakeoverContent::BuildContent(*Vehicle, Entry,
	                                                                         GetTrainingOptionsTable());
	if (not GetAreInfoTakeoversEnabled())
	{
		// The player turned the takeovers off, but the archive still gets the entry and its pop-up.
		AddToArchive(QueuedTakeover);
		return false;
	}
	Enqueue(QueuedTakeover);
	return true;
}

int32 UInfoTakeoverSubsystem::QueueCameraControlsTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::CameraControls);
}

int32 UInfoTakeoverSubsystem::QueueCoverTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::Cover);
}

int32 UInfoTakeoverSubsystem::QueueSpacedArmourTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::SpacedArmour);
}

int32 UInfoTakeoverSubsystem::QueueNomadicConversionTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::NomadicConversion);
}

int32 UInfoTakeoverSubsystem::QueueBxpPackUpTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::BxpPackUp);
}

int32 UInfoTakeoverSubsystem::QueueWaitedTutorialTakeover(const EInfoTakeoverKind Kind)
{
	if (not GetAreInfoTakeoversEnabled())
	{
		// Still archived, so the tutorial can be reread after the player turned the takeovers off. At the very start
		// of a mission the HUD may not exist yet; it is then simply not archived.
		if (FRTS_Statics::GetMainGameUI(this) != nullptr)
		{
			FQueuedInfoTakeover SkippedTakeover;
			SkippedTakeover.Kind = Kind;
			AddToArchive(SkippedTakeover);
		}
		return INDEX_NONE;
	}
	FQueuedInfoTakeover QueuedTakeover;
	QueuedTakeover.Kind = Kind;
	QueuedTakeover.RequestId = M_NextRequestId++;
	const int32 RequestId = QueuedTakeover.RequestId;
	Enqueue(QueuedTakeover);
	return RequestId;
}

void UInfoTakeoverSubsystem::Enqueue(const FQueuedInfoTakeover& QueuedTakeover)
{
	M_QueuedTakeovers.Add(QueuedTakeover);
	TryOpenNextTakeover();
}

void UInfoTakeoverSubsystem::TryOpenNextTakeover()
{
	if (IsValid(M_ActiveTakeover) || M_QueuedTakeovers.IsEmpty())
	{
		return;
	}
	if (not GetAreInfoTakeoversEnabled())
	{
		DropQueuedTakeoversWhileTurnedOff();
		return;
	}
	if (not GetCanOpenTakeoverNow())
	{
		StartRetryTicker();
		return;
	}
	const FQueuedInfoTakeover NextTakeover = M_QueuedTakeovers[0];
	M_QueuedTakeovers.RemoveAt(0);
	M_ActiveTakeover = CreateTakeoverWidget(NextTakeover);
	M_ActiveTakeoverContent = NextTakeover;
	if (not IsValid(M_ActiveTakeover))
	{
		// A broken widget class must not block the takeovers behind it; the entry still reaches the archive.
		AddToArchive(NextTakeover);
		BroadcastRequestFinished(NextTakeover, EInfoTakeoverRequestResult::Skipped);
		TryOpenNextTakeover();
		return;
	}
	M_ActiveTakeover->OnTakeoverClosed.AddUObject(this, &UInfoTakeoverSubsystem::HandleTakeoverClosed);
	M_ActiveTakeover->OpenTakeover();
}

void UInfoTakeoverSubsystem::DropQueuedTakeoversWhileTurnedOff()
{
	// "Never show again" was just ticked: the waiting takeovers go straight to the archive. Moved out first, as a
	// released mission may queue again from its callback.
	const TArray<FQueuedInfoTakeover> DroppedTakeovers = MoveTemp(M_QueuedTakeovers);
	M_QueuedTakeovers.Reset();
	for (const FQueuedInfoTakeover& DroppedTakeover : DroppedTakeovers)
	{
		AddToArchive(DroppedTakeover);
		BroadcastRequestFinished(DroppedTakeover, EInfoTakeoverRequestResult::Skipped);
	}
}

bool UInfoTakeoverSubsystem::GetCanOpenTakeoverNow() const
{
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController) || FRTS_Statics::GetMainGameUI(this) == nullptr)
	{
		return false;
	}
	// The start-game screen pauses without the lock; a takeover closing over it would unpause the game under it.
	const UPlayerStartGameControl* const StartGameControl = PlayerController->FindComponentByClass<
		UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		return false;
	}
	return PlayerController->GetCanStartScreenTakeover();
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover)
{
	switch (QueuedTakeover.Kind)
	{
	case EInfoTakeoverKind::Vehicle:
		return CreateVehicleTakeoverWidget(QueuedTakeover);
	case EInfoTakeoverKind::CameraControls:
		return CreateCameraControlsTakeoverWidget();
	case EInfoTakeoverKind::Cover:
		return CreateCoverTakeoverWidget();
	case EInfoTakeoverKind::SpacedArmour:
		return CreateSpacedArmourTakeoverWidget();
	case EInfoTakeoverKind::NomadicConversion:
		return CreateNomadicConversionTakeoverWidget();
	case EInfoTakeoverKind::BxpPackUp:
		return CreateBxpPackUpTakeoverWidget();
	default:
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: buildings and squads have no takeover widget yet."));
		return nullptr;
	}
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateVehicleTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_VehicleInfoTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->VehicleTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_VehicleInfoTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_VehicleInfoTakeover::StaticClass();
	}
	UW_VehicleInfoTakeover* const VehicleTakeover = CreateWidget<UW_VehicleInfoTakeover>(PlayerController, WidgetClass);
	if (not IsValid(VehicleTakeover))
	{
		return nullptr;
	}
	VehicleTakeover->SetupVehicle(QueuedTakeover.VehicleContent);
	return VehicleTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateCameraControlsTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_CameraControlsTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->CameraControlsTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_CameraControlsTakeover could not be loaded; showing "
			"the unstyled C++ widget."));
		WidgetClass = UW_CameraControlsTakeover::StaticClass();
	}
	UW_CameraControlsTakeover* const CameraControlsTakeover = CreateWidget<UW_CameraControlsTakeover>(
		PlayerController, WidgetClass);
	if (not IsValid(CameraControlsTakeover))
	{
		return nullptr;
	}
	CameraControlsTakeover->SetupCameraControls();
	return CameraControlsTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateCoverTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_CoverTakeover> WidgetClass = UInfoTakeoverSettings::Get()->CoverTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_CoverTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_CoverTakeover::StaticClass();
	}
	UW_CoverTakeover* const CoverTakeover = CreateWidget<UW_CoverTakeover>(PlayerController, WidgetClass);
	if (not IsValid(CoverTakeover))
	{
		return nullptr;
	}
	CoverTakeover->SetupCover();
	return CoverTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateSpacedArmourTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_SpacedArmourTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->SpacedArmourTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_SpacedArmourTakeover could not be loaded; showing "
			"the unstyled C++ widget."));
		WidgetClass = UW_SpacedArmourTakeover::StaticClass();
	}
	UW_SpacedArmourTakeover* const SpacedArmourTakeover = CreateWidget<UW_SpacedArmourTakeover>(
		PlayerController, WidgetClass);
	if (not IsValid(SpacedArmourTakeover))
	{
		return nullptr;
	}
	SpacedArmourTakeover->SetupSpacedArmour();
	return SpacedArmourTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateNomadicConversionTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_NomadicConversionTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->NomadicConversionTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_NomadicConversionTakeover could not be loaded; showing "
			"the unstyled C++ widget."));
		WidgetClass = UW_NomadicConversionTakeover::StaticClass();
	}
	UW_NomadicConversionTakeover* const NomadicConversionTakeover = CreateWidget<UW_NomadicConversionTakeover>(
		PlayerController, WidgetClass);
	if (not IsValid(NomadicConversionTakeover))
	{
		return nullptr;
	}
	NomadicConversionTakeover->SetupNomadicConversion();
	return NomadicConversionTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateBxpPackUpTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_BxpPackUpTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->BxpPackUpTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_BxpPackUpTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_BxpPackUpTakeover::StaticClass();
	}
	UW_BxpPackUpTakeover* const BxpPackUpTakeover = CreateWidget<UW_BxpPackUpTakeover>(PlayerController, WidgetClass);
	if (not IsValid(BxpPackUpTakeover))
	{
		return nullptr;
	}
	BxpPackUpTakeover->SetupBxpPackUp();
	return BxpPackUpTakeover;
}

void UInfoTakeoverSubsystem::HandleTakeoverClosed(UW_InfoTakeover* ClosedTakeover)
{
	if (ClosedTakeover != M_ActiveTakeover)
	{
		return;
	}
	M_ActiveTakeover = nullptr;
	// Copied: a mission released by the broadcast may queue and open the next takeover at once.
	const FQueuedInfoTakeover ClosedTakeoverContent = M_ActiveTakeoverContent;
	// After the panel closed, so the pop-up shows on the unpaused HUD.
	AddToArchive(ClosedTakeoverContent);
	BroadcastRequestFinished(ClosedTakeoverContent, EInfoTakeoverRequestResult::Closed);
	TryOpenNextTakeover();
}

void UInfoTakeoverSubsystem::BroadcastRequestFinished(const FQueuedInfoTakeover& Takeover,
                                                      const EInfoTakeoverRequestResult Result)
{
	if (Takeover.RequestId == INDEX_NONE)
	{
		return;
	}
	OnTakeoverRequestFinished.Broadcast(Takeover.RequestId, Result);
}

void UInfoTakeoverSubsystem::AddToArchive(const FQueuedInfoTakeover& Takeover) const
{
	using namespace InfoTakeoverSubsystemPrivate;
	if (Takeover.Kind == EInfoTakeoverKind::Building || Takeover.Kind == EInfoTakeoverKind::Squad)
	{
		return;
	}
	UMainGameUI* const MainGameUI = FRTS_Statics::GetMainGameUI(this);
	if (MainGameUI == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: no main game UI to archive the takeover in."));
		return;
	}
	FArchiveCustomContent ArchiveContent;
	switch (Takeover.Kind)
	{
	case EInfoTakeoverKind::CameraControls:
		ArchiveContent = MakeCameraControlsArchiveContent();
		break;
	case EInfoTakeoverKind::Cover:
		ArchiveContent = MakeCoverArchiveContent();
		break;
	case EInfoTakeoverKind::SpacedArmour:
		ArchiveContent = MakeSpacedArmourArchiveContent();
		break;
	case EInfoTakeoverKind::NomadicConversion:
		ArchiveContent = MakeNomadicConversionArchiveContent();
		break;
	case EInfoTakeoverKind::BxpPackUp:
		ArchiveContent = MakeBxpPackUpArchiveContent();
		break;
	default:
		ArchiveContent = MakeArchiveContent(Takeover.VehicleContent);
		break;
	}
	MainGameUI->AddNewCustomArchiveItem(ArchiveContent, ArchiveSortingPriority, ArchiveNotificationSeconds);
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeArchiveContent(const FVehicleInfoTakeoverContent& VehicleContent)
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	// Keyed by name: the same vehicle introduced again in a later mission is archived once.
	ArchiveContent.Key = FName(VehicleArchiveKeyPrefix + VehicleContent.DisplayName.ToString());
	ArchiveContent.Title = VehicleContent.DisplayName;
	ArchiveContent.Description = VehicleContent.Description;
	ArchiveContent.ImageTexture = VehicleContent.ImageTexture;
	ArchiveContent.ImageBrush = VehicleContent.ImageBrush;
	ArchiveContent.bHasImage = VehicleContent.bHasImage;
	ArchiveContent.SortType = ERTSArchiveType::Units;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeCameraControlsArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = CameraControlsArchiveKey;
	ArchiveContent.Title = UW_CameraControlsTakeover::GetCameraControlsTitle();
	ArchiveContent.Description = UW_CameraControlsTakeover::GetCameraControlsSummary();
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeCoverArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = CoverArchiveKey;
	ArchiveContent.Title = UW_CoverTakeover::GetCoverTitle();
	ArchiveContent.Description = UW_CoverTakeover::GetCoverSummary();
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeSpacedArmourArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = SpacedArmourArchiveKey;
	ArchiveContent.Title = UW_SpacedArmourTakeover::GetSpacedArmourTitle();
	ArchiveContent.Description = UW_SpacedArmourTakeover::GetSpacedArmourSummary();
	// Like cover: the archive item's picture slot has a fixed size that would squash the 16:9 render.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeNomadicConversionArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = NomadicConversionArchiveKey;
	ArchiveContent.Title = UW_NomadicConversionTakeover::GetNomadicConversionTitle();
	ArchiveContent.Description = UW_NomadicConversionTakeover::GetNomadicConversionSummary();
	// The picture is an animated widget; the archive item only takes a still image.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeBxpPackUpArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = BxpPackUpArchiveKey;
	ArchiveContent.Title = UW_BxpPackUpTakeover::GetBxpPackUpTitle();
	ArchiveContent.Description = UW_BxpPackUpTakeover::GetBxpPackUpSummary();
	// The picture is an animated widget; the archive item only takes a still image.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

void UInfoTakeoverSubsystem::StartRetryTicker()
{
	using namespace InfoTakeoverSubsystemPrivate;
	if (M_RetryTickerHandle.IsValid())
	{
		return;
	}
	M_RetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			TryOpenNextTakeover();
			const bool bStillWaiting = not IsValid(M_ActiveTakeover) && not M_QueuedTakeovers.IsEmpty();
			if (not bStillWaiting)
			{
				M_RetryTickerHandle.Reset();
			}
			return bStillWaiting;
		}), RetryIntervalSeconds);
}

void UInfoTakeoverSubsystem::StopRetryTicker()
{
	if (not M_RetryTickerHandle.IsValid())
	{
		return;
	}
	FTSTicker::GetCoreTicker().RemoveTicker(M_RetryTickerHandle);
	M_RetryTickerHandle.Reset();
}

const UDataTable* UInfoTakeoverSubsystem::GetTrainingOptionsTable()
{
	if (not IsValid(M_TrainingOptionsTable))
	{
		M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	}
	return M_TrainingOptionsTable;
}
