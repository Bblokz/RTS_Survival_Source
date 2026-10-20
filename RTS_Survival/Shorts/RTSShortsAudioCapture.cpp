// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsAudioCapture.h"

#include "Audio.h"
#include "Misc/FileHelper.h"

void FRTSShortsAudioCapture::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
                                               const int32 NumSamples, const int32 NumChannels,
                                               const int32 SampleRate, const double AudioClock)
{
	if (AudioData == nullptr || NumSamples <= 0 || NumChannels <= 0 || SampleRate <= 0)
	{
		return;
	}
	FScopeLock Lock(&M_Lock);
	if (M_FirstBufferClock < 0.0)
	{
		M_FirstBufferClock = AudioClock;
		M_SampleRate = SampleRate;
		M_NumChannels = NumChannels;
	}
	if (SampleRate != M_SampleRate || NumChannels != M_NumChannels)
	{
		// Format changes mid-render would corrupt the timeline; keep the first format and drop the odd buffer.
		return;
	}
	M_Samples.Append(AudioData, NumSamples);
}

const FString& FRTSShortsAudioCapture::GetListenerName() const
{
	static const FString Name(TEXT("RTSShortsAudioCapture"));
	return Name;
}

double FRTSShortsAudioCapture::GetCapturedSeconds() const
{
	FScopeLock Lock(&M_Lock);
	if (M_SampleRate <= 0 || M_NumChannels <= 0)
	{
		return 0.0;
	}
	return static_cast<double>(M_Samples.Num()) / (M_SampleRate * M_NumChannels);
}

double FRTSShortsAudioCapture::WriteWav(const FString& Path, const double StartClock, const double EndClock,
                                        FString& OutError) const
{
	TArray<float> Copy;
	double FirstClock;
	int32 SampleRate;
	int32 NumChannels;
	{
		FScopeLock Lock(&M_Lock);
		Copy = M_Samples;
		FirstClock = M_FirstBufferClock;
		SampleRate = M_SampleRate;
		NumChannels = M_NumChannels;
	}
	if (Copy.Num() == 0 || SampleRate <= 0 || NumChannels <= 0)
	{
		OutError = TEXT("no audio captured");
		return -1.0;
	}
	const int64 TotalFrames = Copy.Num() / NumChannels;
	const int64 StartFrame = FMath::Clamp<int64>(FMath::RoundToInt64((StartClock - FirstClock) * SampleRate), 0,
	                                              TotalFrames);
	const int64 EndFrame = FMath::Clamp<int64>(FMath::RoundToInt64((EndClock - FirstClock) * SampleRate), StartFrame,
	                                            TotalFrames);
	if (EndFrame <= StartFrame)
	{
		OutError = FString::Printf(TEXT("empty clock window %.3f..%.3f (captured %.3f..%.3f)"), StartClock, EndClock,
		                           FirstClock, FirstClock + static_cast<double>(TotalFrames) / SampleRate);
		return -1.0;
	}
	TArray<int16> Pcm;
	Pcm.SetNumUninitialized(static_cast<int32>((EndFrame - StartFrame) * NumChannels));
	const float* Source = Copy.GetData() + StartFrame * NumChannels;
	for (int32 Index = 0; Index < Pcm.Num(); ++Index)
	{
		Pcm[Index] = static_cast<int16>(FMath::Clamp(Source[Index], -1.f, 1.f) * 32767.f);
	}
	TArray<uint8> WaveBytes;
	SerializeWaveFile(WaveBytes, reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16),
	                  NumChannels, SampleRate);
	if (WaveBytes.IsEmpty() || not FFileHelper::SaveArrayToFile(WaveBytes, *Path))
	{
		OutError = TEXT("could not save ") + Path;
		return -1.0;
	}
	return static_cast<double>(EndFrame - StartFrame) / SampleRate;
}
