#include "RTSSteamCaptureAudio.h"

#include "Audio.h"
#include "Misc/FileHelper.h"

namespace RTSSteamCaptureAudioConstants
{
	constexpr float PcmMaximumAmplitude = 32767.0f;
	constexpr int32 PcmBytesPerSample = sizeof(int16);
	constexpr int32 PcmBitsPerSample = PcmBytesPerSample * 8;
}

namespace
{
	void SetSignalMetrics(
		const TArrayView<const int16> PcmSamples,
		float& OutPeakAmplitude,
		double& OutRootMeanSquareAmplitude,
		bool& bOutHasNonSilentSamples)
	{
		OutPeakAmplitude = 0.0f;
		OutRootMeanSquareAmplitude = 0.0;
		bOutHasNonSilentSamples = false;
		if (PcmSamples.IsEmpty())
		{
			return;
		}

		double SumSquaredAmplitude = 0.0;
		for (const int16 PcmSample : PcmSamples)
		{
			const float NormalizedSample = static_cast<float>(PcmSample)
				/ RTSSteamCaptureAudioConstants::PcmMaximumAmplitude;
			OutPeakAmplitude = FMath::Max(OutPeakAmplitude, FMath::Abs(NormalizedSample));
			SumSquaredAmplitude += static_cast<double>(NormalizedSample) * NormalizedSample;
			bOutHasNonSilentSamples = bOutHasNonSilentSamples || PcmSample != 0;
		}

		OutRootMeanSquareAmplitude = FMath::Sqrt(SumSquaredAmplitude / PcmSamples.Num());
	}

	bool CalculateSynchronizedSampleCounts(
		const TArrayView<const float> MasterOutputSamples,
		const int32 ChannelCount,
		const int32 SampleRate,
		const int32 WrittenVideoFrameCount,
		const int32 VideoFramesPerSecond,
		int64& OutSourceSampleFrameCount,
		int64& OutTargetSampleFrameCount,
		int64& OutTargetSampleCount,
		FString& OutError)
	{
		if (ChannelCount <= 0 || SampleRate <= 0 || WrittenVideoFrameCount <= 0 || VideoFramesPerSecond <= 0)
		{
			OutError = TEXT("Cannot save Steam capture audio because its format or video timing is invalid.");
			return false;
		}
		if (MasterOutputSamples.IsEmpty() || MasterOutputSamples.Num() % ChannelCount != 0)
		{
			OutError = TEXT("The Steam capture master submix returned invalid interleaved sample data.");
			return false;
		}

		OutSourceSampleFrameCount = MasterOutputSamples.Num() / ChannelCount;
		OutTargetSampleFrameCount = FMath::RoundToInt64(
			static_cast<double>(WrittenVideoFrameCount) * SampleRate / VideoFramesPerSecond);
		OutTargetSampleCount = OutTargetSampleFrameCount * ChannelCount;
		if (OutTargetSampleFrameCount <= 0
			|| OutTargetSampleCount > MAX_int32
			|| OutTargetSampleCount * RTSSteamCaptureAudioConstants::PcmBytesPerSample > MAX_int32)
		{
			OutError = TEXT("The synchronized Steam capture WAV would exceed the supported PCM file size.");
			return false;
		}
		return true;
	}

	void ConvertToSynchronizedPcm(
		const TArrayView<const float> MasterOutputSamples,
		const int64 TargetSampleCount,
		TArray<int16>& OutPcmSamples)
	{
		OutPcmSamples.SetNumZeroed(static_cast<int32>(TargetSampleCount));
		const int64 SourceSampleCountToCopy = FMath::Min<int64>(
			MasterOutputSamples.Num(),
			TargetSampleCount);
		for (int64 SampleIndex = 0; SampleIndex < SourceSampleCountToCopy; ++SampleIndex)
		{
			const float SourceSample = MasterOutputSamples[SampleIndex];
			const float FiniteSample = FMath::IsFinite(SourceSample) ? SourceSample : 0.0f;
			OutPcmSamples[SampleIndex] = static_cast<int16>(
				FMath::Clamp(FiniteSample, -1.0f, 1.0f)
				* RTSSteamCaptureAudioConstants::PcmMaximumAmplitude);
		}
	}

	void SetAudioResult(
		const int32 ChannelCount,
		const int32 SampleRate,
		const int64 SourceSampleFrameCount,
		const int64 TargetSampleFrameCount,
		const TArrayView<const int16> PcmSamples,
		FRTSSteamCaptureAudioResult& OutResult)
	{
		OutResult.M_SampleRate = SampleRate;
		OutResult.M_ChannelCount = ChannelCount;
		OutResult.M_SourceSampleFrameCount = SourceSampleFrameCount;
		OutResult.M_SynchronizedSampleFrameCount = TargetSampleFrameCount;
		OutResult.M_TrimmedSampleFrameCount = FMath::Max<int64>(
			0,
			SourceSampleFrameCount - TargetSampleFrameCount);
		OutResult.M_PaddedSampleFrameCount = FMath::Max<int64>(
			0,
			TargetSampleFrameCount - SourceSampleFrameCount);
		OutResult.M_DurationSeconds = static_cast<double>(TargetSampleFrameCount) / SampleRate;
		SetSignalMetrics(
			PcmSamples,
			OutResult.M_PeakAmplitude,
			OutResult.M_RootMeanSquareAmplitude,
			OutResult.bM_HasNonSilentSamples);
	}

	bool SavePcmWave(
		const TArrayView<const int16> PcmSamples,
		const int32 ChannelCount,
		const int32 SampleRate,
		const FString& OutputPath,
		FString& OutError)
	{
		TArray<uint8> WaveBytes;
		SerializeWaveFile(
			WaveBytes,
			reinterpret_cast<const uint8*>(PcmSamples.GetData()),
			PcmSamples.Num() * RTSSteamCaptureAudioConstants::PcmBytesPerSample,
			ChannelCount,
			SampleRate);
		if (WaveBytes.IsEmpty() || not FFileHelper::SaveArrayToFile(WaveBytes, *OutputPath))
		{
			OutError = TEXT("Failed to save synchronized Steam capture WAV: ") + OutputPath;
			return false;
		}
		return true;
	}

	bool ReadWaveHeader(
		TArrayView<const uint8> WaveBytes,
		FWaveModInfo& OutParsedWaveInfo,
		FRTSSteamCaptureWaveInfo& OutWaveInfo,
		FString& OutError)
	{
		FString ParseError;
		if (not OutParsedWaveInfo.ReadWaveInfo(WaveBytes.GetData(), WaveBytes.Num(), &ParseError)
			|| OutParsedWaveInfo.pFormatTag == nullptr
			|| OutParsedWaveInfo.pChannels == nullptr
			|| OutParsedWaveInfo.pSamplesPerSec == nullptr
			|| OutParsedWaveInfo.pBitsPerSample == nullptr
			|| OutParsedWaveInfo.SampleDataStart == nullptr)
		{
			OutError = TEXT("Saved Steam capture WAV has an invalid header: ") + ParseError;
			return false;
		}

		OutWaveInfo.M_ChannelCount = *OutParsedWaveInfo.pChannels;
		OutWaveInfo.M_SampleRate = *OutParsedWaveInfo.pSamplesPerSec;
		OutWaveInfo.M_BitsPerSample = *OutParsedWaveInfo.pBitsPerSample;
		OutWaveInfo.M_DataByteCount = OutParsedWaveInfo.SampleDataSize;
		return true;
	}

	bool ValidateWaveFormatAndTiming(
		const FWaveModInfo& ParsedWaveInfo,
		const int32 ExpectedChannelCount,
		const int32 ExpectedSampleRate,
		const int64 ExpectedSampleFrameCount,
		FRTSSteamCaptureWaveInfo& OutWaveInfo,
		FString& OutError)
	{
		if (*ParsedWaveInfo.pFormatTag != FWaveModInfo::WAVE_INFO_FORMAT_PCM
			|| OutWaveInfo.M_BitsPerSample != RTSSteamCaptureAudioConstants::PcmBitsPerSample
			|| OutWaveInfo.M_ChannelCount <= 0
			|| OutWaveInfo.M_SampleRate <= 0)
		{
			OutError = TEXT("Saved Steam capture WAV is not valid 16-bit PCM.");
			return false;
		}

		const int64 BytesPerSampleFrame =
			static_cast<int64>(OutWaveInfo.M_ChannelCount)
			* RTSSteamCaptureAudioConstants::PcmBytesPerSample;
		if (OutWaveInfo.M_DataByteCount % BytesPerSampleFrame != 0)
		{
			OutError = TEXT("Saved Steam capture WAV contains a partial PCM sample frame.");
			return false;
		}

		OutWaveInfo.M_SampleFrameCount = OutWaveInfo.M_DataByteCount / BytesPerSampleFrame;
		OutWaveInfo.M_DurationSeconds = static_cast<double>(OutWaveInfo.M_SampleFrameCount)
			/ OutWaveInfo.M_SampleRate;
		if (OutWaveInfo.M_ChannelCount == ExpectedChannelCount
			&& OutWaveInfo.M_SampleRate == ExpectedSampleRate
			&& OutWaveInfo.M_SampleFrameCount == ExpectedSampleFrameCount)
		{
			return true;
		}

		OutError = FString::Printf(
			TEXT("Saved Steam capture WAV format mismatch. Expected %d channels, %d Hz, %lld frames; got %d channels, %d Hz, %lld frames."),
			ExpectedChannelCount,
			ExpectedSampleRate,
			ExpectedSampleFrameCount,
			OutWaveInfo.M_ChannelCount,
			OutWaveInfo.M_SampleRate,
			OutWaveInfo.M_SampleFrameCount);
		return false;
	}

	bool SetWaveSignalMetrics(
		const FWaveModInfo& ParsedWaveInfo,
		FRTSSteamCaptureWaveInfo& OutWaveInfo,
		FString& OutError)
	{
		const int64 PcmSampleCount =
			OutWaveInfo.M_DataByteCount / RTSSteamCaptureAudioConstants::PcmBytesPerSample;
		if (PcmSampleCount > MAX_int32
			|| OutWaveInfo.M_DataByteCount > MAX_int32)
		{
			OutError = TEXT("Saved Steam capture WAV contains too many PCM samples to validate.");
			return false;
		}

		TArray<int16> PcmSamples;
		PcmSamples.SetNumUninitialized(static_cast<int32>(PcmSampleCount));
		FMemory::Memcpy(
			PcmSamples.GetData(),
			ParsedWaveInfo.SampleDataStart,
			OutWaveInfo.M_DataByteCount);
		SetSignalMetrics(
			PcmSamples,
			OutWaveInfo.M_PeakAmplitude,
			OutWaveInfo.M_RootMeanSquareAmplitude,
			OutWaveInfo.bM_HasNonSilentSamples);
		return true;
	}
}

bool FRTSSteamCaptureAudio::SaveSynchronizedWave(
	const TArrayView<const float> MasterOutputSamples,
	const int32 ChannelCount,
	const int32 SampleRate,
	const int32 WrittenVideoFrameCount,
	const int32 VideoFramesPerSecond,
	const FString& OutputPath,
	FRTSSteamCaptureAudioResult& OutResult,
	FString& OutError)
{
	OutResult = FRTSSteamCaptureAudioResult();
	OutError.Reset();

	int64 SourceSampleFrameCount = 0;
	int64 TargetSampleFrameCount = 0;
	int64 TargetSampleCount = 0;
	if (not CalculateSynchronizedSampleCounts(
		MasterOutputSamples,
		ChannelCount,
		SampleRate,
		WrittenVideoFrameCount,
		VideoFramesPerSecond,
		SourceSampleFrameCount,
		TargetSampleFrameCount,
		TargetSampleCount,
		OutError))
	{
		return false;
	}

	TArray<int16> SynchronizedPcmSamples;
	ConvertToSynchronizedPcm(
		MasterOutputSamples,
		TargetSampleCount,
		SynchronizedPcmSamples);
	SetAudioResult(
		ChannelCount,
		SampleRate,
		SourceSampleFrameCount,
		TargetSampleFrameCount,
		SynchronizedPcmSamples,
		OutResult);
	return SavePcmWave(
		SynchronizedPcmSamples,
		ChannelCount,
		SampleRate,
		OutputPath,
		OutError);
}

bool FRTSSteamCaptureAudio::ValidateWaveFile(
	const FString& WavePath,
	const int32 ExpectedChannelCount,
	const int32 ExpectedSampleRate,
	const int64 ExpectedSampleFrameCount,
	FRTSSteamCaptureWaveInfo& OutWaveInfo,
	FString& OutError)
{
	OutWaveInfo = FRTSSteamCaptureWaveInfo();
	OutError.Reset();

	TArray<uint8> WaveBytes;
	if (not FFileHelper::LoadFileToArray(WaveBytes, *WavePath))
	{
		OutError = TEXT("Failed to read the saved Steam capture WAV: ") + WavePath;
		return false;
	}

	FWaveModInfo ParsedWaveInfo;
	if (not ReadWaveHeader(
		WaveBytes,
		ParsedWaveInfo,
		OutWaveInfo,
		OutError))
	{
		return false;
	}
	if (not ValidateWaveFormatAndTiming(
		ParsedWaveInfo,
		ExpectedChannelCount,
		ExpectedSampleRate,
		ExpectedSampleFrameCount,
		OutWaveInfo,
		OutError))
	{
		return false;
	}
	return SetWaveSignalMetrics(
		ParsedWaveInfo,
		OutWaveInfo,
		OutError);
}
