// RTSMusicManager.h

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Music/RTSMusicTypes.h"
#include "RTSMusicManager.generated.h"

class UAudioComponent;


/**
 * @class URTSMusicManager
 * @brief Central music controller that persists in the GameInstance and handles all background music playback.
 *
 * This object is instantiated by the GameInstance and manages music playback throughout the game,
 * with track definitions provided once at startup. It supports playing music based on type,
 * looping tracks, randomization, and optional fading between tracks.
 *
 * @details
 * === How to use ===
 * 1. In your GameInstance blueprint, call:
 *    - `URTSGameInstance::GetMusicManager()->InitMusicManager(...)`
 *      to provide your array of `FRTSMusicTypes`, one for each music category.
 * 2. In your PlayerController (on every level load), call:
 *    - `URTSGameInstance::GetMusicManager()->SetupMusicManagerForNewWorld(this, ERTSMusicType::MainTheme)`
 *      passing in `this` as the world context object and the starting track type.
 * 3. To switch music at runtime:
 *    - Call `PlayNewMusicTracks(...)` to play a different category of music, optionally with fade-out.
 *    - Call `PlayMusicLoop(...)` to loop a track multiple times and then switch to a different one.
 * 4. (Optional) In `EndPlay()` of your PlayerController, call `TeardownForOldWorld()` to explicitly clean up.
 *
 * @note All sound playback is handled through a single 2D UAudioComponent created per-world,
 * @note and attached to the world context (typically the PlayerController) via `SpawnSound2D(...)`.
 *
 * @note Music does not persist across level transitions; a new audio component is created
 * @note in every world when `SetupMusicManagerForNewWorld()` is called.
 *
 * @note You are responsible for calling `SetupMusicManagerForNewWorld()` in every new level’s PlayerController.
 *
 * === Lifecycle Overview ===
 * @note The MusicManager itself is created once in the GameInstance (`NewObject<URTSMusicManager>`) during `Init()`.
 * @note It is stored in a UPROPERTY and optionally `AddToRoot()` to prevent garbage collection.
 * @note The music manager lives for the entire game session unless explicitly removed.
 *
 * @note The UAudioComponent is created using `UGameplayStatics::SpawnSound2D()` per level load.
 * @note It is NOT persistent across level transitions (`bPersistAcrossLevelTransition = false`).
 * @note It is explicitly stopped and destroyed when the PlayerController or world unloads.
 * @note A new component is spawned in the new world when the PlayerController calls `SetupMusicManagerForNewWorld()`.
 *
 * @note This design ensures no overlap of audio between maps, avoids duplicate playback bugs,
 * @note and guarantees a clean music state for each level load.
 */

UCLASS(Blueprintable)
class RTS_SURVIVAL_API URTSMusicManager : public UObject
{
    GENERATED_BODY()

public:
    /** @brief Initialize with your music type definitions. */
    UFUNCTION(BlueprintCallable, Category="Music")
    void InitMusicManagerTracks(const TArray<FRTSMusicTypes>& InMusicTypes);

    /**
     * @brief Set up for a new world and play initial track.
     * @param InWorldContextObject  The context for spawning audio (typically PlayerController).
     * @param MusicTypeStart        Which music type to start immediately.
     */
    UFUNCTION(BlueprintCallable, Category="Music")
    void SetupMusicManagerForNewWorld(UObject* InWorldContextObject, ERTSMusicType MusicTypeStart);

    /**
     * @brief Switch to a new music category at runtime.
     * @param NewMusicType  Category to play.
     * @param bFade         If true, fade out current track over 5s before switching.
     */
    UFUNCTION(BlueprintCallable, Category="Music")
    void PlayNewMusicTracks(ERTSMusicType NewMusicType, bool bFade = false);

    /**
     * @brief Loop the current track a number of extra times, then pick a new one.
     * @param NewMusicType   Category to play.
     * @param NumLoops       Number of extra loops (plays 1 + NumLoops).
     * @param bFadeIntoLoop  If true, fade out before first loop.
     */
    UFUNCTION(BlueprintCallable, Category="Music")
    void PlayMusicLoop(ERTSMusicType NewMusicType, int32 NumLoops, bool bFadeIntoLoop = false);

    /** @brief Stop immediately (no fade) and cancel looping. */
    UFUNCTION(BlueprintCallable, Category="Music")
    void StopMusic();

    /** @brief Tear down the audio component and clear world context. */
    UFUNCTION(BlueprintCallable, Category="Music")
    void TeardownForOldWorld();

private:
    /** @brief Finds definition struct for a music type, or nullptr if none. */
    const FRTSMusicTypes* FindMusicDef(ERTSMusicType MusicType) const;

    /** @brief Returns a random track index from Def, excluding CurrentIndex if possible. */
    int32 GetNextRandomTrackIndex(const FRTSMusicTypes& Def, int32 CurrentIndex) const;

    /** @brief Configures looping state for PlayMusicLoop(). */
    void SetupLoopState(ERTSMusicType MusicType, int32 NumLoops, bool bFadeIntoLoop);

    /** @brief Stops current track and immediately plays at M_CurrentTrackIndex. */
    void StopAndPlayCurrentTrack() const;

    /** @brief Fades out current track over 5s, then calls OnFadeFinished(). */
    void FadeOutCurrentTrack();

    /** @brief Plays the track at M_CurrentTrackIndex on M_AudioComponent. */
    void PlayCurrentTrack() const;

    /** @brief Destroys the existing audio component if any. */
    void DestroyAudioComponent();

    /** @brief Spawns a new audio component for SoundToPlay. */
    bool SetupAudioComponent(USoundBase* SoundToPlay);

    /** @brief Called when a track finishes playing. */
    UFUNCTION()
    void OnTrackFinished();

    /** @brief Called after fade-out completes. */
    UFUNCTION()
    void OnFadeFinished();

    /** @brief Plays the track at the given index (internal). */
    void PlayTrack(int32 TrackIndex) const;

    /** @brief Chooses the next track index for non-looped playback. */
    int32 ChooseNextTrackIndex() const;

    /** @brief Reports error when no tracks exist for MusicType. */
    void OnNoMusicForType(ERTSMusicType MusicType);

    /** @brief Checks that M_WorldContextObject is valid. */
    bool EnsureValidWorldContext() const;

    /** @brief Checks that M_AudioComponent is valid. */
    bool EnsureValidAudioComponent() const;

private:
    /** Your bucketed playlists from InitMusicManagerTracks. */
    UPROPERTY()
    TArray<FRTSMusicTypes> M_MusicDefinitions;

    /** What music type we’re currently playing. */
    ERTSMusicType M_CurrentMusicType = ERTSMusicType::None;

    /** Index within the current music type’s track array. */
    int32 M_CurrentTrackIndex = INDEX_NONE;

    /** World context object for spawning sounds & timers. */
    UPROPERTY()
    UObject* M_WorldContextObject = nullptr;

    /** The currently playing audio component. */
    UPROPERTY()
    UAudioComponent* M_AudioComponent = nullptr;

    /** Timer handle for fade-out callbacks. */
    FTimerHandle M_FadeTimerHandle;

    /** Are we in loop mode? */
    bool bM_LoopMode = false;

    /** Number of extra loops to play (Play 1 + M_LoopCount times). */
    int32 M_LoopCount = 0;

    /** Remaining loops of the current track. */
    int32 M_LoopsRemaining = 0;

    /** Fade before looping first time? */
    bool bM_LoopFade = false;
};
