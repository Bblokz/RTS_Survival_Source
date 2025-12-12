#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExplosionManagerSubsystem.generated.h"

class UGameExplosionsManager;

/**
 * A World Subsystem that provides global access to the Explosion Manager.
 * It looks up the ACPPGameState and retrieves its UGameExplosionsManager pointer.
 */
UCLASS()
class RTS_SURVIVAL_API UExplosionManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Returns the UGameExplosionsManager from the current ACPPGameState, if valid. */
    UFUNCTION(BlueprintCallable, Category="Explosion Manager")
    UGameExplosionsManager* GetExplosionManager() const { return ExplosionManager; }

    /** Called when the subsystem is initialized for a given UWorld. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Called when the subsystem is de-initialized (e.g., World teardown). */
    virtual void Deinitialize() override;

    void SetExplosionManager(UGameExplosionsManager* NewExplosionManager);

private:
    /** Cache the pointer once we find it, so we can return it without repeated lookups. */
    UPROPERTY()
    UGameExplosionsManager* ExplosionManager;
};
