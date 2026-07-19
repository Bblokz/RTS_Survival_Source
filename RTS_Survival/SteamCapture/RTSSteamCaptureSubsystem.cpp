#include "RTSSteamCaptureSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace RTSSteamCaptureSubsystemConstants
{
	constexpr float PendingWriteSleepSeconds = 0.01f;
	constexpr int32 SessionGuidCharacters = 8;
	constexpr float TargetFrameCountEpsilon = 0.001f;
	constexpr float ManualStopMaxDurationSnapSeconds = 0.25f;
	const TCHAR* const FramesDirectoryName = TEXT("Frames");
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
		SettingsJson->SetStringField(TEXT("captureSource"), TEXT("PlayerViewport"));
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
}

void URTSSteamCaptureSubsystem::Deinitialize()
{
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
	const int32 ResolutionY)
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

	if (not StartCapture_CreateSessionDirectory(*CaptureSettings))
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

	StopCaptureInternal(RTSSteamCaptureSubsystemConstants::ManualStopReason, true);
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

void URTSSteamCaptureSubsystem::AbortStartCapture()
{
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
	M_LastCapturedFramePixels.Empty();
	bM_IsRecording = false;
	bM_IsStopping = false;
	bM_DisableMaxDurationUntilFunctionCall = false;
	bM_OverrideResolution = false;
	M_OutputResolution = FIntPoint::ZeroValue;
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

	QueueDueFrames(CaptureSettings);

	if (bReachedMaxDuration)
	{
		StopCaptureInternal(RTSSteamCaptureSubsystemConstants::MaxDurationStopReason, true);
	}
}

void URTSSteamCaptureSubsystem::QueueDueFrames(const URTSSteamCaptureSettings& CaptureSettings)
{
	const int32 TargetFrameCount = GetTargetFrameCount(CaptureSettings);
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
	TArray<FColor>& OutFramePixels) const
{
	if (not GetIsValidPlayerController())
	{
		return false;
	}

	TArray<FColor> ViewportPixels;
	FIntPoint ViewportResolution = FIntPoint::ZeroValue;
	if (not ReadPlayerViewportPixels(ViewportPixels, ViewportResolution))
	{
		return false;
	}

	return ResizeFrameToOutputResolution(ViewportPixels, ViewportResolution, OutFramePixels);
}

bool URTSSteamCaptureSubsystem::ReadPlayerViewportPixels(
	TArray<FColor>& OutViewportPixels,
	FIntPoint& OutViewportResolution) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot capture the player viewport because the world is invalid."));
		return false;
	}

	const UGameViewportClient* GameViewportClient = World->GetGameViewport();
	if (not IsValid(GameViewportClient) || GameViewportClient->Viewport == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot capture the player viewport because it is invalid."));
		return false;
	}

	OutViewportResolution = GameViewportClient->Viewport->GetSizeXY();
	if (OutViewportResolution.X <= 0 || OutViewportResolution.Y <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot capture the player viewport because its resolution is invalid."));
		return false;
	}

	return GameViewportClient->Viewport->ReadPixels(OutViewportPixels);
}

bool URTSSteamCaptureSubsystem::ResizeFrameToOutputResolution(
	const TArray<FColor>& ViewportPixels,
	const FIntPoint& ViewportResolution,
	TArray<FColor>& OutFramePixels) const
{
	const int32 ExpectedViewportPixelCount = ViewportResolution.X * ViewportResolution.Y;
	if (ViewportPixels.Num() != ExpectedViewportPixelCount || M_OutputResolution.X <= 0 || M_OutputResolution.Y <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot resize Steam capture frame because its pixel data is invalid."));
		return false;
	}

	if (ViewportResolution == M_OutputResolution)
	{
		OutFramePixels = ViewportPixels;
	}
	else
	{
		OutFramePixels.SetNumZeroed(M_OutputResolution.X * M_OutputResolution.Y);
		const FImageView SourceImage(ViewportPixels.GetData(), ViewportResolution.X, ViewportResolution.Y);
		const FImageView OutputImage(OutFramePixels.GetData(), M_OutputResolution.X, M_OutputResolution.Y);
		FImageCore::ResizeImage(SourceImage, OutputImage);
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

int32 URTSSteamCaptureSubsystem::GetTargetFrameCount(
	const URTSSteamCaptureSettings& CaptureSettings) const
{
	const int32 FramesPerSecond = FMath::Max(1, CaptureSettings.M_FramesPerSecond);
	const float TargetFrameCount = M_RecordingElapsedSeconds * static_cast<float>(FramesPerSecond);
	return FMath::FloorToInt(TargetFrameCount + RTSSteamCaptureSubsystemConstants::TargetFrameCountEpsilon);
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

	if (bWaitForFrameWrites && IsValid(CaptureSettings) && CaptureSettings->bM_WaitForFrameWritesOnStop)
	{
		WaitForFrameWrites(*CaptureSettings);
	}

	WriteMetadata(StopReason, StopTime);
	M_PlayerController.Reset();
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
	MetadataJson->SetBoolField(
		TEXT("disabledMaxDurationUntilFunctionCall"),
		bM_DisableMaxDurationUntilFunctionCall);
	MetadataJson->SetBoolField(TEXT("overrodeResolution"), bM_OverrideResolution);
	MetadataJson->SetNumberField(TEXT("outputResolutionX"), M_OutputResolution.X);
	MetadataJson->SetNumberField(TEXT("outputResolutionY"), M_OutputResolution.Y);
	MetadataJson->SetStringField(TEXT("sessionDirectory"), M_SessionDirectory);
	MetadataJson->SetStringField(TEXT("framesDirectory"), M_FramesDirectory);
	if (IsValid(CaptureSettings))
	{
		MetadataJson->SetNumberField(TEXT("targetFrameCount"), GetTargetFrameCount(*CaptureSettings));
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
