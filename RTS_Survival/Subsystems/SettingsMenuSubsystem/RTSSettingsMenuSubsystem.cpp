#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RHI.h"
#include "RTS_Survival/Audio/Settings/RTSAudioDeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Scalability.h"
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
	constexpr bool bDefaultVSyncEnabled = false;
	constexpr bool bDefaultInvertYAxis = false;

	const ERTSWindowMode DefaultWindowMode = ERTSWindowMode::Fullscreen;
	const ERTSScalabilityQuality DefaultScalabilityQuality = ERTSScalabilityQuality::Epic;
	const FIntPoint DefaultResolution = FIntPoint(1920, 1080);

	FString BuildResolutionLabel(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
	}

	void ApplyScalabilityGroupsToQualityLevels(const FRTSScalabilityGroupSettings& ScalabilityGroups, Scalability::FQualityLevels& QualityLevels)
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

	FIntPoint SelectDefaultResolution(
		const TArray<FIntPoint>& SupportedResolutions,
		const FIntPoint& DesktopResolution,
		const FIntPoint& CurrentResolution)
	{
		if (SupportedResolutions.Contains(DesktopResolution))
		{
			return DesktopResolution;
		}

		if (SupportedResolutions.Contains(CurrentResolution))
		{
			return CurrentResolution;
		}

		if (SupportedResolutions.Num() > 0)
		{
			return SupportedResolutions[0];
		}

		if (DesktopResolution.X >= MinResolutionDimension && DesktopResolution.Y >= MinResolutionDimension)
		{
			return DesktopResolution;
		}

		if (CurrentResolution.X >= MinResolutionDimension && CurrentResolution.Y >= MinResolutionDimension)
		{
			return CurrentResolution;
		}

		return DefaultResolution;
	}
}

void URTSSettingsMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnsureGameUserSettingsInstance();
	CacheSupportedResolutions();
	CacheSettingsSnapshots();
	CacheAudioSettingsFromDeveloperSettings();
	ApplyAudioSettings();
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
	CacheAudioSettingsFromDeveloperSettings();
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

void URTSSettingsMenuSubsystem::SetPendingMasterVolume(const float NewVolume)
{
	const float ClampedVolume = FMath::Clamp(NewVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	M_PendingSettings.M_AudioSettings.M_MasterVolume = ClampedVolume;
}

void URTSSettingsMenuSubsystem::SetPendingAudioVolume(const ERTSAudioType AudioType, const float NewVolume)
{
	const float ClampedVolume = FMath::Clamp(NewVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	switch (AudioType)
	{
	case ERTSAudioType::SFXAndWeapons:
		M_PendingSettings.M_AudioSettings.M_SfxAndWeaponsVolume = ClampedVolume;
		return;
	case ERTSAudioType::Music:
		M_PendingSettings.M_AudioSettings.M_MusicVolume = ClampedVolume;
		return;
	case ERTSAudioType::Voicelines:
		M_PendingSettings.M_AudioSettings.M_VoicelinesVolume = ClampedVolume;
		return;
	case ERTSAudioType::Announcer:
		M_PendingSettings.M_AudioSettings.M_AnnouncerVolume = ClampedVolume;
		return;
	case ERTSAudioType::UI:
		M_PendingSettings.M_AudioSettings.M_UiVolume = ClampedVolume;
		return;
	default:
		RTSFunctionLibrary::ReportError(TEXT("Unhandled audio type when setting pending volume."));
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

void URTSSettingsMenuSubsystem::SetPendingOverwriteAllPlayerHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_OverwriteAllPlayerHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingPlayerTankHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_PlayerTankHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingPlayerSquadHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_PlayerSquadHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingPlayerNomadicHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_PlayerNomadicHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingPlayerBxpHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_PlayerBxpHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingPlayerAircraftHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_PlayerAircraftHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingOverwriteAllEnemyHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_OverwriteAllEnemyHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingEnemyTankHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_EnemyTankHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingEnemySquadHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_EnemySquadHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingEnemyNomadicHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_EnemyNomadicHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingEnemyBxpHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_EnemyBxpHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::SetPendingEnemyAircraftHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_PendingSettings.M_GameplaySettings.M_EnemyAircraftHpBarStrat = NewStrategy;
}

void URTSSettingsMenuSubsystem::ApplySettings()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	const FRTSSettingsSnapshot PreviousCurrentSettings = M_CurrentSettings;
	ApplyPendingSettingsToGameUserSettings();
	ApplyNonResolutionSettings();
	ApplyResolutionSettings();

	M_CurrentSettings = M_PendingSettings;
	ApplyGameplaySettingsDiff(PreviousCurrentSettings.M_GameplaySettings, M_CurrentSettings.M_GameplaySettings);
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

void URTSSettingsMenuSubsystem::SetPendingSettingsToDefaults()
{
	if (not GetIsValidGameUserSettings())
	{
		return;
	}

	URTSGameUserSettings* const GameUserSettings = GetGameUserSettingsChecked();
	if (GameUserSettings == nullptr)
	{
		return;
	}

	FRTSSettingsSnapshot DefaultSettings;
	const FIntPoint DesktopResolution = GameUserSettings->GetDesktopResolution();
	const FIntPoint CurrentResolution = GameUserSettings->GetScreenResolution();

	DefaultSettings.M_GraphicsSettings.M_DisplaySettings.M_WindowMode = RTSSettingsMenuSubsystemPrivate::DefaultWindowMode;
	DefaultSettings.M_GraphicsSettings.M_DisplaySettings.M_Resolution = RTSSettingsMenuSubsystemPrivate::SelectDefaultResolution(
		M_SupportedResolutions,
		DesktopResolution,
		CurrentResolution
	);
	DefaultSettings.M_GraphicsSettings.M_DisplaySettings.bM_VSyncEnabled = RTSSettingsMenuSubsystemPrivate::bDefaultVSyncEnabled;
	DefaultSettings.M_GraphicsSettings.M_DisplaySettings.M_OverallQuality = RTSSettingsMenuSubsystemPrivate::DefaultScalabilityQuality;
	DefaultSettings.M_GraphicsSettings.M_DisplaySettings.M_FrameRateLimit = RTSSettingsMenuSubsystemPrivate::UnlimitedFrameRateLimit;

	RTSSettingsMenuSubsystemPrivate::ApplyOverallQualityToScalabilityGroups(
		DefaultSettings.M_GraphicsSettings.M_ScalabilityGroups,
		RTSSettingsMenuSubsystemPrivate::DefaultScalabilityQuality
	);

	DefaultSettings.M_AudioSettings.M_MasterVolume = RTSGameUserSettingsRanges::DefaultVolume;
	DefaultSettings.M_AudioSettings.M_MusicVolume = RTSGameUserSettingsRanges::DefaultVolume;
	DefaultSettings.M_AudioSettings.M_SfxAndWeaponsVolume = RTSGameUserSettingsRanges::DefaultVolume;
	DefaultSettings.M_AudioSettings.M_VoicelinesVolume = RTSGameUserSettingsRanges::DefaultVolume;
	DefaultSettings.M_AudioSettings.M_AnnouncerVolume = RTSGameUserSettingsRanges::DefaultVolume;
	DefaultSettings.M_AudioSettings.M_UiVolume = RTSGameUserSettingsRanges::DefaultVolume;

	DefaultSettings.M_ControlSettings.M_MouseSensitivity = RTSGameUserSettingsRanges::DefaultMouseSensitivity;
	DefaultSettings.M_ControlSettings.bM_InvertYAxis = RTSSettingsMenuSubsystemPrivate::bDefaultInvertYAxis;

	DefaultSettings.M_GameplaySettings.M_OverwriteAllPlayerHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::NotInitialized;
	DefaultSettings.M_GameplaySettings.M_PlayerTankHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_PlayerSquadHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_PlayerNomadicHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_PlayerBxpHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_PlayerAircraftHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_OverwriteAllEnemyHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::NotInitialized;
	DefaultSettings.M_GameplaySettings.M_EnemyTankHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_EnemySquadHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_EnemyNomadicHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_EnemyBxpHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
	DefaultSettings.M_GameplaySettings.M_EnemyAircraftHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	M_PendingSettings = DefaultSettings;
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

	if (GEngine == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("GEngine was null while replacing the game user settings instance."));
		return;
	}

	GEngine->GameUserSettings = CreatedSettings;
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

void URTSSettingsMenuSubsystem::CacheAudioSettingsFromDeveloperSettings()
{
	const URTSAudioDeveloperSettings* const AudioSettings = GetDefault<URTSAudioDeveloperSettings>();
	if (AudioSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Audio settings were missing while caching audio mix data."));
		return;
	}

	M_SettingsSoundMix = AudioSettings->UserSettingsSoundMix;
	M_SoundClassesByType = AudioSettings->SoundClassesByType;

	if (M_SettingsSoundMix.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UserSettingsSoundMix is not configured in RTSAudioSettings."));
	}

	if (M_SoundClassesByType.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("SoundClassesByType is empty in RTSAudioSettings."));
	}
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

	AudioDevice->SetTransientPrimaryVolume(GameUserSettings->GetMasterVolume());
	if (M_SettingsSoundMix.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Settings sound mix is required to apply per-channel volumes."));
		return;
	}

	ApplyAudioChannelVolume(ERTSAudioType::SFXAndWeapons, GameUserSettings->GetSfxAndWeaponsVolume());
	ApplyAudioChannelVolume(ERTSAudioType::Music, GameUserSettings->GetMusicVolume());
	ApplyAudioChannelVolume(ERTSAudioType::Voicelines, GameUserSettings->GetVoicelinesVolume());
	ApplyAudioChannelVolume(ERTSAudioType::Announcer, GameUserSettings->GetAnnouncerVolume());
	ApplyAudioChannelVolume(ERTSAudioType::UI, GameUserSettings->GetUiVolume());
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
			// todo.
			// M_BaseYawScale = PlayerController->GetInputYawScale();
			// M_BasePitchScale = PlayerController->GetInputPitchScale();
			bM_BaseInputScaleInitialised = true;
		}

		ApplyControlSettingsToPlayerController(PlayerController);
	}
}

void URTSSettingsMenuSubsystem::ApplyGameplaySettingsDiff(
	const FRTSGameplaySettings& PreviousSettings,
	const FRTSGameplaySettings& CurrentSettings)
{
	UGameUnitManager* const GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}

	if (PreviousSettings.M_PlayerTankHpBarStrat != CurrentSettings.M_PlayerTankHpBarStrat)
	{
		GameUnitManager->ApplyPlayerTankHealthBarVisibilityStrategy(CurrentSettings.M_PlayerTankHpBarStrat);
	}

	if (PreviousSettings.M_PlayerSquadHpBarStrat != CurrentSettings.M_PlayerSquadHpBarStrat)
	{
		GameUnitManager->ApplyPlayerSquadHealthBarVisibilityStrategy(CurrentSettings.M_PlayerSquadHpBarStrat);
	}

	if (PreviousSettings.M_PlayerNomadicHpBarStrat != CurrentSettings.M_PlayerNomadicHpBarStrat)
	{
		GameUnitManager->ApplyPlayerNomadicHealthBarVisibilityStrategy(CurrentSettings.M_PlayerNomadicHpBarStrat);
	}

	if (PreviousSettings.M_PlayerBxpHpBarStrat != CurrentSettings.M_PlayerBxpHpBarStrat)
	{
		GameUnitManager->ApplyPlayerBxpHealthBarVisibilityStrategy(CurrentSettings.M_PlayerBxpHpBarStrat);
	}

	if (PreviousSettings.M_PlayerAircraftHpBarStrat != CurrentSettings.M_PlayerAircraftHpBarStrat)
	{
		GameUnitManager->ApplyPlayerAircraftHealthBarVisibilityStrategy(CurrentSettings.M_PlayerAircraftHpBarStrat);
	}

	if (PreviousSettings.M_EnemyTankHpBarStrat != CurrentSettings.M_EnemyTankHpBarStrat)
	{
		GameUnitManager->ApplyEnemyTankHealthBarVisibilityStrategy(CurrentSettings.M_EnemyTankHpBarStrat);
	}

	if (PreviousSettings.M_EnemySquadHpBarStrat != CurrentSettings.M_EnemySquadHpBarStrat)
	{
		GameUnitManager->ApplyEnemySquadHealthBarVisibilityStrategy(CurrentSettings.M_EnemySquadHpBarStrat);
	}

	if (PreviousSettings.M_EnemyNomadicHpBarStrat != CurrentSettings.M_EnemyNomadicHpBarStrat)
	{
		GameUnitManager->ApplyEnemyNomadicHealthBarVisibilityStrategy(CurrentSettings.M_EnemyNomadicHpBarStrat);
	}

	if (PreviousSettings.M_EnemyBxpHpBarStrat != CurrentSettings.M_EnemyBxpHpBarStrat)
	{
		GameUnitManager->ApplyEnemyBxpHealthBarVisibilityStrategy(CurrentSettings.M_EnemyBxpHpBarStrat);
	}

	if (PreviousSettings.M_EnemyAircraftHpBarStrat != CurrentSettings.M_EnemyAircraftHpBarStrat)
	{
		GameUnitManager->ApplyEnemyAircraftHealthBarVisibilityStrategy(CurrentSettings.M_EnemyAircraftHpBarStrat);
	}
}

void URTSSettingsMenuSubsystem::ApplyAudioChannelVolume(const ERTSAudioType AudioType, const float VolumeToApply)
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
	if (not TryGetSoundClassForAudioType(AudioType, TargetSoundClass))
	{
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

	const float DefaultPitch = 1.0f;
	const float DefaultFadeInTime = 0.0f;
	const bool bDefaultApplyToChildren = true;

	UGameplayStatics::SetSoundMixClassOverride(
		World,
		SettingsSoundMix,
		LoadedSoundClass,
		VolumeToApply,
		DefaultPitch,
		DefaultFadeInTime,
		bDefaultApplyToChildren
	);
	UGameplayStatics::PushSoundMixModifier(World, SettingsSoundMix);
}

bool URTSSettingsMenuSubsystem::TryGetSoundClassForAudioType(
	const ERTSAudioType AudioType,
	TSoftObjectPtr<USoundClass>& OutSoundClass) const
{
	const TSoftObjectPtr<USoundClass>* const FoundSoundClass = M_SoundClassesByType.Find(AudioType);
	if (FoundSoundClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Audio type was not configured in RTSAudioSettings."));
		return false;
	}

	OutSoundClass = *FoundSoundClass;
	return true;
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

	// todo
	// PlayerControllerToApply->SetInputYawScale(M_BaseYawScale * SensitivityMultiplier);
	// PlayerControllerToApply->SetInputPitchScale(M_BasePitchScale * SensitivityMultiplier * PitchFactor);
}

FRTSSettingsSnapshot URTSSettingsMenuSubsystem::BuildSnapshotFromSettings(const URTSGameUserSettings& GameUserSettings) const
{
	FRTSSettingsSnapshot Snapshot;

	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_WindowMode = GetMenuWindowMode(GameUserSettings.GetFullscreenMode());
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_Resolution = GameUserSettings.GetScreenResolution();
	Snapshot.M_GraphicsSettings.M_DisplaySettings.bM_VSyncEnabled = GameUserSettings.IsVSyncEnabled();
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_OverallQuality = GetScalabilityQualityFromLevel(GameUserSettings.GetOverallScalabilityLevel());
	Snapshot.M_GraphicsSettings.M_DisplaySettings.M_FrameRateLimit = GameUserSettings.GetFrameRateLimit();

	const Scalability::FQualityLevels QualityLevels = GameUserSettings.GetQualityLevels();
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_ViewDistance = GetScalabilityQualityFromLevel(QualityLevels.ViewDistanceQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Shadows = GetScalabilityQualityFromLevel(QualityLevels.ShadowQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Textures = GetScalabilityQualityFromLevel(QualityLevels.TextureQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_Effects = GetScalabilityQualityFromLevel(QualityLevels.EffectsQuality);
	Snapshot.M_GraphicsSettings.M_ScalabilityGroups.M_PostProcessing = GetScalabilityQualityFromLevel(QualityLevels.PostProcessQuality);

	Snapshot.M_AudioSettings.M_MasterVolume = GameUserSettings.GetMasterVolume();
	Snapshot.M_AudioSettings.M_MusicVolume = GameUserSettings.GetMusicVolume();
	Snapshot.M_AudioSettings.M_SfxAndWeaponsVolume = GameUserSettings.GetSfxAndWeaponsVolume();
	Snapshot.M_AudioSettings.M_VoicelinesVolume = GameUserSettings.GetVoicelinesVolume();
	Snapshot.M_AudioSettings.M_AnnouncerVolume = GameUserSettings.GetAnnouncerVolume();
	Snapshot.M_AudioSettings.M_UiVolume = GameUserSettings.GetUiVolume();

	Snapshot.M_ControlSettings.M_MouseSensitivity = GameUserSettings.GetMouseSensitivity();
	Snapshot.M_ControlSettings.bM_InvertYAxis = GameUserSettings.GetInvertYAxis();
	Snapshot.M_GameplaySettings.M_OverwriteAllPlayerHpBarStrat = GameUserSettings.GetOverwriteAllPlayerHpBarStrat();
	Snapshot.M_GameplaySettings.M_PlayerTankHpBarStrat = GameUserSettings.GetPlayerTankHpBarStrat();
	Snapshot.M_GameplaySettings.M_PlayerSquadHpBarStrat = GameUserSettings.GetPlayerSquadHpBarStrat();
	Snapshot.M_GameplaySettings.M_PlayerNomadicHpBarStrat = GameUserSettings.GetPlayerNomadicHpBarStrat();
	Snapshot.M_GameplaySettings.M_PlayerBxpHpBarStrat = GameUserSettings.GetPlayerBxpHpBarStrat();
	Snapshot.M_GameplaySettings.M_PlayerAircraftHpBarStrat = GameUserSettings.GetPlayerAircraftHpBarStrat();
	Snapshot.M_GameplaySettings.M_OverwriteAllEnemyHpBarStrat = GameUserSettings.GetOverwriteAllEnemyHpBarStrat();
	Snapshot.M_GameplaySettings.M_EnemyTankHpBarStrat = GameUserSettings.GetEnemyTankHpBarStrat();
	Snapshot.M_GameplaySettings.M_EnemySquadHpBarStrat = GameUserSettings.GetEnemySquadHpBarStrat();
	Snapshot.M_GameplaySettings.M_EnemyNomadicHpBarStrat = GameUserSettings.GetEnemyNomadicHpBarStrat();
	Snapshot.M_GameplaySettings.M_EnemyBxpHpBarStrat = GameUserSettings.GetEnemyBxpHpBarStrat();
	Snapshot.M_GameplaySettings.M_EnemyAircraftHpBarStrat = GameUserSettings.GetEnemyAircraftHpBarStrat();

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

	Scalability::FQualityLevels QualityLevels = GameUserSettingsToApply.GetQualityLevels();
	RTSSettingsMenuSubsystemPrivate::ApplyScalabilityGroupsToQualityLevels(SnapshotToApply.M_GraphicsSettings.M_ScalabilityGroups, QualityLevels);
	GameUserSettingsToApply.SetQualityLevels(QualityLevels);

	GameUserSettingsToApply.SetMasterVolume(SnapshotToApply.M_AudioSettings.M_MasterVolume);
	GameUserSettingsToApply.SetMusicVolume(SnapshotToApply.M_AudioSettings.M_MusicVolume);
	GameUserSettingsToApply.SetSfxAndWeaponsVolume(SnapshotToApply.M_AudioSettings.M_SfxAndWeaponsVolume);
	GameUserSettingsToApply.SetVoicelinesVolume(SnapshotToApply.M_AudioSettings.M_VoicelinesVolume);
	GameUserSettingsToApply.SetAnnouncerVolume(SnapshotToApply.M_AudioSettings.M_AnnouncerVolume);
	GameUserSettingsToApply.SetUiVolume(SnapshotToApply.M_AudioSettings.M_UiVolume);
	GameUserSettingsToApply.SetMouseSensitivity(SnapshotToApply.M_ControlSettings.M_MouseSensitivity);
	GameUserSettingsToApply.SetInvertYAxis(SnapshotToApply.M_ControlSettings.bM_InvertYAxis);
	GameUserSettingsToApply.SetOverwriteAllPlayerHpBarStrat(SnapshotToApply.M_GameplaySettings.M_OverwriteAllPlayerHpBarStrat);
	GameUserSettingsToApply.SetPlayerTankHpBarStrat(SnapshotToApply.M_GameplaySettings.M_PlayerTankHpBarStrat);
	GameUserSettingsToApply.SetPlayerSquadHpBarStrat(SnapshotToApply.M_GameplaySettings.M_PlayerSquadHpBarStrat);
	GameUserSettingsToApply.SetPlayerNomadicHpBarStrat(SnapshotToApply.M_GameplaySettings.M_PlayerNomadicHpBarStrat);
	GameUserSettingsToApply.SetPlayerBxpHpBarStrat(SnapshotToApply.M_GameplaySettings.M_PlayerBxpHpBarStrat);
	GameUserSettingsToApply.SetPlayerAircraftHpBarStrat(SnapshotToApply.M_GameplaySettings.M_PlayerAircraftHpBarStrat);
	GameUserSettingsToApply.SetOverwriteAllEnemyHpBarStrat(SnapshotToApply.M_GameplaySettings.M_OverwriteAllEnemyHpBarStrat);
	GameUserSettingsToApply.SetEnemyTankHpBarStrat(SnapshotToApply.M_GameplaySettings.M_EnemyTankHpBarStrat);
	GameUserSettingsToApply.SetEnemySquadHpBarStrat(SnapshotToApply.M_GameplaySettings.M_EnemySquadHpBarStrat);
	GameUserSettingsToApply.SetEnemyNomadicHpBarStrat(SnapshotToApply.M_GameplaySettings.M_EnemyNomadicHpBarStrat);
	GameUserSettingsToApply.SetEnemyBxpHpBarStrat(SnapshotToApply.M_GameplaySettings.M_EnemyBxpHpBarStrat);
	GameUserSettingsToApply.SetEnemyAircraftHpBarStrat(SnapshotToApply.M_GameplaySettings.M_EnemyAircraftHpBarStrat);
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
