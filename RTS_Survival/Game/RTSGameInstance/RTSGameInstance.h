#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "RTSGameInstance.generated.h"


class URTSMusicManager;

UCLASS()
class RTS_SURVIVAL_API URTSGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** Blueprint can grab this and call InitMusicManager(...) on it. */
    UFUNCTION(BlueprintCallable, Category="Music")
    URTSMusicManager* GetMusicManager() const { return MusicManager; }

protected:
    // Called on init when the music manager is setup and needs to be initialized.
    UFUNCTION(BlueprintImplementableEvent, Category="Music")
    void SetupMusicManager();

    virtual void Shutdown() override;

private:
    /** Our persistent music player. */
    UPROPERTY()
    URTSMusicManager* MusicManager;

    void BeginLoadingScreen(const FString& MapName);
    void EndLoadingScreen(UWorld* LoadedWorld);

    void StopLoadingScreen();

    FTimerHandle LoadingScreenTimerHandle;

    // This is the map we start at, do not wait the extra seconds.
    bool bIsLoadingProfileMap;

};
