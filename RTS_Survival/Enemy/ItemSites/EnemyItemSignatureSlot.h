// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyItemSiteTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "EnemyItemSignatureSlot.generated.h"

/**
 * @brief Place on a spot of a hand-placed enemy base where item content goes (hangar, strip, tank hall); empty in
 * game until AEnemyItemSiteDirector fills it for the attacked item from its rows, its ContentSet, then the fillers.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyItemSignatureSlot : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyItemSignatureSlot();

	/**
	 * @brief Spawns this slot's content for the item type once, as the slot's enemy player.
	 * @param ItemType Item the site is built for.
	 * @param OutSpawnedContent The spawned actor; null when the slot has nothing for this item or the spawn failed.
	 * @param bOutIsSignatureTarget Whether the spawned actor counts toward the victory.
	 * @return true when an actor was spawned. A missing entry is normal and not reported; a failed spawn is.
	 */
	bool TryFillForItem(EMapEnemyItem ItemType, AActor*& OutSpawnedContent, bool& bOutIsSignatureTarget);

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Signature Slot")
	AActor* GetSpawnedContent() const { return M_SpawnedContent.Get(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Signature Slot")
	bool GetIsFilled() const { return bM_IsFilled; }

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

	// Shown above the slot in the editor and in logs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Signature Slot")
	FName SlotId = NAME_None;

	// Shared per-item table for this kind of spot; the entries below win over it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Signature Slot")
	TObjectPtr<UEnemyItemSlotContentSet> ContentSet = nullptr;

	// What this slot spawns per attacked item type.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Signature Slot")
	TMap<EMapEnemyItem, FEnemyItemSlotContent> ContentByItem;

	// Spawned for item types without an entry here or in the ContentSet. Not a victory target by default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Signature Slot")
	FEnemyItemSlotContent FillerContent;

	// Editor drawing only: half size of the footprint box, so slots can be spaced without the meshes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Signature Slot", meta = (ClampMin = "50.0"))
	FVector2D FootprintHalfExtent = FVector2D(800.f, 800.f);

protected:
	virtual FString GetPlacementKindName() const override { return TEXT("SIGNATURE"); }
	virtual FName GetPlacementId() const override { return SlotId; }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual bool GetNeedsEnemyController() const override { return false; }

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedContent = nullptr;

	bool bM_IsFilled = false;

	AActor* SpawnContent(const FEnemyItemSlotContent& Content) const;
};
