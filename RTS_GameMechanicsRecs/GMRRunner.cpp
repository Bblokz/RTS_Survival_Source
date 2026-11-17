// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "GMRRunner.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "JsonObjectConverter.h"
#include "LevelSequence.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
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
#include "MovieScene.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "GMRDirector.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogGMRRunner, Log, All);

TWeakObjectPtr<UGMRRunner> UGMRRunner::StaticInstance;

namespace GMRRunnerPrivate
{
	// Image and audio writes are still queued when the executor reports; give them time before the process ends.
	constexpr float ExitDelaySeconds = 12.f;
	// The deferred "Module Load" runs inside a tick; start the render a few ticks later, outside that call stack.
	constexpr int32 TicksBeforeRender = 3;
	constexpr float TailSeconds = 0.5f;
}

UMoviePipeline* UGMRExecutor::GetRunningPipeline() const
{
	return Cast<UMoviePipeline>(ActiveMoviePipeline.Get());
}

UGMRRunner* UGMRRunner::Get()
{
	return StaticInstance.Get();
}

UMoviePipeline* UGMRRunner::GetActivePipeline() const
{
	return IsValid(M_Executor) ? M_Executor->GetRunningPipeline() : nullptr;
}

void UGMRRunner::StartJob(const FString& JobPath)
{
	StaticInstance = this;
	if (not LoadJobFile(JobPath))
	{
		UE_LOG(LogGMRRunner, Error, TEXT("GMR_JOB_LOAD_FAILED path=%s"), *JobPath);
		ExitProcess(3);
		return;
	}
	ApplyProcessOverrides();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGMRRunner::OnPostLoadMap);
	M_StartTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UGMRRunner::TickStartRender));
	UE_LOG(LogGMRRunner, Display, TEXT("GMR_JOB_LOADED name=%s map=%s aspect=%s res=%dx%d fps=%d seconds=%.1f events=%d shots=%d out=%s"),
	       *M_Job.JobName, *M_Job.MapPath, *M_Job.Aspect, M_Job.ResolutionX, M_Job.ResolutionY, M_Job.Fps,
	       M_Job.DurationSeconds, M_Job.Events.Num(), M_Job.Shots.Num(), *M_Job.OutputDir);
}

bool UGMRRunner::LoadJobFile(const FString& Path)
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
	M_Job.Fps = FMath::Max(1, M_Job.Fps);
	return not M_Job.MapPath.IsEmpty() && M_Job.Shots.Num() > 0;
}

void UGMRRunner::ApplyProcessOverrides() const
{
	// A -RenderOffscreen process is never the foreground window and the engine mutes audio while unfocused.
	FApp::SetHasFocusFunction([]() { return true; });
	FApp::SetUnfocusedVolumeMultiplier(1.f);
	FApp::SetVolumeMultiplier(1.f);
	// Recordings show the realistic vehicle balance, like every short (lower health, overmatch kills).
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		URTSGameInstance* const GameInstance = Cast<URTSGameInstance>(Context.OwningGameInstance);
		if (IsValid(GameInstance))
		{
			GameInstance->SetVehicleBalance(ERTSVehicleBalance::Realistic);
		}
	}
}

bool UGMRRunner::TickStartRender(const float DeltaSeconds)
{
	++M_StartTicks;
	if (M_StartTicks < GMRRunnerPrivate::TicksBeforeRender)
	{
		return true;
	}
	StartRender();
	return false;
}

void UGMRRunner::StartRender()
{
	UMoviePipelineQueueEngineSubsystem* QueueSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
	if (not IsValid(QueueSubsystem))
	{
		UE_LOG(LogGMRRunner, Error, TEXT("GMR_NO_MRQ_SUBSYSTEM"));
		ExitProcess(4);
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

	M_Executor = NewObject<UGMRExecutor>(this, TEXT("GMRExecutor"));
	M_Executor->OnExecutorFinished().AddUObject(this, &UGMRRunner::OnExecutorFinished);
	UE_LOG(LogGMRRunner, Display, TEXT("GMR_RENDER_START job=%s seconds=%.1f"), *M_Job.JobName, GetTotalSeconds());
	QueueSubsystem->RenderQueueWithExecutorInstance(M_Executor);
}

void UGMRRunner::OnPostLoadMap(UWorld* LoadedWorld)
{
	if (not bM_RenderStarted || not GetIsJobMap(LoadedWorld))
	{
		return;
	}
	SpawnDirector(LoadedWorld);
}

bool UGMRRunner::GetIsJobMap(const UWorld* World) const
{
	if (not IsValid(World))
	{
		return false;
	}
	return World->GetName() == FPackageName::GetShortName(M_Job.MapPath);
}

void UGMRRunner::SpawnDirector(UWorld* World)
{
	// Deferred so the job is set before BeginPlay and the first tick.
	AGMRDirector* Director = World->SpawnActorDeferred<AGMRDirector>(AGMRDirector::StaticClass(), FTransform::Identity);
	if (not IsValid(Director))
	{
		UE_LOG(LogGMRRunner, Error, TEXT("GMR_DIRECTOR_SPAWN_FAILED"));
		return;
	}
	Director->InitDirector(M_Job);
	Director->FinishSpawning(FTransform::Identity);
	M_Director = Director;
	UE_LOG(LogGMRRunner, Display, TEXT("GMR_DIRECTOR_SPAWNED world=%s"), *World->GetName());
}

void UGMRRunner::OnExecutorFinished(UMoviePipelineExecutorBase* Executor, const bool bSuccess)
{
	if (M_Director.IsValid())
	{
		M_Director->NotifyRenderFinished();
	}
	UE_LOG(LogGMRRunner, Display, TEXT("GMR_RENDER_FINISHED job=%s success=%s, exiting in %.0fs"), *M_Job.JobName,
	       bSuccess ? TEXT("true") : TEXT("false"), GMRRunnerPrivate::ExitDelaySeconds);
	const int32 Status = bSuccess ? 0 : 2;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Status](float)
	{
		ExitProcess(Status);
		return false;
	}), GMRRunnerPrivate::ExitDelaySeconds);
}

void UGMRRunner::ExitProcess(const int32 Status) const
{
	UE_LOG(LogGMRRunner, Display, TEXT("GMR_JOB_DONE job=%s success=%s"), *M_Job.JobName,
	       Status == 0 ? TEXT("true") : TEXT("false"));
	GLog->Flush();
	FPlatformMisc::RequestExitWithStatus(false, Status);
}

ULevelSequence* UGMRRunner::BuildSequence() const
{
	const FName SequenceName(*(TEXT("LS_GMR_") + M_Job.JobName));
	ULevelSequence* Sequence = NewObject<ULevelSequence>(GetTransientPackage(), SequenceName, RF_Transient | RF_Public);
	Sequence->Initialize();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	MovieScene->SetDisplayRate(FFrameRate(M_Job.Fps, 1));
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber EndFrame = (GetTotalSeconds() * TickResolution).RoundToFrame();
	MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), EndFrame));
	return Sequence;
}

UMoviePipelinePrimaryConfig* UGMRRunner::BuildConfig() const
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
	// No MRQ wave output: its recording buffer is reset mid-render; the director's submix capture is the audio.

	UMoviePipelineAntiAliasingSetting* AntiAliasing = Cast<UMoviePipelineAntiAliasingSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineAntiAliasingSetting::StaticClass()));
	AntiAliasing->SpatialSampleCount = FMath::Max(1, M_Job.SpatialSamples);
	AntiAliasing->TemporalSampleCount = FMath::Max(1, M_Job.TemporalSamples);
	AntiAliasing->bOverrideAntiAliasing = false;
	AntiAliasing->EngineWarmUpCount = M_Job.EngineWarmUpFrames;
	AntiAliasing->RenderWarmUpCount = M_Job.EngineWarmUpFrames;
	AntiAliasing->bRenderWarmUpFrames = true;
	AntiAliasing->bUseCameraCutForWarmUp = false;

	UMoviePipelineGameOverrideSetting* GameOverride = Cast<UMoviePipelineGameOverrideSetting>(
		Config->FindOrAddSettingByClass(UMoviePipelineGameOverrideSetting::StaticClass()));
	if (not M_Job.GameModeClassPath.IsEmpty())
	{
		GameOverride->SoftGameModeOverride = TSoftClassPtr<AGameModeBase>(FSoftClassPath(M_Job.GameModeClassPath));
	}
	GameOverride->bCinematicQualitySettings = true;
	return Config;
}

float UGMRRunner::GetTotalSeconds() const
{
	return M_Job.WarmupSeconds + M_Job.DurationSeconds + GMRRunnerPrivate::TailSeconds;
}
