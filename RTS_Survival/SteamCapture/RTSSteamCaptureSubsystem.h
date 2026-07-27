#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureAudio.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureFrameWriter.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureViewport.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSSteamCaptureSubsystem.generated.h"

class ACPPController;
class FJsonObject;
class URTSSteamCaptureSettings;

enum class ERTSSteamCapturePendingStop : uint8
{
	None,
	Manual,
	MaxDuration
};

/**
 * @brief Keeps optional master-output recording state together for one Steam capture session.
 * The saved and validated flags distinguish a completed WAV from a request that only started.
 */
USTRUCT()
struct FRTSSteamCaptureAudioSessionState
{
	GENERATED_BODY()

	FAudioDeviceHandle M_AudioDevice;
	FString M_AudioFilePath;
	FString M_LastError;
	FRTSSteamCaptureAudioResult M_Result;
	FRTSSteamCaptureWaveInfo M_WaveInfo;
	bool bM_RecordAudio = false;
	bool bM_RecordingStarted = false;
	bool bM_FileSaved = false;
	bool bM_FileValidated = false;
};

/**
 * @brief PIE-only subsystem that renders the player's game view into a resolution-independent viewport.
 * It also optionally records the player's final master mix and locks WAV duration to the written frames.
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
	 * @param bRecordAudio True saves the player's final master mix as a frame-synchronized WAV.
	 * @return True when recording started or an existing session was already recording.
	 */
	bool StartCapture(
		ACPPController* PlayerController,
		bool bDisableMaxDurationUntilFunctionCall,
		bool bOverrideResolution,
		int32 ResolutionX,
		int32 ResolutionY,
		bool bRecordAudio);
	bool StopCapture();
	bool GetIsRecording() const { return bM_IsRecording; }

private:
	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;
	FRTSSteamCaptureFrameWriter M_FrameWriter;
	TUniquePtr<FRTSSteamCaptureViewport> M_CaptureViewport;
	TSharedPtr<FRTSSteamCaptureViewExtension, ESPMode::ThreadSafe> M_CaptureViewExtension;
	FDelegateHandle M_EndFrameDelegateHandle;

	UPROPERTY()
	FRTSSteamCaptureAudioSessionState M_AudioSession;

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
	int32 M_RecordingFrameRate = 0;
	bool bM_IsRecording = false;
	bool bM_IsStopping = false;
	bool bM_DisableMaxDurationUntilFunctionCall = false;
	bool bM_OverrideResolution = false;
	FIntPoint M_OutputResolution = FIntPoint::ZeroValue;
	ERTSSteamCapturePendingStop M_PendingStop = ERTSSteamCapturePendingStop::None;
	TArray<FColor> M_LastCapturedFramePixels;

	const URTSSteamCaptureSettings* GetCaptureSettings() const;
	bool GetCanStartCapture(ACPPController* PlayerController, const URTSSteamCaptureSettings* CaptureSettings) const;
	bool GetIsPieWorld() const;
	bool StartCapture_CreateSessionDirectory(const URTSSteamCaptureSettings& CaptureSettings);
	bool StartCapture_CreateViewport();
	bool StartCapture_StartAudio(const URTSSteamCaptureSettings& CaptureSettings);
	void AbortStartCapture();
	void ResetSessionState();
	void DestroyCaptureViewport();

	void TickCapture(float DeltaTime, const URTSSteamCaptureSettings& CaptureSettings);
	void OnEndFrame();
	void QueueDueFrames(const URTSSteamCaptureSettings& CaptureSettings);
	bool CaptureFramePixels(TArray<FColor>& OutFramePixels);
	bool QueueFrameWrite(const URTSSteamCaptureSettings& CaptureSettings, TArray<FColor>&& FramePixels);
	FString BuildFramePath(int32 FrameNumber) const;
	FString BuildSessionName(const URTSSteamCaptureSettings& CaptureSettings) const;
	int32 GetTargetFrameCount() const;
	FString GetPendingStopReason() const;

	void StopCaptureInternal(const FString& StopReason, bool bWaitForFrameWrites);
	bool StopCapture_SaveAudio();

	/**
	 * @brief Converts the stopped master-mix buffer into the session artifact without losing its native format.
	 * @param MasterOutputSamples Interleaved samples returned by the recorded main submix.
	 * @param ChannelCount Native device output channel count.
	 * @param SampleRate Native device output sample rate.
	 * @return True when the WAV was written and validated from disk.
	 */
	bool SaveCapturedAudio(
		TArrayView<const float> MasterOutputSamples,
		int32 ChannelCount,
		int32 SampleRate);
	bool ValidateSavedAudio();
	bool FailAudioCapture(const FString& ErrorMessage);
	void WaitForFrameWrites(const URTSSteamCaptureSettings& CaptureSettings) const;
	void WriteMetadata(const FString& StopReason, const FDateTime& StopTime) const;

	/**
	 * @brief Keeps session timing and output facts together so the WAV/image relationship stays auditable.
	 * @param MetadataJson Session metadata object receiving the capture fields.
	 * @param StopReason Stable reason recorded for the completed session.
	 * @param StopTime Local time at which recording stopped.
	 */
	void AddSessionMetadata(
		const TSharedRef<FJsonObject>& MetadataJson,
		const FString& StopReason,
		const FDateTime& StopTime) const;
	void AddAudioMetadata(const TSharedRef<FJsonObject>& MetadataJson) const;

	bool GetIsValidPlayerController() const;
};
