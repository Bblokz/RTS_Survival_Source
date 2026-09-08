// RTSMusicManager.cpp

#include "RTSMusicManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void URTSMusicManager::InitMusicManagerTracks(const TArray<FRTSMusicTypes>& InMusicTypes)
{
    M_MusicDefinitions = InMusicTypes;
}

void URTSMusicManager::SetupMusicManagerForNewWorld(
    UObject* InWorldContextObject,
    const ERTSMusicType MusicTypeStart,
    const ERTSMusicType MusicPlayedOnExhaustion)
{
    TeardownForOldWorld();
    M_WorldContextObject = InWorldContextObject;
    bM_LoopMode          = false;
    M_CurrentMusicType   = MusicTypeStart;
    ResetMusicExhaustion(MusicPlayedOnExhaustion);

    const FRTSMusicTypes* DefPtr = FindMusicDef(MusicTypeStart);
    if (not DefPtr || DefPtr->MusicTracks.Num() == 0)
    {
        OnNoMusicForType(MusicTypeStart);
        return;
    }

    M_CurrentTrackIndex = 0;
    SetupAudioComponent(DefPtr->MusicTracks[0]);
}

void URTSMusicManager::PlayNewMusicTracks(
    const ERTSMusicType NewMusicType,
    const bool bFade,
    const ERTSMusicType MusicPlayedOnExhaustion)
{
    if (not EnsureValidWorldContext() || not EnsureValidAudioComponent())
    {
        return;
    }
    if (M_CurrentMusicType == NewMusicType && M_AudioComponent->IsPlaying())
    {
        // Keep the current song and progress when updating the same category's fallback.
        M_MusicPlayedOnExhaustion = MusicPlayedOnExhaustion;
        return;
    }

    bM_LoopMode = false;

    const FRTSMusicTypes* DefPtr = FindMusicDef(NewMusicType);
    if (not DefPtr || DefPtr->MusicTracks.Num() == 0)
    {
        OnNoMusicForType(NewMusicType);
        return;
    }

    M_CurrentMusicType   = NewMusicType;
    ResetMusicExhaustion(MusicPlayedOnExhaustion);
    M_CurrentTrackIndex  = GetNextRandomTrackIndex(*DefPtr, M_CurrentTrackIndex);

    if (bFade && M_AudioComponent->IsPlaying())
    {
        FadeOutCurrentTrack();
        return;
    }

    StopAndPlayCurrentTrack();
}

void URTSMusicManager::PlayMusicLoop(
    const ERTSMusicType NewMusicType,
    const int32 NumLoops,
    const bool bFadeIntoLoop,
    const ERTSMusicType MusicPlayedOnExhaustion)
{
    if (not EnsureValidWorldContext() || not EnsureValidAudioComponent())
    {
        return;
    }

    const FRTSMusicTypes* DefPtr = FindMusicDef(NewMusicType);
    if (not DefPtr || DefPtr->MusicTracks.Num() == 0)
    {
        OnNoMusicForType(NewMusicType);
        return;
    }

    SetupLoopState(NewMusicType, NumLoops, bFadeIntoLoop);
    ResetMusicExhaustion(MusicPlayedOnExhaustion);
    M_CurrentTrackIndex = GetNextRandomTrackIndex(*DefPtr, M_CurrentTrackIndex);

    if (bM_LoopFade && M_AudioComponent->IsPlaying())
    {
        FadeOutCurrentTrack();
        return;
    }

    StopAndPlayCurrentTrack();
}

void URTSMusicManager::StopMusic()
{
    bM_LoopMode = false;
    ResetMusicExhaustion(ERTSMusicType::None);
    CancelPendingTrackCompletion();
    if (M_AudioComponent.Get() && M_AudioComponent->IsPlaying())
    {
        M_AudioComponent->OnAudioFinished.RemoveDynamic(
            this,
            &URTSMusicManager::OnTrackFinished
        );
        M_AudioComponent->Stop();
    }
}

void URTSMusicManager::TeardownForOldWorld()
{
    StopMusic();
    DestroyAudioComponent();
    M_WorldContextObject = nullptr;
}



const FRTSMusicTypes* URTSMusicManager::FindMusicDef(const ERTSMusicType MusicType) const
{
    for (const auto& Def : M_MusicDefinitions)
    {
        if (Def.MusicType == MusicType)
        {
            return &Def;
        }
    }
    return nullptr;
}

int32 URTSMusicManager::GetNextRandomTrackIndex(
    const FRTSMusicTypes& Def,
    const int32 CurrentIndex) const
{
    const int32 Num = Def.MusicTracks.Num();
    if (Num <= 1)
    {
        return Num == 1 ? 0 : INDEX_NONE;
    }

    int32 NewIndex;
    do
    {
        NewIndex = FMath::RandRange(0, Num - 1);
    }
    // Ensure to not play the same track twice.
    while (NewIndex == CurrentIndex);

    return NewIndex;
}

void URTSMusicManager::SetupLoopState(
    const ERTSMusicType MusicType,
    const int32 NumLoops,
    const bool bFadeIntoLoop)
{
    bM_LoopMode      = true;
    M_LoopCount      = FMath::Max(0, NumLoops);
    M_LoopsRemaining = M_LoopCount;
    bM_LoopFade      = bFadeIntoLoop;
    M_CurrentMusicType = MusicType;
}

void URTSMusicManager::StopAndPlayCurrentTrack()
{
    if (not EnsureValidAudioComponent())
    {
        return;
    }

    CancelPendingTrackCompletion();
    M_AudioComponent->Stop();
    PlayCurrentTrack();
}

void URTSMusicManager::CancelPendingTrackCompletion()
{
    if (UObject* WorldContextObject = M_WorldContextObject.Get())
    {
        if (UWorld* World = WorldContextObject->GetWorld())
        {
            World->GetTimerManager().ClearTimer(M_FadeTimerHandle);
        }
    }

    if (UAudioComponent* AudioComponent = M_AudioComponent.Get())
    {
        AudioComponent->OnAudioFinished.RemoveDynamic(this, &URTSMusicManager::OnTrackFinished);
    }
}

void URTSMusicManager::ResetMusicExhaustion(const ERTSMusicType MusicPlayedOnExhaustion)
{
    M_MusicPlayedOnExhaustion = MusicPlayedOnExhaustion;
    M_CompletedTrackIndices.Reset();
}

bool URTSMusicManager::TryPlayMusicOnExhaustion()
{
    if (M_MusicPlayedOnExhaustion == ERTSMusicType::None)
    {
        return false;
    }

    const ERTSMusicType MusicTypeOnExhaustion = M_MusicPlayedOnExhaustion;
    ResetMusicExhaustion(ERTSMusicType::None);
    const FRTSMusicTypes* MusicDefinition = FindMusicDef(MusicTypeOnExhaustion);
    if (not MusicDefinition || MusicDefinition->MusicTracks.IsEmpty())
    {
        OnNoMusicForType(MusicTypeOnExhaustion);
        return false;
    }

    bM_LoopMode = false;
    M_CurrentMusicType = MusicTypeOnExhaustion;
    M_CurrentTrackIndex = GetNextRandomTrackIndex(*MusicDefinition, INDEX_NONE);
    PlayCurrentTrack();
    return true;
}

void URTSMusicManager::FadeOutCurrentTrack()
{
    if (not EnsureValidAudioComponent() || not EnsureValidWorldContext())
    {
        return;
    }

    CancelPendingTrackCompletion();
    constexpr float FadeDurationSeconds = 5.f;
    M_AudioComponent->FadeOut(FadeDurationSeconds, 0.f);
    UWorld* World = GEngine->GetWorldFromContextObjectChecked(M_WorldContextObject.Get());
    World->GetTimerManager().SetTimer(
        M_FadeTimerHandle,
        this,
        &URTSMusicManager::OnFadeFinished,
        FadeDurationSeconds,
        false
    );
}

void URTSMusicManager::PlayCurrentTrack()
{
    PlayTrack(M_CurrentTrackIndex);
}

void URTSMusicManager::DestroyAudioComponent()
{
    if (M_AudioComponent.Get())
    {
        M_AudioComponent->OnAudioFinished.RemoveDynamic(
            this,
            &URTSMusicManager::OnTrackFinished
        );
        M_AudioComponent->DestroyComponent();
        M_AudioComponent = nullptr;
    }
}

bool URTSMusicManager::SetupAudioComponent(USoundBase* SoundToPlay)
{
    if (not EnsureValidWorldContext())
    {
        return false;
    }

    DestroyAudioComponent();

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(M_WorldContextObject.Get());
    if (not World)
    {
        RTSFunctionLibrary::ReportError(
            TEXT("RTSMusicManager: invalid world!")
        );
        return false;
    }

    M_AudioComponent = UGameplayStatics::SpawnSound2D(
        World,
        SoundToPlay,
        1.f,  // VolumeMultiplier
        1.f,  // PitchMultiplier
        0.f,  // StartTime
        nullptr,
        false, // bPersistAcrossLevelTransition
        false  // bAutoDestroy
    );

    if (not EnsureValidAudioComponent())
    {
        return false;
    }

    ConfigureAudioComponentForPausedPlayback();
    M_AudioComponent->OnAudioFinished.AddDynamic(
        this,
        &URTSMusicManager::OnTrackFinished
    );
    return true;
}

void URTSMusicManager::ConfigureAudioComponentForPausedPlayback() const
{
    if (not EnsureValidAudioComponent())
    {
        return;
    }

    M_AudioComponent->SetUISound(true);
    M_AudioComponent->SetTickableWhenPaused(true);
}



void URTSMusicManager::OnFadeFinished()
{
    // FadeOut stops playback; ensure it has stopped before restoring the completion delegate.
    StopAndPlayCurrentTrack();
}

void URTSMusicManager::OnTrackFinished()
{
    if (bM_LoopMode && M_LoopsRemaining > 0)
    {
        --M_LoopsRemaining;
        PlayCurrentTrack();
        return;
    }

    M_CompletedTrackIndices.Add(M_CurrentTrackIndex);
    int32 NextTrackIndex = ChooseNextTrackIndex();
    if (NextTrackIndex == INDEX_NONE)
    {
        if (TryPlayMusicOnExhaustion())
        {
            return;
        }
        NextTrackIndex = ChooseNextTrackIndex();
    }
    if (NextTrackIndex == INDEX_NONE)
    {
        return;
    }

    M_CurrentTrackIndex = NextTrackIndex;
    M_LoopsRemaining = M_LoopCount;
    PlayCurrentTrack();
}

void URTSMusicManager::PlayTrack(const int32 TrackIndex)
{
    if (not EnsureValidAudioComponent())
    {
        return;
    }

    const FRTSMusicTypes* DefPtr = FindMusicDef(M_CurrentMusicType);
    if (not DefPtr || not DefPtr->MusicTracks.IsValidIndex(TrackIndex))
    {
        return;
    }

    USoundBase* Sound = DefPtr->MusicTracks[TrackIndex];
    if (Sound)
    {
        M_AudioComponent->OnAudioFinished.AddUniqueDynamic(this, &URTSMusicManager::OnTrackFinished);
        M_AudioComponent->SetSound(Sound);
        M_AudioComponent->Play();
    }
}

int32 URTSMusicManager::ChooseNextTrackIndex() const
{
    const FRTSMusicTypes* DefPtr = FindMusicDef(M_CurrentMusicType);
    if (not DefPtr)
    {
        return INDEX_NONE;
    }

    if (M_MusicPlayedOnExhaustion != ERTSMusicType::None)
    {
        return ChooseUnplayedTrackIndex(*DefPtr);
    }

    return GetNextRandomTrackIndex(*DefPtr, M_CurrentTrackIndex);
}

int32 URTSMusicManager::ChooseUnplayedTrackIndex(const FRTSMusicTypes& MusicDefinition) const
{
    TArray<int32> UnplayedTrackIndices;
    for (int32 TrackIndex = 0; TrackIndex < MusicDefinition.MusicTracks.Num(); ++TrackIndex)
    {
        if (not M_CompletedTrackIndices.Contains(TrackIndex))
        {
            UnplayedTrackIndices.Add(TrackIndex);
        }
    }
    if (UnplayedTrackIndices.IsEmpty())
    {
        return INDEX_NONE;
    }
    const int32 RandomIndex = FMath::RandRange(0, UnplayedTrackIndices.Num() - 1);
    return UnplayedTrackIndices[RandomIndex];
}

void URTSMusicManager::OnNoMusicForType(ERTSMusicType MusicType)
{
    RTSFunctionLibrary::ReportError(
        TEXT("RTSMusicManager: no tracks for music type ")
        + UEnum::GetValueAsString(MusicType)
    );
}

bool URTSMusicManager::EnsureValidWorldContext() const
{
    if (M_WorldContextObject.IsValid())
    {
        return true;
    }

    RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
        this, TEXT("M_WorldContextObject"), TEXT("EnsureValidWorldContext"), this
    );
    return false;
}

bool URTSMusicManager::EnsureValidAudioComponent() const
{
    if (M_AudioComponent.IsValid())
    {
        return true;
    }

    RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
        this, TEXT("M_AudioComponent"), TEXT("EnsureValidAudioComponent"), this
    );
    return false;
}
