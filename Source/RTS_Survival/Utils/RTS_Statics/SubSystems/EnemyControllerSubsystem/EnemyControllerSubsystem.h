// EnemyControllerSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyControllerSubsystem.generated.h"

class AEnemyController;

/**
 * A World Subsystem that provides global access to the Enemy Controller.
 * Caches the pointer when the controller initializes so other actors can retrieve it.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyControllerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** Returns the AEnemyController from the world, if valid. */
    UFUNCTION(BlueprintCallable, Category="Enemy Controller")
    AEnemyController* GetEnemyController() const { return M_EnemyController; }

    /** Called when the subsystem is initialized for a given UWorld. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Called when the subsystem is de-initialized (e.g., World teardown). */
    virtual void Deinitialize() override;

    /** Called by the controller to register itself. */
    void SetEnemyController(AEnemyController* NewEnemyController);

private:
    /** Cached pointer to the one-and-only enemy controller in this world. */
    UPROPERTY()
    AEnemyController* M_EnemyController = nullptr;
};
