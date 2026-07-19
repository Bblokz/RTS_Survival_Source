#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureFrameWriter.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSSteamCaptureSubsystem.generated.h"

class ACPPController;
class URTSSteamCaptureSettings;

/**
 * @brief PIE-only world subsystem that records the rendered player viewport while the designer plays.
 * Gameplay only toggles recording; this subsystem owns frame timing, resizing, and capture package output.
 */
UCLASS()
class RTS_SURVIVAL_API URTSSteamCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	/**
	 * @brief Starts a capture session with either the configured limit or explicit-stop lifetime.
	 * @param PlayerController Controller whose camera supplies the captured view.
	 * @param bDisableMaxDurationUntilFunctionCall True keeps recording until StopCapture is called.
	 * @param bOverrideResolution True uses the supplied resolution instead of project settings.
	 * @param ResolutionX Horizontal resolution used when overriding project settings.
	 * @param ResolutionY Vertical resolution used when overriding project settings.
	 * @return True when recording started or an existing session was already recording.
	 */
	bool StartCapture(
		ACPPController* PlayerController,
		bool bDisableMaxDurationUntilFunctionCall,
		bool bOverrideResolution,
		int32 ResolutionX,
		int32 ResolutionY);
	bool StopCapture();
	bool GetIsRecording() const { return bM_IsRecording; }

private:
	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;
	FRTSSteamCaptureFrameWriter M_FrameWriter;

	FString M_SessionDirectory;
	FString M_FramesDirectory;
	FString M_MetadataPath;
	FString M_SessionName;
	FGuid M_SessionGuid;
	FDateTime M_SessionStartDateTime;
	float M_RecordingElapsedSeconds = 0.0f;
	int32 M_CapturedFrameCount = 0;
	int32 M_DuplicatedFrameCount = 0;
	int32 M_DroppedFrameCount = 0;
	bool bM_IsRecording = false;
	bool bM_IsStopping = false;
	bool bM_DisableMaxDurationUntilFunctionCall = false;
	bool bM_OverrideResolution = false;
	FIntPoint M_OutputResolution = FIntPoint::ZeroValue;
	TArray<FColor> M_LastCapturedFramePixels;

	const URTSSteamCaptureSettings* GetCaptureSettings() const;
	bool GetCanStartCapture(ACPPController* PlayerController, const URTSSteamCaptureSettings* CaptureSettings) const;
	bool GetIsPieWorld() const;
	bool StartCapture_CreateSessionDirectory(const URTSSteamCaptureSettings& CaptureSettings);
	void AbortStartCapture();
	void ResetSessionState();

	void TickCapture(float DeltaTime, const URTSSteamCaptureSettings& CaptureSettings);
	void QueueDueFrames(const URTSSteamCaptureSettings& CaptureSettings);
	bool CaptureFramePixels(TArray<FColor>& OutFramePixels) const;

	/**
	 * @brief Reads the completed main view so capture inherits the player's exposure and temporal history.
	 * @param OutViewportPixels Display-encoded pixels from the player viewport.
	 * @param OutViewportResolution Resolution associated with OutViewportPixels.
	 * @return True when the viewport supplied a complete frame.
	 */
	bool ReadPlayerViewportPixels(TArray<FColor>& OutViewportPixels, FIntPoint& OutViewportResolution) const;

	/**
	 * @brief Preserves the viewport's final display color while adapting it to the requested recording size.
	 * @param ViewportPixels Display-encoded pixels read from the rendered player viewport.
	 * @param ViewportResolution Resolution associated with ViewportPixels.
	 * @param OutFramePixels Resized display-encoded pixels ready for PNG output.
	 * @return True when the source pixels and resolutions were valid.
	 */
	bool ResizeFrameToOutputResolution(
		const TArray<FColor>& ViewportPixels,
		const FIntPoint& ViewportResolution,
		TArray<FColor>& OutFramePixels) const;
	bool QueueFrameWrite(const URTSSteamCaptureSettings& CaptureSettings, TArray<FColor>&& FramePixels);
	FString BuildFramePath(int32 FrameNumber) const;
	FString BuildSessionName(const URTSSteamCaptureSettings& CaptureSettings) const;
	int32 GetTargetFrameCount(const URTSSteamCaptureSettings& CaptureSettings) const;

	void StopCaptureInternal(const FString& StopReason, bool bWaitForFrameWrites);
	void WaitForFrameWrites(const URTSSteamCaptureSettings& CaptureSettings) const;
	void WriteMetadata(const FString& StopReason, const FDateTime& StopTime) const;

	bool GetIsValidPlayerController() const;
};
