// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsRenderSubsystem.h"

#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "LevelSequence.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MoviePipeline.h"
#include "MoviePipelineAntiAliasingSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineGameOverrideSetting.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueEngineSubsystem.h"
#include "MoviePipelineWaveOutput.h"
#include "MovieScene.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#include "RTSShortsDirector.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsRender, Log, All);

namespace
{
	constexpr float ExitDelaySeconds = 12.f;
}

UMoviePipeline* URTSShortsExecutor::GetShortsPipeline() const
{
	return Cast<UMoviePipeline>(ActiveMoviePipeline.Get());
}

void URTSShortsRenderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString JobPath;
	if (not FParse::Value(FCommandLine::Get(), TEXT("RTSShortsJob="), JobPath))
	{
		return;
	}
	JobPath = JobPath.TrimQuotes();
	if (not LoadJobFile(JobPath))
	{
		UE_LOG(LogRTSShortsRender, Error, TEXT("RTS_SHORTS_JOB_LOAD_FAILED path=%s"), *JobPath);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSShortsAudioPass")))
	{
		M_Job.bRender = false;
		M_Job.bRecordAudio = true;
	}
	// A -RenderOffscreen process is never the foreground window, and the engine mutes audio while unfocused
	// (FApp::HasFocus -> IsThisApplicationForeground, UnfocusedVolumeMultiplier 0). Count as focused for good.
	FApp::SetHasFocusFunction([]() { return true; });
	FApp::SetUnfocusedVolumeMultiplier(1.f);
	FApp::SetVolumeMultiplier(1.f);
	UE_LOG(LogRTSShortsRender, Display, TEXT("RTS_SHORTS_FOCUS_FORCED hasFocus=%s unfocusedMultiplier=%.2f"),
	       FApp::HasFocus() ? TEXT("true") : TEXT("false"), FApp::GetUnfocusedVolumeMultiplier());
	bM_HasJob = true;
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &URTSShortsRenderSubsystem::OnPostLoadMap);
	UE_LOG(LogRTSShortsRender, Display, TEXT("RTS_SHORTS_JOB_LOADED name=%s segments=%d out=%s"),
	       *M_Job.JobName, M_Job.Segments.Num(), *M_Job.OutputDir);
}

void URTSShortsRenderSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Deinitialize();
}

UMoviePipeline* URTSShortsRenderSubsystem::GetActivePipeline() const
{
	return IsValid(M_Executor) ? M_Executor->GetShortsPipeline() : nullptr;
}

bool URTSShortsRenderSubsystem::LoadJobFile(const FString& Path)
{
	FString Json;
	if (not FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}
	if (not FJsonObjectConverter::JsonObjectStringToUStruct(Json, &M_Job))
	{
		return false;
	}
	if (M_Job.OutputDir.IsEmpty())
	{
		M_Job.OutputDir = FPaths::GetPath(Path);
	}
	return M_Job.Segments.Num() > 0;
}

void URTSShortsRenderSubsystem::OnPostLoadMap(UWorld* LoadedWorld)
{
	if (not bM_HasJob || not GetIsShortsMap(LoadedWorld))
	{
		return;
	}
	if (not M_Job.bRender)
	{
		SpawnDirector(LoadedWorld);
		return;
	}
	if (not bM_RenderStarted)
	{
		// The in-process executor reloads the map itself; the director is spawned on that second load.
		StartRender(LoadedWorld);
		return;
	}
	SpawnDirector(LoadedWorld);
}

bool URTSShortsRenderSubsystem::GetIsShortsMap(const UWorld* World) const
{
	if (not IsValid(World))
	{
		return false;
	}
	const FString MapName = FPackageName::GetShortName(M_Job.MapPath);
	return World->GetName() == MapName;
}

void URTSShortsRenderSubsystem::SpawnDirector(UWorld* World)
{
	// Deferred so the job is set before BeginPlay and the first tick.
	ARTSShortsDirector* Director = World->SpawnActorDeferred<ARTSShortsDirector>(
		ARTSShortsDirector::StaticClass(), FTransform::Identity);
	if (not IsValid(Director))
	{
		UE_LOG(LogRTSShortsRender, Error, TEXT("RTS_SHORTS_DIRECTOR_SPAWN_FAILED"));
		return;
	}
	Director->InitShortsDirector(M_Job);
	Director->FinishSpawning(FTransform::Identity);
	M_Director = Director;
}

void URTSShortsRenderSubsystem::StartRender(UWorld* World)
{
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
	if (not IsValid(QueueSubsystem))
	{
		UE_LOG(LogRTSShortsRender, Error, TEXT("RTS_SHORTS_NO_MRQ_SUBSYSTEM"));
		return;
	}
	bM_RenderStarted = true;
	M_Sequence = BuildSequence();
	M_Config = BuildConfig();

	UMoviePipelineQueue* Queue = QueueSubsystem->GetQueue();
	Queue->DeleteAllJobs();
	UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	Job->JobName = M_Job.JobName;
	Job->Map = FSoftObjectPath(M_Job.MapPath + TEXT(".") + FPackageName::GetShortName(M_Job.MapPath));
	Job->SetSequence(FSoftObjectPath(M_Sequence));
	Job->SetConfiguration(M_Config);

	M_Executor = NewObject<URTSShortsExecutor>(this, TEXT("RTSShortsExecutor"));
	M_Executor->OnExecutorFinished().AddUObject(this, &URTSShortsRenderSubsystem::OnExecutorFinished);
	UE_LOG(LogRTSShortsRender, Display, TEXT("RTS_SHORTS_RENDER_START job=%s seconds=%.1f fps=%d res=%dx%d"),
	       *M_Job.JobName, GetTotalSeconds(), M_Job.Fps, M_Job.ResolutionX, M_Job.ResolutionY);
	QueueSubsystem->RenderQueueWithExecutorInstance(M_Executor);
}

void URTSShortsRenderSubsystem::OnExecutorFinished(UMoviePipelineExecutorBase* Executor, const bool bSuccess)
{
	if (IsValid(M_Director))
	{
		M_Director->NotifyRenderFinished();
	}
	UE_LOG(LogRTSShortsRender, Display, TEXT("RTS_SHORTS_RENDER_FINISHED job=%s success=%s, exiting in %.0fs"),
	       *M_Job.JobName, bSuccess ? TEXT("true") : TEXT("false"), ExitDelaySeconds);
	// Image and wave writes are still queued when the executor reports; give them time before the process ends.
	const int32 Status = bSuccess ? 0 : 2;
	GetGameInstance()->GetTimerManager().SetTimer(M_ExitTimer, FTimerDelegate::CreateWeakLambda(this, [this, Status]()
	{
		UE_LOG(LogRTSShortsRender, Display, TEXT("RTS_SHORTS_JOB_DONE job=%s success=%s"), *M_Job.JobName,
		       Status == 0 ? TEXT("true") : TEXT("false"));
		GLog->Flush();
		FPlatformMisc::RequestExitWithStatus(false, Status);
	}), ExitDelaySeconds, false);
}

ULevelSequence* URTSShortsRenderSubsystem::BuildSequence() const
{
	const FName SequenceName(*(TEXT("LS_Shorts_") + M_Job.JobName));
	ULevelSequence* Sequence = NewObject<ULevelSequence>(GetTransientPackage(), SequenceName, RF_Transient | RF_Public);
	Sequence->Initialize();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	MovieScene->SetDisplayRate(FFrameRate(M_Job.Fps, 1));
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber EndFrame = (GetTotalSeconds() * TickResolution).RoundToFrame();
	MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), EndFrame));
	return Sequence;
}

UMoviePipelinePrimaryConfig* URTSShortsRenderSubsystem::BuildConfig() const
{
	UMoviePipelinePrimaryConfig* Config = NewObject<UMoviePipelinePrimaryConfig>(GetTransientPackage(), NAME_None, RF_Transient);

	UMoviePipelineOutputSetting* Output = Cast<UMoviePipelineOutputSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
	Output->OutputDirectory.Path = M_Job.OutputDir;
	Output->FileNameFormat = TEXT("{sequence_name}.{frame_number}");
	Output->OutputResolution = FIntPoint(M_Job.ResolutionX, M_Job.ResolutionY);
	Output->bUseCustomFrameRate = true;
	Output->OutputFrameRate = FFrameRate(M_Job.Fps, 1);
	Output->bOverrideExistingOutput = true;
	Output->ZeroPadFrameNumbers = 5;
	Output->bFlushDiskWritesPerShot = true;

	Config->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());
	Config->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_JPG::StaticClass());
	// No MRQ wave output: its recording buffer is reset mid-render (audio lost) and its finalize asserts on long
	// sequences; the director's own submix capture (<Job>_capture.wav) is the audio track.

	UMoviePipelineAntiAliasingSetting* AntiAliasing = Cast<UMoviePipelineAntiAliasingSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineAntiAliasingSetting::StaticClass()));
	AntiAliasing->SpatialSampleCount = FMath::Max(1, M_Job.SpatialSamples);
	AntiAliasing->TemporalSampleCount = FMath::Max(1, M_Job.TemporalSamples);
	AntiAliasing->bOverrideAntiAliasing = false;
	AntiAliasing->EngineWarmUpCount = M_Job.EngineWarmUpFrames;
	AntiAliasing->RenderWarmUpCount = M_Job.RenderWarmUpFrames;
	AntiAliasing->bRenderWarmUpFrames = true;
	AntiAliasing->bUseCameraCutForWarmUp = false;

	UMoviePipelineGameOverrideSetting* GameOverride = Cast<UMoviePipelineGameOverrideSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineGameOverrideSetting::StaticClass()));
	GameOverride->SoftGameModeOverride = TSoftClassPtr<AGameModeBase>(FSoftClassPath(M_Job.GameModeClassPath));
	GameOverride->bCinematicQualitySettings = true;
	return Config;
}

float URTSShortsRenderSubsystem::GetTotalSeconds() const
{
	float Seconds = M_Job.WarmupSeconds;
	for (const FRTSShortsSegment& Segment : M_Job.Segments)
	{
		Seconds += Segment.SlotSeconds;
	}
	return Seconds;
}
