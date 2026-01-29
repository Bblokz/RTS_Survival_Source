#include "RTSGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Audio/Settings/RTSAudioDeveloperSettings.h"
#include "RTS_Survival/Music/RTSMusicManager/RTSMusicManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void URTSGameInstance::Init()
{
	Super::Init();

	// Bind to level loading delegates
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &URTSGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &URTSGameInstance::EndLoadingScreen);
	bIsLoadingProfileMap = true;

	// Create (but don’t configure) our music manager
	MusicManager = NewObject<URTSMusicManager>(this, URTSMusicManager::StaticClass());
	SetupMusicManager();

	InitializeAudioSettings();
}

void URTSGameInstance::Shutdown()
{
    if (MusicManager)
    {
        MusicManager->TeardownForOldWorld();
        // no RemoveFromRoot needed since we never called AddToRoot()
    }
    Super::Shutdown();
}

void URTSGameInstance::InitializeAudioSettings()
{
	const URTSAudioDeveloperSettings* const AudioSettings = GetDefault<URTSAudioDeveloperSettings>();
	if (AudioSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Audio settings were missing while initializing audio."));
		return;
	}

	if (AudioSettings->UserSettingsSoundMix.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UserSettingsSoundMix is not configured in RTSAudioSettings."));
		return;
	}

	USoundMix* const SettingsSoundMix = AudioSettings->UserSettingsSoundMix.LoadSynchronous();
	if (SettingsSoundMix == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to load UserSettingsSoundMix from RTSAudioSettings."));
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("World was null while initializing audio settings."));
		return;
	}

	EnsureSoundMixHasAudioClasses(SettingsSoundMix, AudioSettings->SoundClassesByType);
	UGameplayStatics::PushSoundMixModifier(World, SettingsSoundMix);
}

void URTSGameInstance::EnsureSoundMixHasAudioClasses(
	USoundMix* SettingsSoundMix,
	const TMap<ERTSAudioType, TSoftObjectPtr<USoundClass>>& SoundClassesByType)
{
	if (SettingsSoundMix == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Sound mix was null while validating audio classes."));
		return;
	}

	const float DefaultAdjusterVolume = 1.0f;
	const float DefaultAdjusterPitch = 1.0f;
	const bool bDefaultApplyToChildren = true;

	for (const TPair<ERTSAudioType, TSoftObjectPtr<USoundClass>>& AudioEntry : SoundClassesByType)
	{
		if (AudioEntry.Value.IsNull())
		{
			RTSFunctionLibrary::ReportError(TEXT("Audio settings sound class map contains a null entry."));
			continue;
		}

		USoundClass* const SoundClass = AudioEntry.Value.LoadSynchronous();
		if (SoundClass == nullptr)
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to load a sound class from RTSAudioSettings."));
			continue;
		}

		bool bSoundClassFound = false;
		for (const FSoundClassAdjuster& Adjuster : SettingsSoundMix->SoundClassEffects)
		{
			if (Adjuster.SoundClassObject == SoundClass)
			{
				bSoundClassFound = true;
				break;
			}
		}

		if (bSoundClassFound)
		{
			continue;
		}

		RTSFunctionLibrary::ReportError(TEXT("Sound mix is missing a sound class adjuster entry."));

		FSoundClassAdjuster NewAdjuster;
		NewAdjuster.SoundClassObject = SoundClass;
		NewAdjuster.VolumeAdjuster = DefaultAdjusterVolume;
		NewAdjuster.PitchAdjuster = DefaultAdjusterPitch;
		NewAdjuster.bApplyToChildren = bDefaultApplyToChildren;
		SettingsSoundMix->SoundClassEffects.Add(NewAdjuster);
	}
}


void URTSGameInstance::BeginLoadingScreen(const FString& MapName)
{
}

void URTSGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
}

void URTSGameInstance::StopLoadingScreen()
{
}
