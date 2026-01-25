#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSGameUserSettingsPrivate
{
	constexpr float MinFrameRateLimit = 0.0f;
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

float URTSGameUserSettings::GetSfxVolume() const
{
	return M_SfxVolume;
}

float URTSGameUserSettings::GetMouseSensitivity() const
{
	return M_MouseSensitivity;
}

bool URTSGameUserSettings::GetInvertYAxis() const
{
	return bM_InvertYAxis;
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

void URTSGameUserSettings::SetSfxVolume(const float NewSfxVolume)
{
	M_SfxVolume = NewSfxVolume;
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

	const float ClampedSfxVolume = FMath::Clamp(M_SfxVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	if (not FMath::IsNearlyEqual(ClampedSfxVolume, M_SfxVolume))
	{
		RTSFunctionLibrary::ReportError(TEXT("SFX volume was out of range and has been clamped."));
		M_SfxVolume = ClampedSfxVolume;
	}

	const float ClampedMouseSensitivity = FMath::Clamp(M_MouseSensitivity, RTSGameUserSettingsRanges::MinMouseSensitivity, RTSGameUserSettingsRanges::MaxMouseSensitivity);
	if (not FMath::IsNearlyEqual(ClampedMouseSensitivity, M_MouseSensitivity))
	{
		RTSFunctionLibrary::ReportError(TEXT("Mouse sensitivity was out of range and has been clamped."));
		M_MouseSensitivity = ClampedMouseSensitivity;
	}

	const float FrameRateLimit = GetFrameRateLimit();
	if (FrameRateLimit < MinFrameRateLimit)
	{
		RTSFunctionLibrary::ReportError(TEXT("Frame rate limit was negative and has been reset to unlimited."));
		SetFrameRateLimit(MinFrameRateLimit);
	}
}
