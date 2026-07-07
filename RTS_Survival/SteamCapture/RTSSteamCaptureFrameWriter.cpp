#include "RTSSteamCaptureFrameWriter.h"

#include "Async/Async.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

FRTSSteamCaptureFrameWriter::FRTSSteamCaptureFrameWriter()
	: M_State(MakeShared<FRTSSteamCaptureFrameWriterState, ESPMode::ThreadSafe>())
{
}

void FRTSSteamCaptureFrameWriter::ResetFailedWriteCount() const
{
	M_State->M_FailedWriteCount.Reset();
}

void FRTSSteamCaptureFrameWriter::WritePngFrameAsync(
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
			const bool bSaved = SavePngFrame(OutputPath, Width, Height, FramePixels);
			if (not bSaved)
			{
				State->M_FailedWriteCount.Increment();
			}
			State->M_PendingWriteCount.Decrement();
		});
}

int32 FRTSSteamCaptureFrameWriter::GetPendingWriteCount() const
{
	return M_State->M_PendingWriteCount.GetValue();
}

int32 FRTSSteamCaptureFrameWriter::GetFailedWriteCount() const
{
	return M_State->M_FailedWriteCount.GetValue();
}

bool FRTSSteamCaptureFrameWriter::SavePngFrame(
	const FString& OutputPath,
	const int32 Width,
	const int32 Height,
	const TArray<FColor>& FramePixels)
{
	if (Width <= 0 || Height <= 0)
	{
		return false;
	}
	if (FramePixels.Num() != Width * Height)
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
