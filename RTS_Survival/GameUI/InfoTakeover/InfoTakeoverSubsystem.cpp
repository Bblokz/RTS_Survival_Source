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
#include "SquadInfoTakeoverContent.h"
#include "VehicleInfoTakeoverContent.h"
#include "W_BxpPackUpTakeover.h"
#include "W_CapturingTrucksTakeover.h"
#include "W_DocumentTakeover.h"
#include "W_CameraControlsTakeover.h"
#include "W_CoverTakeover.h"
#include "W_NomadicConversionTakeover.h"
#include "W_SpacedArmourTakeover.h"
#include "W_VehicleInfoTakeover.h"
#include "W_RattleFeedTakeover.h"
#include "W_WespeBarrageTakeover.h"
#include "W_DigInTakeover.h"
#include "W_WallBreachTakeover.h"
#include "W_AdrenalineShotTakeover.h"
#include "W_VulturesAtTakeover.h"

namespace InfoTakeoverSubsystemPrivate
{
	// How often a waiting takeover checks whether the screen is free, in real seconds.
	constexpr float RetryIntervalSeconds = 0.25f;
	// Matches the missions' AddArchiveItem default.
	constexpr float ArchiveNotificationSeconds = 20.f;
	constexpr int32 ArchiveSortingPriority = 0;
	const TCHAR* const VehicleArchiveKeyPrefix = TEXT("InfoTakeover.Vehicle.");
	const TCHAR* const SquadArchiveKeyPrefix = TEXT("InfoTakeover.Squad.");
	const FName CameraControlsArchiveKey(TEXT("InfoTakeover.CameraControls"));
	const FName CoverArchiveKey(TEXT("InfoTakeover.Cover"));
	const FName SpacedArmourArchiveKey(TEXT("InfoTakeover.SpacedArmour"));
	const FName NomadicConversionArchiveKey(TEXT("InfoTakeover.NomadicConversion"));
	const FName BxpPackUpArchiveKey(TEXT("InfoTakeover.BxpPackUp"));
	const FName VulturesAtArchiveKey(TEXT("InfoTakeover.VulturesAt"));
	const FName RattleFeedArchiveKey(TEXT("InfoTakeover.RattleFeed"));
	const FName WespeBarrageArchiveKey(TEXT("InfoTakeover.WespeBarrage"));
	const FName DigInArchiveKey(TEXT("InfoTakeover.DigIn"));
	const FName WallBreachArchiveKey(TEXT("InfoTakeover.WallBreach"));
	const FName AdrenalineShotArchiveKey(TEXT("InfoTakeover.AdrenalineShot"));
	const FName CapturingTrucksArchiveKey(TEXT("InfoTakeover.CapturingTrucks"));
	const TCHAR* const DocumentArchiveKeyPrefix = TEXT("InfoTakeover.Document.");
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

TArray<FName> UInfoTakeoverSubsystem::GetSquadLibraryEntryNames()
{
	const UDataTable* const Library = UInfoTakeoverSettings::Get()->SquadLibrary.LoadSynchronous();
	return IsValid(Library) ? Library->GetRowNames() : TArray<FName>();
}

const FSquadInfoTakeoverEntry* UInfoTakeoverSubsystem::FindSquadLibraryEntry(const FName LibraryEntry)
{
	const UDataTable* const Library = UInfoTakeoverSettings::Get()->SquadLibrary.LoadSynchronous();
	if (not IsValid(Library))
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: the squad library DataTable in Project Settings > Info "
			"Takeovers could not be loaded."));
		return nullptr;
	}
	return Library->FindRow<FSquadInfoTakeoverEntry>(LibraryEntry, TEXT("UInfoTakeoverSubsystem"), false);
}

bool UInfoTakeoverSubsystem::QueueSquadTakeoverFromLibrary(AActor* Infantry, const FName LibraryEntry)
{
	const FSquadInfoTakeoverEntry* const Entry = FindSquadLibraryEntry(LibraryEntry);
	if (Entry == nullptr)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Info takeover: no row '%s' in the squad library (DT_SquadInfoTakeovers)."),
			*LibraryEntry.ToString()));
		return false;
	}
	return QueueSquadTakeover(Infantry, *Entry);
}

bool UInfoTakeoverSubsystem::QueueSquadTakeover(AActor* Infantry, const FSquadInfoTakeoverEntry& Entry)
{
	if (not IsValid(Infantry) || not SquadInfoTakeoverContent::GetIsInfantry(*Infantry))
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: the squad passed to the mission node is not a valid squad "
			"controller or infantry unit."));
		return false;
	}
	FQueuedInfoTakeover QueuedTakeover;
	QueuedTakeover.Kind = EInfoTakeoverKind::Squad;
	QueuedTakeover.SquadContent = SquadInfoTakeoverContent::BuildContent(*Infantry, Entry, GetTrainingOptionsTable());
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

int32 UInfoTakeoverSubsystem::QueueVulturesAtTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::VulturesAt);
}

int32 UInfoTakeoverSubsystem::QueueRattleFeedTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::RattleFeed);
}

int32 UInfoTakeoverSubsystem::QueueWespeBarrageTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::WespeBarrage);
}

int32 UInfoTakeoverSubsystem::QueueDigInTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::DigIn);
}

int32 UInfoTakeoverSubsystem::QueueWallBreachTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::WallBreach);
}

int32 UInfoTakeoverSubsystem::QueueAdrenalineShotTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::AdrenalineShot);
}

int32 UInfoTakeoverSubsystem::QueueCapturingTrucksTakeover()
{
	return QueueWaitedTutorialTakeover(EInfoTakeoverKind::CapturingTrucks);
}

int32 UInfoTakeoverSubsystem::QueueDocumentTakeover(const FDocumentTakeoverContent& Content)
{
	// Story content, not unit intel: "never show again" does not hide it.
	FQueuedInfoTakeover QueuedTakeover;
	QueuedTakeover.Kind = EInfoTakeoverKind::Document;
	QueuedTakeover.DocumentContent = Content;
	QueuedTakeover.RequestId = M_NextRequestId++;
	const int32 RequestId = QueuedTakeover.RequestId;
	Enqueue(QueuedTakeover);
	return RequestId;
}

int32 UInfoTakeoverSubsystem::QueueWaitedTutorialTakeover(const EInfoTakeoverKind Kind)
{
	// Game-mechanic field manuals always show: "never show again" only hides unit intel.
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
		// May release missions that queue again; the queue is re-checked below.
		DropQueuedUnitIntelWhileTurnedOff();
	}
	if (IsValid(M_ActiveTakeover) || M_QueuedTakeovers.IsEmpty())
	{
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

void UInfoTakeoverSubsystem::DropQueuedUnitIntelWhileTurnedOff()
{
	// "Never show again" was just ticked: the waiting unit intel goes straight to the archive, while the game-mechanic
	// field manuals keep their place in the queue. The queue is settled first, as a released mission may queue again
	// from its callback.
	TArray<FQueuedInfoTakeover> DroppedTakeovers;
	TArray<FQueuedInfoTakeover> KeptTakeovers;
	for (const FQueuedInfoTakeover& QueuedTakeover : M_QueuedTakeovers)
	{
		if (InfoTakeoverKinds::GetIsUnitIntel(QueuedTakeover.Kind))
		{
			DroppedTakeovers.Add(QueuedTakeover);
		}
		else
		{
			KeptTakeovers.Add(QueuedTakeover);
		}
	}
	M_QueuedTakeovers = MoveTemp(KeptTakeovers);
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
	case EInfoTakeoverKind::Squad:
		return CreateSquadTakeoverWidget(QueuedTakeover);
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
	case EInfoTakeoverKind::VulturesAt:
		return CreateVulturesAtTakeoverWidget();
	case EInfoTakeoverKind::RattleFeed:
		return CreateRattleFeedTakeoverWidget();
	case EInfoTakeoverKind::WespeBarrage:
		return CreateWespeBarrageTakeoverWidget();
	case EInfoTakeoverKind::DigIn:
		return CreateDigInTakeoverWidget();
	case EInfoTakeoverKind::WallBreach:
		return CreateWallBreachTakeoverWidget();
	case EInfoTakeoverKind::AdrenalineShot:
		return CreateAdrenalineShotTakeoverWidget();
	case EInfoTakeoverKind::CapturingTrucks:
		return CreateCapturingTrucksTakeoverWidget();
	case EInfoTakeoverKind::Document:
		return CreateDocumentTakeoverWidget(QueuedTakeover);
	default:
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: buildings have no takeover widget yet."));
		return nullptr;
	}
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateVehicleTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const
{
	UW_VehicleInfoTakeover* const VehicleTakeover = CreateUnitInfoWidget();
	if (not IsValid(VehicleTakeover))
	{
		return nullptr;
	}
	VehicleTakeover->SetupVehicle(QueuedTakeover.VehicleContent);
	return VehicleTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateSquadTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const
{
	UW_VehicleInfoTakeover* const SquadTakeover = CreateUnitInfoWidget();
	if (not IsValid(SquadTakeover))
	{
		return nullptr;
	}
	SquadTakeover->SetupSquad(QueuedTakeover.SquadContent);
	return SquadTakeover;
}

UW_VehicleInfoTakeover* UInfoTakeoverSubsystem::CreateUnitInfoWidget() const
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
	return CreateWidget<UW_VehicleInfoTakeover>(PlayerController, WidgetClass);
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

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateVulturesAtTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_VulturesAtTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->VulturesAtTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_VulturesAtTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_VulturesAtTakeover::StaticClass();
	}
	UW_VulturesAtTakeover* const VulturesAtTakeover = CreateWidget<UW_VulturesAtTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(VulturesAtTakeover))
	{
		return nullptr;
	}
	VulturesAtTakeover->SetupVulturesAt();
	return VulturesAtTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateRattleFeedTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_RattleFeedTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->RattleFeedTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_RattleFeedTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_RattleFeedTakeover::StaticClass();
	}
	UW_RattleFeedTakeover* const RattleFeedTakeover = CreateWidget<UW_RattleFeedTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(RattleFeedTakeover))
	{
		return nullptr;
	}
	RattleFeedTakeover->SetupRattleFeed();
	return RattleFeedTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateWespeBarrageTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_WespeBarrageTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->WespeBarrageTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_WespeBarrageTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_WespeBarrageTakeover::StaticClass();
	}
	UW_WespeBarrageTakeover* const WespeBarrageTakeover = CreateWidget<UW_WespeBarrageTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(WespeBarrageTakeover))
	{
		return nullptr;
	}
	WespeBarrageTakeover->SetupWespeBarrage();
	return WespeBarrageTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateDigInTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_DigInTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->DigInTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_DigInTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_DigInTakeover::StaticClass();
	}
	UW_DigInTakeover* const DigInTakeover = CreateWidget<UW_DigInTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(DigInTakeover))
	{
		return nullptr;
	}
	DigInTakeover->SetupDigIn();
	return DigInTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateWallBreachTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_WallBreachTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->WallBreachTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_WallBreachTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_WallBreachTakeover::StaticClass();
	}
	UW_WallBreachTakeover* const WallBreachTakeover = CreateWidget<UW_WallBreachTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(WallBreachTakeover))
	{
		return nullptr;
	}
	WallBreachTakeover->SetupWallBreach();
	return WallBreachTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateAdrenalineShotTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_AdrenalineShotTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->AdrenalineShotTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_AdrenalineShotTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_AdrenalineShotTakeover::StaticClass();
	}
	UW_AdrenalineShotTakeover* const AdrenalineShotTakeover = CreateWidget<UW_AdrenalineShotTakeover>(PlayerController,
		WidgetClass);
	if (not IsValid(AdrenalineShotTakeover))
	{
		return nullptr;
	}
	AdrenalineShotTakeover->SetupAdrenalineShot();
	return AdrenalineShotTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateCapturingTrucksTakeoverWidget() const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_CapturingTrucksTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->CapturingTrucksTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_CapturingTrucksTakeover could not be loaded; showing "
			"the unstyled C++ widget."));
		WidgetClass = UW_CapturingTrucksTakeover::StaticClass();
	}
	UW_CapturingTrucksTakeover* const CapturingTrucksTakeover = CreateWidget<UW_CapturingTrucksTakeover>(
		PlayerController, WidgetClass);
	if (not IsValid(CapturingTrucksTakeover))
	{
		return nullptr;
	}
	CapturingTrucksTakeover->SetupCapturingTrucks();
	return CapturingTrucksTakeover;
}

UW_InfoTakeover* UInfoTakeoverSubsystem::CreateDocumentTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const
{
	ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return nullptr;
	}
	TSubclassOf<UW_DocumentTakeover> WidgetClass =
		UInfoTakeoverSettings::Get()->DocumentTakeoverWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Info takeover: WBP_DocumentTakeover could not be loaded; showing the "
			"unstyled C++ widget."));
		WidgetClass = UW_DocumentTakeover::StaticClass();
	}
	UW_DocumentTakeover* const DocumentTakeover = CreateWidget<UW_DocumentTakeover>(PlayerController, WidgetClass);
	if (not IsValid(DocumentTakeover))
	{
		return nullptr;
	}
	DocumentTakeover->SetupDocument(QueuedTakeover.DocumentContent);
	return DocumentTakeover;
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
	if (Takeover.Kind == EInfoTakeoverKind::Building)
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
	case EInfoTakeoverKind::Squad:
		ArchiveContent = MakeSquadArchiveContent(Takeover.SquadContent);
		break;
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
	case EInfoTakeoverKind::VulturesAt:
		ArchiveContent = MakeVulturesAtArchiveContent();
		break;
	case EInfoTakeoverKind::RattleFeed:
		ArchiveContent = MakeRattleFeedArchiveContent();
		break;
	case EInfoTakeoverKind::WespeBarrage:
		ArchiveContent = MakeWespeBarrageArchiveContent();
		break;
	case EInfoTakeoverKind::DigIn:
		ArchiveContent = MakeDigInArchiveContent();
		break;
	case EInfoTakeoverKind::WallBreach:
		ArchiveContent = MakeWallBreachArchiveContent();
		break;
	case EInfoTakeoverKind::AdrenalineShot:
		ArchiveContent = MakeAdrenalineShotArchiveContent();
		break;
	case EInfoTakeoverKind::CapturingTrucks:
		ArchiveContent = MakeCapturingTrucksArchiveContent();
		break;
	case EInfoTakeoverKind::Document:
		ArchiveContent = MakeDocumentArchiveContent(Takeover.DocumentContent);
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

FArchiveCustomContent UInfoTakeoverSubsystem::MakeSquadArchiveContent(const FSquadInfoTakeoverContent& SquadContent)
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	// Keyed by name: the same squad introduced again in a later mission is archived once.
	ArchiveContent.Key = FName(SquadArchiveKeyPrefix + SquadContent.DisplayName.ToString());
	ArchiveContent.Title = SquadContent.DisplayName;
	ArchiveContent.Description = SquadContent.Description;
	ArchiveContent.ImageTexture = SquadContent.ImageTexture;
	ArchiveContent.ImageBrush = SquadContent.ImageBrush;
	ArchiveContent.bHasImage = SquadContent.bHasImage;
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

FArchiveCustomContent UInfoTakeoverSubsystem::MakeVulturesAtArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = VulturesAtArchiveKey;
	ArchiveContent.Title = UW_VulturesAtTakeover::GetVulturesAtTitle();
	ArchiveContent.Description = UW_VulturesAtTakeover::GetVulturesAtSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeRattleFeedArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = RattleFeedArchiveKey;
	ArchiveContent.Title = UW_RattleFeedTakeover::GetRattleFeedTitle();
	ArchiveContent.Description = UW_RattleFeedTakeover::GetRattleFeedSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeWespeBarrageArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = WespeBarrageArchiveKey;
	ArchiveContent.Title = UW_WespeBarrageTakeover::GetWespeBarrageTitle();
	ArchiveContent.Description = UW_WespeBarrageTakeover::GetWespeBarrageSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeDigInArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = DigInArchiveKey;
	ArchiveContent.Title = UW_DigInTakeover::GetDigInTitle();
	ArchiveContent.Description = UW_DigInTakeover::GetDigInSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeWallBreachArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = WallBreachArchiveKey;
	ArchiveContent.Title = UW_WallBreachTakeover::GetWallBreachTitle();
	ArchiveContent.Description = UW_WallBreachTakeover::GetWallBreachSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeAdrenalineShotArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = AdrenalineShotArchiveKey;
	ArchiveContent.Title = UW_AdrenalineShotTakeover::GetAdrenalineShotTitle();
	ArchiveContent.Description = UW_AdrenalineShotTakeover::GetAdrenalineShotSummary();
	// The picture is a flipbook atlas; the archive item only takes a still image of a fixed size.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeCapturingTrucksArchiveContent()
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	ArchiveContent.Key = CapturingTrucksArchiveKey;
	ArchiveContent.Title = UW_CapturingTrucksTakeover::GetCapturingTrucksTitle();
	ArchiveContent.Description = UW_CapturingTrucksTakeover::GetCapturingTrucksSummary();
	// The picture is an animated widget; the archive item only takes a still image.
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::GameMechanics;
	return ArchiveContent;
}

FArchiveCustomContent UInfoTakeoverSubsystem::MakeDocumentArchiveContent(
	const FDocumentTakeoverContent& DocumentContent)
{
	using namespace InfoTakeoverSubsystemPrivate;
	FArchiveCustomContent ArchiveContent;
	const FString KeySuffix = DocumentContent.ArchiveKey.IsNone()
		                          ? DocumentContent.Title.ToString()
		                          : DocumentContent.ArchiveKey.ToString();
	ArchiveContent.Key = FName(DocumentArchiveKeyPrefix + KeySuffix);
	ArchiveContent.Title = DocumentContent.Title;
	// The archive keeps the sheet as plain text: intro, heading, then one paragraph per line.
	TArray<FString> Paragraphs;
	Paragraphs.Add(DocumentContent.Description.ToString());
	Paragraphs.Add(DocumentContent.PaperHeading.ToString());
	for (const FDocumentTakeoverLine& Line : DocumentContent.PaperLines)
	{
		Paragraphs.Add(Line.Text.ToString());
	}
	Paragraphs.Add(DocumentContent.PaperFooter.ToString());
	Paragraphs.RemoveAll([](const FString& Paragraph) { return Paragraph.IsEmpty(); });
	ArchiveContent.Description = FText::FromString(FString::Join(Paragraphs, TEXT("\n\n")));
	ArchiveContent.bHasImage = false;
	ArchiveContent.SortType = ERTSArchiveType::Lore;
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
