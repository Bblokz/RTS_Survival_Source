// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISubmixBufferListener.h"

/**
 * @brief Captures every buffer the main submix renders, keyed by the audio clock, so the shorts director can write
 * its own WAV. Movie Render Queue records the same submix through FMixerSubmix::OnStartRecordingOutput, which any
 * later StartRecording call on the main submix silently resets; this listener's buffer is never reset.
 * Buffers arrive on the audio render thread; everything else runs on the game thread.
 */
class FRTSShortsAudioCapture final : public ISubmixBufferListener
{
public:
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples,
	                               int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	virtual const FString& GetListenerName() const override;

	/** Seconds captured so far (all channels, from the first buffer). */
	double GetCapturedSeconds() const;
	int32 GetSampleRate() const { return M_SampleRate; }
	int32 GetNumChannels() const { return M_NumChannels; }
	double GetFirstBufferClock() const { return M_FirstBufferClock; }

	/**
	 * Writes the samples whose audio clock falls in [StartClock, EndClock) as a 16-bit PCM WAV.
	 * @return the seconds written, or a negative value when nothing could be written.
	 */
	double WriteWav(const FString& Path, double StartClock, double EndClock, FString& OutError) const;

private:
	mutable FCriticalSection M_Lock;
	TArray<float> M_Samples;
	double M_FirstBufferClock = -1.0;
	int32 M_SampleRate = 0;
	int32 M_NumChannels = 0;
};
