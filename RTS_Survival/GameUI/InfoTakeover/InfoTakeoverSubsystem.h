// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "InfoTakeoverTypes.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "InfoTakeoverSubsystem.generated.h"

class ATankMaster;
class UDataTable;
class UW_InfoTakeover;
class UW_VehicleInfoTakeover;

/** How a takeover that a caller waits on ended. */
enum class EInfoTakeoverRequestResult : uint8
{
	// The player closed it; the game is unpaused again.
	Closed,
	// It never opened: unit intel is turned off or its widget could not be created.
	Skipped,
	// The world ended before it closed; nothing should continue.
	Cancelled
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInfoTakeoverRequestFinished, int32 /*RequestId*/,
                                     EInfoTakeoverRequestResult /*Result*/);

/** A takeover waiting for the screen; one kind's content is filled. */
USTRUCT()
struct FQueuedInfoTakeover
{
	GENERATED_BODY()

	UPROPERTY()
	EInfoTakeoverKind Kind = EInfoTakeoverKind::Vehicle;

	UPROPERTY()
	FVehicleInfoTakeoverContent VehicleContent;

	UPROPERTY()
	FSquadInfoTakeoverContent SquadContent;

	// Document takeovers only: the caller's text.
	UPROPERTY()
	FDocumentTakeoverContent DocumentContent;

	// Set when a caller waits for the close (OnTakeoverRequestFinished); INDEX_NONE for fire-and-forget takeovers.
	UPROPERTY()
	int32 RequestId = INDEX_NONE;
};

/**
 * @brief Opens info takeovers for the mission nodes, one at a time: a call while another takeover, a cinematic, the
 * tech tree or the archive holds the screen waits in a queue. Content is taken when the node is called. Unit intel
 * (vehicles, squads) does nothing once the player ticked "never show these info widgets again" (the gameplay setting is
 * off); game-mechanic field manuals always show and ignore the setting. Every takeover is also archived (with the
 * archive pop-up) when it closes, or at once while unit intel is off. A caller that must
 * wait for the close (the camera controls node) gets a request id that OnTakeoverRequestFinished reports once.
 */
UCLASS()
class RTS_SURVIVAL_API UInfoTakeoverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/**
	 * @brief Queues the vehicle takeover of a library prefab.
	 * @param Vehicle The live vehicle whose weapons and armour are shown.
	 * @param LibraryEntry Row name in the vehicle library (DT_VehicleInfoTakeovers).
	 * @return False when unit intel is turned off or the vehicle or row is missing.
	 */
	bool QueueVehicleTakeoverFromLibrary(ATankMaster* Vehicle, const FName LibraryEntry);

	/**
	 * @brief Queues a vehicle takeover with the caller's own text and picture.
	 * @param Vehicle The live vehicle whose weapons and armour are shown.
	 * @param Entry Name, description, year and picture; empty fields fall back to the unit's training data.
	 * @return False when unit intel is turned off or the vehicle is missing.
	 */
	bool QueueVehicleTakeover(ATankMaster* Vehicle, const FVehicleInfoTakeoverEntry& Entry);

	/**
	 * @brief Queues the squad takeover of a library prefab.
	 * @param Infantry A squad controller or a solo infantry unit; its soldiers and weapons are shown.
	 * @param LibraryEntry Row name in the squad library (DT_SquadInfoTakeovers).
	 * @return False when unit intel is turned off or the infantry or row is missing.
	 */
	bool QueueSquadTakeoverFromLibrary(AActor* Infantry, const FName LibraryEntry);

	/**
	 * @brief Queues a squad takeover with the caller's own text and picture.
	 * @param Infantry A squad controller or a solo infantry unit; its soldiers and weapons are shown.
	 * @param Entry Name, description, year and picture; empty fields fall back to the unit's training data.
	 * @return False when unit intel is turned off or the infantry is missing.
	 */
	bool QueueSquadTakeover(AActor* Infantry, const FSquadInfoTakeoverEntry& Entry);

	/**
	 * @brief Queues the camera controls field manual; a mission waits on OnTakeoverRequestFinished with the returned id
	 * so its voice lines start only after the player closed it.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while
	 * unit intel is off.
	 */
	int32 QueueCameraControlsTakeover();

	/**
	 * @brief Queues the cover field manual (what the green, yellow and red move preview means), waited on like the
	 * camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueCoverTakeover();

	/**
	 * @brief Queues the spaced armour field manual (side plates and how hits knock them off), waited on like the
	 * camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueSpacedArmourTakeover();

	/**
	 * @brief Queues the nomadic conversion field manual (a truck deploys into a building and packs up again), waited on
	 * like the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueNomadicConversionTakeover();

	/**
	 * @brief Queues the Vultures AT stick grenade field manual (a short clip and how the volley works), waited on like
	 * the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueVulturesAtTakeover();

	/**
	 * @brief Queues the Rattle-Feed field manual (a short clip and how the Panzer II's autocannon burst works), waited on
	 * like the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueRattleFeedTakeover();

	/**
	 * @brief Queues the Wespe Barrage field manual (a short clip and how the Wespe's area barrage works), waited on like the camera
	 * controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueWespeBarrageTakeover();

	/**
	 * @brief Queues the Dig In field manual (a short clip and how a tank digs in behind sandbags), waited on like the camera
	 * controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueDigInTakeover();

	/**
	 * @brief Queues the Wall Breach field manual (a short clip and how medium and heavy tanks drive through Soviet
	 * concrete walls), waited on like the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueWallBreachTakeover();

	/**
	 * @brief Queues the Adrenaline Shot field manual (a short clip and how the Panzer 38(t)'s crew rush works), waited on like the camera
	 * controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueAdrenalineShotTakeover();

	/**
	 * @brief Queues the building expansion pack-up field manual (expansions pack up with their nomadic building and come
	 * back after it deploys), waited on like the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueBxpPackUpTakeover();

	/**
	 * @brief Queues the capturing trucks field manual (scuttle countdown, the squad and health rules, capture versus
	 * destroy), waited on like the camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; field manuals show even while unit intel is off.
	 */
	int32 QueueCapturingTrucksTakeover();

	/**
	 * @brief Queues a captured document (paper sheet with the caller's text), waited on like the camera controls
	 * takeover. Documents are story content: they show even while unit intel is off and are archived under Lore.
	 * @param Content Title, intro, the sheet's heading, lines and stamp, and the archive key.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once.
	 */
	int32 QueueDocumentTakeover(const FDocumentTakeoverContent& Content);

	// Fires once per queued request id: closed, skipped (broken widget) or cancelled.
	FOnInfoTakeoverRequestFinished OnTakeoverRequestFinished;

	/**
	 * @return False after "never show these info widgets again" on a unit takeover; the settings menu turns it back on.
	 * Only unit intel follows it; game-mechanic field manuals always show.
	 */
	static bool GetAreInfoTakeoversEnabled();

	/** @return The row names of the vehicle library, for the mission node's dropdown. */
	static TArray<FName> GetVehicleLibraryEntryNames();

	/** @return The library prefab, or null when the row is missing. */
	static const FVehicleInfoTakeoverEntry* FindVehicleLibraryEntry(const FName LibraryEntry);

	/** @return The row names of the squad library, for the mission node's dropdown. */
	static TArray<FName> GetSquadLibraryEntryNames();

	/** @return The library prefab, or null when the row is missing. */
	static const FSquadInfoTakeoverEntry* FindSquadLibraryEntry(const FName LibraryEntry);

	UW_InfoTakeover* GetActiveTakeover() const { return M_ActiveTakeover; }
	int32 GetQueuedTakeoverCount() const { return M_QueuedTakeovers.Num(); }

private:
	UPROPERTY()
	TArray<FQueuedInfoTakeover> M_QueuedTakeovers;

	UPROPERTY()
	TObjectPtr<UW_InfoTakeover> M_ActiveTakeover;

	// What the open takeover shows; archived when it closes.
	UPROPERTY()
	FQueuedInfoTakeover M_ActiveTakeoverContent;

	// DT_TrainingOptions, kept loaded for the default titles and pictures.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable;

	// Real-time retry while something else holds the screen; game timers would not run under a pause.
	FTSTicker::FDelegateHandle M_RetryTickerHandle;

	// The id the next waited-on request gets.
	int32 M_NextRequestId = 0;

	void Enqueue(const FQueuedInfoTakeover& QueuedTakeover);
	void TryOpenNextTakeover();
	void DropQueuedUnitIntelWhileTurnedOff();
	bool GetCanOpenTakeoverNow() const;
	void StartRetryTicker();
	void StopRetryTicker();

	/** @return The opened widget, or null when its class could not be created. */
	UW_InfoTakeover* CreateTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover);
	UW_InfoTakeover* CreateVehicleTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const;
	// Squads use the vehicle widget class (one style for every unit takeover).
	UW_InfoTakeover* CreateSquadTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const;
	/** @return The styled vehicle widget, or the unstyled C++ widget when the Blueprint cannot be loaded. */
	UW_VehicleInfoTakeover* CreateUnitInfoWidget() const;
	UW_InfoTakeover* CreateCameraControlsTakeoverWidget() const;
	UW_InfoTakeover* CreateCoverTakeoverWidget() const;
	UW_InfoTakeover* CreateSpacedArmourTakeoverWidget() const;
	UW_InfoTakeover* CreateNomadicConversionTakeoverWidget() const;
	UW_InfoTakeover* CreateBxpPackUpTakeoverWidget() const;
	UW_InfoTakeover* CreateVulturesAtTakeoverWidget() const;
	UW_InfoTakeover* CreateRattleFeedTakeoverWidget() const;
	UW_InfoTakeover* CreateWespeBarrageTakeoverWidget() const;
	UW_InfoTakeover* CreateDigInTakeoverWidget() const;
	UW_InfoTakeover* CreateWallBreachTakeoverWidget() const;
	UW_InfoTakeover* CreateAdrenalineShotTakeoverWidget() const;
	UW_InfoTakeover* CreateCapturingTrucksTakeoverWidget() const;
	UW_InfoTakeover* CreateDocumentTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const;

	/**
	 * @brief Queues a field manual takeover that a mission node waits on; shared by every tutorial kind.
	 * @param Kind The tutorial to show.
	 * @return The request id; field manuals show even while unit intel is turned off.
	 */
	int32 QueueWaitedTutorialTakeover(const EInfoTakeoverKind Kind);
	void HandleTakeoverClosed(UW_InfoTakeover* ClosedTakeover);
	void BroadcastRequestFinished(const FQueuedInfoTakeover& Takeover, const EInfoTakeoverRequestResult Result);

	/**
	 * @brief Every takeover also becomes an archive entry with the same description and picture, with its pop-up;
	 * this happens even when the player turned the takeovers off.
	 * @param Takeover The takeover's content.
	 */
	void AddToArchive(const FQueuedInfoTakeover& Takeover) const;
	static FArchiveCustomContent MakeArchiveContent(const FVehicleInfoTakeoverContent& VehicleContent);
	static FArchiveCustomContent MakeSquadArchiveContent(const FSquadInfoTakeoverContent& SquadContent);
	static FArchiveCustomContent MakeCameraControlsArchiveContent();
	static FArchiveCustomContent MakeCoverArchiveContent();
	static FArchiveCustomContent MakeSpacedArmourArchiveContent();
	static FArchiveCustomContent MakeNomadicConversionArchiveContent();
	static FArchiveCustomContent MakeBxpPackUpArchiveContent();
	static FArchiveCustomContent MakeVulturesAtArchiveContent();
	static FArchiveCustomContent MakeRattleFeedArchiveContent();
	static FArchiveCustomContent MakeWespeBarrageArchiveContent();
	static FArchiveCustomContent MakeDigInArchiveContent();
	static FArchiveCustomContent MakeWallBreachArchiveContent();
	static FArchiveCustomContent MakeAdrenalineShotArchiveContent();
	static FArchiveCustomContent MakeCapturingTrucksArchiveContent();
	static FArchiveCustomContent MakeDocumentArchiveContent(const FDocumentTakeoverContent& DocumentContent);
	const UDataTable* GetTrainingOptionsTable();
};
