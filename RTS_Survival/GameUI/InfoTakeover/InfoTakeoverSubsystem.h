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

/** How a takeover that a caller waits on ended. */
enum class EInfoTakeoverRequestResult : uint8
{
	// The player closed it; the game is unpaused again.
	Closed,
	// It never opened: the takeovers are turned off or its widget could not be created.
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

	// Set when a caller waits for the close (OnTakeoverRequestFinished); INDEX_NONE for fire-and-forget takeovers.
	UPROPERTY()
	int32 RequestId = INDEX_NONE;
};

/**
 * @brief Opens info takeovers for the mission nodes, one at a time: a call while another takeover, a cinematic, the
 * tech tree or the archive holds the screen waits in a queue. Content is taken when the node is called. Calls do
 * nothing once the player ticked "never show these info widgets again" (the gameplay setting is off). Every takeover is
 * also archived (with the archive pop-up) when it closes, or at once while the takeovers are off. A caller that must
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
	 * @return False when takeovers are turned off or the vehicle or row is missing.
	 */
	bool QueueVehicleTakeoverFromLibrary(ATankMaster* Vehicle, const FName LibraryEntry);

	/**
	 * @brief Queues a vehicle takeover with the caller's own text and picture.
	 * @param Vehicle The live vehicle whose weapons and armour are shown.
	 * @param Entry Name, description, year and picture; empty fields fall back to the unit's training data.
	 * @return False when takeovers are turned off or the vehicle is missing.
	 */
	bool QueueVehicleTakeover(ATankMaster* Vehicle, const FVehicleInfoTakeoverEntry& Entry);

	/**
	 * @brief Queues the camera controls field manual; a mission waits on OnTakeoverRequestFinished with the returned id
	 * so its voice lines start only after the player closed it.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; INDEX_NONE when takeovers are off
	 * (nothing opens and nothing is broadcast, so the caller continues at once).
	 */
	int32 QueueCameraControlsTakeover();

	/**
	 * @brief Queues the cover field manual (what the green, yellow and red move preview means), waited on like the
	 * camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; INDEX_NONE when takeovers are off.
	 */
	int32 QueueCoverTakeover();

	/**
	 * @brief Queues the spaced armour field manual (side plates and how hits knock them off), waited on like the
	 * camera controls takeover.
	 * @return The request id that OnTakeoverRequestFinished reports exactly once; INDEX_NONE when takeovers are off.
	 */
	int32 QueueSpacedArmourTakeover();

	// Fires once per queued request id: closed, skipped (turned off while waiting, broken widget) or cancelled.
	FOnInfoTakeoverRequestFinished OnTakeoverRequestFinished;

	/** @return False after "never show these info widgets again"; the settings menu turns it back on. */
	static bool GetAreInfoTakeoversEnabled();

	/** @return The row names of the vehicle library, for the mission node's dropdown. */
	static TArray<FName> GetVehicleLibraryEntryNames();

	/** @return The library prefab, or null when the row is missing. */
	static const FVehicleInfoTakeoverEntry* FindVehicleLibraryEntry(const FName LibraryEntry);

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
	void DropQueuedTakeoversWhileTurnedOff();
	bool GetCanOpenTakeoverNow() const;
	void StartRetryTicker();
	void StopRetryTicker();

	/** @return The opened widget, or null when its class could not be created. */
	UW_InfoTakeover* CreateTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover);
	UW_InfoTakeover* CreateVehicleTakeoverWidget(const FQueuedInfoTakeover& QueuedTakeover) const;
	UW_InfoTakeover* CreateCameraControlsTakeoverWidget() const;
	UW_InfoTakeover* CreateCoverTakeoverWidget() const;
	UW_InfoTakeover* CreateSpacedArmourTakeoverWidget() const;

	/**
	 * @brief Queues a field manual takeover that a mission node waits on; shared by every tutorial kind.
	 * @param Kind The tutorial to show.
	 * @return The request id; INDEX_NONE when takeovers are off (it is archived instead when the HUD exists).
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
	static FArchiveCustomContent MakeCameraControlsArchiveContent();
	static FArchiveCustomContent MakeCoverArchiveContent();
	static FArchiveCustomContent MakeSpacedArmourArchiveContent();
	const UDataTable* GetTrainingOptionsTable();
};
