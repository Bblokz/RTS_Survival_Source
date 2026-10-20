// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineInProcessExecutor.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RTSShortsJobTypes.h"

#include "RTSShortsRenderSubsystem.generated.h"

class ARTSShortsDirector;
class ULevelSequence;
class UMoviePipeline;
class UMoviePipelinePrimaryConfig;

/**
 * @brief In-process executor that exposes the running pipeline so the director can read the output frame.
 */
UCLASS()
class RTS_SURVIVAL_API URTSShortsExecutor : public UMoviePipelineInProcessExecutor
{
	GENERATED_BODY()

public:
	UMoviePipeline* GetShortsPipeline() const;
};

/**
 * @brief Reads -RTSShortsJob=<file> from the command line, builds a Movie Render Queue job for the shorts map
 * entirely at runtime (transient sequence and config) and spawns the director once the map is up.
 * Exits the process when the render finishes so a launcher script can run jobs back to back.
 */
UCLASS()
class RTS_SURVIVAL_API URTSShortsRenderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool GetHasJob() const { return bM_HasJob; }
	const FRTSShortsJob& GetJob() const { return M_Job; }
	UMoviePipeline* GetActivePipeline() const;

private:
	void OnPostLoadMap(UWorld* LoadedWorld);
	bool GetIsShortsMap(const UWorld* World) const;
	void SpawnDirector(UWorld* World);
	void StartRender(UWorld* World);
	void OnExecutorFinished(UMoviePipelineExecutorBase* Executor, bool bSuccess);

	ULevelSequence* BuildSequence() const;
	UMoviePipelinePrimaryConfig* BuildConfig() const;
	float GetTotalSeconds() const;

	bool LoadJobFile(const FString& Path);

	FRTSShortsJob M_Job;
	bool bM_HasJob = false;
	bool bM_RenderStarted = false;

	UPROPERTY()
	TObjectPtr<ULevelSequence> M_Sequence;

	UPROPERTY()
	TObjectPtr<UMoviePipelinePrimaryConfig> M_Config;

	UPROPERTY()
	TObjectPtr<URTSShortsExecutor> M_Executor;

	UPROPERTY()
	TObjectPtr<ARTSShortsDirector> M_Director;

	FTimerHandle M_ExitTimer;
};
