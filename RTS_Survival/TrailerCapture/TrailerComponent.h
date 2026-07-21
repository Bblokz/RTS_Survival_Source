#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FrameGrabber.h"
#include "HAL/PlatformProcess.h"
#include "RTSTrailerFrameWriter.h"
#include "TrailerComponent.generated.h"

class FSceneViewport;
class FJsonObject;
class URTSTrailerCaptureSettings;
class USoundSubmix;
class USoundWave;

UENUM(BlueprintType)
enum class ERTSTrailerRecordingState : uint8
{
	Idle,
	Recording,
	Finalizing,
	Encoding,
	Completed,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRTSTrailerRecordingCompleted,
	bool,
	bSuccess,
	const FString&,
	FinalMp4Path);

struct FRTSTrailerCaptureGeometry
{
	FIntPoint M_SourceViewportSize = FIntPoint::ZeroValue;
	FIntPoint M_CaptureSize = FIntPoint::ZeroValue;
	FIntPoint M_OutputSize = FIntPoint::ZeroValue;
	FIntPoint M_CropOffset = FIntPoint::ZeroValue;
};

struct FRTSTrailerSessionPaths
{
	FString M_SessionDirectory;
	FString M_FramesDirectory;
	FString M_AudioFile;
	FString M_MetadataFile;
	FString M_FinalMp4File;
};

struct FRTSTrailerFrameCounters
{
	int32 M_CapturedBackbufferCount = 0;
	int32 M_QueuedOutputFrameCount = 0;
	int32 M_DuplicatedFrameCount = 0;
	int32 M_DroppedRenderedFrameCount = 0;
	int32 M_OutstandingCaptureRequestCount = 0;
};

/**
 * @brief Records the owning local player's composed viewport and master submix for trailer creation.
 * StartRecording and StopRecording are intended to be called from development-only UI or Blueprint tools.
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTrailerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrailerComponent();

	/**
	 * @brief Starts a bounded viewport/audio session so the final file matches the requested delivery size.
	 * @param bOverrideResolution Whether to use the supplied resolution instead of project settings.
	 * @param ResolutionX Requested output width when overriding.
	 * @param ResolutionY Requested output height when overriding.
	 * @return True when capture was initialized and a first backbuffer was requested.
	 */
	UFUNCTION(BlueprintCallable, Category="Trailer Capture")
	bool StartRecording(bool bOverrideResolution, int32 ResolutionX, int32 ResolutionY);

	UFUNCTION(BlueprintCallable, Category="Trailer Capture")
	bool StopRecording();

	UFUNCTION(BlueprintPure, Category="Trailer Capture")
	ERTSTrailerRecordingState GetRecordingState() const;

	UFUNCTION(BlueprintPure, Category="Trailer Capture")
	bool GetIsRecording() const;

	UFUNCTION(BlueprintPure, Category="Trailer Capture")
	FString GetLastOutputPath() const;

	UFUNCTION(BlueprintPure, Category="Trailer Capture")
	FString GetLastError() const;

	UPROPERTY(BlueprintAssignable, Category="Trailer Capture")
	FRTSTrailerRecordingCompleted OnRecordingCompleted;

	/** @brief Advances capture and encoding without blocking gameplay or Slate rendering. */
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetCanStartRecording(const URTSTrailerCaptureSettings*& OutSettings);
	bool GetIsSupportedBuildAndWorld() const;

	/**
	 * @brief Selects and validates the delivery size before allocating GPU capture resources.
	 * @param Settings Project defaults used when bOverrideResolution is false.
	 * @param bOverrideResolution Whether the supplied dimensions take precedence.
	 * @param ResolutionX Supplied delivery width.
	 * @param ResolutionY Supplied delivery height.
	 * @return True when the selected size is compatible with yuv420p.
	 */
	bool ResolveOutputGeometry(
		const URTSTrailerCaptureSettings& Settings,
		bool bOverrideResolution,
		int32 ResolutionX,
		int32 ResolutionY);
	bool CreateSessionDirectories(const URTSTrailerCaptureSettings& Settings);
	bool InitializeViewportCapture();
	bool CalculateCaptureGeometry();
	bool GetIsViewportSizeUnchanged() const;
	bool InitializeAudioCapture(const URTSTrailerCaptureSettings& Settings);

	void TickRecording(const URTSTrailerCaptureSettings& Settings);
	void TickFinalizing(const URTSTrailerCaptureSettings& Settings);
	void TickEncoding(const URTSTrailerCaptureSettings& Settings);
	void UpdateRecordingTimeAndRequestFrame(const URTSTrailerCaptureSettings& Settings);
	void DrainCapturedFrames();
	void ProcessCapturedFrame(FCapturedFrameData&& CapturedFrame);

	/**
	 * @brief Materializes every CFR slot through a newly received frame without hiding missed slots.
	 * @param TargetFrameIndex CFR index assigned when this backbuffer was requested.
	 * @param CurrentPixels Newly captured pixels used after any required duplicates.
	 * @return True when every required PNG write fit within the configured bound.
	 */
	bool QueueFramesThroughTarget(int32 TargetFrameIndex, TArray<FColor>&& CurrentPixels);
	void QueuePngFrame(TArray<FColor>&& FramePixels);

	void BeginFinalization(const FString& StopReason);
	void RequestFrameForIndex(int32 FrameIndex);
	void StopFrameGrabberAfterFinalFrame();
	void StopAudioCapture();

	/**
	 * @brief Moves an active session into retained-intermediate cleanup while preserving its first error.
	 * @param ErrorMessage Actionable failure description exposed to Blueprint and metadata.
	 * @param StopReason Stable machine-readable failure reason.
	 */
	void BeginCaptureFailure(const FString& ErrorMessage, const FString& StopReason);
	void FinishFailedSession();
	void FinishSuccessfulSession(const URTSTrailerCaptureSettings& Settings);
	void AbortForEndPlay();

	bool StartEncoding(const URTSTrailerCaptureSettings& Settings);
	FString BuildFFmpegArguments(const URTSTrailerCaptureSettings& Settings) const;
	void AppendEncoderOutput();
	void CloseEncoderResources();
	void DeleteSuccessfulIntermediates() const;

	void ResetSessionState();
	void ReleaseAudioDelegate();
	void WriteMetadata(bool bSuccess) const;
	void AddCaptureMetadata(const TSharedRef<FJsonObject>& Metadata) const;
	void AddFrameAndAudioMetadata(const TSharedRef<FJsonObject>& Metadata) const;
	void AddPathAndEncoderMetadata(const TSharedRef<FJsonObject>& Metadata) const;
	void ReadAudioFormatFromWav();
	void SetLastError(const FString& ErrorMessage);
	bool GetIsValidMainSubmix() const;

	FString BuildFramePath(int32 OneBasedFrameNumber) const;
	int32 GetRequiredFrameCount(double ElapsedSeconds, const URTSTrailerCaptureSettings& Settings) const;
	int32 GetMaximumFrameCount(const URTSTrailerCaptureSettings& Settings) const;

	UFUNCTION()
	void OnAudioFileWritten(const USoundWave* RecordedSoundWave);

	ERTSTrailerRecordingState M_RecordingState = ERTSTrailerRecordingState::Idle;
	FRTSTrailerCaptureGeometry M_CaptureGeometry;
	FRTSTrailerSessionPaths M_SessionPaths;
	FRTSTrailerFrameCounters M_FrameCounters;
	FRTSTrailerFrameWriter M_FrameWriter;

	TUniquePtr<FFrameGrabber> M_FrameGrabber;
	TWeakPtr<FSceneViewport> M_SceneViewport;
	TArray<FColor> M_LastCapturedFramePixels;
	TWeakObjectPtr<USoundSubmix> M_MainSubmix;

	FGuid M_SessionGuid;
	FString M_SessionName;
	FString M_StopReason;
	FString M_LastOutputPath;
	FString M_LastError;
	FString M_EncoderOutput;
	FDateTime M_StartDateTime;
	FDateTime M_StopDateTime;

	double M_StartPlatformSeconds = 0.0;
	double M_FinalizationStartPlatformSeconds = 0.0;
	double M_EncodingStartPlatformSeconds = 0.0;
	double M_ActualCaptureDurationSeconds = 0.0;
	int32 M_TargetFrameCount = 0;
	int32 M_LastRequestedFrameIndex = INDEX_NONE;
	int32 M_HighestReceivedFrameIndex = INDEX_NONE;
	int32 M_AudioSampleRate = 0;
	int32 M_AudioChannelCount = 0;
	int32 M_RecordingFrameRate = 0;
	int32 M_EncoderReturnCode = INDEX_NONE;
	uint64 M_LastCaptureRequestEngineFrame = MAX_uint64;

	FProcHandle M_EncoderProcessHandle;
	void* M_EncoderReadPipe = nullptr;
	void* M_EncoderWritePipe = nullptr;

	bool bM_OverrideResolution = false;
	bool bM_AudioRecordingStarted = false;
	bool bM_AudioStopRequested = false;
	bool bM_AudioFileWritten = false;
	bool bM_FrameGrabberStopRequested = false;
	bool bM_CaptureFailed = false;
	bool bM_CompletionBroadcast = false;
	bool bM_EndPlayAborted = false;
};
