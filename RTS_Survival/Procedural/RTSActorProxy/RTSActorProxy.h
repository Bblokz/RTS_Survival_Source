// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/DeveloperSettings.h"

#include "RTSActorProxy.generated.h"

class URTSRuntimeSeedManager;
struct FStreamableHandle;

UCLASS()
class RTS_SURVIVAL_API ARTSActorProxy : public AActor
{
	GENERATED_BODY()

public:
	ARTSActorProxy();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	// Array of options to spawn. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<const TSoftClassPtr<AActor>> OptionsToSpawn;

private:

	bool InitializeRuntimeSeedManager();
	TWeakObjectPtr<URTSRuntimeSeedManager> M_RuntimeSeedManager;

	// Used to load assets asynchronously.
	FStreamableManager M_StreamableManager;

	void StartLoadRandomActor();
	void OnPickedClassToSpawn(const TSoftClassPtr<AActor> ClassToSpawn);
	void OnFailedToPickClassToSpawn();

	int32 M_MaxAttemptsGetRandomSeededClass = DeveloperSettings::Procedural::RTSActorProxyMaxAttemptsGetRandomSeededClass;
	FTimerHandle M_RandomActorPickTimer;

	TSharedPtr<FStreamableHandle> M_AssetLoadingStreamHandle;
	

	/**
	 * @brief Attempts to pick a random actor using the seed manager, if the seed manager is not initialized it returns false
	 * and we should try again later
	 * @param OutClass The class to spawn. 
	 * @return true if a random actor was picked, false otherwise.
	 */
	bool GetRandomActorOptionSeedManager(TSoftClassPtr<AActor>& OutClass) const;

	/**
	 * @brief Falls back to default random streams to pick a class to spawn.
	 * @param OutClass The class to spawn
	 * @return Whether we could successfully pick a class with any random stream.
	 */
	bool GetRandomActorOptionAnyRandStream(TSoftClassPtr<AActor>& OutClass) const;

	/**
	 * @brief When we tried enough times to use the random seed system to pick a class and failed;
	 * then use any random stream.
	 * @post The M_RandomActorPickTimer is invalidated.
	 */
	void OnPickActorClassAttemptsExhausted();

	void OnAsyncLoadingComplete(
		const FSoftObjectPath AssetPath);

	bool EnsureValidPostLoad(const UWorld* World, const UObject* LoadedAsset, const UClass* LoadedClass) const;
};
