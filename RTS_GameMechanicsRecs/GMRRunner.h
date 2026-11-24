// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "MoviePipelineInProcessExecutor.h"
#include "UObject/Object.h"

#include "GMRJobTypes.h"

#include "GMRRunner.generated.h"

class AGMRDirector;
class ULevelSequence;
class UMoviePipeline;
class UMoviePipelinePrimaryConfig;

/** @brief In-process executor that exposes the running pipeline, so the director can key on the output frame. */
UCLASS()
class UGMRExecutor : public UMoviePipelineInProcessExecutor
{
	GENERATED_BODY()

public:
	UMoviePipeline* GetRunningPipeline() const;
};

/**
 * @brief Owns one recording take: reads the job, renders the job's map with Movie Render Queue (transient sequence
 * and config, nothing saved), spawns the director when MRQ has loaded the map and exits the process when done.
 * Created and rooted by the module when -GMRJob=<file> is on the command line.
 */
UCLASS()
class UGMRRunner : public UObject
{
	GENERATED_BODY()

public:
	void StartJob(const FString& JobPath);

	static UGMRRunner* Get();
	UMoviePipeline* GetActivePipeline() const;
	const FGMRJob& GetJob() const { return M_Job; }

private:
	bool LoadJobFile(const FString& Path);
	void ApplyProcessOverrides() const;
	bool TickStartRender(float DeltaSeconds);
	void StartRender();
	void OnPostLoadMap(UWorld* LoadedWorld);
	bool GetIsJobMap(const UWorld* World) const;
	void SpawnDirector(UWorld* World);
	void OnExecutorFinished(UMoviePipelineExecutorBase* Executor, bool bSuccess);
	void ExitProcess(int32 Status) const;

	ULevelSequence* BuildSequence() const;
	UMoviePipelinePrimaryConfig* BuildConfig() const;
	float GetTotalSeconds() const;

	FGMRJob M_Job;

	UPROPERTY()
	TObjectPtr<UGMRExecutor> M_Executor;

	UPROPERTY()
	TObjectPtr<ULevelSequence> M_Sequence;

	UPROPERTY()
	TObjectPtr<UMoviePipelinePrimaryConfig> M_Config;

	UPROPERTY()
	TWeakObjectPtr<AGMRDirector> M_Director;

	FTSTicker::FDelegateHandle M_StartTickerHandle;
	bool bM_RenderStarted = false;
	int32 M_StartTicks = 0;

	static TWeakObjectPtr<UGMRRunner> StaticInstance;
};
