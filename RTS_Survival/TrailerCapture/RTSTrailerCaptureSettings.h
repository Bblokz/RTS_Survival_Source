#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSTrailerCaptureSettings.generated.h"

namespace RTSTrailerCaptureDefaults
{
	inline constexpr int32 OutputResolutionX = 1920;
	inline constexpr int32 OutputResolutionY = 1080;
	inline constexpr int32 FramesPerSecond = 60;
	inline constexpr float MaximumDurationSeconds = 120.0f;
	inline constexpr int32 VideoConstantRateFactor = 16;
	inline constexpr int32 AudioBitrateKbps = 320;
	inline constexpr int32 MaximumPendingPngWrites = 90;
	inline constexpr float FinalizationTimeoutSeconds = 120.0f;
	inline constexpr float EncodingTimeoutSeconds = 600.0f;
}

/**
 * @brief Project settings for player-viewport trailer recording and FFmpeg encoding.
 * Designers can tune output quality and safety limits without changing the capture implementation.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Trailer Capture"))
class RTS_SURVIVAL_API URTSTrailerCaptureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSTrailerCaptureSettings();

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Output", meta=(ClampMin="2", UIMin="2"))
	int32 M_DefaultResolutionX = RTSTrailerCaptureDefaults::OutputResolutionX;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Output", meta=(ClampMin="2", UIMin="2"))
	int32 M_DefaultResolutionY = RTSTrailerCaptureDefaults::OutputResolutionY;

	// Relative paths are placed under the project's Saved directory.
	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Output")
	FString M_OutputRoot = TEXT("TrailerCaptures");

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Timing", meta=(ClampMin="1", ClampMax="120", UIMin="1", UIMax="60"))
	int32 M_FramesPerSecond = RTSTrailerCaptureDefaults::FramesPerSecond;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Timing", meta=(ClampMin="0.1", UIMin="0.1"))
	float M_MaximumDurationSeconds = RTSTrailerCaptureDefaults::MaximumDurationSeconds;

	// Leave empty to use ffmpeg.exe from the process PATH.
	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding", meta=(FilePathFilter="exe"))
	FFilePath M_FFmpegExecutable;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding")
	FString M_H264Codec = TEXT("libx264");

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding", meta=(ClampMin="0", ClampMax="51", UIMin="0", UIMax="51"))
	int32 M_VideoConstantRateFactor = RTSTrailerCaptureDefaults::VideoConstantRateFactor;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding")
	FString M_H264Preset = TEXT("slow");

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding", meta=(ClampMin="32", UIMin="32"))
	int32 M_AudioBitrateKbps = RTSTrailerCaptureDefaults::AudioBitrateKbps;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Encoding")
	bool bM_DeleteIntermediatesAfterSuccessfulEncoding = true;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Safety", meta=(ClampMin="1", UIMin="1"))
	int32 M_MaximumPendingPngWrites = RTSTrailerCaptureDefaults::MaximumPendingPngWrites;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Safety", meta=(ClampMin="1.0", UIMin="1.0"))
	float M_FinalizationTimeoutSeconds = RTSTrailerCaptureDefaults::FinalizationTimeoutSeconds;

	UPROPERTY(Config, EditAnywhere, Category="Trailer Capture|Safety", meta=(ClampMin="1.0", UIMin="1.0"))
	float M_EncodingTimeoutSeconds = RTSTrailerCaptureDefaults::EncodingTimeoutSeconds;
};
