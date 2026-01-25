// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"

#include "Components/AudioComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Audio/Pooling/RTSPooledAudio.h"
#include "RTS_Survival/Audio/Settings/RTSSpatialAudioSettings.h"

#include "PlayerAudioController.generated.h"

/**
 * @brief Internal per-instance data for pooled spatial audio.
 */
USTRUCT()
struct RTS_SURVIVAL_API FRTSPooledSpatialAudioInstance
{
	GENERATED_BODY()

	UPROPERTY()
	UAudioComponent* Component = nullptr;

	// True while this audio component is currently considered active by the pool.
	bool bIsActive = false;

	// World time when the current sound started; used to find the "oldest" active instance.
	float StartTimeSeconds = -1.0f;
};

/**
 * @brief Represents a single voice-line that has been queued to play later.
 */
USTRUCT()
struct FQueuedVoiceLine
{
	GENERATED_BODY()

	/** The sound to play when the current line finishes. */
	UPROPERTY()
	USoundBase* VoiceLine = nullptr;

	/** The enum type of the queued voice-line. */
	ERTSVoiceLine VoiceLineType = ERTSVoiceLine::None;

	/** Whether to force this line to interrupt when dequeued. */
	bool bForcePlay = false;


	/** @return true if this entry currently holds a valid queued line. */
	bool IsValid() const { return VoiceLine != nullptr; }

	/** Clear out the queued entry. */
	void Reset()
	{
		VoiceLine = nullptr;
		VoiceLineType = ERTSVoiceLine::None;
		bForcePlay = false;
	}
};

USTRUCT()
struct FQueuedAnnouncerVoiceLine
{
	GENERATED_BODY()
	/** The sound to play when the current line finishes. */
	UPROPERTY()
	USoundBase* VoiceLine = nullptr;

	EAnnouncerVoiceLineType VoiceLineType = EAnnouncerVoiceLineType::None;

	bool IsQueuedLineACustomAnnouncerLine() const
	{
		return (VoiceLineType == EAnnouncerVoiceLineType::Custom) && (VoiceLine != nullptr);
	}

	bool IsValid() const { return VoiceLine != nullptr; }

	bool IsCustomVoiceLine() const { return VoiceLineType == EAnnouncerVoiceLineType::Custom; }

	void Reset()
	{
		VoiceLine = nullptr;
		VoiceLineType = EAnnouncerVoiceLineType::None;
	}
};

USTRUCT()
struct FResourceTickSettings
{
	GENERATED_BODY()

	UPROPERTY()
	USoundBase* M_ResourceTickSound = nullptr;

	UPROPERTY()
	FTimerHandle M_ResourceTickSoundInitHandle;

	// Name of the parameter inside the resource‐tick sound cue’s modulator
	UPROPERTY()
	FName M_ResourceTickParamName = NAME_None;

	// Current value of the resource‐tick parameter
	UPROPERTY()
	float M_ResourceTickSpeed = 0.f;
	// Max value of the resource‐tick parameter
	UPROPERTY()
	float M_ResourceTickMaxSpeed = 1.f;

	UPROPERTY()
	float M_ResourceTickInitSpeed = 1.f;
};

USTRUCT()
struct FCurrentVoiceLineState
{
	GENERATED_BODY()
	void SetCurrentVoiceLineAsRTSVoiceLine(const ERTSVoiceLine VoiceLineType)
	{
		M_CurrentVoiceLineType = VoiceLineType;
		M_CurrentAnnouncerVoiceLineType = EAnnouncerVoiceLineType::None;
	}

	void SetCurrentVoiceLineAsAnnouncerVoiceLine(const EAnnouncerVoiceLineType AnnouncerVoiceLineType)
	{
		M_CurrentAnnouncerVoiceLineType = AnnouncerVoiceLineType;
		M_CurrentVoiceLineType = ERTSVoiceLine::None;
	}

	bool IsCurrentVoiceLineAnnouncerVoiceLine() const
	{
		return M_CurrentAnnouncerVoiceLineType != EAnnouncerVoiceLineType::None;
	}

	bool IsCurrentCustomAnnouncerVoiceLine() const
	{
		return M_CurrentAnnouncerVoiceLineType == EAnnouncerVoiceLineType::Custom;
	}

	bool IsCurrentVoiceLineRTSVoiceLine() const
	{
		return M_CurrentVoiceLineType != ERTSVoiceLine::None;
	}

	ERTSVoiceLine GetCurrentRTSVoiceLine() const
	{
		return M_CurrentVoiceLineType;
	}

	EAnnouncerVoiceLineType GetCurrentAnnouncerVoiceLine() const
	{
		return M_CurrentAnnouncerVoiceLineType;
	}

	void Reset()
	{
		M_CurrentVoiceLineType = ERTSVoiceLine::None;
		M_CurrentAnnouncerVoiceLineType = EAnnouncerVoiceLineType::None;
	}

private:
	/** What kind of line is currently playing (or last played) */
	ERTSVoiceLine M_CurrentVoiceLineType = ERTSVoiceLine::None;

	EAnnouncerVoiceLineType M_CurrentAnnouncerVoiceLineType = EAnnouncerVoiceLineType::None;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerAudioController : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerAudioController();

	/** 
	 * Play a voice line for the given unit.
	 * @param PrimarySelectedUnit	The unit whose type determines which lines to use
	 * @param VoiceLineType		The specific kind of line (Select, Attack, etc.)
	 * @param bForcePlay			If true, stops any current line and plays this one immediately
	 * @param bQueueIfNotPlayed 	If true, queues this line to play if currently another line is playing.
	 */
	void PlayVoiceLine(const AActor* PrimarySelectedUnit, ERTSVoiceLine VoiceLineType, bool bForcePlay = false,
	                   bool bQueueIfNotPlayed = false);

	/**
	 * 
	 * @param Type The announcer line to play.
	 * @param bQueueIfNotPlayed Will queue the line if queue is NOT taken by custom announcer voice line (those have priority). 
	 */
	void PlayAnnouncerVoiceLine(
		const EAnnouncerVoiceLineType Type,
		const bool bQueueIfNotPlayed = true,
		const bool InterruptRegularVoiceLines = false);

	/**
	 * @brief Play a custom announcer voice line. Will interrupt any non-custom announcer voice line currently playing.
	 * @param CustomVoiceLineSound The custom voice line to play.
	 * @param bQueueIfNotPlayed Queue the custom line if another custom line is still playing.
	 */
	void PlayCustomAnnouncerVoiceLine(
		USoundBase* CustomVoiceLineSound,
		const bool bQueueIfNotPlayed);

	void SetSuppressRegularVoiceLines(const bool bSuppress);


	/**
	 * @brief Play a 3D (spatial) voice‐line at a world location.
	 * @param PrimarySelectedUnit  The unit whose voice‐line category to use.
	 * @param VoiceLineType        Which event (Select, Attack, etc.).
	 * @param Location             World location to play the sound at.
	 * @param bIgnorePlayerCooldown
	 * @return The audio component if the voice line could be played.
	 * @post Audio component has not started playing yet; caller must invoke Play() on it after overriding attenuation/concurrency as needed.
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	UAudioComponent* PlaySpatialVoiceLine(
		const AActor* PrimarySelectedUnit,
		ERTSVoiceLine VoiceLineType,
		const FVector& Location, const bool bIgnorePlayerCooldown
	);
	/**
	 * @brief Retrieve (and advance) the next voice‐line asset for the given unit type and event.
	 *
	 * This will behave exactly as if you had called PlayVoiceLine() (minus the actual audio playback),
	 * advancing the internal index so subsequent calls cycle through the array.
	 *
	 * @param UnitType        The unit’s voice‐line category (e.g. LightTank).
	 * @param VoiceLineType   The specific event (Select, Attack, etc.).
	 * @return The next USoundBase* in the rotation, or nullptr on error.
	 */
	UFUNCTION(BlueprintCallable, Category="Audio")
	USoundBase* GetNextVoiceLine(ERTSVoiceLineUnitType UnitType, ERTSVoiceLine VoiceLineType);

	/**
	 * @brief Init the pause times for the audio controller.
	 * @param ExtraPauseTimes Extra time to add after a voice line of a type has finished that indicates how much longer
	 * the audio controller must wait before being able to play another voice line of the same type.
	 * @param VoiceLineSpatialConcurrency
	 * @param VoiceLineSpatialAttenuation
	 * @param PauseTimesAnnouncer
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitPlayerAudioPauseTimes(
		TMap<ERTSVoiceLine, float> ExtraPauseTimes, USoundConcurrency* VoiceLineSpatialConcurrency, USoundAttenuation*
		VoiceLineSpatialAttenuation, const TMap<EAnnouncerVoiceLineType, float> PauseTimesAnnouncer);
	/**
	 * @brief Setup the voice lines for the audio controller.
	 * @note if a voice line unit type was already registered this will not overwrite it.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitUnitVoiceLine(FRTSVoiceLineSettings NewUnitVoiceLines);

	/**
	 * @brief Setup the voice lines for the announcer
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitAnnouncerVoiceLines(FAnnouncerVoiceLineData AnnouncerVoiceLines);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitResourceTickSound(USoundBase* ResourceTickSound, const FName& ResourceTickName,
	                           const float ResourceTickMaxSpeed, const float
	                           InitResourceTickSpeed);

	void PlayResourceTickSound(const bool bPlay, const float IntensityRequested);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Core playback logic */
	void PlayAudio(USoundBase* VoiceLine, ERTSVoiceLine VoiceLineType, bool bForcePlay, bool bQueueIfNotPlayed);


	/** If a line finishes and one is queued, play it now. */
	UFUNCTION()
	void HandleAudioFinished();

	/** Record a line to play as soon as the current one finishes.
	 * Only queues if no announcer is queued as it would otherwise take very long for this line to be played (after announcer lines).
	 */
	void QueueVoiceLine(
		USoundBase* VoiceLine,
		ERTSVoiceLine VoiceLineType,
		bool bForcePlay
	);

	UPROPERTY()
	FQueuedVoiceLine M_QueuedVoiceLineEntry;

	UPROPERTY()
	FQueuedAnnouncerVoiceLine M_QueuedAnnouncerVoiceLineEntry;

	/** The audio component we spawn on BeginPlay()
	 * this component plays the voice lines of the units*/
	UPROPERTY()
	UAudioComponent* M_VoiceLineAudioComponent = nullptr;

	bool GetIsValidVoiceLineAudioComp() const;


	/** The audio component we spawn on BeginPlay()
	 * this component plays the resource spending tick sound*/
	UPROPERTY()
	UAudioComponent* M_ResourceTickAudioComponent = nullptr;

	FResourceTickSettings ResourceTickSettings;

	bool GetIsValidResourceAudioComponent() const;


	/** All voice‐line data, keyed by unit type */
	UPROPERTY()
	TMap<ERTSVoiceLineUnitType, FUnitVoiceLinesData> M_UnitVoiceLineData;

	/** All regular announcer lines (not the custom ones) keyed by announce line */
	UPROPERTY()
	FAnnouncerVoiceLineData M_AnnouncerVoiceLineData;

	/** Extra pause (seconds) to add after a line finishes before the same type can play again */
	UPROPERTY()
	TMap<ERTSVoiceLine, float> M_AudioPauseTimes;

	/** At what time the cooldown is over for a given voice line type */
	float GetRTSVoiceLineEndTimeAfterCooldown(const ERTSVoiceLine VoiceLineType, const USoundBase* ValidVoiceLine,
	                                          const float Now) const;
	/** At what time the cooldown is over for an announcer voice line type */
	float GetAnnouncerVoiceLineEndTimeAfterCooldown(const EAnnouncerVoiceLineType VoiceLineType,
	                                                const USoundBase* ValidVoiceLine, const float Now) const;
	

	/** Extra pause (seconds) to add after an announcer line finishes before the same type can play again. */
	UPROPERTY()
	TMap<EAnnouncerVoiceLineType, float> M_AnnouncerExtraPauseTimes;

	/** Tracks when each announcer line type is next allowed to play again (world time in seconds). */
	UPROPERTY()
	TMap<EAnnouncerVoiceLineType, float> M_AnnouncerPlayCooldownEndTimes;

	/** Tracks the time after which the cooldown is complete and a new voice line is allwoed to play */
	UPROPERTY()
	float M_VoiceLineCooldownEndTime;

	/** What kind of line is currently playing (or last played) */
	FCurrentVoiceLineState M_CurrentVoiceLineState;

	bool bM_SuppressRegularVoiceLines = false;

	/** Helper to figure out what unit‐type map to use */
	static bool GetValidVoiceLineTypeForUnit(const AActor* PrimarySelectedUnit,
	                                         ERTSVoiceLineUnitType& OutVoiceLineUnitType);

	/** Pulls a single sound asset out of our TMap */
	USoundBase* GetVoiceLineFromTypes(ERTSVoiceLineUnitType UnitType,
	                                  ERTSVoiceLine VoiceLineType);

	USoundBase* GetAnnouncerVoiceLineFromType(EAnnouncerVoiceLineType VoiceLineType);

	void QueueAnnouncerLineIfNoAnnouncerLineIsQueued(
		const EAnnouncerVoiceLineType VoiceLineType,
		USoundBase* VoiceLine);

	/** Error‐reporting if you ask for a unit‐type we don’t have data for */
	static void OnCannotFindUnitType(ERTSVoiceLineUnitType UnitType, ERTSVoiceLine VoiceLineType);

	/** @return Whether this voice line type is already playing and should block the new one on a not force play*/
	bool IsRTSVoiceLineOnCooldown(const float Now);
	bool IsSameRTSVoiceLineAsPrevious(const ERTSVoiceLine VoiceLineType) const;
	bool IsAnnouncerVoiceLineOnCooldown(const EAnnouncerVoiceLineType VoiceLineType, const float Now);
	bool IsSameAnnouncerVoiceLineAsPrevious(const EAnnouncerVoiceLineType VoiceLineType) const;
	void DebugAudioController(const FString& Message) const;

	void AdjustVoiceLineForCombatSituation(const AActor* PrimarySelectedUnit,
	                                       ERTSVoiceLine& OutVoiceLineType,
	                                       const ERTSVoiceLineUnitType OutVoiceLineUnitType,
	                                       bool& OutOverrideForcePlay);
	/**
	 * @brief Chooses the appropriate selection voice line variant based on combat, damage, and rapid re-selection.
	 *
	 * @param UnitType             The unit’s voice-line category (used for annoyance tracking).
	 * @param bIsInCombat          True if the unit is currently engaged in combat.
	 * @param bIsDamaged           True if the unit’s health is ≤ 80%.
	 * @param OutVoiceLineType     [out] Set to the chosen voice-line enum (Select, SelectNeedRepairs, SelectStressed, or SelectAnnoyed).
	 * @param OutOverrideForcePlay [out] Set to true when SelectAnnoyed is chosen, forcing immediate playback.
	 *
	 * @pre Caller must have already determined that VoiceLineType == ERTSVoiceLine::Select.
	 * @post On SelectAnnoyed, the internal selection counter resets and OutOverrideForcePlay == true.
	 * @note Rapidly selecting the same unit type 4 times within SelectionAnnoyWindow will trigger SelectAnnoyed.
	 * @note When SelectAnnoyed is chosen, OutOverrideForcePlay will be set so that the annoyed line interrupts any current audio.
	 */
	void DetermineSelectionVoiceLine(
		ERTSVoiceLineUnitType UnitType,
		const bool bIsInCombat, const bool bIsDamaged,
		ERTSVoiceLine& OutVoiceLineType, bool& OutOverrideForcePlay);

	void DetermineAttackVoiceLine(const bool bIsInCombat, const bool bIsDamaged,
	                              ERTSVoiceLine& OutVoiceLineType) const;

	/** Tracks quick-select counts per unit type */
	TMap<ERTSVoiceLineUnitType, int32> M_SelectionCounts;

	/** When the quick-select window started per unit type */
	TMap<ERTSVoiceLineUnitType, float> M_SelectionWindowStartTimes;

	/** After this many selects in WINDOW seconds → annoyed */
	static constexpr int32 SelectionAnnoyThreshold = 4;

	/** Time window (seconds) for annoyance counting */
	static constexpr float SelectionAnnoyWindow = 3.0f;

	UPROPERTY()
	USoundConcurrency* M_VoiceLineConcurrencySettings = nullptr;
	UPROPERTY()
	USoundAttenuation* M_VoiceLineAttenuationSettings = nullptr;

	/**
     * @brief Play a spatial voice line using the pooled UAudioComponents.
     * @param VoiceLine  The sound asset to play.
     * @param Location   World location to emit the sound.
     * @param Owner      Owning actor (used only for a sensible rotation).
	 * @param bIsForceRequest  true when called by ForcePlaySpatialVoiceLine and we may reuse the oldest active instance.
	 * @return The pooled audio component if the voice line could be played, otherwise nullptr.
     */
	UAudioComponent* SpawnSpatialVoiceLineOneShot(
		USoundBase* VoiceLine,
		const FVector& Location,
		const AActor* Owner,
		const bool bIsForceRequest);

	/** Per-type cooldown end times for spatial lines. */
	UPROPERTY()
	TMap<ERTSVoiceLine, float> M_SpatialCooldownEndTimes;

	void OnValidResourceSound() const;
	void NoValidTickSound_SetTimer();

	// === Pooled spatial audio state ===


	// Owner actor for all pooled spatial UAudioComponents.
	UPROPERTY()
	ARTSPooledAudio* M_SpatialAudioPoolOwner;

	// Pre-allocated spatial audio instances.
	UPROPERTY()
	TArray<FRTSPooledSpatialAudioInstance> M_SpatialAudioPoolInstances;

	// Indices of free / currently active pooled audio instances.
	TArray<int32> M_SpatialAudioFreeList;
	TArray<int32> M_SpatialAudioActiveList;

	// Default attenuation loaded from URTSSpatialAudioSettings.
	UPROPERTY()
	USoundAttenuation* M_DefaultSpatialAttenuation = nullptr;

	// === Pooled spatial audio helpers ===

	/** Initialize pool owner + components from URTSSpatialAudioSettings. Called from BeginPlay. */
	void BeginPlay_InitSpatialAudioPool();

	/** Destroy pool components and the pool owner. Called from EndPlay. */
	void EndPlay_ShutdownSpatialAudioPool();

	/** Ensure the pool owner actor exists. */
	void Init_CreateSpatialAudioPoolOwner();

	/** Spawn and configure pooled UAudioComponents. */
	void Init_SpawnSpatialAudioPool(const int32 PoolSize);

	/** Get whether the pool owner actor is valid, with error logging. */
	bool GetIsValidSpatialAudioPoolOwner() const;

	/** Find the oldest active instance; returns INDEX_NONE if none. */
	int32 FindOldestActiveSpatialAudioIndex() const;

	/**
	 * @brief Reserve/allocate a pool index.
	 * @param bIsForceRequest true when coming from ForcePlaySpatialVoiceLine (can reuse oldest).
	 * @return Pool index or INDEX_NONE when no instance can be allocated.
	 */
	int32 AllocateSpatialAudioIndex(const bool bIsForceRequest);

	/** Mark an instance dormant and return it to the free list. */
	void MarkSpatialIndexDormant(const int32 Index);

	/** Per-frame update to automatically mark finished audio as dormant. */
	void Tick_UpdateSpatialAudioPool(const float DeltaTime);

	/**
	 * @brief Acquire a UAudioComponent from the pool.
	 * @param bIsForceRequest true when we are allowed to interrupt the oldest instance.
	 * @return A pooled audio component or nullptr when no free component is available.
	 */
	UAudioComponent* AcquirePooledSpatialAudioComponent(const bool bIsForceRequest);

	bool GetHasToPlayAnnouncerLineAs2DSound(EAnnouncerVoiceLineType Type) const;
	void PlayAnnouncerLineAs2DSound(EAnnouncerVoiceLineType Type);
};
