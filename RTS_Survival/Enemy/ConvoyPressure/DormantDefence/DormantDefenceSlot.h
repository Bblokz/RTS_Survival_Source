// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"

#include "DormantDefenceSlot.generated.h"

class ADormantDefenceSlot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDormantDefenceSlotActivated, ADormantDefenceSlot*, Slot,
                                             AActor*, SpawnedDefence);

/**
 * @brief Placed in a mission map where an extra enemy defence (bunker, AT pit, vehicle) appears later. Nothing of it
 * exists in game until Activate spawns DefenceClass at the slot's transform for the slot's enemy player, so a dormant
 * defence is never seen, targeted, counted on the minimap or in a kill mission. The convoy pressure director
 * activates slots for escaped cargo; a mission can call Activate itself.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ADormantDefenceSlot : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	ADormantDefenceSlot();

	/**
	 * @brief Spawns the defence once; later calls return what the first one spawned.
	 * @return The spawned defence, or null when the class is not set or could not spawn (reported).
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Dormant Defence")
	AActor* Activate();

	// True after the first Activate, also when that spawn failed or the defence has died since.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Dormant Defence")
	bool GetIsActivated() const { return bM_IsActivated; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Dormant Defence")
	AActor* GetSpawnedDefence() const { return M_SpawnedDefence.Get(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Dormant Defence")
	FName GetSlotId() const { return SlotId; }

	UPROPERTY(BlueprintAssignable, Category = "Dormant Defence")
	FOnDormantDefenceSlotActivated OnActivated;

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

protected:
	virtual FString GetPlacementKindName() const override { return TEXT("DORMANT"); }
	virtual FName GetPlacementId() const override { return SlotId; }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	// Shown above the slot in the editor and in logs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dormant Defence")
	FName SlotId = NAME_None;

	// The unit or building Blueprint that appears; it is spawned as the slot's enemy player.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dormant Defence")
	TSoftClassPtr<AActor> DefenceClass;

	// Editor drawing only: half size of the footprint box, so slots can be spaced without the mesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dormant Defence", meta = (ClampMin = "50.0"))
	FVector2D FootprintHalfExtent = FVector2D(400.f, 400.f);

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedDefence = nullptr;

	bool bM_IsActivated = false;

	AActor* SpawnDefence() const;
};
