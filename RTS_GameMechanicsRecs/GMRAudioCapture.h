// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISubmixBufferListener.h"

/**
 * @brief Records every buffer the main submix renders, keyed by the audio clock, so the recording director can
 * write the take's WAV itself. Movie Render Queue's own wave output is reset mid-render (see the Shorts design doc),
 * so this listener is the only audio track. Buffers arrive on the audio render thread.
 */
class FGMRAudioCapture final : public ISubmixBufferListener
{
public:
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples,
	                               int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	virtual const FString& GetListenerName() const override;

	double GetCapturedSeconds() const;
	double GetFirstBufferClock() const;

	/**
	 * @brief Writes the samples whose audio clock falls in [StartClock, EndClock) as 16-bit PCM.
	 * @param Path Output WAV path.
	 * @param StartClock Audio clock of the first frame of the take.
	 * @param EndClock Audio clock just after the last frame of the take.
	 * @param OutError Why nothing was written.
	 * @return Seconds written, negative when nothing could be written.
	 */
	double WriteWav(const FString& Path, double StartClock, double EndClock, FString& OutError) const;

private:
	mutable FCriticalSection M_Lock;
	TArray<float> M_Samples;
	double M_FirstBufferClock = -1.0;
	int32 M_SampleRate = 0;
	int32 M_NumChannels = 0;
};
