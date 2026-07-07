#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSSteamCaptureSettings.generated.h"

namespace RTSSteamCaptureDefaults
{
	inline constexpr int32 OutputResolutionX = 1170;
	inline constexpr int32 OutputResolutionY = 658;
	inline constexpr int32 FramesPerSecond = 30;
	inline constexpr float MaxDurationSeconds = 12.0f;
	inline constexpr int32 MaxPendingFrameWrites = 90;
	inline constexpr float MaxFrameWriteFlushSeconds = 60.0f;
	inline constexpr float CaptureFovDegrees = 60.0f;
	inline constexpr float FovMultiplier = 1.0f;
}

/**
 * @brief Camera transform tuning for the Steam capture camera that shadows the player view.
 * Designers use offsets to keep gameplay framing readable without changing the actual player camera.
 */
USTRUCT(BlueprintType)
struct FRTSSteamCaptureCameraSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Camera")
	bool bM_MatchPlayerCameraFov = true;

	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="5.0", ClampMax="170.0", UIMin="5.0", UIMax="120.0", EditCondition="not bM_MatchPlayerCameraFov"))
	float M_CaptureFovDegrees = RTSSteamCaptureDefaults::CaptureFovDegrees;

	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.1", ClampMax="3.0", UIMin="0.5", UIMax="1.5"))
	float M_FovMultiplier = RTSSteamCaptureDefaults::FovMultiplier;

	UPROPERTY(EditAnywhere, Category="Camera")
	FVector M_LocalLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Camera")
	FRotator M_RotationOffset = FRotator::ZeroRotator;
};

/**
 * @brief Project settings for the PIE-only Steam capture tool.
 * Designers tune resolution, duration, frame rate, and camera offsets here while gameplay code only toggles capture.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Steam Capture"))
class RTS_SURVIVAL_API URTSSteamCaptureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSSteamCaptureSettings();

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Global")
	bool bM_EnableSteamCapture = true;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Output")
	FString M_OutputDirectoryName = TEXT("SteamCaptures");

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Output")
	FString M_FileNamePrefix = TEXT("SteamCapture");

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Output", meta=(ClampMin="1", UIMin="1"))
	int32 M_OutputResolutionX = RTSSteamCaptureDefaults::OutputResolutionX;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Output", meta=(ClampMin="1", UIMin="1"))
	int32 M_OutputResolutionY = RTSSteamCaptureDefaults::OutputResolutionY;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Timing", meta=(ClampMin="1", ClampMax="120", UIMin="1", UIMax="60"))
	int32 M_FramesPerSecond = RTSSteamCaptureDefaults::FramesPerSecond;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Timing", meta=(ClampMin="0.1", UIMin="0.1"))
	float M_MaxDurationSeconds = RTSSteamCaptureDefaults::MaxDurationSeconds;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Timing")
	bool bM_AutoStopAtMaxDuration = true;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Performance", meta=(ClampMin="1", UIMin="1"))
	int32 M_MaxPendingFrameWrites = RTSSteamCaptureDefaults::MaxPendingFrameWrites;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Performance")
	bool bM_WaitForFrameWritesOnStop = true;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Performance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MaxFrameWriteFlushSeconds = RTSSteamCaptureDefaults::MaxFrameWriteFlushSeconds;

	UPROPERTY(Config, EditAnywhere, Category="Steam Capture|Camera")
	FRTSSteamCaptureCameraSettings M_CameraSettings;
};
