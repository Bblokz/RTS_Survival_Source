#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"

struct FRTSTrailerFrameWriterState
{
	FThreadSafeCounter M_PendingWriteCount;
	FThreadSafeCounter M_FailedWriteCount;
};

/**
 * @brief Compresses display-encoded viewport pixels to PNG on background workers.
 * The component bounds submissions while this helper safely outlives any queued task.
 */
class RTS_SURVIVAL_API FRTSTrailerFrameWriter
{
public:
	FRTSTrailerFrameWriter();

	void Reset() const;

	/**
	 * @brief Moves pixels into a self-contained task so component teardown cannot invalidate a write.
	 * @param OutputPath Absolute PNG destination for this frame.
	 * @param Width Pixel width represented by FramePixels.
	 * @param Height Pixel height represented by FramePixels.
	 * @param FramePixels Display-encoded BGRA pixels transferred to the worker.
	 */
	void WritePngFrameAsync(FString OutputPath, int32 Width, int32 Height, TArray<FColor>&& FramePixels) const;

	int32 GetPendingWriteCount() const;
	int32 GetFailedWriteCount() const;

private:
	TSharedRef<FRTSTrailerFrameWriterState, ESPMode::ThreadSafe> M_State;

	static bool SavePngFrame(const FString& OutputPath, int32 Width, int32 Height, const TArray<FColor>& FramePixels);
};
