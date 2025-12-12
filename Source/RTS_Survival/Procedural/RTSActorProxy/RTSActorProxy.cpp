// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSActorProxy.h"

#include "RTS_Survival/Environment/InstanceHelpers/FRTSInstanceHelpers.h"
#include "RTS_Survival/Procedural/LandscapeDivider/RTSLandscapeDivider.h"
#include "RTS_Survival/Procedural/LandscapeDivider/SeedSettings/RuntimeSeedManager/RTSRuntimeSeedManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values
ARTSActorProxy::ARTSActorProxy()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void ARTSActorProxy::BeginPlay()
{
	Super::BeginPlay();
	if (not InitializeRuntimeSeedManager())
	{
		RTSFunctionLibrary::ReportError("Actor Proxy failed to initialize runtime seed manager: " + GetName());
		Destroy();
	}
	// Uses a max amount of attempts to get a random seeded class.
	StartLoadRandomActor();
}

void ARTSActorProxy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cancel loading of the asset if it is still loading.
	if (M_AssetLoadingStreamHandle.IsValid())
	{
		M_AssetLoadingStreamHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ARTSActorProxy::BeginDestroy()
{
	if (M_RandomActorPickTimer.IsValid())
	{
		M_RandomActorPickTimer.Invalidate();
	}

	// Cancel loading of the asset if it is still loading.
	if (M_AssetLoadingStreamHandle.IsValid())
	{
		M_AssetLoadingStreamHandle.Reset();
	}
	Super::BeginDestroy();
}

bool ARTSActorProxy::InitializeRuntimeSeedManager()
{
	const auto LandscapeDivider = FRTS_Statics::GetRTSLandscapeDivider(this);
	if (not LandscapeDivider)
	{
		RTSFunctionLibrary::ReportError("Could not find landscape divider in actor proxy: " + GetName());
		return false;
	}
	M_RuntimeSeedManager = LandscapeDivider->GetSeedManager();
	return M_RuntimeSeedManager.IsValid();
}

void ARTSActorProxy::StartLoadRandomActor()
{
	if (not M_RuntimeSeedManager.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Actor proxy cannot pick random actor using runtime seed manager as it is invalid: " + GetName());
		return;
	}
	// Attempt to access seed immediately.
	TSoftClassPtr<AActor> ClassToSpawn = nullptr;
	if (not GetRandomActorOptionSeedManager(ClassToSpawn))
	{
		M_MaxAttemptsGetRandomSeededClass--;
		TWeakObjectPtr<ARTSActorProxy> WeakThis(this);
		auto LambdaGetClass = [WeakThis]() -> void
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			// If attempts are exhausted, use fallback.
			if (WeakThis.Get()->M_MaxAttemptsGetRandomSeededClass <= 0)
			{
				WeakThis.Get()->OnPickActorClassAttemptsExhausted();
				return;
			}

			// Decrement attempts once per try.
			WeakThis.Get()->M_MaxAttemptsGetRandomSeededClass--;

			TSoftClassPtr<AActor> Class = nullptr;
			if (WeakThis.Get()->GetRandomActorOptionSeedManager(Class))
			{
				// Invalidates timer.
				WeakThis.Get()->OnPickedClassToSpawn(Class);
			}
		};

		// Set timer to try again every second.
		GetWorld()->GetTimerManager().SetTimer(M_RandomActorPickTimer, LambdaGetClass, 1.0f, true);
	}
	else
	{
		// We have Immediate access to random seed; spawn class.
		OnPickedClassToSpawn(ClassToSpawn);
	}
}

void ARTSActorProxy::OnPickedClassToSpawn(const TSoftClassPtr<AActor> ClassToSpawn)
{
	M_RandomActorPickTimer.Invalidate();
	if (ClassToSpawn.IsValid())
	{
		// Is already loaded; handle immediately.
		OnAsyncLoadingComplete(ClassToSpawn.ToSoftObjectPath());
		return;
	}
	// Start async load request.
	M_AssetLoadingStreamHandle = M_StreamableManager.RequestAsyncLoad(ClassToSpawn.ToSoftObjectPath(),
	                                                                  FStreamableDelegate::CreateUObject(
		                                                                  this, &ARTSActorProxy::OnAsyncLoadingComplete,
		                                                                  ClassToSpawn.ToSoftObjectPath()));
}

void ARTSActorProxy::OnFailedToPickClassToSpawn()
{
	RTSFunctionLibrary::ReportError("ActorProxy Failed to pick class to spawn: " + GetName());
	M_RandomActorPickTimer.Invalidate();
	Destroy();
}

bool ARTSActorProxy::GetRandomActorOptionSeedManager(TSoftClassPtr<AActor>& OutClass) const
{
	OutClass = nullptr;
	if (OptionsToSpawn.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("Actor proxy has no options to spawn: " + GetName());
		return false;
	}

	if (not M_RuntimeSeedManager.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Actor proxy cannot pick random actor using runtime seed manager as it is invalid: " + GetName());
		OutClass = OptionsToSpawn[FMath::RandRange(0, OptionsToSpawn.Num() - 1)];
		// We do not retry as it would result in getting an invalid manager again.
		return true;
	}
	int32 Option;
	const bool bIsManagerInit = M_RuntimeSeedManager.Get()->GetSeededRandInt(0, OptionsToSpawn.Num() - 1, Option);
	if (not bIsManagerInit)
	{
		return false;
	}
	OutClass = OptionsToSpawn[Option];
	return true;
}

bool ARTSActorProxy::GetRandomActorOptionAnyRandStream(TSoftClassPtr<AActor>& OutClass) const
{
	OutClass = nullptr;
	if (OptionsToSpawn.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("Actor proxy has no options to spawn: " + GetName());
		return false;
	}
	OutClass = OptionsToSpawn[FMath::RandRange(0, OptionsToSpawn.Num() - 1)];
	return true;
}

void ARTSActorProxy::OnPickActorClassAttemptsExhausted()
{
	RTSFunctionLibrary::ReportError(
		"Max attempts to get random seeded class reached: " + GetName() +
		"\nWill revert back to using regular random stream.");

	TSoftClassPtr<AActor> ClassFallback = nullptr;
	if (GetRandomActorOptionAnyRandStream(ClassFallback))
	{
		OnPickedClassToSpawn(ClassFallback);
		return;
	}
	OnFailedToPickClassToSpawn();
}

void ARTSActorProxy::OnAsyncLoadingComplete(const FSoftObjectPath AssetPath)
{
	if (M_AssetLoadingStreamHandle.IsValid())
	{
		M_AssetLoadingStreamHandle.Reset();
	}
	UWorld* World = GetWorld();
	UObject* LoadedAsset = AssetPath.ResolveObject();
	UClass* LoadedClass = Cast<UClass>(LoadedAsset);
	if (not EnsureValidPostLoad(World, LoadedAsset, LoadedClass))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector SpawnLocation = GetActorLocation();
	SpawnLocation.Z = FRTSInstanceHelpers::VisibilityTraceForZOnLandscape(SpawnLocation, World, SpawnLocation.Z);
	AActor* SpawnedActor = World->SpawnActor<AActor>(LoadedClass, SpawnLocation, GetActorRotation(), SpawnParams);
	if (not IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn valid actor for asset: " + AssetPath.ToString() +
			"\n At actor proxy: " + GetName());
		return;
	}
	SpawnedActor->SetActorScale3D(GetActorScale3D());
	Destroy();
}

bool ARTSActorProxy::EnsureValidPostLoad(const UWorld* World, const UObject* LoadedAsset,
                                         const UClass* LoadedClass) const
{
	if (not World || not LoadedAsset || not LoadedClass)
	{
		const FString WorldName = World ? World->GetName() : TEXT("None");
		const FString AssetObjectName = LoadedAsset ? LoadedAsset->GetName() : TEXT("None");
		const FString AssetClassName = LoadedClass ? LoadedClass->GetName() : TEXT("None");
		RTSFunctionLibrary::ReportError(
			"The asset could not be loaded on actor proxy: " + GetName() + " in world: " + WorldName + " Asset: " +
			AssetObjectName + " Class: " + AssetClassName +
			"\n See RTSActorProxy::EnsureValidPostLoad");
		return false;
	}
	return true;
}
