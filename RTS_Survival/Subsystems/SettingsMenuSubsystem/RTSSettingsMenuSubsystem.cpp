#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RHI.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace RTSSettingsMenuSubsystemPrivate
{
	constexpr int32 MinScalabilityLevel = 0;
	constexpr int32 MaxScalabilityLevel = 3;
	constexpr float UnlimitedFrameRateLimit = 0.0f;
	constexpr int32 MinResolutionDimension = 1;
	constexpr float InvertedPitchFactor = -1.0f;
	constexpr float NormalPitchFactor = 1.0f;

	FString BuildResolutionLabel(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
	}

	void ApplyScalabilityGroupsToQualityLevels(const FRTSScalabilityGroupSettings& ScalabilityGroups, FQualityLevels& QualityLevels)
	{
		QualityLevels.ViewDistanceQuality = static_cast<int32>(ScalabilityGroups.M_ViewDistance);
		QualityLevels.ShadowQuality = static_cast<int32>(ScalabilityGroups.M_Shadows);
		QualityLevels.TextureQuality = static_cast<int32>(ScalabilityGroups.M_Textures);
		QualityLevels.EffectsQuality = static_cast<int32>(ScalabilityGroups.M_Effects);
		QualityLevels.PostProcessQuality = static_cast<int32>(ScalabilityGroups.M_PostProcessing);
	}

	void ApplyOverallQualityToScalabilityGroups(FRTSScalabilityGroupSettings& ScalabilityGroups, const ERTSScalabilityQuality OverallQuality)
	{
		ScalabilityGroups.M_ViewDistance = OverallQuality;
		ScalabilityGroups.M_Shadows = OverallQuality;
		ScalabilityGroups.M_Textures = OverallQuality;
		ScalabilityGroups.M_Effects = OverallQuality;
		ScalabilityGroups.M_PostProcessing = OverallQuality;
	}
}

void URTSSettingsMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnsureGameUserSettingsInstance();
	CacheSupportedResolutions();
	CacheSettingsSnapshots();
}

void URTSSettingsMenuSubsystem::Deinitialize()
{
	M_GameUserSettings = nullptr;
	Super::Deinitialize();
}

void URTSSettingsMenuSubsystem::LoadSettings()
{
	CacheSupportedResolutions();
	CacheSettingsSnapshots();
}

bool URTSSettingsMenuSubsystem::GetCurrentSettingsValues(FRTSSettingsSnapshot& OutSettingsValues) const
{
	if (not GetIsValidGameUserSettings())
	{
		return false;
	}

	OutSettingsValues = M_CurrentSettings;
	return true;
}

bool URTSSettingsMenuSubsystem::GetPendingSettingsValues(FRTSSettingsSnapshot& OutSettingsValues) const
{
	if (not GetIsValidGameUserSettings())
	{
		return false;
	}

	OutSettingsValues = M_PendingSettings;
	return true;
}

TArray<FIntPoint> URTSSettingsMenuSubsystem::GetSupportedResolutions() const
{
	return M_SupportedResolutions;
}

TArray<FString> URTSSettingsMenuSubsystem::GetSupportedResolutionLabels() const
{
	return M_SupportedResolutionLabels;
}

void URTSSettingsMenuSubsystem::SetPendingResolution(const FIntPoint& NewResolution)
{
	if (NewResolution.X < RTSSettingsMenuSubsystemPrivate::MinResolutionDimension ||
		NewResolution.Y < RTSSettingsMenuSubsystemPrivate::MinResolutionDimension)
	{
		RTSFunctionLibrary::ReportError(TEXT("Pending resolution contained non-positive dimensions."));
		return;
	}

	M_PendingSettings.M_GraphicsSettings.M_DisplaySettings.M_Resolution = NewResolution;
}

void URTSSettingsMenuSubsystem::SetPendingWindowMode(const ERTSWindowMode NewWindowMode)
{
	M_PendingSettings.M_GraphicsSettings.M_DisplaySettings.M_WindowMode = NewWindowMode;
}

void URTSSettingsMenuSubsystem::SetPendingVSyncEnabled(const bool bNewVSyncEnabled)
{
	M_PendingSettings.M_GraphicsSettings.M_DisplaySettings.bM_VSyncEnabled = bNewVSyncEnabled;
}

void URTSSettingsMenuSubsystem::SetPendingGraphicsQuality(const ERTSScalabilityQuality NewGraphicsQuality)
{
	M_PendingSettings.M_GraphicsSettings.M_DisplaySettings.M_OverallQuality = NewGraphicsQuality;
	RTSSettingsMenuSubsystemPrivate::ApplyOverallQualityToScalabilityGroups(
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups,
		NewGraphicsQuality
	);
}

void URTSSettingsMenuSubsystem::SetPendingScalabilityGroupQuality(
	const ERTSScalabilityGroup ScalabilityGroup,
	const ERTSScalabilityQuality NewQuality)
{
	switch (ScalabilityGroup)
	{
	case ERTSScalabilityGroup::ViewDistance:
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups.M_ViewDistance = NewQuality;
		return;
	case ERTSScalabilityGroup::Shadows:
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups.M_Shadows = NewQuality;
		return;
	case ERTSScalabilityGroup::Textures:
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups.M_Textures = NewQuality;
		return;
	case ERTSScalabilityGroup::Effects:
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups.M_Effects = NewQuality;
		return;
	case ERTSScalabilityGroup::PostProcessing:
		M_PendingSettings.M_GraphicsSettings.M_ScalabilityGroups.M_PostProcessing = NewQuality;
		return;
	default:
		RTSFunctionLibrary::ReportError(TEXT("Unhandled scalability group when setting pending quality."));
		return;
	}
}

void URTSSettingsMenuSubsystem::SetPendingFrameRateLimit(const float NewFrameRateLimit)
{
	const float ClampedFrameRateLimit = FMath::Max(NewFrameRateLimit, RTSSettingsMenuSubsystemPrivate::UnlimitedFrameRateLimit);
	M_PendingSettings.M_GraphicsSettings.M_DisplaySettings.M_FrameRateLimit = ClampedFrameRateLimit;
}

void URTSSettingsMenuSubsystem::SetPendingAudioVolume(const ERTSAudioChannel AudioChannel, const float NewVolume)
{
	const float ClampedVolume = FMath::Clamp(NewVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	switch (AudioChannel)
	{
	case ERTSAudioChannel::Master:
		M_PendingSettings.M_AudioSettings.M_MasterVolume = ClampedVolume;
		return;
	case ERTSAudioChannel::Music:
		M_PendingSettings.M_AudioSettings.M_MusicVolume = ClampedVolume;
		return;
	case ERTSAudioChannel::Sfx:
		M_PendingSettings.M_AudioSettings.M_SfxVolume = ClampedVolume;
		return;
	default:
		RTSFunctionLibrary::ReportError(TEXT("Unhandled audio channel when setting pending volume."));
		return;
	}
}

void URTSSettingsMenuSubsystem::SetPendingMouseSensitivity(const float NewMouseSensitivity)
{
	const float ClampedSensitivity = FMath::Clamp(
		NewMouseSensitivity,
		RTSGameUserSettingsRanges::MinMouseSensitivity,
		RTSGameUserSettingsRanges::MaxMouseSensitivity
	);
	M_PendingSettings.M_ControlSettings.M_MouseSensitivity = ClampedSensitivity;
}

void URTSSettingsMenuSubsystem::SetPendingInvertYAxis(const bool bNewInvertYAxis)
{
	M_PendingSettings.M_ControlSettings.bM_InvertYAxis = bNewInvertYAxis;
}

void URTSSettingsMenuSubsystem::ApplySettings()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	ApplyPendingSettingsToGameUserSettings();
	ApplyNonResolutionSettings();
	ApplyResolutionSettings();

	M_CurrentSettings = M_PendingSettings;
}

void URTSSettingsMenuSubsystem::SaveSettings()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	ApplySnapshotToSettings(M_CurrentSettings, *GameUserSettings);
	GameUserSettings->SaveSettings();
}

void URTSSettingsMenuSubsystem::RevertSettings()
{
	ResetPendingSettingsToCurrent();
}

void URTSSettingsMenuSubsystem::ConfigureAudioMixAndClasses(
	const TSoftObjectPtr<USoundMix> NewSettingsSoundMix,
	const TSoftObjectPtr<USoundClass> NewMasterSoundClass,
	const TSoftObjectPtr<USoundClass> NewMusicSoundClass,
	const TSoftObjectPtr<USoundClass> NewSfxSoundClass)
{
	M_SettingsSoundMix = NewSettingsSoundMix;
	M_MasterSoundClass = NewMasterSoundClass;
	M_MusicSoundClass = NewMusicSoundClass;
	M_SfxSoundClass = NewSfxSoundClass;

	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	ApplyAudioSettings();
}

bool URTSSettingsMenuSubsystem::GetIsValidGameUserSettings() const
{
	if (not M_GameUserSettings.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GameUserSettings"),
			TEXT("URTSSettingsMenuSubsystem::GetIsValidGameUserSettings"),
			this
		);
		return false;
	}

	return true;
}

URTSGameUserSettings* URTSSettingsMenuSubsystem::GetGameUserSettingsChecked() const
{
	if (not GetIsValidGameUserSettings())
	{
		return nullptr;
	}

	return M_GameUserSettings.Get();
}

void URTSSettingsMenuSubsystem::EnsureGameUserSettingsInstance()
{
	UGameUserSettings* const ExistingSettings = UGameUserSettings::GetGameUserSettings();
	if (ExistingSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Game user settings singleton was null."));
		return;
	}

	URTSGameUserSettings* const ExistingCustomSettings = Cast<URTSGameUserSettings>(ExistingSettings);
	if (ExistingCustomSettings != nullptr)
	{
		M_GameUserSettings = ExistingCustomSettings;
		ExistingCustomSettings->LoadSettings(false);
		return;
	}

	URTSGameUserSettings* const CreatedSettings = NewObject<URTSGameUserSettings>(GetTransientPackage(), URTSGameUserSettings::StaticClass());
	if (CreatedSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create RTS game user settings instance."));
		return;
	}

	UGameUserSettings::SetGameUserSettings(CreatedSettings);
	CreatedSettings->LoadSettings(true);
	M_GameUserSettings = CreatedSettings;
}

void URTSSettingsMenuSubsystem::CacheSupportedResolutions()
{
	M_SupportedResolutions.Empty();
	M_SupportedResolutionLabels.Empty();

	FScreenResolutionArray AvailableResolutions;
	const bool bResolutionsFound = RHIGetAvailableResolutions(AvailableResolutions, false);
	if (bResolutionsFound)
	{
		for (const FScreenResolutionRHI& Resolution : AvailableResolutions)
		{
			AddSupportedResolutionUnique(FIntPoint(Resolution.Width, Resolution.Height));
		}
	}

	if (M_SupportedResolutions.Num() > 0)
	{
		return;
	}

	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	const FIntPoint CurrentResolution = GetGameUserSettingsChecked()->GetScreenResolution();
	AddSupportedResolutionUnique(CurrentResolution);
}

void URTSSettingsMenuSubsystem::CacheSettingsSnapshots()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	GameUserSettings->LoadSettings(false);
	M_CurrentSettings = BuildSnapshotFromSettings(*GameUserSettings);
	ResetPendingSettingsToCurrent();
}

void URTSSettingsMenuSubsystem::ResetPendingSettingsToCurrent()
{
	M_PendingSettings = M_CurrentSettings;
}

void URTSSettingsMenuSubsystem::ApplyPendingSettingsToGameUserSettings()
{
	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	if (GameUserSettings == nullptr)
	{
		return;
	}

	ApplySnapshotToSettings(M_PendingSettings, *GameUserSettings);
}

void URTSSettingsMenuSubsystem::ApplyResolutionSettings()
{
	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	if (GameUserSettings == nullptr)
	{
		return;
	}

	GameUserSettings->ApplyResolutionSettings(false);
	GameUserSettings->ConfirmVideoMode();
}

void URTSSettingsMenuSubsystem::ApplyNonResolutionSettings()
{
	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	if (GameUserSettings == nullptr)
	{
		return;
	}

	GameUserSettings->ApplyNonResolutionSettings();
	ApplyAudioSettings();
	ApplyControlSettings();
}

void URTSSettingsMenuSubsystem::ApplyAudioSettings()
{
	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	if (GameUserSettings == nullptr)
	{
		return;
	}

	if (GEngine == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("GEngine was null while applying audio settings."));
		return;
	}

	FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
	if (not AudioDevice.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Main audio device was invalid while applying audio settings."));
		return;
	}

	AudioDevice->SetTransientMasterVolume(GameUserSettings->GetMasterVolume());
	if (M_SettingsSoundMix.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Settings sound mix is required to apply per-channel volumes."));
		return;
	}

	ApplyAudioChannelVolume(ERTSAudioChannel::Master, GameUserSettings->GetMasterVolume());
	ApplyAudioChannelVolume(ERTSAudioChannel::Music, GameUserSettings->GetMusicVolume());
	ApplyAudioChannelVolume(ERTSAudioChannel::Sfx, GameUserSettings->GetSfxVolume());
}

void URTSSettingsMenuSubsystem::ApplyControlSettings()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("World was null while applying control settings."));
		return;
	}

	for (FConstPlayerControllerIterator ControllerIterator = World->GetPlayerControllerIterator(); ControllerIterator; ++ControllerIterator)
	{
		APlayerController* const PlayerController = ControllerIterator->Get();
		if (PlayerController == nullptr)
		{
			continue;
		}

		if (not bM_BaseInputScaleInitialised)
		{
			M_BaseYawScale = PlayerController->InputYawScale;
			M_BasePitchScale = PlayerController->InputPitchScale;
			bM_BaseInputScaleInitialised = true;
		}

		ApplyControlSettingsToPlayerController(PlayerController);
	}
}

void URTSSettingsMenuSubsystem::ApplyAudioChannelVolume(const ERTSAudioChannel AudioChannel, const float VolumeToApply)
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("World was null while applying audio channel volume."));
		return;
	}

	if (M_SettingsSoundMix.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Settings sound mix is required to apply channel volumes."));
		return;
	}

	USoundMix* const SettingsSoundMix = M_SettingsSoundMix.LoadSynchronous();
	if (SettingsSoundMix == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Settings sound mix could not be loaded."));
		return;
	}

	TSoftObjectPtr<USoundClass> TargetSoundClass;
	switch (AudioChannel)
	{
	case ERTSAudioChannel::Master:
		TargetSoundClass = M_MasterSoundClass;
		break;
	case ERTSAudioChannel::Music:
		TargetSoundClass = M_MusicSoundClass;
		break;
	case ERTSAudioChannel::Sfx:
		TargetSoundClass = M_SfxSoundClass;
		break;
	default:
		RTSFunctionLibrary::ReportError(TEXT("Unhandled audio channel while applying volume."));
		return;
	}

	if (TargetSoundClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Audio channel sound class was not configured."));
		return;
	}

	USoundClass* const LoadedSoundClass = TargetSoundClass.LoadSynchronous();
	if (LoadedSoundClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Audio channel sound class could not be loaded."));
		return;
	}

	UGameplayStatics::SetSoundMixClassOverride(World, SettingsSoundMix, LoadedSoundClass, VolumeToApply, 1.0f, 0.0f, true);
	UGameplayStatics::PushSoundMixModifier(World, SettingsSoundMix);
}

void URTSSettingsMenuSubsystem::ApplyControlSettingsToPlayerController(APlayerController* PlayerControllerToApply) const
{
	if (PlayerControllerToApply == nullptr)
	{
		return;
	}

	const float SensitivityMultiplier = M_PendingSettings.M_ControlSettings.M_MouseSensitivity;
	const float PitchFactor = M_PendingSettings.M_ControlSettings.bM_InvertYAxis
		? RTSSettingsMenuSubsystemPrivate::InvertedPitchFactor
		: RTSSettingsMenuSubsystemPrivate::NormalPitchFactor;

	PlayerControllerToApply->InputYawScale = M_BaseYawScale * SensitivityMultiplier;
	PlayerControllerToApply->InputPitchScale = M_BasePitchScale * SensitivityMultiplier * PitchFactor;
}

FRTSSettingsSnapshot URTSSettingsMenuSubsystem::BuildSnapshotFromSettings(const URTSGameUserSettings& GameUserSettings) const
{
	FRTSSettingsSnapshot Snapshot;

	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_WindowMode = GetMenuWindowMode(GameUserSettings.GetFullscreenMode());
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_Resolution = GameUserSettings.GetScreenResolution();
	Snapshot.M_GraphicsSettings.M_DisplaySettings.bM_VSyncEnabled = GameUserSettings.IsVSyncEnabled();
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_OverallQuality = GetScalabilityQualityFromLevel(GameUserSettings.GetOverallScalabilityLevel());
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_FrameRateLimit = GameUserSettings.GetFrameRateLimit();

	const FQualityLevels QualityLevels = GameUserSettings.GetQualityLevels();
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_ViewDistance = GetScalabilityQualityFromLevel(QualityLevels.ViewDistanceQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Shadows = GetScalabilityQualityFromLevel(QualityLevels.ShadowQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Textures = GetScalabilityQualityFromLevel(QualityLevels.TextureQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Effects = GetScalabilityQualityFromLevel(QualityLevels.EffectsQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_PostProcessing = GetScalabilityQualityFromLevel(QualityLevels.PostProcessQuality);

	Snapshot.M_AudioSettings.M_MasterVolume = GameUserSettings.GetMasterVolume();
	Snapshot.M_AudioSettings.M_MusicVolume = GameUserSettings.GetMusicVolume();
	Snapshot.M_AudioSettings.M_SfxVolume = GameUserSettings.GetSfxVolume();

	Snapshot.M_ControlSettings.M_MouseSensitivity = GameUserSettings.GetMouseSensitivity();
	Snapshot.M_ControlSettings.bM_InvertYAxis = GameUserSettings.GetInvertYAxis();

	return Snapshot;
}

void URTSSettingsMenuSubsystem::ApplySnapshotToSettings(
	const FRTSSettingsSnapshot& SnapshotToApply,
	URTSGameUserSettings& GameUserSettingsToApply)
{
	GameUserSettingsToApply.SetFullscreenMode(GetNativeWindowMode(SnapshotToApply.M_GraphicsSettings.M_DisplaySettings.M_WindowMode));
	GameUserSettingsToApply.SetScreenResolution(SnapshotToApply.M_GraphicsSettings.M_DisplaySettings.M_Resolution);
	GameUserSettingsToApply.SetVSyncEnabled(SnapshotToApply.M_GraphicsSettings.M_DisplaySettings.bM_VSyncEnabled);
	GameUserSettingsToApply.SetOverallScalabilityLevel(GetLevelFromScalabilityQuality(SnapshotToApply.M_GraphicsSettings.M_DisplaySettings.M_OverallQuality));
	GameUserSettingsToApply.SetFrameRateLimit(SnapshotToApply.M_GraphicsSettings.M_DisplaySettings.M_FrameRateLimit);

	FQualityLevels QualityLevels = GameUserSettingsToApply.GetQualityLevels();
	RTSSettingsMenuSubsystemPrivate::ApplyScalabilityGroupsToQualityLevels(SnapshotToApply.M_GraphicsSettings.M_ScalabilityGroups, QualityLevels);
	GameUserSettingsToApply.SetQualityLevels(QualityLevels);

	GameUserSettingsToApply.SetMasterVolume(SnapshotToApply.M_AudioSettings.M_MasterVolume);
	GameUserSettingsToApply.SetMusicVolume(SnapshotToApply.M_AudioSettings.M_MusicVolume);
	GameUserSettingsToApply.SetSfxVolume(SnapshotToApply.M_AudioSettings.M_SfxVolume);
	GameUserSettingsToApply.SetMouseSensitivity(SnapshotToApply.M_ControlSettings.M_MouseSensitivity);
	GameUserSettingsToApply.SetInvertYAxis(SnapshotToApply.M_ControlSettings.bM_InvertYAxis);
}

void URTSSettingsMenuSubsystem::AddSupportedResolutionUnique(const FIntPoint& ResolutionToAdd)
{
	if (ResolutionToAdd.X < RTSSettingsMenuSubsystemPrivate::MinResolutionDimension ||
		ResolutionToAdd.Y < RTSSettingsMenuSubsystemPrivate::MinResolutionDimension)
	{
		return;
	}

	const FString ResolutionLabel = RTSSettingsMenuSubsystemPrivate::BuildResolutionLabel(ResolutionToAdd);
	if (M_SupportedResolutionLabels.Contains(ResolutionLabel))
	{
		return;
	}

	M_SupportedResolutions.Add(ResolutionToAdd);
	M_SupportedResolutionLabels.Add(ResolutionLabel);
}

ERTSScalabilityQuality URTSSettingsMenuSubsystem::GetScalabilityQualityFromLevel(const int32 ScalabilityLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(ScalabilityLevel, RTSSettingsMenuSubsystemPrivate::MinScalabilityLevel, RTSSettingsMenuSubsystemPrivate::MaxScalabilityLevel);
	return static_cast<ERTSScalabilityQuality>(ClampedLevel);
}

int32 URTSSettingsMenuSubsystem::GetLevelFromScalabilityQuality(const ERTSScalabilityQuality ScalabilityQuality) const
{
	const int32 Level = static_cast<int32>(ScalabilityQuality);
	return FMath::Clamp(Level, RTSSettingsMenuSubsystemPrivate::MinScalabilityLevel, RTSSettingsMenuSubsystemPrivate::MaxScalabilityLevel);
}

EWindowMode::Type URTSSettingsMenuSubsystem::GetNativeWindowMode(const ERTSWindowMode WindowMode) const
{
	switch (WindowMode)
	{
	case ERTSWindowMode::Fullscreen:
		return EWindowMode::Fullscreen;
	case ERTSWindowMode::WindowedFullscreen:
		return EWindowMode::WindowedFullscreen;
	case ERTSWindowMode::Windowed:
		return EWindowMode::Windowed;
	default:
		return EWindowMode::WindowedFullscreen;
	}
}

ERTSWindowMode URTSSettingsMenuSubsystem::GetMenuWindowMode(const EWindowMode::Type WindowMode) const
{
	switch (WindowMode)
	{
	case EWindowMode::Fullscreen:
		return ERTSWindowMode::Fullscreen;
	case EWindowMode::WindowedFullscreen:
		return ERTSWindowMode::WindowedFullscreen;
	case EWindowMode::Windowed:
		return ERTSWindowMode::Windowed;
	default:
		return ERTSWindowMode::WindowedFullscreen;
	}
}
