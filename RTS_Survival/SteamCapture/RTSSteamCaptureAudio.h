#pragma once

#include "CoreMinimal.h"

/**
 * @brief Describes how master-output samples were aligned to the written video frame sequence.
 * Metadata uses this to make audio/video synchronization and any end padding or trimming auditable.
 */
struct FRTSSteamCaptureAudioResult
{
	int32 M_SampleRate = 0;
	int32 M_ChannelCount = 0;
	int64 M_SourceSampleFrameCount = 0;
	int64 M_SynchronizedSampleFrameCount = 0;
	int64 M_TrimmedSampleFrameCount = 0;
	int64 M_PaddedSampleFrameCount = 0;
	double M_DurationSeconds = 0.0;
	float M_PeakAmplitude = 0.0f;
	double M_RootMeanSquareAmplitude = 0.0;
	bool bM_HasNonSilentSamples = false;
};

/**
 * @brief Describes PCM data parsed back from a saved WAV.
 * Reading the file back verifies the on-disk artifact rather than trusting the write request.
 */
struct FRTSSteamCaptureWaveInfo
{
	int32 M_SampleRate = 0;
	int32 M_ChannelCount = 0;
	int32 M_BitsPerSample = 0;
	int64 M_SampleFrameCount = 0;
	int64 M_DataByteCount = 0;
	double M_DurationSeconds = 0.0;
	float M_PeakAmplitude = 0.0f;
	double M_RootMeanSquareAmplitude = 0.0;
	bool bM_HasNonSilentSamples = false;
};

/**
 * @brief Converts the final player mix to a duration-locked PCM WAV and validates it from disk.
 * Audio stays at the device sample rate while its length follows the exact written-frame duration.
 */
class RTS_SURVIVAL_API FRTSSteamCaptureAudio
{
public:
	/**
	 * @brief Prevents audio drift by matching PCM length to the image sequence rather than wall-clock stop jitter.
	 * @param MasterOutputSamples Interleaved float samples captured from the player's main submix.
	 * @param ChannelCount Number of interleaved output channels.
	 * @param SampleRate Native audio-device sample rate.
	 * @param WrittenVideoFrameCount Number of PNG frames successfully queued for the session.
	 * @param VideoFramesPerSecond Frame rate FFmpeg must use for the image sequence.
	 * @param OutputPath Absolute WAV output path.
	 * @param OutResult Alignment and signal metrics for metadata and verification.
	 * @param OutError Actionable failure description when the WAV cannot be produced.
	 * @return True when a synchronized PCM WAV was saved.
	 */
	static bool SaveSynchronizedWave(
		TArrayView<const float> MasterOutputSamples,
		int32 ChannelCount,
		int32 SampleRate,
		int32 WrittenVideoFrameCount,
		int32 VideoFramesPerSecond,
		const FString& OutputPath,
		FRTSSteamCaptureAudioResult& OutResult,
		FString& OutError);

	/**
	 * @brief Proves the saved artifact contains the expected PCM format, duration, and readable sample data.
	 * @param WavePath Absolute path of the WAV to inspect.
	 * @param ExpectedChannelCount Channel count recorded from the audio device.
	 * @param ExpectedSampleRate Sample rate recorded from the audio device.
	 * @param ExpectedSampleFrameCount Duration-locked number of PCM frames.
	 * @param OutWaveInfo Parsed on-disk format and signal metrics.
	 * @param OutError Actionable validation failure description.
	 * @return True when the WAV is valid 16-bit PCM with the exact expected format and length.
	 */
	static bool ValidateWaveFile(
		const FString& WavePath,
		int32 ExpectedChannelCount,
		int32 ExpectedSampleRate,
		int64 ExpectedSampleFrameCount,
		FRTSSteamCaptureWaveInfo& OutWaveInfo,
		FString& OutError);
};
