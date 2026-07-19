#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureFrameWriter.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSSteamCaptureSubsystem.generated.h"

class ACameraPawn;
class ACPPController;
class ARTSSteamCaptureCameraActor;
class UCameraComponent;
class URTSSteamCaptureSettings;
class UTextureRenderTarget2D;

/**
 * @brief PIE-only world subsystem that captures fixed-aspect Steam store footage while the designer plays.
 * Gameplay only toggles recording; this subsystem owns camera mirroring, frame timing, and capture package output.
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
	TObjectPtr<ARTSSteamCaptureCameraActor> M_CaptureActor;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> M_RenderTarget;

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
	bool StartCapture_CreateRenderTarget();
	bool StartCapture_SpawnCaptureActor();
	void AbortStartCapture();
	void ResetSessionState();
	void DestroyCaptureActorIfNeeded();

	void TickCapture(float DeltaTime, const URTSSteamCaptureSettings& CaptureSettings);
	void QueueDueFrames(const URTSSteamCaptureSettings& CaptureSettings);
	bool CaptureFramePixels(const URTSSteamCaptureSettings& CaptureSettings, TArray<FColor>& OutFramePixels);
	bool QueueFrameWrite(const URTSSteamCaptureSettings& CaptureSettings, TArray<FColor>&& FramePixels);
	bool ReadRenderTargetPixels(TArray<FColor>& OutPixels) const;
	FString BuildFramePath(int32 FrameNumber) const;
	FString BuildSessionName(const URTSSteamCaptureSettings& CaptureSettings) const;
	int32 GetTargetFrameCount(const URTSSteamCaptureSettings& CaptureSettings) const;

	void StopCaptureInternal(const FString& StopReason, bool bWaitForFrameWrites);
	void WaitForFrameWrites(const URTSSteamCaptureSettings& CaptureSettings) const;
	void WriteMetadata(const FString& StopReason, const FDateTime& StopTime) const;

	bool GetIsValidPlayerController() const;
	bool GetIsValidCaptureActor() const;
	bool GetIsValidRenderTarget() const;
};
