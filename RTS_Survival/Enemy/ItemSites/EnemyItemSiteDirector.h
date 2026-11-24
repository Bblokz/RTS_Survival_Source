// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyItemSiteTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "EnemyItemSiteDirector.generated.h"

class AEnemyItemSiteDirector;
class UEnemyDivisionArmySpawner;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyItemSiteBuilt, AEnemyItemSiteDirector*, SiteDirector);

/**
 * @brief Place one at the enemy start of every handmade enemy map. On an item map it fills the signature and strength
 * slots for the attacked item; on a field map it spawns the attacked division's army. It sets up the enemy AI, and the
 * mission reads GetIsSiteBuilt and GetVictoryTargets. See Source/Docs/Campaign/EnemyItemMaps_Plan.md.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AEnemyItemSiteDirector : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	AEnemyItemSiteDirector();

	/**
	 * @brief Fills the slots (item map) or spawns the division army (field map) for a site context; runs once.
	 * BeginPlay calls it with the world mission context; tests call it directly. An army finishes asynchronously.
	 * @param SiteContext Item type or division army, seed and strength the site is built for.
	 * @return false when the site was already started.
	 */
	bool BuildSite(const FEnemyItemSiteContext& SiteContext);

	// False until the site content (or every unit of a division army) has spawned; zero targets before that is not a
	// win.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	bool GetIsSiteBuilt() const { return bM_IsSiteBuilt; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	FEnemyItemSiteContext GetSiteContext() const { return M_SiteContext; }

	// The spawned signature targets that are still alive (not destroyed, not dying).
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	TArray<AActor*> GetSignatureTargets() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	int32 GetSpawnedSignatureTargetCount() const { return M_SpawnedSignatureTargetCount; }

	// What must die for the win: the living signature targets on an item map, the living army units on a field map.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	TArray<AActor*> GetVictoryTargets() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	int32 GetSpawnedVictoryTargetCount() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Enemy Item Site")
	int32 GetActivatedStrengthSlotCount() const { return M_ActivatedStrengthSlotCount; }

	UPROPERTY(BlueprintAssignable, Category = "Enemy Item Site")
	FOnEnemyItemSiteBuilt OnSiteBuilt;

	virtual FColor GetPlacementColor() const override;

	// Per item type: decision tree and training for the enemy AI. Optional; without it the map's own enemy
	// controller settings are used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Item Site")
	TObjectPtr<UEnemyItemSiteProfiles> Profiles = nullptr;

	// Used whenever the world map did not launch the operation (PIE, a direct level load; the PIE startup
	// settings' mission context is ignored here): which item to build and how strong it is. bFromMissionContext is
	// ignored.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Item Site|Test")
	FEnemyItemSiteContext TestContext;

	// Field maps: how the attacked division stands and what it does. Its units come from the world map, or from
	// TestContext.Army when the map was opened directly.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Item Site|Division Army")
	FEnemyDivisionArmySettings ArmySettings;

	// Starts the enemy strategic AI once the profile is applied, for controllers that wait for a mission trigger.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Item Site")
	bool bStartEnemyAIThinking = true;

	// Editor drawing only: rough size of the enemy base around this actor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Item Site", meta = (ClampMin = "500.0"))
	float BaseRadius = 4000.f;

protected:
	virtual void BeginPlay() override;

	virtual FString GetPlacementKindName() const override { return TEXT("ENEMY SITE"); }
	virtual FName GetPlacementId() const override;
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;

private:
	FEnemyItemSiteContext M_SiteContext;

	UPROPERTY()
	TObjectPtr<UEnemyDivisionArmySpawner> M_ArmySpawner;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SignatureTargets;

	int32 M_SpawnedSignatureTargetCount = 0;
	int32 M_ActivatedStrengthSlotCount = 0;
	// Set when BuildSite runs; the site only counts as built once a division army has finished spawning too.
	bool bM_HasStartedSite = false;
	bool bM_IsSiteBuilt = false;

	bool GetIsValidArmySpawner() const;
	void BuildSiteFromMissionContext();
	void BuildSite_FillSignatureSlots();
	void BuildSite_ActivateStrengthSlots();
	void BuildSite_ReportMissingSignatureTargets() const;
	void BuildSite_SpawnDivisionArmy();
	void OnDivisionArmySpawned(int32 SpawnedUnitCount);
	void FinishBuildingSite();
	void ApplyProfileToEnemyAI(AEnemyController& EnemyController) const;
	bool GetNeedsEnemyAIDecisionTree() const;
	const FEnemyItemSiteProfile* FindProfileForSite() const;
	FString GetSiteItemName() const;
};
