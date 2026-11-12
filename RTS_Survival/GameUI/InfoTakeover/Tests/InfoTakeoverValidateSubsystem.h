// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "InfoTakeoverValidateSubsystem.generated.h"

class ACPPController;
class ATankMaster;
class UW_InfoTakeover;

enum class EInfoTakeoverValidatePhase : uint8
{
	WaitingForHud,
	StartingGame,
	SettlingVehicles,
	WatchingTakeovers,
	// A takeover queued while the screen is locked must wait, then open from the retry ticker once unlocked.
	DeferralLocked,
	DeferralReleased,
	// Pop-ups on the HUD, then the opened archive, captured; the custom entries are counted.
	ArchivePopups,
	ArchiveOpen,
	Done
};

/**
 * @brief Headless check of the vehicle info takeovers, started with -InfoTakeoverValidate (best with -game
 * -RenderOffscreen): spawns every library row's reference vehicle hidden below the map, logs the content each would
 * show, queues them all plus a very long and a very short custom one, screenshots the typing stages to
 * Saved/InfoTakeover/, ticks "never show again" on the last and checks the switch, a locked screen and the archive entries. Grep INFO_TAKEOVER. Restores the
 * player's setting and quits. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UInfoTakeoverValidateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EInfoTakeoverValidatePhase M_Phase = EInfoTakeoverValidatePhase::WaitingForHud;
	double M_PhaseStartSeconds = 0.0;

	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TArray<TObjectPtr<ATankMaster>> M_Vehicles;

	TArray<FName> M_VehicleRowNames;

	// The takeover being watched, its label for screenshots and when it opened.
	TWeakObjectPtr<UW_InfoTakeover> M_WatchedTakeover;
	FString M_WatchedLabel;
	double M_WatchedOpenSeconds = 0.0;
	double M_WatchedDoneSeconds = 0.0;
	int32 M_WatchedShotMask = 0;
	uint64 M_DoneShotFrame = 0;
	bool bM_HasCapturedArchive = false;
	bool bM_HasCapturedPopups = false;

	TArray<FString> M_TakeoverLabels;
	int32 M_OpenedTakeoverCount = 0;
	int32 M_Failures = 0;
	bool bM_OriginalSettingValue = true;

	void EnterPhase(const EInfoTakeoverValidatePhase NewPhase);
	double GetSecondsInPhase() const;

	void TickWaitingForHud();
	void TickStartingGame();
	void TickSettlingVehicles();
	void TickWatchingTakeovers();

	void SpawnReferenceVehicles();
	void LogAndQueueTakeovers();
	void LogVehicleContent(const FName RowName, const ATankMaster& Vehicle) const;
	void QueueCustomLayoutTakeovers(ATankMaster& Vehicle);

	void StartWatching(UW_InfoTakeover& Takeover);
	void WatchActiveTakeover(UW_InfoTakeover& Takeover);
	void CloseWatchedTakeover(UW_InfoTakeover& Takeover);
	void LogTakeoverState(const UW_InfoTakeover& Takeover, const TCHAR* Moment) const;
	void Capture(const FString& Label) const;

	void CheckNeverShowAgainAndStartDeferral();
	void StartDeferralCheck();
	void TickDeferralLocked();
	void TickDeferralReleased();
	void RestoreSettingAndFinish();
	void TickArchivePopups();
	void TickArchiveOpen();
	void CheckArchiveEntries();
	void DismissDifficultyPicker() const;
	void Fail(const FString& Reason);
	void Finish();
};
