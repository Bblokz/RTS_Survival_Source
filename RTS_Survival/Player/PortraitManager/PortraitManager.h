// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "Sound/SoundBase.h"
#include "PortraitManager.generated.h"

class UW_Portrait;
class UPlayerAudioController;

/**
 * @brief Single queued portrait request (type + voice line).
 */
USTRUCT()
struct FPortraitQueueEntry
{
	GENERATED_BODY()

	UPROPERTY()
	ERTSPortraitTypes PortraitType = ERTSPortraitTypes::None;

	UPROPERTY()
	USoundBase* VoiceLine = nullptr;

	/** Cached duration of the voice line in seconds. */
	UPROPERTY()
	float DurationSeconds = 0.0f;

	bool IsValid() const
	{
		return (PortraitType != ERTSPortraitTypes::None) && VoiceLine;
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerPortraitManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerPortraitManager();

	/**
	 * @brief Initialize the portrait manager with the UI widget and audio controller.
	 * @param InPortraitWidget Widget instance already created by the player controller.
	 * @param AudioControllerForVoiceLines Audio controller used to play custom announcer lines.
	 */
	void InitPortraitManager(UW_Portrait* InPortraitWidget, UPlayerAudioController* AudioControllerForVoiceLines);

	/**
	 * @brief Queue a new portrait + voice line.
	 * The portrait will be shown for as long as the voice line duration.
	 * Voice line is played as a custom announcer line on the player audio controller.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PlayPortrait(ERTSPortraitTypes PortraitType, USoundBase* VoiceLine);

	/** Immediately hide the active portrait and clear all queued portrait requests. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void StopCurrentPortraitPlayback();

	/**
	 * @brief Shows portraits on HostWidget instead of the HUD portrait until ClearCinematicPortraitHost. A cinematic
	 * overlay that hides the HUD (the mission camera tour) hosts its own copy, so speaker and words stay on screen.
	 * A portrait that is playing moves over with its subtitle, which keeps following the audio.
	 */
	void SetCinematicPortraitHost(UW_Portrait* HostWidget);

	/** Hands portraits back to the HUD widget when HostWidget is the current host; a playing line moves back. */
	void ClearCinematicPortraitHost(const UW_Portrait* HostWidget);

	/** The HUD portrait (part of the main game UI). */
	UW_Portrait* GetHudPortraitWidget() const { return M_PortraitWidget.Get(); }

	/** The widget portraits show on now: the cinematic host while one is set, else the HUD portrait. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Portrait")
	UW_Portrait* GetActivePortraitWidget() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Portrait")
	bool GetIsPortraitPlaying() const { return bM_IsPortraitPlaying; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_Portrait> M_PortraitWidget;

	UPROPERTY()
	TWeakObjectPtr<UPlayerAudioController> M_AudioController;

	// Overlay copy of the portrait that replaces the HUD portrait while a cinematic hides the HUD.
	UPROPERTY()
	TWeakObjectPtr<UW_Portrait> M_CinematicHostWidget;

	// The request on screen while bM_IsPortraitPlaying; moved to the other widget when the host changes.
	UPROPERTY()
	FPortraitQueueEntry M_CurrentRequest;

	// Queue of pending portrait requests.
	UPROPERTY()
	TArray<FPortraitQueueEntry> M_PortraitQueue;

	// True while a portrait is currently active.
	UPROPERTY()
	bool bM_IsPortraitPlaying = false;

	// Timer driving "portrait finished" based on voice-line duration.
	UPROPERTY()
	FTimerHandle M_CurrentPortraitTimerHandle;

	/** Validate the weak portrait widget and log an error if invalid. */
	bool GetIsValidPortraitWidget() const;

	/** Validates the widget portraits show on now (host or HUD). */
	bool GetIsValidActivePortraitWidget() const;

	/** Hides the portrait on From and, while a line is playing, shows it with its subtitle on To. */
	void MovePlayingPortrait(UW_Portrait* From, UW_Portrait* To) const;

	/** Validate the weak audio controller and log an error if invalid. */
	bool GetIsValidAudioController() const;

	/** Add a new portrait request to the queue. */
	void EnqueuePortraitRequest(ERTSPortraitTypes PortraitType, USoundBase* VoiceLine);

	/** Start the next portrait from the queue or hide the widget if none remain. */
	void StartNextPortraitFromQueue();

	/** Name + word-timed subtitle under the portrait on Widget. */
	void StartSubtitlesForRequest(const FPortraitQueueEntry& Request, UW_Portrait& Widget) const;

	/** Called when the current portrait duration (voice line) has elapsed. */
	void OnCurrentPortraitFinished();
};
