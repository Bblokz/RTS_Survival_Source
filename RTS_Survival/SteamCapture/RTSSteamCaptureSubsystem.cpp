#include "RTSSteamCaptureSubsystem.h"

#include "AudioDevice.h"
#include "CanvasTypes.h"
#include "DSP/AlignedBuffer.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundSubmix.h"
#include "UnrealClient.h"

namespace RTSSteamCaptureSubsystemConstants
{
	constexpr float PendingWriteSleepSeconds = 0.01f;
	constexpr int32 SessionGuidCharacters = 8;
	constexpr float TargetFrameCountEpsilon = 0.001f;
	constexpr float ManualStopMaxDurationSnapSeconds = 0.25f;
	const TCHAR* const FramesDirectoryName = TEXT("Frames");
	const TCHAR* const AudioFileName = TEXT("Audio.wav");
	const TCHAR* const MetadataFileName = TEXT("metadata.json");
	const TCHAR* const ManualStopReason = TEXT("ManualStop");
	const TCHAR* const MaxDurationStopReason = TEXT("MaxDuration");
}

namespace
{
	FString GetSafeOutputName(const FString& RawName, const FString& FallbackName)
	{
		const FString SourceName = RawName.IsEmpty() ? FallbackName : RawName;
		return FPaths::MakeValidFileName(SourceName);
	}

	TSharedRef<FJsonObject> BuildSettingsMetadata(const URTSSteamCaptureSettings& CaptureSettings)
	{
		TSharedRef<FJsonObject> SettingsJson = MakeShared<FJsonObject>();
		SettingsJson->SetNumberField(TEXT("outputResolutionX"), CaptureSettings.M_OutputResolutionX);
		SettingsJson->SetNumberField(TEXT("outputResolutionY"), CaptureSettings.M_OutputResolutionY);
		SettingsJson->SetNumberField(TEXT("framesPerSecond"), CaptureSettings.M_FramesPerSecond);
		SettingsJson->SetNumberField(TEXT("maxDurationSeconds"), CaptureSettings.M_MaxDurationSeconds);
		SettingsJson->SetBoolField(TEXT("autoStopAtMaxDuration"), CaptureSettings.bM_AutoStopAtMaxDuration);
		SettingsJson->SetStringField(TEXT("captureSource"), TEXT("DedicatedPlayerViewport"));
		return SettingsJson;
	}
}

bool URTSSteamCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld))
	{
		return false;
	}

#if WITH_EDITOR
	return OuterWorld->WorldType == EWorldType::PIE;
#else
	return false;
#endif
}

void URTSSteamCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetSessionState();
	M_EndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(
		this,
		&URTSSteamCaptureSubsystem::OnEndFrame);
}

void URTSSteamCaptureSubsystem::Deinitialize()
{
	if (M_EndFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(M_EndFrameDelegateHandle);
		M_EndFrameDelegateHandle.Reset();
	}

	StopCaptureInternal(TEXT("SubsystemDeinitialize"), true);
	Super::Deinitialize();
}

void URTSSteamCaptureSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (not bM_IsRecording || bM_IsStopping)
	{
		return;
	}

	const URTSSteamCaptureSettings* CaptureSettings = GetCaptureSettings();
	if (not IsValid(CaptureSettings))
	{
		StopCaptureInternal(TEXT("InvalidSettings"), true);
		return;
	}

	TickCapture(DeltaTime, *CaptureSettings);
}

TStatId URTSSteamCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSSteamCaptureSubsystem, STATGROUP_Tickables);
}

bool URTSSteamCaptureSubsystem::StartCapture(
	ACPPController* PlayerController,
	const bool bDisableMaxDurationUntilFunctionCall,
	const bool bOverrideResolution,
	const int32 ResolutionX,
	const int32 ResolutionY,
	const bool bRecordAudio)
{
	if (bM_IsRecording)
	{
		return true;
	}

	const URTSSteamCaptureSettings* CaptureSettings = GetCaptureSettings();
	if (not GetCanStartCapture(PlayerController, CaptureSettings))
	{
		return false;
	}

	ResetSessionState();
	M_PlayerController = PlayerController;
	bM_DisableMaxDurationUntilFunctionCall = bDisableMaxDurationUntilFunctionCall;
	bM_OverrideResolution = bOverrideResolution;
	M_AudioSession.bM_RecordAudio = bRecordAudio;
	M_RecordingFrameRate = FMath::Max(1, CaptureSettings->M_FramesPerSecond);
	M_OutputResolution.X = FMath::Max(
		1,
		bOverrideResolution ? ResolutionX : CaptureSettings->M_OutputResolutionX);
	M_OutputResolution.Y = FMath::Max(
		1,
		bOverrideResolution ? ResolutionY : CaptureSettings->M_OutputResolutionY);
	M_SessionGuid = FGuid::NewGuid();
	M_SessionStartDateTime = FDateTime::Now();
	M_SessionName = BuildSessionName(*CaptureSettings);
	M_FrameWriter.ResetFailedWriteCount();

	if (not StartCapture_CreateSessionDirectory(*CaptureSettings)
		|| not StartCapture_CreateViewport()
		|| not StartCapture_StartAudio(*CaptureSettings))
	{
		AbortStartCapture();
		return false;
	}

	bM_IsRecording = true;
	return true;
}

bool URTSSteamCaptureSubsystem::StopCapture()
{
	if (not bM_IsRecording)
	{
		return true;
	}

	M_PendingStop = ERTSSteamCapturePendingStop::Manual;
	return true;
}

const URTSSteamCaptureSettings* URTSSteamCaptureSubsystem::GetCaptureSettings() const
{
	const URTSSteamCaptureSettings* CaptureSettings = GetDefault<URTSSteamCaptureSettings>();
	if (IsValid(CaptureSettings))
	{
		return CaptureSettings;
	}

	RTSFunctionLibrary::ReportError(TEXT("RTS Steam Capture settings were not available."));
	return nullptr;
}

bool URTSSteamCaptureSubsystem::GetCanStartCapture(
	ACPPController* PlayerController,
	const URTSSteamCaptureSettings* CaptureSettings) const
{
	if (not IsValid(CaptureSettings) || not CaptureSettings->bM_EnableSteamCapture)
	{
		return false;
	}
	if (not GetIsPieWorld())
	{
		RTSFunctionLibrary::ReportError(TEXT("Steam capture can only be started in PIE."));
		return false;
	}
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot start Steam capture because player controller is invalid."));
		return false;
	}

	const UWorld* PlayerWorld = PlayerController->GetWorld();
	const UGameViewportClient* GameViewportClient = IsValid(PlayerWorld) ? PlayerWorld->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient) || GameViewportClient->Viewport == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot start Steam capture because the player viewport is invalid."));
		return false;
	}
	return true;
}

bool URTSSteamCaptureSubsystem::GetIsPieWorld() const
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	return IsValid(World) && World->WorldType == EWorldType::PIE;
#else
	return false;
#endif
}

bool URTSSteamCaptureSubsystem::StartCapture_CreateSessionDirectory(
	const URTSSteamCaptureSettings& CaptureSettings)
{
	const FString OutputDirectoryName = GetSafeOutputName(
		CaptureSettings.M_OutputDirectoryName,
		TEXT("SteamCaptures"));
	const FString RootDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), OutputDirectoryName);
	M_SessionDirectory = FPaths::Combine(RootDirectory, M_SessionName);
	M_FramesDirectory = FPaths::Combine(
		M_SessionDirectory,
		RTSSteamCaptureSubsystemConstants::FramesDirectoryName);
	M_AudioSession.M_AudioFilePath = FPaths::Combine(
		M_SessionDirectory,
		RTSSteamCaptureSubsystemConstants::AudioFileName);
	M_MetadataPath = FPaths::Combine(
		M_SessionDirectory,
		RTSSteamCaptureSubsystemConstants::MetadataFileName);

	const bool bCreated = IFileManager::Get().MakeDirectory(*M_FramesDirectory, true);
	if (not bCreated)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create Steam capture frame directory: ") + M_FramesDirectory);
	}
	return bCreated;
}

bool URTSSteamCaptureSubsystem::StartCapture_CreateViewport()
{
	const UWorld* World = GetWorld();
	UGameViewportClient* GameViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient) || GameViewportClient->Viewport == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot create Steam capture viewport because the player viewport is invalid."));
		return false;
	}

	M_CaptureViewport = MakeUnique<FRTSSteamCaptureViewport>(GameViewportClient);
	if (not M_CaptureViewport->Initialize(M_OutputResolution, *GameViewportClient->Viewport))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to initialize the dedicated Steam capture viewport."));
		M_CaptureViewport.Reset();
		return false;
	}

	M_CaptureViewExtension = FSceneViewExtensions::NewExtension<FRTSSteamCaptureViewExtension>(
		M_CaptureViewport.Get(),
		World->GetFeatureLevel());
	return M_CaptureViewExtension != nullptr;
}

bool URTSSteamCaptureSubsystem::StartCapture_StartAudio(
	const URTSSteamCaptureSettings& CaptureSettings)
{
	if (not M_AudioSession.bM_RecordAudio)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return FailAudioCapture(TEXT("Cannot start Steam capture audio because the world is invalid."));
	}

	M_AudioSession.M_AudioDevice = World->GetAudioDevice();
	if (not M_AudioSession.M_AudioDevice)
	{
		return FailAudioCapture(TEXT("Cannot record Steam capture audio because no audio device is available."));
	}

	USoundSubmix& MainSubmix = M_AudioSession.M_AudioDevice->GetMainSubmixObject();
	const float ExpectedDuration = FMath::Max(0.1f, CaptureSettings.M_MaxDurationSeconds);
	M_AudioSession.M_AudioDevice->StartRecording(&MainSubmix, ExpectedDuration);
	M_AudioSession.bM_RecordingStarted = true;
	return true;
}

void URTSSteamCaptureSubsystem::AbortStartCapture()
{
	DestroyCaptureViewport();
	M_PlayerController.Reset();
	bM_IsRecording = false;
}

void URTSSteamCaptureSubsystem::ResetSessionState()
{
	M_PlayerController.Reset();
	M_SessionDirectory.Reset();
	M_FramesDirectory.Reset();
	M_MetadataPath.Reset();
	M_SessionName.Reset();
	M_SessionGuid.Invalidate();
	M_SessionStartDateTime = FDateTime();
	M_RecordingElapsedSeconds = 0.0f;
	M_CapturedFrameCount = 0;
	M_DuplicatedFrameCount = 0;
	M_DroppedFrameCount = 0;
	M_RecordingFrameRate = 0;
	M_LastCapturedFramePixels.Empty();
	M_AudioSession = FRTSSteamCaptureAudioSessionState();
	bM_IsRecording = false;
	bM_IsStopping = false;
	bM_DisableMaxDurationUntilFunctionCall = false;
	bM_OverrideResolution = false;
	M_OutputResolution = FIntPoint::ZeroValue;
	M_PendingStop = ERTSSteamCapturePendingStop::None;
}

void URTSSteamCaptureSubsystem::DestroyCaptureViewport()
{
	if (M_CaptureViewport != nullptr)
	{
		M_CaptureViewport->Release();
	}

	M_CaptureViewExtension.Reset();
	M_CaptureViewport.Reset();
}

void URTSSteamCaptureSubsystem::TickCapture(
	const float DeltaTime,
	const URTSSteamCaptureSettings& CaptureSettings)
{
	const float MaxDurationSeconds = FMath::Max(0.1f, CaptureSettings.M_MaxDurationSeconds);
	const float NewRecordingElapsedSeconds = M_RecordingElapsedSeconds + DeltaTime;
	const bool bReachedMaxDuration = not bM_DisableMaxDurationUntilFunctionCall
		&& CaptureSettings.bM_AutoStopAtMaxDuration
		&& NewRecordingElapsedSeconds >= MaxDurationSeconds;
	M_RecordingElapsedSeconds = bReachedMaxDuration
		                            ? MaxDurationSeconds
		                            : NewRecordingElapsedSeconds;

	if (bReachedMaxDuration && M_PendingStop == ERTSSteamCapturePendingStop::None)
	{
		M_PendingStop = ERTSSteamCapturePendingStop::MaxDuration;
	}
}

void URTSSteamCaptureSubsystem::OnEndFrame()
{
	if (not bM_IsRecording || bM_IsStopping)
	{
		return;
	}

	const URTSSteamCaptureSettings* CaptureSettings = GetCaptureSettings();
	if (not IsValid(CaptureSettings))
	{
		StopCaptureInternal(TEXT("InvalidSettings"), true);
		return;
	}

	QueueDueFrames(*CaptureSettings);
	if (M_PendingStop != ERTSSteamCapturePendingStop::None)
	{
		StopCaptureInternal(GetPendingStopReason(), true);
	}
}

void URTSSteamCaptureSubsystem::QueueDueFrames(const URTSSteamCaptureSettings& CaptureSettings)
{
	const int32 TargetFrameCount = GetTargetFrameCount();
	if (M_CapturedFrameCount >= TargetFrameCount)
	{
		return;
	}

	TArray<FColor> CurrentFramePixels;
	if (not CaptureFramePixels(CurrentFramePixels))
	{
		return;
	}

	M_LastCapturedFramePixels = CurrentFramePixels;
	if (not QueueFrameWrite(CaptureSettings, MoveTemp(CurrentFramePixels)))
	{
		return;
	}

	while (M_CapturedFrameCount < TargetFrameCount)
	{
		if (M_LastCapturedFramePixels.IsEmpty())
		{
			return;
		}

		TArray<FColor> DuplicatedFramePixels = M_LastCapturedFramePixels;
		if (not QueueFrameWrite(CaptureSettings, MoveTemp(DuplicatedFramePixels)))
		{
			return;
		}
		++M_DuplicatedFrameCount;
	}
}

bool URTSSteamCaptureSubsystem::CaptureFramePixels(
	TArray<FColor>& OutFramePixels)
{
	if (not GetIsValidPlayerController()
		|| M_CaptureViewport == nullptr
		|| not M_CaptureViewport->GetIsInitialized())
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot capture frame because the dedicated capture viewport is invalid."));
		return false;
	}

	UWorld* World = GetWorld();
	UGameViewportClient* GameViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot capture frame because the game viewport client is invalid."));
		return false;
	}

	M_CaptureViewport->EnqueueBeginRenderFrame(false);

	FCanvas CaptureCanvas(
		M_CaptureViewport.Get(),
		nullptr,
		World,
		World->GetFeatureLevel(),
		FCanvas::CDM_DeferDrawing,
		GameViewportClient->ShouldDPIScaleSceneCanvas() ? GameViewportClient->GetDPIScale() : 1.0f);
	CaptureCanvas.SetRenderTargetRect(FIntRect(FIntPoint::ZeroValue, M_OutputResolution));
	GameViewportClient->Draw(M_CaptureViewport.Get(), &CaptureCanvas);
	CaptureCanvas.Flush_GameThread();
	if (FCanvas* DebugCanvas = M_CaptureViewport->GetDebugCanvas())
	{
		DebugCanvas->SetAllowedModes(FCanvas::Allow_DeleteOnRender);
		DebugCanvas->Flush_GameThread(true);
	}

	M_CaptureViewport->EnqueueEndRenderFrame(false, false);

	FReadSurfaceDataFlags ReadSurfaceFlags(RCM_UNorm, CubeFace_MAX);
	ReadSurfaceFlags.SetLinearToGamma(false);
	if (not M_CaptureViewport->ReadPixels(OutFramePixels, ReadSurfaceFlags)
		|| OutFramePixels.Num() != M_OutputResolution.X * M_OutputResolution.Y)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to read the dedicated Steam capture viewport."));
		return false;
	}

	for (FColor& FramePixel : OutFramePixels)
	{
		FramePixel.A = MAX_uint8;
	}
	return true;
}

bool URTSSteamCaptureSubsystem::QueueFrameWrite(
	const URTSSteamCaptureSettings& CaptureSettings,
	TArray<FColor>&& FramePixels)
{
	const int32 MaxPendingFrameWrites = FMath::Max(1, CaptureSettings.M_MaxPendingFrameWrites);
	if (M_FrameWriter.GetPendingWriteCount() >= MaxPendingFrameWrites)
	{
		++M_DroppedFrameCount;
		return false;
	}

	++M_CapturedFrameCount;
	M_FrameWriter.WritePngFrameAsync(
		BuildFramePath(M_CapturedFrameCount),
		M_OutputResolution.X,
		M_OutputResolution.Y,
		MoveTemp(FramePixels));
	return true;
}

FString URTSSteamCaptureSubsystem::BuildFramePath(const int32 FrameNumber) const
{
	const FString FileName = FString::Printf(TEXT("Frame_%06d.png"), FrameNumber);
	return FPaths::Combine(M_FramesDirectory, FileName);
}

FString URTSSteamCaptureSubsystem::BuildSessionName(
	const URTSSteamCaptureSettings& CaptureSettings) const
{
	const FString Prefix = GetSafeOutputName(CaptureSettings.M_FileNamePrefix, TEXT("SteamCapture"));
	const FString Stamp = M_SessionStartDateTime.ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString GuidText = M_SessionGuid.ToString(EGuidFormats::Digits).Left(
		RTSSteamCaptureSubsystemConstants::SessionGuidCharacters);
	return FString::Printf(TEXT("%s_%s_%s"), *Prefix, *Stamp, *GuidText);
}

int32 URTSSteamCaptureSubsystem::GetTargetFrameCount() const
{
	const float TargetFrameCount = M_RecordingElapsedSeconds * static_cast<float>(M_RecordingFrameRate);
	return FMath::FloorToInt(TargetFrameCount + RTSSteamCaptureSubsystemConstants::TargetFrameCountEpsilon);
}

FString URTSSteamCaptureSubsystem::GetPendingStopReason() const
{
	switch (M_PendingStop)
	{
	case ERTSSteamCapturePendingStop::Manual:
		return RTSSteamCaptureSubsystemConstants::ManualStopReason;
	case ERTSSteamCapturePendingStop::MaxDuration:
		return RTSSteamCaptureSubsystemConstants::MaxDurationStopReason;
	case ERTSSteamCapturePendingStop::None:
	default:
		return TEXT("UnknownStop");
	}
}

bool URTSSteamCaptureSubsystem::StopCapture_SaveAudio()
{
	if (not M_AudioSession.bM_RecordAudio)
	{
		return true;
	}
	if (not M_AudioSession.bM_RecordingStarted)
	{
		return FailAudioCapture(TEXT("Steam capture audio was requested but master submix recording was not active."));
	}
	if (not M_AudioSession.M_AudioDevice)
	{
		return FailAudioCapture(TEXT("Cannot stop Steam capture audio because the audio device is unavailable."));
	}

	USoundSubmix& MainSubmix = M_AudioSession.M_AudioDevice->GetMainSubmixObject();
	float ChannelCount = 0.0f;
	float SampleRate = 0.0f;
	Audio::FAlignedFloatBuffer& MasterOutputSamples = M_AudioSession.M_AudioDevice->StopRecording(
		&MainSubmix,
		ChannelCount,
		SampleRate);
	M_AudioSession.bM_RecordingStarted = false;

	const int32 RoundedChannelCount = FMath::RoundToInt(ChannelCount);
	const int32 RoundedSampleRate = FMath::RoundToInt(SampleRate);
	if (MasterOutputSamples.IsEmpty() || RoundedChannelCount <= 0 || RoundedSampleRate <= 0)
	{
		M_AudioSession.M_AudioDevice.Reset();
		return FailAudioCapture(TEXT("The player master submix did not return recordable audio samples."));
	}

	const bool bSavedAudio = SaveCapturedAudio(
		MasterOutputSamples,
		RoundedChannelCount,
		RoundedSampleRate);
	M_AudioSession.M_AudioDevice.Reset();
	return bSavedAudio;
}

bool URTSSteamCaptureSubsystem::SaveCapturedAudio(
	const TArrayView<const float> MasterOutputSamples,
	const int32 ChannelCount,
	const int32 SampleRate)
{
	FString SaveError;
	if (not FRTSSteamCaptureAudio::SaveSynchronizedWave(
		MasterOutputSamples,
		ChannelCount,
		SampleRate,
		M_CapturedFrameCount,
		M_RecordingFrameRate,
		M_AudioSession.M_AudioFilePath,
		M_AudioSession.M_Result,
		SaveError))
	{
		return FailAudioCapture(SaveError);
	}
	M_AudioSession.bM_FileSaved = true;
	return ValidateSavedAudio();
}

bool URTSSteamCaptureSubsystem::ValidateSavedAudio()
{
	FString ValidationError;
	if (not FRTSSteamCaptureAudio::ValidateWaveFile(
		M_AudioSession.M_AudioFilePath,
		M_AudioSession.M_Result.M_ChannelCount,
		M_AudioSession.M_Result.M_SampleRate,
		M_AudioSession.M_Result.M_SynchronizedSampleFrameCount,
		M_AudioSession.M_WaveInfo,
		ValidationError))
	{
		return FailAudioCapture(ValidationError);
	}

	M_AudioSession.bM_FileValidated = true;
	return true;
}

bool URTSSteamCaptureSubsystem::FailAudioCapture(const FString& ErrorMessage)
{
	M_AudioSession.M_LastError = ErrorMessage;
	RTSFunctionLibrary::ReportError(ErrorMessage);
	return false;
}

void URTSSteamCaptureSubsystem::StopCaptureInternal(
	const FString& StopReason,
	const bool bWaitForFrameWrites)
{
	if (not bM_IsRecording)
	{
		return;
	}

	bM_IsStopping = true;
	bM_IsRecording = false;
	const FDateTime StopTime = FDateTime::Now();
	const URTSSteamCaptureSettings* CaptureSettings = GetCaptureSettings();
	if (IsValid(CaptureSettings)
		&& StopReason == RTSSteamCaptureSubsystemConstants::ManualStopReason
		&& not bM_DisableMaxDurationUntilFunctionCall
		&& CaptureSettings->bM_AutoStopAtMaxDuration)
	{
		const float MaxDurationSeconds = FMath::Max(0.1f, CaptureSettings->M_MaxDurationSeconds);
		if (MaxDurationSeconds - M_RecordingElapsedSeconds <=
			RTSSteamCaptureSubsystemConstants::ManualStopMaxDurationSnapSeconds)
		{
			M_RecordingElapsedSeconds = MaxDurationSeconds;
			QueueDueFrames(*CaptureSettings);
		}
	}

	StopCapture_SaveAudio();
	if (bWaitForFrameWrites && IsValid(CaptureSettings) && CaptureSettings->bM_WaitForFrameWritesOnStop)
	{
		WaitForFrameWrites(*CaptureSettings);
	}

	WriteMetadata(StopReason, StopTime);
	DestroyCaptureViewport();
	M_PlayerController.Reset();
	M_PendingStop = ERTSSteamCapturePendingStop::None;
	bM_IsStopping = false;
}

void URTSSteamCaptureSubsystem::WaitForFrameWrites(
	const URTSSteamCaptureSettings& CaptureSettings) const
{
	const double WaitStartTime = FPlatformTime::Seconds();
	while (M_FrameWriter.GetPendingWriteCount() > 0)
	{
		const double WaitDuration = FPlatformTime::Seconds() - WaitStartTime;
		if (WaitDuration >= CaptureSettings.M_MaxFrameWriteFlushSeconds)
		{
			RTSFunctionLibrary::ReportError(TEXT("Timed out while waiting for Steam capture frame writes."));
			return;
		}

		FPlatformProcess::Sleep(RTSSteamCaptureSubsystemConstants::PendingWriteSleepSeconds);
	}
}

void URTSSteamCaptureSubsystem::WriteMetadata(
	const FString& StopReason,
	const FDateTime& StopTime) const
{
	if (M_MetadataPath.IsEmpty())
	{
		return;
	}

	const URTSSteamCaptureSettings* CaptureSettings = GetCaptureSettings();
	TSharedRef<FJsonObject> MetadataJson = MakeShared<FJsonObject>();
	AddSessionMetadata(MetadataJson, StopReason, StopTime);
	AddAudioMetadata(MetadataJson);
	if (IsValid(CaptureSettings))
	{
		MetadataJson->SetNumberField(TEXT("targetFrameCount"), GetTargetFrameCount());
		MetadataJson->SetObjectField(TEXT("settings"), BuildSettingsMetadata(*CaptureSettings));
	}

	if (const UWorld* World = GetWorld())
	{
		MetadataJson->SetStringField(TEXT("mapName"), World->GetMapName());
	}

	FString SerializedMetadata;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedMetadata);
	if (not FJsonSerializer::Serialize(MetadataJson, Writer))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to serialize Steam capture metadata."));
		return;
	}

	const bool bSaved = FFileHelper::SaveStringToFile(SerializedMetadata, *M_MetadataPath);
	if (not bSaved)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to save Steam capture metadata: ") + M_MetadataPath);
	}
}

void URTSSteamCaptureSubsystem::AddSessionMetadata(
	const TSharedRef<FJsonObject>& MetadataJson,
	const FString& StopReason,
	const FDateTime& StopTime) const
{
	MetadataJson->SetStringField(TEXT("sessionName"), M_SessionName);
	MetadataJson->SetStringField(TEXT("sessionGuid"), M_SessionGuid.ToString(EGuidFormats::Digits));
	MetadataJson->SetStringField(TEXT("startTimeLocal"), M_SessionStartDateTime.ToIso8601());
	MetadataJson->SetStringField(TEXT("stopTimeLocal"), StopTime.ToIso8601());
	MetadataJson->SetStringField(TEXT("stopReason"), StopReason);
	MetadataJson->SetNumberField(TEXT("capturedFrameCount"), M_CapturedFrameCount);
	MetadataJson->SetNumberField(TEXT("duplicatedFrameCount"), M_DuplicatedFrameCount);
	MetadataJson->SetNumberField(TEXT("droppedFrameCount"), M_DroppedFrameCount);
	MetadataJson->SetNumberField(TEXT("failedFrameWriteCount"), M_FrameWriter.GetFailedWriteCount());
	MetadataJson->SetNumberField(TEXT("pendingFrameWriteCount"), M_FrameWriter.GetPendingWriteCount());
	MetadataJson->SetNumberField(TEXT("recordedSeconds"), M_RecordingElapsedSeconds);
	MetadataJson->SetNumberField(TEXT("framesPerSecond"), M_RecordingFrameRate);
	MetadataJson->SetNumberField(
		TEXT("videoDurationSeconds"),
		M_RecordingFrameRate > 0
			? static_cast<double>(M_CapturedFrameCount) / M_RecordingFrameRate
			: 0.0);
	MetadataJson->SetBoolField(
		TEXT("disabledMaxDurationUntilFunctionCall"),
		bM_DisableMaxDurationUntilFunctionCall);
	MetadataJson->SetBoolField(TEXT("overrodeResolution"), bM_OverrideResolution);
	MetadataJson->SetNumberField(TEXT("outputResolutionX"), M_OutputResolution.X);
	MetadataJson->SetNumberField(TEXT("outputResolutionY"), M_OutputResolution.Y);
	MetadataJson->SetStringField(TEXT("sessionDirectory"), M_SessionDirectory);
	MetadataJson->SetStringField(TEXT("framesDirectory"), M_FramesDirectory);
}

void URTSSteamCaptureSubsystem::AddAudioMetadata(
	const TSharedRef<FJsonObject>& MetadataJson) const
{
	const double VideoDurationSeconds = M_RecordingFrameRate > 0
		? static_cast<double>(M_CapturedFrameCount) / M_RecordingFrameRate
		: 0.0;
	TSharedRef<FJsonObject> AudioJson = MakeShared<FJsonObject>();
	AudioJson->SetBoolField(TEXT("requested"), M_AudioSession.bM_RecordAudio);
	AudioJson->SetBoolField(TEXT("fileSaved"), M_AudioSession.bM_FileSaved);
	AudioJson->SetBoolField(TEXT("fileValidated"), M_AudioSession.bM_FileValidated);
	AudioJson->SetStringField(TEXT("file"), M_AudioSession.M_AudioFilePath);
	AudioJson->SetStringField(TEXT("error"), M_AudioSession.M_LastError);
	AudioJson->SetNumberField(TEXT("sampleRate"), M_AudioSession.M_WaveInfo.M_SampleRate);
	AudioJson->SetNumberField(TEXT("channelCount"), M_AudioSession.M_WaveInfo.M_ChannelCount);
	AudioJson->SetNumberField(TEXT("bitsPerSample"), M_AudioSession.M_WaveInfo.M_BitsPerSample);
	AudioJson->SetNumberField(TEXT("sourceSampleFrameCount"), M_AudioSession.M_Result.M_SourceSampleFrameCount);
	AudioJson->SetNumberField(
		TEXT("synchronizedSampleFrameCount"),
		M_AudioSession.M_Result.M_SynchronizedSampleFrameCount);
	AudioJson->SetNumberField(TEXT("trimmedSampleFrameCount"), M_AudioSession.M_Result.M_TrimmedSampleFrameCount);
	AudioJson->SetNumberField(TEXT("paddedSampleFrameCount"), M_AudioSession.M_Result.M_PaddedSampleFrameCount);
	AudioJson->SetNumberField(TEXT("durationSeconds"), M_AudioSession.M_WaveInfo.M_DurationSeconds);
	AudioJson->SetNumberField(
		TEXT("durationDeltaFromVideoSeconds"),
		M_AudioSession.M_WaveInfo.M_DurationSeconds - VideoDurationSeconds);
	AudioJson->SetNumberField(TEXT("peakAmplitude"), M_AudioSession.M_WaveInfo.M_PeakAmplitude);
	AudioJson->SetNumberField(
		TEXT("rootMeanSquareAmplitude"),
		M_AudioSession.M_WaveInfo.M_RootMeanSquareAmplitude);
	AudioJson->SetBoolField(TEXT("hasNonSilentSamples"), M_AudioSession.M_WaveInfo.bM_HasNonSilentSamples);
	MetadataJson->SetObjectField(TEXT("audio"), AudioJson);
}

bool URTSSteamCaptureSubsystem::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("GetIsValidPlayerController"),
		this);
	return false;
}
