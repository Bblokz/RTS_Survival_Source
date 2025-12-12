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

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_Portrait> M_PortraitWidget;

	UPROPERTY()
	TWeakObjectPtr<UPlayerAudioController> M_AudioController;

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

	/** Validate the weak audio controller and log an error if invalid. */
	bool GetIsValidAudioController() const;

	/** Add a new portrait request to the queue. */
	void EnqueuePortraitRequest(ERTSPortraitTypes PortraitType, USoundBase* VoiceLine);

	/** Start the next portrait from the queue or hide the widget if none remain. */
	void StartNextPortraitFromQueue();

	/** Called when the current portrait duration (voice line) has elapsed. */
	void OnCurrentPortraitFinished();
};
