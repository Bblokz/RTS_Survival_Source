#pragma once

#include "CoreMinimal.h"
#include "FViewportScreenshotTask.generated.h"

/**
 * @brief Owns screenshot capture state + cropping and file saving.
 *        Call Request(...) and it handles the rest at end-of-frame.
 */
USTRUCT()
struct FViewportScreenshotTask
{
	GENERATED_BODY()

public:
	/**
	 * @brief Queue a full-res viewport screenshot and save a centrally-cropped copy.
	 * @param World        World used to resolve the GameViewport.
	 * @param bIncludeUI   Capture Slate UI as well.
	 * @param InnerPercent Fraction [0..1] of the viewport (centered) to keep; 1 keeps the full frame.
	 * @return true if the request was queued successfully, false on validation failure.
	 */
	bool Request(const UWorld* World, bool bIncludeUI, float InnerPercent);

	/** Unbinds the delegate if still bound (safe to call multiple times). */
	void Shutdown();

private:
	void OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);
	static void CropCentral(const TArray<FColor>& Src, int32 SrcW, int32 SrcH, float InnerPct,
	                        TArray<FColor>& Out, int32& OutW, int32& OutH);

	/** Build a unique output path in the engine’s screenshot directory. */
	static FString BuildUniqueOutputPath(float InnerPct, uint64 SerialCounter);

private:
	TWeakObjectPtr<UGameViewportClient> M_GameViewportClient;
	FDelegateHandle                      M_ScreenshotDelegateHandle;

	float  M_PendingInnerPercent = 1.f;
	bool   bM_Pending            = false;
	bool   bM_IncludeUI          = false;
	uint64 M_SerialCounter       = 0;   // bumps every shot to ensure uniqueness
};
