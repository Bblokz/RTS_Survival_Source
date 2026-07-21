#include "TrailerComponent.h"

#include "AudioDevice.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTSTrailerCaptureSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RHIGlobals.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundSubmix.h"
#include "Widgets/SViewport.h"

namespace RTSTrailerCaptureConstants
{
	constexpr uint32 FrameGrabberSurfaceCount = 3;
	constexpr int32 SessionGuidCharacters = 8;
	constexpr int32 MaximumEncoderOutputCharacters = 16384;
	constexpr double MinimumAudioRecordingSeconds = 0.1;
	const TCHAR* const FramesDirectoryName = TEXT("Frames");
	const TCHAR* const AudioFileBaseName = TEXT("Audio");
	const TCHAR* const AudioFileName = TEXT("Audio.wav");
	const TCHAR* const MetadataFileName = TEXT("metadata.json");
	const TCHAR* const ManualStopReason = TEXT("ManualStop");
	const TCHAR* const MaximumDurationStopReason = TEXT("MaximumDuration");
	const TCHAR* const EndPlayStopReason = TEXT("EndPlayAborted");
}

namespace
{
	struct FRTSTrailerFramePayload final : IFramePayload
	{
		explicit FRTSTrailerFramePayload(const int32 InFrameIndex)
			: M_FrameIndex(InFrameIndex)
		{
		}

		int32 M_FrameIndex = INDEX_NONE;
	};

	FString GetRecordingStateName(const ERTSTrailerRecordingState RecordingState)
	{
		switch (RecordingState)
		{
		case ERTSTrailerRecordingState::Idle: return TEXT("Idle");
		case ERTSTrailerRecordingState::Recording: return TEXT("Recording");
		case ERTSTrailerRecordingState::Finalizing: return TEXT("Finalizing");
		case ERTSTrailerRecordingState::Encoding: return TEXT("Encoding");
		case ERTSTrailerRecordingState::Completed: return TEXT("Completed");
		case ERTSTrailerRecordingState::Failed: return TEXT("Failed");
		default: return TEXT("Unknown");
		}
	}

	FString GetSafeEncoderToken(const FString& Candidate, const FString& Fallback)
	{
		if (Candidate.IsEmpty())
		{
			return Fallback;
		}

		for (const TCHAR Character : Candidate)
		{
			if (not FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				return Fallback;
			}
		}
		return Candidate;
	}

	uint16 ReadLittleEndianUInt16(const TArray<uint8>& Bytes, const int32 Offset)
	{
		return static_cast<uint16>(Bytes[Offset])
			| static_cast<uint16>(static_cast<uint16>(Bytes[Offset + 1]) << 8);
	}

	uint32 ReadLittleEndianUInt32(const TArray<uint8>& Bytes, const int32 Offset)
	{
		return static_cast<uint32>(Bytes[Offset])
			| (static_cast<uint32>(Bytes[Offset + 1]) << 8)
			| (static_cast<uint32>(Bytes[Offset + 2]) << 16)
			| (static_cast<uint32>(Bytes[Offset + 3]) << 24);
	}

	bool HasChunkId(const TArray<uint8>& Bytes, const int32 Offset, const ANSICHAR* ChunkId)
	{
		return Offset >= 0
			&& Offset + 4 <= Bytes.Num()
			&& Bytes[Offset] == static_cast<uint8>(ChunkId[0])
			&& Bytes[Offset + 1] == static_cast<uint8>(ChunkId[1])
			&& Bytes[Offset + 2] == static_cast<uint8>(ChunkId[2])
			&& Bytes[Offset + 3] == static_cast<uint8>(ChunkId[3]);
	}

	FString GetAudioChannelLayoutName(const int32 ChannelCount)
	{
		switch (ChannelCount)
		{
		case 1: return TEXT("mono");
		case 2: return TEXT("stereo");
		case 6: return TEXT("5.1");
		case 8: return TEXT("7.1");
		default: return ChannelCount > 0 ? FString::Printf(TEXT("%d channels"), ChannelCount) : TEXT("unknown");
		}
	}
}

UTrailerComponent::UTrailerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UTrailerComponent::StartRecording(
	const bool bOverrideResolution,
	const int32 ResolutionX,
	const int32 ResolutionY)
{
	if (M_RecordingState == ERTSTrailerRecordingState::Recording
		|| M_RecordingState == ERTSTrailerRecordingState::Finalizing
		|| M_RecordingState == ERTSTrailerRecordingState::Encoding)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot start trailer recording while another session is active."));
		return false;
	}
	if (M_FrameWriter.GetPendingWriteCount() > 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot start trailer recording while prior PNG writes are still pending."));
		return false;
	}

	ResetSessionState();
	const URTSTrailerCaptureSettings* Settings = nullptr;
	if (not GetCanStartRecording(Settings))
	{
		M_StopReason = TEXT("StartValidationFailed");
		FinishFailedSession();
		return false;
	}

	bM_OverrideResolution = bOverrideResolution;
	M_RecordingFrameRate = FMath::Max(1, Settings->M_FramesPerSecond);
	M_StartDateTime = FDateTime::Now();
	M_SessionGuid = FGuid::NewGuid();
	if (not ResolveOutputGeometry(*Settings, bOverrideResolution, ResolutionX, ResolutionY)
		|| not CreateSessionDirectories(*Settings)
		|| not InitializeViewportCapture())
	{
		M_StopReason = TEXT("CaptureInitializationFailed");
		FinishFailedSession();
		return false;
	}

	M_StartPlatformSeconds = FPlatformTime::Seconds();
	if (not InitializeAudioCapture(*Settings))
	{
		BeginCaptureFailure(TEXT("Failed to initialize master submix recording."), TEXT("AudioInitializationFailed"));
		FinishFailedSession();
		return false;
	}

	M_FrameGrabber->StartCapturingFrames();
	M_RecordingState = ERTSTrailerRecordingState::Recording;
	RequestFrameForIndex(0);
	return true;
}

bool UTrailerComponent::StopRecording()
{
	if (M_RecordingState == ERTSTrailerRecordingState::Recording)
	{
		BeginFinalization(RTSTrailerCaptureConstants::ManualStopReason);
		return true;
	}
	if (M_RecordingState == ERTSTrailerRecordingState::Finalizing
		|| M_RecordingState == ERTSTrailerRecordingState::Encoding)
	{
		return true;
	}
	return false;
}

ERTSTrailerRecordingState UTrailerComponent::GetRecordingState() const
{
	return M_RecordingState;
}

bool UTrailerComponent::GetIsRecording() const
{
	return M_RecordingState == ERTSTrailerRecordingState::Recording;
}

FString UTrailerComponent::GetLastOutputPath() const
{
	return M_LastOutputPath;
}

FString UTrailerComponent::GetLastError() const
{
	return M_LastError;
}

void UTrailerComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (M_RecordingState != ERTSTrailerRecordingState::Recording
		&& M_RecordingState != ERTSTrailerRecordingState::Finalizing
		&& M_RecordingState != ERTSTrailerRecordingState::Encoding)
	{
		return;
	}

	const URTSTrailerCaptureSettings* Settings = GetDefault<URTSTrailerCaptureSettings>();
	if (not IsValid(Settings))
	{
		BeginCaptureFailure(TEXT("Trailer capture settings became unavailable."), TEXT("InvalidSettings"));
		return;
	}

	switch (M_RecordingState)
	{
	case ERTSTrailerRecordingState::Recording: TickRecording(*Settings); return;
	case ERTSTrailerRecordingState::Finalizing: TickFinalizing(*Settings); return;
	case ERTSTrailerRecordingState::Encoding: TickEncoding(*Settings); return;
	default: return;
	}
}

void UTrailerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortForEndPlay();
	Super::EndPlay(EndPlayReason);
}

bool UTrailerComponent::GetCanStartRecording(const URTSTrailerCaptureSettings*& OutSettings)
{
	if (not GetIsSupportedBuildAndWorld())
	{
		SetLastError(TEXT("Trailer recording is available only in Windows PIE and Windows Development game worlds."));
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (not IsValid(PlayerController) || not PlayerController->IsLocalController())
	{
		SetLastError(TEXT("Trailer recording requires an owning local player controller."));
		return false;
	}

	OutSettings = GetDefault<URTSTrailerCaptureSettings>();
	if (not IsValid(OutSettings))
	{
		SetLastError(TEXT("Trailer capture settings are unavailable."));
		return false;
	}
	return true;
}

bool UTrailerComponent::GetIsSupportedBuildAndWorld() const
{
#if PLATFORM_WINDOWS && (WITH_EDITOR || UE_BUILD_DEVELOPMENT)
	const UWorld* World = GetWorld();
	return IsValid(World)
		&& (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
#else
	return false;
#endif
}

bool UTrailerComponent::ResolveOutputGeometry(
	const URTSTrailerCaptureSettings& Settings,
	const bool bOverrideResolution,
	const int32 ResolutionX,
	const int32 ResolutionY)
{
	const int32 OutputWidth = bOverrideResolution ? ResolutionX : Settings.M_DefaultResolutionX;
	const int32 OutputHeight = bOverrideResolution ? ResolutionY : Settings.M_DefaultResolutionY;
	if (OutputWidth <= 0 || OutputHeight <= 0 || OutputWidth % 2 != 0 || OutputHeight % 2 != 0)
	{
		SetLastError(TEXT("Trailer output dimensions must be positive even numbers for yuv420p encoding."));
		return false;
	}

	M_CaptureGeometry.M_OutputSize = FIntPoint(OutputWidth, OutputHeight);
	return true;
}

bool UTrailerComponent::CreateSessionDirectories(const URTSTrailerCaptureSettings& Settings)
{
	const FString TimeStamp = M_StartDateTime.ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString GuidText = M_SessionGuid.ToString(EGuidFormats::Digits).Left(
		RTSTrailerCaptureConstants::SessionGuidCharacters);
	M_SessionName = FString::Printf(TEXT("Trailer_%s_%s"), *TimeStamp, *GuidText);

	FString OutputRoot = Settings.M_OutputRoot.IsEmpty() ? TEXT("TrailerCaptures") : Settings.M_OutputRoot;
	if (FPaths::IsRelative(OutputRoot))
	{
		OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), OutputRoot);
	}

	M_SessionPaths.M_SessionDirectory = FPaths::Combine(OutputRoot, M_SessionName);
	M_SessionPaths.M_FramesDirectory = FPaths::Combine(
		M_SessionPaths.M_SessionDirectory,
		RTSTrailerCaptureConstants::FramesDirectoryName);
	M_SessionPaths.M_AudioFile = FPaths::Combine(
		M_SessionPaths.M_SessionDirectory,
		RTSTrailerCaptureConstants::AudioFileName);
	M_SessionPaths.M_MetadataFile = FPaths::Combine(
		M_SessionPaths.M_SessionDirectory,
		RTSTrailerCaptureConstants::MetadataFileName);
	M_SessionPaths.M_FinalMp4File = FPaths::Combine(
		M_SessionPaths.M_SessionDirectory,
		M_SessionName + TEXT(".mp4"));

	if (IFileManager::Get().MakeDirectory(*M_SessionPaths.M_FramesDirectory, true))
	{
		return true;
	}
	SetLastError(TEXT("Failed to create trailer capture directory: ") + M_SessionPaths.M_FramesDirectory);
	return false;
}

bool UTrailerComponent::InitializeViewportCapture()
{
	UWorld* World = GetWorld();
	UGameViewportClient* GameViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient))
	{
		SetLastError(TEXT("The game viewport client is unavailable."));
		return false;
	}

	FSceneViewport* RawSceneViewport = GameViewportClient->GetGameViewport();
	TSharedPtr<SViewport> ViewportWidget = GameViewportClient->GetGameViewportWidget();
	TSharedPtr<ISlateViewport> SlateViewport = ViewportWidget.IsValid()
		? ViewportWidget->GetViewportInterface().Pin()
		: nullptr;
	if (RawSceneViewport == nullptr || not SlateViewport.IsValid()
		|| SlateViewport.Get() != static_cast<ISlateViewport*>(RawSceneViewport))
	{
		SetLastError(TEXT("The local Slate scene viewport is unavailable."));
		return false;
	}
	if (not FSlateApplication::IsInitialized()
		|| not FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef()).IsValid())
	{
		SetLastError(TEXT("The game viewport is not attached to a Slate window."));
		return false;
	}

	TSharedPtr<FSceneViewport> SceneViewport = StaticCastSharedPtr<FSceneViewport>(SlateViewport);
	M_CaptureGeometry.M_SourceViewportSize = SceneViewport->GetSizeXY();
	if (M_CaptureGeometry.M_SourceViewportSize.X <= 0 || M_CaptureGeometry.M_SourceViewportSize.Y <= 0)
	{
		SetLastError(TEXT("The game viewport has an invalid size."));
		return false;
	}
	if (not CalculateCaptureGeometry())
	{
		return false;
	}

	M_SceneViewport = SceneViewport;
	M_FrameGrabber = MakeUnique<FFrameGrabber>(
		SceneViewport.ToSharedRef(),
		M_CaptureGeometry.M_CaptureSize,
		PF_B8G8R8A8,
		RTSTrailerCaptureConstants::FrameGrabberSurfaceCount);
	return true;
}

bool UTrailerComponent::CalculateCaptureGeometry()
{
	const double SourceAspect = static_cast<double>(M_CaptureGeometry.M_SourceViewportSize.X)
		/ static_cast<double>(M_CaptureGeometry.M_SourceViewportSize.Y);
	const double OutputAspect = static_cast<double>(M_CaptureGeometry.M_OutputSize.X)
		/ static_cast<double>(M_CaptureGeometry.M_OutputSize.Y);
	if (SourceAspect > OutputAspect)
	{
		M_CaptureGeometry.M_CaptureSize.Y = M_CaptureGeometry.M_OutputSize.Y;
		M_CaptureGeometry.M_CaptureSize.X = FMath::CeilToInt(M_CaptureGeometry.M_OutputSize.Y * SourceAspect);
	}
	else
	{
		M_CaptureGeometry.M_CaptureSize.X = M_CaptureGeometry.M_OutputSize.X;
		M_CaptureGeometry.M_CaptureSize.Y = FMath::CeilToInt(M_CaptureGeometry.M_OutputSize.X / SourceAspect);
	}

	if ((M_CaptureGeometry.M_CaptureSize.X - M_CaptureGeometry.M_OutputSize.X) % 2 != 0)
	{
		++M_CaptureGeometry.M_CaptureSize.X;
	}
	if ((M_CaptureGeometry.M_CaptureSize.Y - M_CaptureGeometry.M_OutputSize.Y) % 2 != 0)
	{
		++M_CaptureGeometry.M_CaptureSize.Y;
	}
	M_CaptureGeometry.M_CropOffset = (M_CaptureGeometry.M_CaptureSize - M_CaptureGeometry.M_OutputSize) / 2;

	const int32 MaximumTextureDimension = static_cast<int32>(GetMax2DTextureDimension());
	if (M_CaptureGeometry.M_CaptureSize.X > MaximumTextureDimension
		|| M_CaptureGeometry.M_CaptureSize.Y > MaximumTextureDimension)
	{
		SetLastError(FString::Printf(
			TEXT("The aspect-preserving trailer capture size %dx%d exceeds the RHI limit of %d."),
			M_CaptureGeometry.M_CaptureSize.X,
			M_CaptureGeometry.M_CaptureSize.Y,
			MaximumTextureDimension));
		return false;
	}
	return true;
}

bool UTrailerComponent::InitializeAudioCapture(const URTSTrailerCaptureSettings& Settings)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
	if (not AudioDevice)
	{
		return false;
	}

	USoundSubmix& MainSubmix = AudioDevice->GetMainSubmixObject();
	M_MainSubmix = &MainSubmix;
	MainSubmix.OnSubmixRecordedFileDone.AddUniqueDynamic(this, &UTrailerComponent::OnAudioFileWritten);
	const float ExpectedDuration = FMath::Max(0.1f, Settings.M_MaximumDurationSeconds);
	MainSubmix.StartRecordingOutput(World, ExpectedDuration);
	M_AudioSampleRate = FMath::RoundToInt(AudioDevice->GetSampleRate());
	bM_AudioRecordingStarted = true;
	return true;
}

bool UTrailerComponent::GetIsViewportSizeUnchanged() const
{
	if (M_FrameGrabber == nullptr)
	{
		return true;
	}

	const TSharedPtr<FSceneViewport> SceneViewport = M_SceneViewport.Pin();
	return SceneViewport.IsValid()
		&& SceneViewport->GetSizeXY() == M_CaptureGeometry.M_SourceViewportSize;
}

void UTrailerComponent::TickRecording(const URTSTrailerCaptureSettings& Settings)
{
	if (M_FrameWriter.GetFailedWriteCount() > 0)
	{
		BeginCaptureFailure(TEXT("A trailer PNG frame failed to write."), TEXT("FrameWriteFailed"));
		return;
	}

	if (not GetIsViewportSizeUnchanged())
	{
		BeginCaptureFailure(TEXT("The player viewport was resized during trailer recording."), TEXT("ViewportResized"));
		return;
	}

	DrainCapturedFrames();
	if (M_RecordingState != ERTSTrailerRecordingState::Recording)
	{
		return;
	}
	UpdateRecordingTimeAndRequestFrame(Settings);
}

void UTrailerComponent::UpdateRecordingTimeAndRequestFrame(const URTSTrailerCaptureSettings& Settings)
{
	const double MaximumDuration = FMath::Max(0.1f, Settings.M_MaximumDurationSeconds);
	M_ActualCaptureDurationSeconds = FMath::Min(
		FPlatformTime::Seconds() - M_StartPlatformSeconds,
		MaximumDuration);
	const int32 MaximumFrameIndex = GetMaximumFrameCount(Settings) - 1;
	const int32 DueFrameIndex = FMath::Clamp(
		FMath::FloorToInt(M_ActualCaptureDurationSeconds * FMath::Max(1, Settings.M_FramesPerSecond)),
		0,
		MaximumFrameIndex);

	if (DueFrameIndex > M_LastRequestedFrameIndex
		&& M_FrameCounters.M_OutstandingCaptureRequestCount < static_cast<int32>(RTSTrailerCaptureConstants::FrameGrabberSurfaceCount))
	{
		RequestFrameForIndex(DueFrameIndex);
	}
	else
	{
		++M_FrameCounters.M_DroppedRenderedFrameCount;
	}

	if (M_ActualCaptureDurationSeconds >= MaximumDuration)
	{
		BeginFinalization(RTSTrailerCaptureConstants::MaximumDurationStopReason);
	}
}

void UTrailerComponent::DrainCapturedFrames()
{
	if (M_FrameGrabber == nullptr)
	{
		return;
	}

	TArray<FCapturedFrameData> CapturedFrames = M_FrameGrabber->GetCapturedFrames();
	CapturedFrames.Sort([](const FCapturedFrameData& Left, const FCapturedFrameData& Right)
	{
		const FRTSTrailerFramePayload* LeftPayload = static_cast<const FRTSTrailerFramePayload*>(Left.Payload.Get());
		const FRTSTrailerFramePayload* RightPayload = static_cast<const FRTSTrailerFramePayload*>(Right.Payload.Get());
		return LeftPayload != nullptr && RightPayload != nullptr
			&& LeftPayload->M_FrameIndex < RightPayload->M_FrameIndex;
	});

	M_FrameCounters.M_OutstandingCaptureRequestCount = FMath::Max(
		0,
		M_FrameCounters.M_OutstandingCaptureRequestCount - CapturedFrames.Num());
	for (FCapturedFrameData& CapturedFrame : CapturedFrames)
	{
		ProcessCapturedFrame(MoveTemp(CapturedFrame));
		if (bM_CaptureFailed)
		{
			return;
		}
	}
}

void UTrailerComponent::ProcessCapturedFrame(FCapturedFrameData&& CapturedFrame)
{
	const FRTSTrailerFramePayload* Payload = static_cast<const FRTSTrailerFramePayload*>(CapturedFrame.Payload.Get());
	if (Payload == nullptr
		|| CapturedFrame.BufferSize != M_CaptureGeometry.M_CaptureSize
		|| CapturedFrame.ColorBuffer.Num() != M_CaptureGeometry.M_CaptureSize.X * M_CaptureGeometry.M_CaptureSize.Y)
	{
		BeginCaptureFailure(TEXT("A captured trailer backbuffer had invalid frame data."), TEXT("InvalidFrameData"));
		return;
	}

	++M_FrameCounters.M_CapturedBackbufferCount;
	M_HighestReceivedFrameIndex = FMath::Max(M_HighestReceivedFrameIndex, Payload->M_FrameIndex);
	if (bM_CaptureFailed || Payload->M_FrameIndex < M_FrameCounters.M_QueuedOutputFrameCount)
	{
		return;
	}
	QueueFramesThroughTarget(Payload->M_FrameIndex, MoveTemp(CapturedFrame.ColorBuffer));
}

bool UTrailerComponent::QueueFramesThroughTarget(
	const int32 TargetFrameIndex,
	TArray<FColor>&& CurrentPixels)
{
	const URTSTrailerCaptureSettings* Settings = GetDefault<URTSTrailerCaptureSettings>();
	if (not IsValid(Settings))
	{
		BeginCaptureFailure(TEXT("Trailer capture settings are unavailable."), TEXT("InvalidSettings"));
		return false;
	}

	const int32 MissingFrameCount = TargetFrameIndex - M_FrameCounters.M_QueuedOutputFrameCount;
	const int32 RequiredWriteCount = MissingFrameCount + 1;
	const int32 MaximumPendingWrites = FMath::Max(1, Settings->M_MaximumPendingPngWrites);
	if (M_FrameWriter.GetPendingWriteCount() + RequiredWriteCount > MaximumPendingWrites)
	{
		BeginCaptureFailure(TEXT("Trailer PNG write capacity was exhausted."), TEXT("FrameWriteCapacityExhausted"));
		return false;
	}

	for (int32 MissingFrameIndex = 0; MissingFrameIndex < MissingFrameCount; ++MissingFrameIndex)
	{
		TArray<FColor> DuplicatedPixels = M_LastCapturedFramePixels.IsEmpty()
			? CurrentPixels
			: M_LastCapturedFramePixels;
		QueuePngFrame(MoveTemp(DuplicatedPixels));
		++M_FrameCounters.M_DuplicatedFrameCount;
	}

	TArray<FColor> PixelsForWriter = CurrentPixels;
	QueuePngFrame(MoveTemp(PixelsForWriter));
	M_LastCapturedFramePixels = MoveTemp(CurrentPixels);
	return true;
}

void UTrailerComponent::QueuePngFrame(TArray<FColor>&& FramePixels)
{
	++M_FrameCounters.M_QueuedOutputFrameCount;
	M_FrameWriter.WritePngFrameAsync(
		BuildFramePath(M_FrameCounters.M_QueuedOutputFrameCount),
		M_CaptureGeometry.M_CaptureSize.X,
		M_CaptureGeometry.M_CaptureSize.Y,
		MoveTemp(FramePixels));
}

void UTrailerComponent::BeginFinalization(const FString& StopReason)
{
	if (M_RecordingState != ERTSTrailerRecordingState::Recording)
	{
		return;
	}

	const URTSTrailerCaptureSettings* Settings = GetDefault<URTSTrailerCaptureSettings>();
	if (not IsValid(Settings))
	{
		BeginCaptureFailure(TEXT("Trailer capture settings are unavailable."), TEXT("InvalidSettings"));
		return;
	}

	M_ActualCaptureDurationSeconds = FMath::Min(
		FPlatformTime::Seconds() - M_StartPlatformSeconds,
		static_cast<double>(FMath::Max(0.1f, Settings->M_MaximumDurationSeconds)));
	M_TargetFrameCount = GetRequiredFrameCount(M_ActualCaptureDurationSeconds, *Settings);
	M_StopReason = StopReason;
	M_StopDateTime = FDateTime::Now();
	M_FinalizationStartPlatformSeconds = FPlatformTime::Seconds();
	M_RecordingState = ERTSTrailerRecordingState::Finalizing;
}

void UTrailerComponent::RequestFrameForIndex(const int32 FrameIndex)
{
	if (M_FrameGrabber == nullptr || not M_FrameGrabber->IsCapturingFrames())
	{
		return;
	}
	if (M_LastCaptureRequestEngineFrame == GFrameCounter)
	{
		return;
	}

	M_FrameGrabber->CaptureThisFrame(MakeShared<FRTSTrailerFramePayload, ESPMode::ThreadSafe>(FrameIndex));
	M_LastRequestedFrameIndex = FrameIndex;
	M_LastCaptureRequestEngineFrame = GFrameCounter;
	++M_FrameCounters.M_OutstandingCaptureRequestCount;
}

void UTrailerComponent::TickFinalizing(const URTSTrailerCaptureSettings& Settings)
{
	if (not bM_CaptureFailed && not GetIsViewportSizeUnchanged())
	{
		BeginCaptureFailure(TEXT("The player viewport was resized during trailer finalization."), TEXT("ViewportResized"));
	}
	DrainCapturedFrames();
	if (M_FrameWriter.GetFailedWriteCount() > 0 && not bM_CaptureFailed)
	{
		BeginCaptureFailure(TEXT("A trailer PNG frame failed to write."), TEXT("FrameWriteFailed"));
	}

	if (not bM_CaptureFailed && M_LastRequestedFrameIndex < M_TargetFrameCount - 1
		&& M_FrameCounters.M_OutstandingCaptureRequestCount < static_cast<int32>(RTSTrailerCaptureConstants::FrameGrabberSurfaceCount))
	{
		RequestFrameForIndex(M_TargetFrameCount - 1);
	}
	StopFrameGrabberAfterFinalFrame();
	const double RecordingSeconds = FPlatformTime::Seconds() - M_StartPlatformSeconds;
	if (not bM_CaptureFailed
		&& M_HighestReceivedFrameIndex >= M_TargetFrameCount - 1
		&& RecordingSeconds >= RTSTrailerCaptureConstants::MinimumAudioRecordingSeconds)
	{
		StopAudioCapture();
	}

	const double FinalizationSeconds = FPlatformTime::Seconds() - M_FinalizationStartPlatformSeconds;
	if (FinalizationSeconds >= FMath::Max(1.0f, Settings.M_FinalizationTimeoutSeconds))
	{
		BeginCaptureFailure(TEXT("Trailer finalization timed out."), TEXT("FinalizationTimeout"));
		if (M_FrameGrabber != nullptr)
		{
			M_FrameGrabber->Shutdown();
			M_FrameGrabber.Reset();
		}
		FinishFailedSession();
		return;
	}

	const bool bFramesFinished = M_FrameGrabber == nullptr && M_FrameWriter.GetPendingWriteCount() == 0;
	if (not bFramesFinished)
	{
		return;
	}
	if (bM_CaptureFailed || M_FrameWriter.GetFailedWriteCount() > 0)
	{
		FinishFailedSession();
		return;
	}
	if (not bM_AudioFileWritten)
	{
		return;
	}

	ReadAudioFormatFromWav();
	if (not IFileManager::Get().FileExists(*M_SessionPaths.M_AudioFile))
	{
		SetLastError(TEXT("The master submix WAV file was not created."));
		FinishFailedSession();
		return;
	}
	if (not StartEncoding(Settings))
	{
		FinishFailedSession();
	}
}

void UTrailerComponent::StopFrameGrabberAfterFinalFrame()
{
	if (M_FrameGrabber == nullptr)
	{
		return;
	}

	const bool bReceivedFinalFrame = M_HighestReceivedFrameIndex >= M_TargetFrameCount - 1;
	if ((bReceivedFinalFrame || bM_CaptureFailed) && not bM_FrameGrabberStopRequested
		&& M_FrameGrabber->IsCapturingFrames())
	{
		M_FrameGrabber->StopCapturingFrames();
		bM_FrameGrabberStopRequested = true;
	}

	if (bM_FrameGrabberStopRequested && not M_FrameGrabber->HasOutstandingFrames())
	{
		M_FrameGrabber->GetCapturedFrames();
		M_FrameGrabber.Reset();
		M_SceneViewport.Reset();
	}
}

void UTrailerComponent::StopAudioCapture()
{
	if (not bM_AudioRecordingStarted)
	{
		bM_AudioFileWritten = true;
		return;
	}
	if (bM_AudioStopRequested || not GetIsValidMainSubmix())
	{
		if (not bM_AudioStopRequested)
		{
			bM_AudioFileWritten = true;
		}
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		if (not bM_CaptureFailed)
		{
			SetLastError(TEXT("The world ended before audio recording could stop."));
			M_StopReason = TEXT("AudioWorldInvalid");
		}
		bM_CaptureFailed = true;
		bM_AudioFileWritten = true;
		return;
	}

	M_MainSubmix.Get()->StopRecordingOutput(
		World,
		EAudioRecordingExportType::WavFile,
		RTSTrailerCaptureConstants::AudioFileBaseName,
		M_SessionPaths.M_SessionDirectory);
	bM_AudioStopRequested = true;
}

void UTrailerComponent::BeginCaptureFailure(
	const FString& ErrorMessage,
	const FString& StopReason)
{
	if (not bM_CaptureFailed)
	{
		SetLastError(ErrorMessage);
		M_StopReason = StopReason;
	}
	bM_CaptureFailed = true;
	M_TargetFrameCount = M_FrameCounters.M_QueuedOutputFrameCount;
	M_StopDateTime = FDateTime::Now();

	if (M_RecordingState != ERTSTrailerRecordingState::Finalizing)
	{
		M_RecordingState = ERTSTrailerRecordingState::Finalizing;
		M_FinalizationStartPlatformSeconds = FPlatformTime::Seconds();
	}
	StopAudioCapture();
	if (M_FrameGrabber != nullptr && not M_FrameGrabber->IsCapturingFrames())
	{
		M_FrameGrabber->Shutdown();
		M_FrameGrabber.Reset();
	}
}

bool UTrailerComponent::StartEncoding(const URTSTrailerCaptureSettings& Settings)
{
	const FString ConfiguredExecutable = Settings.M_FFmpegExecutable.FilePath;
	const FString EncoderExecutable = ConfiguredExecutable.IsEmpty()
		? TEXT("ffmpeg.exe")
		: FPaths::ConvertRelativePathToFull(ConfiguredExecutable);
	if (not ConfiguredExecutable.IsEmpty() && not IFileManager::Get().FileExists(*EncoderExecutable))
	{
		SetLastError(TEXT("Configured FFmpeg executable does not exist: ") + EncoderExecutable);
		return false;
	}
	if (not FPlatformProcess::CreatePipe(M_EncoderReadPipe, M_EncoderWritePipe))
	{
		SetLastError(TEXT("Failed to create the FFmpeg output pipe."));
		return false;
	}

	const FString Arguments = BuildFFmpegArguments(Settings);
	M_EncoderProcessHandle = FPlatformProcess::CreateProc(
		*EncoderExecutable,
		*Arguments,
		false,
		true,
		true,
		nullptr,
		0,
		*M_SessionPaths.M_SessionDirectory,
		M_EncoderWritePipe);
	if (not M_EncoderProcessHandle.IsValid())
	{
		CloseEncoderResources();
		SetLastError(TEXT("Failed to launch FFmpeg. Configure its path or add ffmpeg.exe to PATH."));
		return false;
	}

	M_EncodingStartPlatformSeconds = FPlatformTime::Seconds();
	M_RecordingState = ERTSTrailerRecordingState::Encoding;
	return true;
}

FString UTrailerComponent::BuildFFmpegArguments(const URTSTrailerCaptureSettings& Settings) const
{
	const int32 FramesPerSecond = FMath::Max(1, Settings.M_FramesPerSecond);
	const double ExactDuration = static_cast<double>(M_FrameCounters.M_QueuedOutputFrameCount) / FramesPerSecond;
	const FString VideoCodec = GetSafeEncoderToken(Settings.M_H264Codec, TEXT("libx264"));
	const FString VideoPreset = GetSafeEncoderToken(Settings.M_H264Preset, TEXT("slow"));
	const FString FramePattern = FPaths::Combine(M_SessionPaths.M_FramesDirectory, TEXT("Frame_%06d.png"));
	const FString VideoFilter = FString::Printf(
		TEXT("crop=%d:%d:%d:%d,format=yuv420p"),
		M_CaptureGeometry.M_OutputSize.X,
		M_CaptureGeometry.M_OutputSize.Y,
		M_CaptureGeometry.M_CropOffset.X,
		M_CaptureGeometry.M_CropOffset.Y);

	return FString::Printf(
		TEXT("-hide_banner -y -loglevel error -framerate %d -start_number 1 -i \"%s\" -i \"%s\" ")
		TEXT("-map 0:v:0 -map 1:a:0 -frames:v %d -vf \"%s\" -af apad -t %.6f -r %d ")
		TEXT("-c:v %s -preset %s -crf %d -pix_fmt yuv420p -c:a aac -b:a %dk ")
		TEXT("-color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv ")
		TEXT("-movflags +faststart \"%s\""),
		FramesPerSecond,
		*FramePattern,
		*M_SessionPaths.M_AudioFile,
		M_FrameCounters.M_QueuedOutputFrameCount,
		*VideoFilter,
		ExactDuration,
		FramesPerSecond,
		*VideoCodec,
		*VideoPreset,
		FMath::Clamp(Settings.M_VideoConstantRateFactor, 0, 51),
		FMath::Max(32, Settings.M_AudioBitrateKbps),
		*M_SessionPaths.M_FinalMp4File);
}

void UTrailerComponent::TickEncoding(const URTSTrailerCaptureSettings& Settings)
{
	AppendEncoderOutput();
	const double EncodingSeconds = FPlatformTime::Seconds() - M_EncodingStartPlatformSeconds;
	if (EncodingSeconds >= FMath::Max(1.0f, Settings.M_EncodingTimeoutSeconds))
	{
		FPlatformProcess::TerminateProc(M_EncoderProcessHandle, true);
		FPlatformProcess::WaitForProc(M_EncoderProcessHandle);
		SetLastError(TEXT("FFmpeg encoding timed out."));
		M_EncoderReturnCode = INDEX_NONE;
		CloseEncoderResources();
		FinishFailedSession();
		return;
	}
	if (FPlatformProcess::IsProcRunning(M_EncoderProcessHandle))
	{
		return;
	}

	AppendEncoderOutput();
	int32 ReturnCode = INDEX_NONE;
	const bool bGotReturnCode = FPlatformProcess::GetProcReturnCode(M_EncoderProcessHandle, &ReturnCode);
	M_EncoderReturnCode = ReturnCode;
	CloseEncoderResources();
	if (bGotReturnCode && ReturnCode == 0
		&& IFileManager::Get().FileExists(*M_SessionPaths.M_FinalMp4File))
	{
		FinishSuccessfulSession(Settings);
		return;
	}

	SetLastError(FString::Printf(
		TEXT("FFmpeg failed with return code %d. %s"),
		ReturnCode,
		*M_EncoderOutput));
	FinishFailedSession();
}

void UTrailerComponent::AppendEncoderOutput()
{
	if (M_EncoderReadPipe == nullptr)
	{
		return;
	}

	M_EncoderOutput += FPlatformProcess::ReadPipe(M_EncoderReadPipe);
	if (M_EncoderOutput.Len() > RTSTrailerCaptureConstants::MaximumEncoderOutputCharacters)
	{
		M_EncoderOutput.RightChopInline(
			M_EncoderOutput.Len() - RTSTrailerCaptureConstants::MaximumEncoderOutputCharacters);
	}
}

void UTrailerComponent::CloseEncoderResources()
{
	if (M_EncoderProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(M_EncoderProcessHandle);
		M_EncoderProcessHandle.Reset();
	}
	if (M_EncoderReadPipe != nullptr || M_EncoderWritePipe != nullptr)
	{
		FPlatformProcess::ClosePipe(M_EncoderReadPipe, M_EncoderWritePipe);
		M_EncoderReadPipe = nullptr;
		M_EncoderWritePipe = nullptr;
	}
}

void UTrailerComponent::FinishSuccessfulSession(const URTSTrailerCaptureSettings& Settings)
{
	M_RecordingState = ERTSTrailerRecordingState::Completed;
	M_LastOutputPath = M_SessionPaths.M_FinalMp4File;
	M_LastCapturedFramePixels.Empty();
	ReleaseAudioDelegate();
	if (Settings.bM_DeleteIntermediatesAfterSuccessfulEncoding)
	{
		DeleteSuccessfulIntermediates();
	}
	WriteMetadata(true);

	if (not bM_CompletionBroadcast)
	{
		bM_CompletionBroadcast = true;
		OnRecordingCompleted.Broadcast(true, M_LastOutputPath);
	}
}

void UTrailerComponent::FinishFailedSession()
{
	if (M_FrameGrabber != nullptr)
	{
		M_FrameGrabber->Shutdown();
		M_FrameGrabber.Reset();
		M_SceneViewport.Reset();
	}
	ReleaseAudioDelegate();
	CloseEncoderResources();
	M_LastCapturedFramePixels.Empty();
	M_RecordingState = ERTSTrailerRecordingState::Failed;
	if (M_StopDateTime == FDateTime())
	{
		M_StopDateTime = FDateTime::Now();
	}
	WriteMetadata(false);

	if (not bM_CompletionBroadcast)
	{
		bM_CompletionBroadcast = true;
		const FString NoOutputPath;
		OnRecordingCompleted.Broadcast(false, NoOutputPath);
	}
}

void UTrailerComponent::DeleteSuccessfulIntermediates() const
{
	IFileManager::Get().Delete(*M_SessionPaths.M_AudioFile, false, true, true);
	IFileManager::Get().DeleteDirectory(*M_SessionPaths.M_FramesDirectory, false, true);
}

void UTrailerComponent::AbortForEndPlay()
{
	if (M_RecordingState != ERTSTrailerRecordingState::Recording
		&& M_RecordingState != ERTSTrailerRecordingState::Finalizing
		&& M_RecordingState != ERTSTrailerRecordingState::Encoding)
	{
		return;
	}

	bM_EndPlayAborted = true;
	bM_CaptureFailed = true;
	M_StopReason = RTSTrailerCaptureConstants::EndPlayStopReason;
	M_StopDateTime = FDateTime::Now();
	SetLastError(TEXT("Trailer recording was aborted because play ended."));
	StopAudioCapture();
	if (M_EncoderProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(M_EncoderProcessHandle, true);
		FPlatformProcess::WaitForProc(M_EncoderProcessHandle);
	}
	FinishFailedSession();
}

void UTrailerComponent::ResetSessionState()
{
	ReleaseAudioDelegate();
	CloseEncoderResources();
	M_FrameGrabber.Reset();
	M_SceneViewport.Reset();
	M_LastCapturedFramePixels.Empty();
	M_MainSubmix.Reset();
	M_CaptureGeometry = FRTSTrailerCaptureGeometry();
	M_SessionPaths = FRTSTrailerSessionPaths();
	M_FrameCounters = FRTSTrailerFrameCounters();
	M_FrameWriter.Reset();
	M_SessionGuid.Invalidate();
	M_SessionName.Reset();
	M_StopReason.Reset();
	M_LastOutputPath.Reset();
	M_LastError.Reset();
	M_EncoderOutput.Reset();
	M_StartDateTime = FDateTime();
	M_StopDateTime = FDateTime();
	M_StartPlatformSeconds = 0.0;
	M_FinalizationStartPlatformSeconds = 0.0;
	M_EncodingStartPlatformSeconds = 0.0;
	M_ActualCaptureDurationSeconds = 0.0;
	M_TargetFrameCount = 0;
	M_LastRequestedFrameIndex = INDEX_NONE;
	M_HighestReceivedFrameIndex = INDEX_NONE;
	M_AudioSampleRate = 0;
	M_AudioChannelCount = 0;
	M_RecordingFrameRate = 0;
	M_EncoderReturnCode = INDEX_NONE;
	M_LastCaptureRequestEngineFrame = MAX_uint64;
	bM_OverrideResolution = false;
	bM_AudioRecordingStarted = false;
	bM_AudioStopRequested = false;
	bM_AudioFileWritten = false;
	bM_FrameGrabberStopRequested = false;
	bM_CaptureFailed = false;
	bM_CompletionBroadcast = false;
	bM_EndPlayAborted = false;
	M_RecordingState = ERTSTrailerRecordingState::Idle;
}

void UTrailerComponent::ReleaseAudioDelegate()
{
	if (not bM_AudioRecordingStarted)
	{
		return;
	}
	if (GetIsValidMainSubmix())
	{
		M_MainSubmix.Get()->OnSubmixRecordedFileDone.RemoveDynamic(this, &UTrailerComponent::OnAudioFileWritten);
	}
	M_MainSubmix.Reset();
	bM_AudioRecordingStarted = false;
}

void UTrailerComponent::WriteMetadata(const bool bSuccess) const
{
	if (M_SessionPaths.M_MetadataFile.IsEmpty())
	{
		return;
	}

	TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
	Metadata->SetStringField(TEXT("sessionName"), M_SessionName);
	Metadata->SetStringField(TEXT("sessionGuid"), M_SessionGuid.ToString(EGuidFormats::Digits));
	Metadata->SetStringField(TEXT("state"), GetRecordingStateName(M_RecordingState));
	Metadata->SetBoolField(TEXT("success"), bSuccess);
	Metadata->SetStringField(TEXT("error"), M_LastError);
	Metadata->SetStringField(TEXT("stopReason"), M_StopReason);
	Metadata->SetStringField(TEXT("startTimeLocal"), M_StartDateTime.ToIso8601());
	Metadata->SetStringField(TEXT("stopTimeLocal"), M_StopDateTime.ToIso8601());
	Metadata->SetStringField(TEXT("captureSource"), TEXT("FinalSlatePlayerViewport"));
	Metadata->SetStringField(TEXT("pixelEncoding"), TEXT("DisplayEncodedBGRA8"));
	Metadata->SetStringField(TEXT("colorStandard"), TEXT("SDR Rec.709"));
	AddCaptureMetadata(Metadata);
	AddFrameAndAudioMetadata(Metadata);
	AddPathAndEncoderMetadata(Metadata);
	if (const UWorld* World = GetWorld())
	{
		Metadata->SetStringField(TEXT("mapName"), World->GetMapName());
	}

	FString SerializedMetadata;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedMetadata);
	if (not FJsonSerializer::Serialize(Metadata, Writer)
		|| not FFileHelper::SaveStringToFile(SerializedMetadata, *M_SessionPaths.M_MetadataFile))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to write trailer capture metadata: ") + M_SessionPaths.M_MetadataFile);
	}
}

void UTrailerComponent::AddCaptureMetadata(const TSharedRef<FJsonObject>& Metadata) const
{
	Metadata->SetNumberField(TEXT("sourceResolutionX"), M_CaptureGeometry.M_SourceViewportSize.X);
	Metadata->SetNumberField(TEXT("sourceResolutionY"), M_CaptureGeometry.M_SourceViewportSize.Y);
	Metadata->SetNumberField(TEXT("captureResolutionX"), M_CaptureGeometry.M_CaptureSize.X);
	Metadata->SetNumberField(TEXT("captureResolutionY"), M_CaptureGeometry.M_CaptureSize.Y);
	Metadata->SetNumberField(TEXT("outputResolutionX"), M_CaptureGeometry.M_OutputSize.X);
	Metadata->SetNumberField(TEXT("outputResolutionY"), M_CaptureGeometry.M_OutputSize.Y);
	Metadata->SetNumberField(TEXT("cropX"), M_CaptureGeometry.M_CropOffset.X);
	Metadata->SetNumberField(TEXT("cropY"), M_CaptureGeometry.M_CropOffset.Y);
	Metadata->SetBoolField(TEXT("overrodeResolution"), bM_OverrideResolution);
	Metadata->SetNumberField(TEXT("frameRate"), M_RecordingFrameRate);
	Metadata->SetNumberField(TEXT("actualCaptureSeconds"), M_ActualCaptureDurationSeconds);
	Metadata->SetNumberField(TEXT("encodedDurationSeconds"), M_FrameCounters.M_QueuedOutputFrameCount > 0
		? static_cast<double>(M_FrameCounters.M_QueuedOutputFrameCount) / FMath::Max(1, M_RecordingFrameRate)
		: 0.0);
}

void UTrailerComponent::AddFrameAndAudioMetadata(const TSharedRef<FJsonObject>& Metadata) const
{
	Metadata->SetNumberField(TEXT("capturedBackbufferCount"), M_FrameCounters.M_CapturedBackbufferCount);
	Metadata->SetNumberField(TEXT("outputFrameCount"), M_FrameCounters.M_QueuedOutputFrameCount);
	Metadata->SetNumberField(TEXT("duplicatedFrameCount"), M_FrameCounters.M_DuplicatedFrameCount);
	Metadata->SetNumberField(TEXT("droppedRenderedFrameCount"), M_FrameCounters.M_DroppedRenderedFrameCount);
	Metadata->SetNumberField(TEXT("failedPngWriteCount"), M_FrameWriter.GetFailedWriteCount());
	Metadata->SetNumberField(TEXT("pendingPngWriteCount"), M_FrameWriter.GetPendingWriteCount());
	Metadata->SetNumberField(TEXT("audioSampleRate"), M_AudioSampleRate);
	Metadata->SetNumberField(TEXT("audioChannelCount"), M_AudioChannelCount);
	Metadata->SetStringField(TEXT("audioChannelLayout"), GetAudioChannelLayoutName(M_AudioChannelCount));
}

void UTrailerComponent::AddPathAndEncoderMetadata(const TSharedRef<FJsonObject>& Metadata) const
{
	Metadata->SetNumberField(TEXT("encoderReturnCode"), M_EncoderReturnCode);
	Metadata->SetStringField(TEXT("encoderOutput"), M_EncoderOutput);
	Metadata->SetStringField(TEXT("sessionDirectory"), M_SessionPaths.M_SessionDirectory);
	Metadata->SetStringField(TEXT("framesDirectory"), M_SessionPaths.M_FramesDirectory);
	Metadata->SetStringField(TEXT("audioFile"), M_SessionPaths.M_AudioFile);
	Metadata->SetStringField(TEXT("finalMp4File"), M_SessionPaths.M_FinalMp4File);
	Metadata->SetBoolField(TEXT("framesRetained"), IFileManager::Get().DirectoryExists(*M_SessionPaths.M_FramesDirectory));
	Metadata->SetBoolField(TEXT("audioRetained"), IFileManager::Get().FileExists(*M_SessionPaths.M_AudioFile));
	Metadata->SetBoolField(TEXT("endPlayAborted"), bM_EndPlayAborted);
}

void UTrailerComponent::ReadAudioFormatFromWav()
{
	TArray<uint8> WavBytes;
	if (not FFileHelper::LoadFileToArray(WavBytes, *M_SessionPaths.M_AudioFile)
		|| WavBytes.Num() < 12
		|| not HasChunkId(WavBytes, 0, "RIFF")
		|| not HasChunkId(WavBytes, 8, "WAVE"))
	{
		return;
	}

	int32 ChunkOffset = 12;
	while (ChunkOffset + 8 <= WavBytes.Num())
	{
		const uint32 ChunkSize = ReadLittleEndianUInt32(WavBytes, ChunkOffset + 4);
		const int64 NextChunkOffset = static_cast<int64>(ChunkOffset) + 8 + ChunkSize + (ChunkSize % 2);
		if (HasChunkId(WavBytes, ChunkOffset, "fmt ") && ChunkSize >= 16
			&& static_cast<int64>(ChunkOffset) + 24 <= WavBytes.Num())
		{
			M_AudioChannelCount = ReadLittleEndianUInt16(WavBytes, ChunkOffset + 10);
			M_AudioSampleRate = ReadLittleEndianUInt32(WavBytes, ChunkOffset + 12);
			return;
		}
		if (NextChunkOffset <= ChunkOffset || NextChunkOffset > WavBytes.Num())
		{
			return;
		}
		ChunkOffset = static_cast<int32>(NextChunkOffset);
	}
}

void UTrailerComponent::SetLastError(const FString& ErrorMessage)
{
	M_LastError = ErrorMessage;
	RTSFunctionLibrary::ReportError(ErrorMessage);
}

bool UTrailerComponent::GetIsValidMainSubmix() const
{
	if (M_MainSubmix.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_MainSubmix"),
		TEXT("GetIsValidMainSubmix"),
		this);
	return false;
}

FString UTrailerComponent::BuildFramePath(const int32 OneBasedFrameNumber) const
{
	return FPaths::Combine(
		M_SessionPaths.M_FramesDirectory,
		FString::Printf(TEXT("Frame_%06d.png"), OneBasedFrameNumber));
}

int32 UTrailerComponent::GetRequiredFrameCount(
	const double ElapsedSeconds,
	const URTSTrailerCaptureSettings& Settings) const
{
	const int32 FramesPerSecond = FMath::Max(1, Settings.M_FramesPerSecond);
	return FMath::Clamp(
		FMath::CeilToInt(ElapsedSeconds * FramesPerSecond),
		1,
		GetMaximumFrameCount(Settings));
}

int32 UTrailerComponent::GetMaximumFrameCount(const URTSTrailerCaptureSettings& Settings) const
{
	return FMath::Max(
		1,
		FMath::RoundToInt(FMath::Max(0.1f, Settings.M_MaximumDurationSeconds)
			* FMath::Max(1, Settings.M_FramesPerSecond)));
}

void UTrailerComponent::OnAudioFileWritten(const USoundWave* RecordedSoundWave)
{
	bM_AudioFileWritten = true;
	ReadAudioFormatFromWav();
}
