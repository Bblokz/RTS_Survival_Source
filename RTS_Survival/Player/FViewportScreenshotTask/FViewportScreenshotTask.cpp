#include "FViewportScreenshotTask.h"

#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"        // FViewport / TakeHighResScreenShot
#include "HighResScreenshot.h"   // FScreenshotRequest
#include "ImageUtils.h"          // SaveImageByExtension / PNGCompressImageArray
#include "ImageCore.h"           // FImageView / ERawImageFormat
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

bool FViewportScreenshotTask::Request(const UWorld* World, const bool bIncludeUI, float InnerPercent)
{
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("ViewportScreenshotTask: World is null."));
		return false;
	}

	UGameViewportClient* const GameViewportClient = World->GetGameViewport();
	if (GameViewportClient == nullptr || GameViewportClient->Viewport == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("ViewportScreenshotTask: GameViewportClient/Viewport is null."));
		return false;
	}

	// Clear any previous binding and set up state.
	Shutdown();

	M_GameViewportClient   = GameViewportClient;
	bM_Pending             = true;
	bM_IncludeUI           = bIncludeUI;
	M_PendingInnerPercent  = FMath::Clamp(InnerPercent, 0.05f, 1.0f);

	// Bind end-of-frame screenshot delegate. When bound, engine won't auto-write a file. We must save. 
	M_ScreenshotDelegateHandle = UGameViewportClient::OnScreenshotCaptured()
		.AddRaw(this, &FViewportScreenshotTask::OnScreenshotCaptured); // fires once this frame. :contentReference[oaicite:1]{index=1}

	// Hide stat overlays that would otherwise burn in.
	if (APlayerController* const PC = World->GetFirstPlayerController())
	{
		PC->ConsoleCommand(TEXT("stat none"), /*bWriteToLog=*/false);
	}

	// Trigger the capture at current viewport res.
	const bool bTriggered = GameViewportClient->Viewport->TakeHighResScreenShot(); // UE5 path. :contentReference[oaicite:2]{index=2}
	if (not bTriggered)
	{
		UE_LOG(LogTemp, Warning, TEXT("ViewportScreenshotTask: TakeHighResScreenShot() returned false."));
		Shutdown();
		return false;
	}
	return true;
}

void FViewportScreenshotTask::Shutdown()
{
	if (M_ScreenshotDelegateHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(M_ScreenshotDelegateHandle);
		M_ScreenshotDelegateHandle.Reset();
	}
}

void FViewportScreenshotTask::OnScreenshotCaptured(const int32 Width,
                                                   const int32 Height,
                                                   const TArray<FColor>& Bitmap)
{
	// One-shot: unbind immediately.
	Shutdown();

	if (not bM_Pending)
	{
		return;
	}
	bM_Pending = false;

	// Crop central area.
	TArray<FColor> Cropped;
	int32 OutW = Width, OutH = Height;
	CropCentral(Bitmap, Width, Height, M_PendingInnerPercent, Cropped, OutW, OutH);

	// Always write a unique file name in the engine's screenshot directory.
	const FString OutPath = BuildUniqueOutputPath(M_PendingInnerPercent, M_SerialCounter++);

	// Save (auto-picks format by extension).
	FImageView View(const_cast<FColor*>(Cropped.GetData()), OutW, OutH, ERawImageFormat::BGRA8);
	bool bSaved = FImageUtils::SaveImageByExtension(*OutPath, View, /*Quality=*/100); // PNG by ".png". :contentReference[oaicite:3]{index=3}

	if (not bSaved)
	{
		// Fallback PNG encode.
		TArray64<uint8> PngBytes;
		FImageUtils::PNGCompressImageArray(OutW, OutH, TArrayView64<const FColor>(Cropped), PngBytes);
		bSaved = FFileHelper::SaveArrayToFile(PngBytes, *OutPath);
	}

	UE_LOG(LogTemp, Log, TEXT("Viewport screenshot saved: %s (%dx%d, inner %.2f)"),
	       *OutPath, OutW, OutH, M_PendingInnerPercent);
}

void FViewportScreenshotTask::CropCentral(const TArray<FColor>& Src,
                                          const int32 SrcW,
                                          const int32 SrcH,
                                          const float InnerPct,
                                          TArray<FColor>& Out,
                                          int32& OutW,
                                          int32& OutH)
{
	if (InnerPct >= 0.9999f)
	{
		Out  = Src;
		OutW = SrcW;
		OutH = SrcH;
		return;
	}

	const int32 CropW = FMath::Max(1, FMath::FloorToInt(static_cast<float>(SrcW) * InnerPct));
	const int32 CropH = FMath::Max(1, FMath::FloorToInt(static_cast<float>(SrcH) * InnerPct));
	const int32 X0    = (SrcW - CropW) / 2;
	const int32 Y0    = (SrcH - CropH) / 2;

	Out.SetNumUninitialized(CropW * CropH);
	for (int32 Row = 0; Row < CropH; ++Row)
	{
		const FColor* const SrcRow = &Src[(Y0 + Row) * SrcW + X0];
		FMemory::Memcpy(&Out[Row * CropW], SrcRow, sizeof(FColor) * CropW);
	}
	OutW = CropW;
	OutH = CropH;
}

FString FViewportScreenshotTask::BuildUniqueOutputPath(const float InnerPct, const uint64 SerialCounter)
{
	// Target directory where UE stores screenshots per-platform. :contentReference[oaicite:4]{index=4}
	const FString Dir = FPaths::ScreenShotDir();

	// Timestamp (with milliseconds) + monotonic serial + GUID to be collision-proof even in the same frame.
	const FDateTime Now = FDateTime::Now();                             // local time is fine
	const FString   Stamp = Now.ToString(TEXT("%Y%m%d-%H%M%S"));
	const int32     Ms    = Now.GetMillisecond();                       // 0..999  :contentReference[oaicite:5]{index=5}
	const int32     Pct   = FMath::Clamp(FMath::RoundToInt(InnerPct * 100.f), 1, 100);
	const FString   Guid  = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	const FString File = FString::Printf(TEXT("ScreenShot_%s_%03d_%llu_%s_Inner%02d.png"),
	                                     *Stamp, Ms, SerialCounter, *Guid, Pct);
	return FPaths::Combine(Dir, File);
}
