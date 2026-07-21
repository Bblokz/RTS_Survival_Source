#include "RTSTrailerFrameWriter.h"

#include "Async/Async.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

FRTSTrailerFrameWriter::FRTSTrailerFrameWriter()
	: M_State(MakeShared<FRTSTrailerFrameWriterState, ESPMode::ThreadSafe>())
{
}

void FRTSTrailerFrameWriter::Reset() const
{
	M_State->M_FailedWriteCount.Reset();
}

void FRTSTrailerFrameWriter::WritePngFrameAsync(
	FString OutputPath,
	const int32 Width,
	const int32 Height,
	TArray<FColor>&& FramePixels) const
{
	M_State->M_PendingWriteCount.Increment();
	Async(
		EAsyncExecution::ThreadPool,
		[
			State = M_State,
			OutputPath = MoveTemp(OutputPath),
			Width,
			Height,
			FramePixels = MoveTemp(FramePixels)
		]() mutable
		{
			if (not SavePngFrame(OutputPath, Width, Height, FramePixels))
			{
				State->M_FailedWriteCount.Increment();
			}
			State->M_PendingWriteCount.Decrement();
		});
}

int32 FRTSTrailerFrameWriter::GetPendingWriteCount() const
{
	return M_State->M_PendingWriteCount.GetValue();
}

int32 FRTSTrailerFrameWriter::GetFailedWriteCount() const
{
	return M_State->M_FailedWriteCount.GetValue();
}

bool FRTSTrailerFrameWriter::SavePngFrame(
	const FString& OutputPath,
	const int32 Width,
	const int32 Height,
	const TArray<FColor>& FramePixels)
{
	if (Width <= 0 || Height <= 0)
	{
		return false;
	}
	const int64 RequiredPixelCount = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (static_cast<int64>(FramePixels.Num()) != RequiredPixelCount)
	{
		return false;
	}

	TArray64<uint8> PngBytes;
	FImageUtils::PNGCompressImageArray(
		Width,
		Height,
		TArrayView64<const FColor>(FramePixels),
		PngBytes);
	return FFileHelper::SaveArrayToFile(PngBytes, *OutputPath);
}
