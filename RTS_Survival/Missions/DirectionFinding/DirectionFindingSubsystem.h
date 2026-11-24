// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DirectionFindingSubsystem.generated.h"

class ADirectionFindingManager;

/**
 * @brief Lets the minimap and missions reach the map's direction-finding manager without searching the world; the
 * manager registers itself at BeginPlay.
 */
UCLASS()
class RTS_SURVIVAL_API UDirectionFindingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterManager(ADirectionFindingManager* Manager);
	void UnregisterManager(const ADirectionFindingManager* Manager);

	/** @return The map's direction-finding manager, or null on maps without one. */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	ADirectionFindingManager* GetDirectionFindingManager() const { return M_Manager.Get(); }

private:
	UPROPERTY()
	TWeakObjectPtr<ADirectionFindingManager> M_Manager;
};
