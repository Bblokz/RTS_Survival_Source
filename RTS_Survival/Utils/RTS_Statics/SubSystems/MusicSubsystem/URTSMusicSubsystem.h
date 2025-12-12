#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "URTSMusicSubsystem.generated.h"

class URTSMusicManager;
class URTSGameInstance;

/**
 * A lightweight subsystem to give you easy Blueprint/C++ access to
 * the URTSMusicManager owned by your RTSGameInstance.
 */
UCLASS()
class RTS_SURVIVAL_API URTSMusicSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Cache a pointer to your RTSGameInstance on startup. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Clear out pointers on shutdown. */
    virtual void Deinitialize() override;

    /** 
     * Returns the existing music manager from the GameInstance.
     * Reports an error if something is null or mis‐typed.
     */
    UFUNCTION(BlueprintCallable, Category="Music")
    URTSMusicManager* GetMusicManager() const;

private:
    /** To avoid repeated casts, we cache this. */
    UPROPERTY()
    URTSGameInstance* RTSGameInstance = nullptr;
};
