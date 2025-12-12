// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DecalManagerSubsystem.generated.h"

class UGameDecalManager;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UDecalManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
    /** Returns the UGameExplosionsManager from the current ACPPGameState, if valid. */
    UFUNCTION(BlueprintCallable, Category="Explosion Manager")
    UGameDecalManager* GetDecalManager() const { return M_DecalManager; }

    /** Called when the subsystem is initialized for a given UWorld. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Called when the subsystem is de-initialized (e.g., World teardown). */
    virtual void Deinitialize() override;

    void SetDecalManager(UGameDecalManager* NewDecalManager);

private:
    /** Cache the pointer once we find it, so we can return it without repeated lookups. */
    UPROPERTY()
    UGameDecalManager* M_DecalManager;
	
};
