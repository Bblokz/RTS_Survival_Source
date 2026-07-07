#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"

struct FRTSSteamCaptureFrameWriterState
{
	FThreadSafeCounter M_PendingWriteCount;
	FThreadSafeCounter M_FailedWriteCount;
};

/**
 * @brief Compresses and writes captured frame pixels away from the game thread.
 * The subsystem owns capture timing; this helper only tracks async PNG write completion.
 */
class RTS_SURVIVAL_API FRTSSteamCaptureFrameWriter
{
public:
	FRTSSteamCaptureFrameWriter();

	void ResetFailedWriteCount() const;
	void WritePngFrameAsync(FString OutputPath, int32 Width, int32 Height, TArray<FColor>&& FramePixels) const;

	int32 GetPendingWriteCount() const;
	int32 GetFailedWriteCount() const;

private:
	TSharedRef<FRTSSteamCaptureFrameWriterState, ESPMode::ThreadSafe> M_State;

	static bool SavePngFrame(const FString& OutputPath, int32 Width, int32 Height, const TArray<FColor>& FramePixels);
};
