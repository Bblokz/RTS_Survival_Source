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
	 * @brief Replaces only the card slice of the cached player profile; the deck builder hands its deck over here
	 * when it closes, so the cards never live in the menu.
	 * @param PlayerCardData The deck as the card menu left it.
	 */
	void CachePlayerCardData(const FPlayerCardSaveData& PlayerCardData);

	/**
	 * @brief Updates only the division slice of cached world state before saving.
	 * @param WorldDivisionSaveData Current live division state built by UWorldDivisionManager.
	 */
	void CacheWorldDivisionSaveData(const TArray<FWorldDivisionSaveData>& WorldDivisionSaveData);

	UFUNCTION(BlueprintCallable)
	bool SaveCampaignState();
	bool LoadCampaignState(FWorldCampaignState& OutWorldCampaignState, FPlayerProfileSaveData& OutPlayerProfileSaveData);
	int32 AdvanceCurrentTurn();
	int32 GetCurrentTurn() const;
	int32 GetWorldGenerationSeed() const;
	int32 GetEnemyObjectProceduralMapIndex() const;
	void ResetEnemyObjectProceduralMapIndex();
	void SetEnemyObjectProceduralMapIndex(int32 NewIndex);
	int32 AdvanceEnemyObjectProceduralMapIndex(int32 MapCount);

	const FWorldCampaignState& GetCachedWorldCampaignState() const { return M_WorldCampaignState; }

	/**
	 * @brief Whether a campaign save exists on disk, so menus and the world map can offer or take the load path.
	 * @return true when the main campaign slot exists.
	 */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Save")
	static bool GetDoesCampaignSaveExist();

	/**
	 * @brief Points every later save and load at another slot; tests use it so they never touch the real campaign.
	 * @param SlotName Slot to use, or an empty string to go back to the real campaign slot.
	 */
	static void SetCampaignSaveSlotNameOverride(const FString& SlotName);

	static FString GetCampaignSaveSlotName();
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
