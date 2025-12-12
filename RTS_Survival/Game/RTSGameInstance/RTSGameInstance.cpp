#include "RTSGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Music/RTSMusicManager/RTSMusicManager.h"

void URTSGameInstance::Init()
{
	Super::Init();

	// Bind to level loading delegates
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &URTSGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &URTSGameInstance::EndLoadingScreen);
	bIsLoadingProfileMap = true;

	// Create (but don’t configure) our music manager
	MusicManager = NewObject<URTSMusicManager>(this, URTSMusicManager::StaticClass());
	SetupMusicManager();
}

void URTSGameInstance::Shutdown()
{
    if (MusicManager)
    {
        MusicManager->TeardownForOldWorld();
        // no RemoveFromRoot needed since we never called AddToRoot()
    }
    Super::Shutdown();
}


void URTSGameInstance::BeginLoadingScreen(const FString& MapName)
{
}

void URTSGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
}

void URTSGameInstance::StopLoadingScreen()
{
}
