// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "RTSCameraDeveloperSettings.h"

FRTSCameraTuningSettings FRTSCameraTuningSettings::MakeCinematicDefaults()
{
	FRTSCameraTuningSettings CinematicDefaults = FRTSCameraTuningSettings();
	CinematicDefaults.M_MinZoomLimit = 50.f;
	CinematicDefaults.M_MaxZoomLimit = 6000.f;
	CinematicDefaults.M_ZoomSpeed = 150.f;
	CinematicDefaults.M_CameraPitchLimit = 30.f;
	CinematicDefaults.M_CameraPanSpeed = 5.f;
	CinematicDefaults.M_DefaultCameraMovementSpeed = 15.f;
	CinematicDefaults.M_DefaultModifierCameraMovementSpeed = 1.f;
	return CinematicDefaults;
}

URTSCameraDeveloperSettings::URTSCameraDeveloperSettings()
{
	CategoryName = TEXT("RTS");
	SectionName = TEXT("Camera");
}

const URTSCameraDeveloperSettings* URTSCameraDeveloperSettings::Get()
{
	return GetDefault<URTSCameraDeveloperSettings>();
}

const FRTSCameraTuningSettings& URTSCameraDeveloperSettings::GetActiveCameraSettings() const
{
	return bM_UseCinematicCameraSettings ? M_CinematicCameraSettings : M_GameplayCameraSettings;
}
