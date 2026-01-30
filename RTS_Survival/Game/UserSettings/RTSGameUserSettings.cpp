#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSGameUserSettingsPrivate
{
	constexpr float MinFrameRateLimit = 0.0f;
}

const URTSGameUserSettings* URTSGameUserSettings::Get()
{
	UGameUserSettings* const BaseSettings = UGameUserSettings::GetGameUserSettings();
	if (BaseSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UGameUserSettings::GetGameUserSettings returned null."));
		return nullptr;
	}

	const URTSGameUserSettings* const RTSSettings = Cast<URTSGameUserSettings>(BaseSettings);
	if (RTSSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("GameUserSettings is not an instance of URTSGameUserSettings."));
		return nullptr;
	}

	return RTSSettings;
}

void URTSGameUserSettings::LoadSettings(const bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::ApplySettings(const bool bCheckForCommandLineOverrides)
{
	ApplyCustomSettingClamps();
	Super::ApplySettings(bCheckForCommandLineOverrides);
}

void URTSGameUserSettings::SaveSettings()
{
	ApplyCustomSettingClamps();
	Super::SaveSettings();
}

float URTSGameUserSettings::GetMasterVolume() const
{
	return M_MasterVolume;
}

float URTSGameUserSettings::GetMusicVolume() const
{
	return M_MusicVolume;
}

float URTSGameUserSettings::GetSfxAndWeaponsVolume() const
{
	return M_SfxAndWeaponsVolume;
}

float URTSGameUserSettings::GetVoicelinesVolume() const
{
	return M_VoicelinesVolume;
}

float URTSGameUserSettings::GetAnnouncerVolume() const
{
	return M_AnnouncerVolume;
}

float URTSGameUserSettings::GetTransmissionsAndCinematicsVolume() const
{
	return M_TransmissionsAndCinematicsVolume;
}

float URTSGameUserSettings::GetUiVolume() const
{
	return M_UiVolume;
}

float URTSGameUserSettings::GetMouseSensitivity() const
{
	return M_MouseSensitivity;
}

bool URTSGameUserSettings::GetInvertYAxis() const
{
	return bM_InvertYAxis;
}

bool URTSGameUserSettings::GetHideActionButtonHotkeys() const
{
	return bM_HideActionButtonHotkeys;
}

void URTSGameUserSettings::SetMasterVolume(const float NewMasterVolume)
{
	M_MasterVolume = NewMasterVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetMusicVolume(const float NewMusicVolume)
{
	M_MusicVolume = NewMusicVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetSfxAndWeaponsVolume(const float NewSfxAndWeaponsVolume)
{
	M_SfxAndWeaponsVolume = NewSfxAndWeaponsVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetVoicelinesVolume(const float NewVoicelinesVolume)
{
	M_VoicelinesVolume = NewVoicelinesVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetAnnouncerVolume(const float NewAnnouncerVolume)
{
	M_AnnouncerVolume = NewAnnouncerVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetTransmissionsAndCinematicsVolume(const float NewTransmissionsAndCinematicsVolume)
{
	M_TransmissionsAndCinematicsVolume = NewTransmissionsAndCinematicsVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetUiVolume(const float NewUiVolume)
{
	M_UiVolume = NewUiVolume;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetMouseSensitivity(const float NewMouseSensitivity)
{
	M_MouseSensitivity = NewMouseSensitivity;
	ApplyCustomSettingClamps();
}

void URTSGameUserSettings::SetInvertYAxis(const bool bNewInvertYAxis)
{
	bM_InvertYAxis = bNewInvertYAxis;
}

void URTSGameUserSettings::SetHideActionButtonHotkeys(const bool bNewHideActionButtonHotkeys)
{
	bM_HideActionButtonHotkeys = bNewHideActionButtonHotkeys;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetOverwriteAllPlayerHpBarStrat() const
{
	return M_OverwriteAllPlayerHpBarStrat;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetPlayerTankHpBarStrat() const
{
	return M_PlayerTankHpBarStrat;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetPlayerSquadHpBarStrat() const
{
	return M_PlayerSquadHpBarStrat;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetPlayerNomadicHpBarStrat() const
{
	return M_PlayerNomadicHpBarStrat;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetPlayerBxpHpBarStrat() const
{
	return M_PlayerBxpHpBarStrat;
}

ERTSPlayerHealthBarVisibilityStrategy URTSGameUserSettings::GetPlayerAircraftHpBarStrat() const
{
	return M_PlayerAircraftHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetOverwriteAllEnemyHpBarStrat() const
{
	return M_OverwriteAllEnemyHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetEnemyTankHpBarStrat() const
{
	return M_EnemyTankHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetEnemySquadHpBarStrat() const
{
	return M_EnemySquadHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetEnemyNomadicHpBarStrat() const
{
	return M_EnemyNomadicHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetEnemyBxpHpBarStrat() const
{
	return M_EnemyBxpHpBarStrat;
}

ERTSEnemyHealthBarVisibilityStrategy URTSGameUserSettings::GetEnemyAircraftHpBarStrat() const
{
	return M_EnemyAircraftHpBarStrat;
}

void URTSGameUserSettings::SetOverwriteAllPlayerHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_OverwriteAllPlayerHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetPlayerTankHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PlayerTankHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetPlayerSquadHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PlayerSquadHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetPlayerNomadicHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PlayerNomadicHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetPlayerBxpHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PlayerBxpHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetPlayerAircraftHpBarStrat(const ERTSPlayerHealthBarVisibilityStrategy NewStrategy)
{
	M_PlayerAircraftHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetOverwriteAllEnemyHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_OverwriteAllEnemyHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetEnemyTankHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_EnemyTankHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetEnemySquadHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_EnemySquadHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetEnemyNomadicHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_EnemyNomadicHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetEnemyBxpHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_EnemyBxpHpBarStrat = NewStrategy;
}

void URTSGameUserSettings::SetEnemyAircraftHpBarStrat(const ERTSEnemyHealthBarVisibilityStrategy NewStrategy)
{
	M_EnemyAircraftHpBarStrat = NewStrategy;
}

Scalability::FQualityLevels URTSGameUserSettings::GetQualityLevels() const
{
	return Scalability::GetQualityLevels();
}

void URTSGameUserSettings::SetQualityLevels(const Scalability::FQualityLevels& NewQualityLevels)
{
	Scalability::SetQualityLevels(NewQualityLevels);
}

void URTSGameUserSettings::ApplyCustomSettingClamps()
{
	const float ClampedMasterVolume = FMath::Clamp(M_MasterVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedMasterVolume, M_MasterVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("Master volume was out of range and has been clamped."));
		M_MasterVolume = ClampedMasterVolume;
	}

	const float ClampedMusicVolume = FMath::Clamp(M_MusicVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedMusicVolume, M_MusicVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("Music volume was out of range and has been clamped."));
		M_MusicVolume = ClampedMusicVolume;
	}

	const float ClampedSfxVolume = FMath::Clamp(M_SfxAndWeaponsVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedSfxVolume, M_SfxAndWeaponsVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("SFX and weapons volume was out of range and has been clamped."));
		M_SfxAndWeaponsVolume = ClampedSfxVolume;
	}

	const float ClampedVoicelinesVolume = FMath::Clamp(M_VoicelinesVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedVoicelinesVolume, M_VoicelinesVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("Voicelines volume was out of range and has been clamped."));
		M_VoicelinesVolume = ClampedVoicelinesVolume;
	}

	const float ClampedAnnouncerVolume = FMath::Clamp(M_AnnouncerVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedAnnouncerVolume, M_AnnouncerVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("Announcer volume was out of range and has been clamped."));
		M_AnnouncerVolume = ClampedAnnouncerVolume;
	}

	const float ClampedTransmissionsAndCinematicsVolume = FMath::Clamp(
		M_TransmissionsAndCinematicsVolume,
		RTSGameUserSettingsRanges::MinVolume,
		RTSGameUserSettingsRanges::MaxVolume
	);
	if (not FMath::IsNearlyEqual(ClampedTransmissionsAndCinematicsVolume, M_TransmissionsAndCinematicsVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("Transmissions and cinematics volume was out of range and has been clamped."));
		M_TransmissionsAndCinematicsVolume = ClampedTransmissionsAndCinematicsVolume;
	}

	const float ClampedUiVolume = FMath::Clamp(M_UiVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedUiVolume, M_UiVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("UI volume was out of range and has been clamped."));
		M_UiVolume = ClampedUiVolume;
	}

	const float ClampedMouseSensitivity = FMath::Clamp(M_MouseSensitivity, RTSGameUserSettingsRanges::MinMouseSensitivity, RTSGameUserSettingsRanges::MaxMouseSensitivity);
	if (not FMath::IsNearlyEqual(ClampedMouseSensitivity, M_MouseSensitivity))
	{
		RTSFunctionLibrary::ReportError(TEXT("Mouse sensitivity was out of range and has been clamped."));
		M_MouseSensitivity = ClampedMouseSensitivity;
	}

	const float CurrentFrameRateLimit = GetFrameRateLimit();
	if (CurrentFrameRateLimit < RTSGameUserSettingsPrivate::MinFrameRateLimit)
	{
		RTSFunctionLibrary::ReportError(TEXT("Frame rate limit was negative and has been reset to unlimited."));
		SetFrameRateLimit(RTSGameUserSettingsPrivate::MinFrameRateLimit);
	}
}
