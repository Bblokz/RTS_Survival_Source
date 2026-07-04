// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/FPlayerProfileSaveData.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "WorldStateAndSaveManager.generated.h"

class AGeneratorWorldCampaign;
class AWorldPlayerController;

/**
 * @brief Owned by the world player controller to keep save-ready campaign and player state in sync.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldStateAndSaveManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldStateAndSaveManager();

	void CacheCurrentWorldState(const AGeneratorWorldCampaign& WorldGenerator);
	void CachePlayerProfileSaveData(const FPlayerProfileSaveData& PlayerProfileSaveData);

	/**
	 * @brief Updates only the division slice of cached world state before saving.
	 * @param WorldDivisionSaveData Current live division state built by UWorldDivisionManager.
	 */
	void CacheWorldDivisionSaveData(const TArray<FWorldDivisionSaveData>& WorldDivisionSaveData);

	UFUNCTION(BlueprintCallable)
	bool SaveCampaignState();
	bool LoadCampaignState(FWorldCampaignState& OutWorldCampaignState, FPlayerProfileSaveData& OutPlayerProfileSaveData);

	const FWorldCampaignState& GetCachedWorldCampaignState() const { return M_WorldCampaignState; }
	const FPlayerProfileSaveData& GetCachedPlayerProfileSaveData() const { return M_PlayerProfileSaveData; }

private:
	FWorldCampaignState AggregateWorldCampaignState() const;
	FPlayerProfileSaveData AggregatePlayerSaveState() const;
	bool SaveMainCampaignSlot(const FWorldCampaignState& WorldCampaignState,
	                          const FPlayerProfileSaveData& PlayerProfileSaveData) const;
	bool SaveBackupCampaignSlot() const;
	USaveWorldCampaign* CreateSavePayload(const FWorldCampaignState& WorldCampaignState,
	                                     const FPlayerProfileSaveData& PlayerProfileSaveData) const;
	FString GetMainSaveFilePath() const;
	FString GetBackupSaveFilePath() const;
	bool EnsureBackupDirectoryExists() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World Campaign|Save", meta = (AllowPrivateAccess = "true"))
	FWorldCampaignState M_WorldCampaignState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World Campaign|Save", meta = (AllowPrivateAccess = "true"))
	FPlayerProfileSaveData M_PlayerProfileSaveData;
};
