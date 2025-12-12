// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "ScavObjectAsyncLoader.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values
AScavObjectAsyncLoader::AScavObjectAsyncLoader(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AScavObjectAsyncLoader::BeginPlay()
{
    Super::BeginPlay();

    // Create a delegate for the callback function when the class has been loaded
    FStreamableDelegate OnScavObjectLoadedDelegate = FStreamableDelegate::CreateUObject(
        this, &AScavObjectAsyncLoader::OnScavObjectLoaded);

    // Set a timer to wait for 2 seconds before trying to load the object
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, OnScavObjectLoadedDelegate]()
    {
        if (ScavObjectToLoad.IsValid())
        {
            // Class is already loaded, so call the delegate immediately
            OnScavObjectLoadedDelegate.ExecuteIfBound();
        }
        else
        {
            // Load the class asynchronously and bind the delegate to be called when loading is complete
            UAssetManager::GetStreamableManager().RequestAsyncLoad(ScavObjectToLoad.ToSoftObjectPath(), OnScavObjectLoadedDelegate);
        }
    }, 2.0f, false);
}

void AScavObjectAsyncLoader::OnScavObjectLoaded()
{
    // Check if the class is loaded successfully
    if (ScavObjectToLoad.IsValid())
    {
        // Spawn the scavenging object at the loader's location
        FVector SpawnLocation = GetActorLocation();
        FRotator SpawnRotation = GetActorRotation();
        
        FActorSpawnParameters SpawnParams;
        AScavengeableObject* SpawnedObject = GetWorld()->SpawnActor<AScavengeableObject>(
            ScavObjectToLoad.Get(), SpawnLocation, SpawnRotation, SpawnParams);

        // Destroy this loader actor as it has completed its task
        Destroy();
    }
    else
    {
        RTSFunctionLibrary::PrintString("Failed to load scavenging object class!");
        Destroy();
    }
}

