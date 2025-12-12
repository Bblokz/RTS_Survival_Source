// RTSMusicManager.cpp

#include "RTSMusicManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void URTSMusicManager::InitMusicManagerTracks(const TArray<FRTSMusicTypes>& InMusicTypes)
{
    M_MusicDefinitions = InMusicTypes;
}

void URTSMusicManager::SetupMusicManagerForNewWorld(
    UObject* InWorldContextObject,
    const ERTSMusicType MusicTypeStart)
{
    M_WorldContextObject = InWorldContextObject;
    bM_LoopMode          = false;
    M_CurrentMusicType   = MusicTypeStart;

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
    const bool bFade)
{
    if (not EnsureValidWorldContext() || not EnsureValidAudioComponent())
    {
        return;
    }
    if(M_CurrentMusicType == NewMusicType && M_AudioComponent->IsPlaying())
    {
        // If the same music type is requested, we can just return.
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
    const bool bFadeIntoLoop)
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
    if (M_AudioComponent && M_AudioComponent->IsPlaying())
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

void URTSMusicManager::StopAndPlayCurrentTrack() const
{
    if (not EnsureValidAudioComponent())
    {
        return;
    }

    M_AudioComponent->Stop();
    PlayCurrentTrack();
}

void URTSMusicManager::FadeOutCurrentTrack()
{
    if (not EnsureValidAudioComponent() || not EnsureValidWorldContext())
    {
        return;
    }

    M_AudioComponent->FadeOut(5.f, 0.f);
    UWorld* World = GEngine->GetWorldFromContextObjectChecked(M_WorldContextObject);
    World->GetTimerManager().SetTimer(
        M_FadeTimerHandle,
        this,
        &URTSMusicManager::OnFadeFinished,
        5.f,
        false
    );
}

void URTSMusicManager::PlayCurrentTrack() const
{
    PlayTrack(M_CurrentTrackIndex);
}

void URTSMusicManager::DestroyAudioComponent()
{
    if (M_AudioComponent)
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

    UWorld* World = GEngine->GetWorldFromContextObjectChecked(M_WorldContextObject);
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

    if (not IsValid(M_AudioComponent))
    {
        RTSFunctionLibrary::ReportError(
            TEXT("RTSMusicManager: failed to spawn audio component.")
        );
        return false;
    }

    M_AudioComponent->OnAudioFinished.AddDynamic(
        this,
        &URTSMusicManager::OnTrackFinished
    );
    return true;
}



void URTSMusicManager::OnFadeFinished()
{
    PlayTrack(M_CurrentTrackIndex);
}

void URTSMusicManager::OnTrackFinished()
{
    if (bM_LoopMode)
    {
        if (M_LoopsRemaining > 0)
        {
            --M_LoopsRemaining;
            PlayCurrentTrack();
            return;
        }

        const int32 Next = ChooseNextTrackIndex();
        if (Next != INDEX_NONE)
        {
            M_CurrentTrackIndex = Next;
            M_LoopsRemaining    = M_LoopCount;
            PlayCurrentTrack();
        }
        return;
    }

    const int32 Next = ChooseNextTrackIndex();
    if (Next != INDEX_NONE)
    {
        M_CurrentTrackIndex = Next;
        PlayCurrentTrack();
    }
}

void URTSMusicManager::PlayTrack(const int32 TrackIndex) const
{
    if (not EnsureValidAudioComponent())
    {
        return;
    }

    const FRTSMusicTypes* DefPtr = FindMusicDef(M_CurrentMusicType);
    if (not DefPtr || !DefPtr->MusicTracks.IsValidIndex(TrackIndex))
    {
        return;
    }

    USoundBase* Sound = DefPtr->MusicTracks[TrackIndex];
    if (Sound)
    {
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

    const int32 Num = DefPtr->MusicTracks.Num();
    if (Num <= 1)
    {
        return Num == 1 ? 0 : INDEX_NONE;
    }

    int32 NewIndex;
    do
    {
        NewIndex = FMath::RandRange(0, Num - 1);
    }
    while (NewIndex == M_CurrentTrackIndex);

    return NewIndex;
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
    if (IsValid(M_WorldContextObject))
    {
        return true;
    }

    RTSFunctionLibrary::ReportError(
        TEXT("RTSMusicManager: world context not set. Call SetupMusicManagerForNewWorld.")
    );
    return false;
}

bool URTSMusicManager::EnsureValidAudioComponent() const
{
    if (IsValid(M_AudioComponent))
    {
        return true;
    }

    RTSFunctionLibrary::ReportError(
        TEXT("RTSMusicManager: no valid audio component.")
    );
    return false;
}
