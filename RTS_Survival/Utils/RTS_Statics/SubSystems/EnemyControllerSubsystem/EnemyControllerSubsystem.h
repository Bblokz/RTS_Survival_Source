// EnemyControllerSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyControllerSubsystem.generated.h"

class AEnemyController;

/**
 * A World Subsystem that provides global access to the Enemy Controllers: one per AI player.
 * Caches the pointer when a controller initializes so other actors can retrieve it by the player it plays.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyControllerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Returns the default AEnemyController from the world, if valid: the one of the first AI player, or the
     * lowest-numbered registered one when that player has none.
     */
    UFUNCTION(BlueprintCallable, Category="Enemy Controller")
    AEnemyController* GetEnemyController() const;

    /** Returns the controller that plays this AI player (2..4), or null when the map has none for it. */
    UFUNCTION(BlueprintCallable, Category="Enemy Controller")
    AEnemyController* GetEnemyControllerForPlayer(int32 Player) const;

    /** Returns every registered controller, lowest player number first. */
    UFUNCTION(BlueprintCallable, Category="Enemy Controller")
    TArray<AEnemyController*> GetAllEnemyControllers() const;

    /** Called when the subsystem is initialized for a given UWorld. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Called when the subsystem is de-initialized (e.g., World teardown). */
    virtual void Deinitialize() override;

    /**
     * @brief Called by the controller to register itself under the player it plays.
     * A second controller for a player that already has a live one is reported and not registered.
     */
    void SetEnemyController(AEnemyController* NewEnemyController);

private:
    // Indexed by player number; a slot is empty until the controller of that player registers. The controllers
    // belong to the level, so a destroyed or unloaded one simply reads as empty again.
    UPROPERTY()
    TArray<TWeakObjectPtr<AEnemyController>> M_EnemyControllersByPlayer;

    void EnsurePlayerSlots();
};
