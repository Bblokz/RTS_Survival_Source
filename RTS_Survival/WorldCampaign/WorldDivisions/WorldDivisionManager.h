// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Turn/WorldTurnType.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "WorldDivisionManager.generated.h"

class AGeneratorWorldCampaign;
class AWorldDivisionBase;
class AWorldPlayerController;
class UWorldDataComponent;

/**
 * @brief Controller-owned runtime system for spawning, moving, saving, and applying world divisions.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldDivisionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDivisionManager();

	void InitializeNewCampaignDivisions(AWorldPlayerController* WorldPlayerController,
	                                    AGeneratorWorldCampaign* WorldGenerator,
	                                    ERTSGameDifficulty GameDifficulty);
	void RestoreSavedCampaignDivisions(AWorldPlayerController* WorldPlayerController,
	                                   AGeneratorWorldCampaign* WorldGenerator,
	                                   ERTSGameDifficulty GameDifficulty,
	                                   const TArray<FWorldDivisionSaveData>& DivisionSaveData);

	void MovePlayerDivisions(ERTSGameDifficulty GameDifficulty);
	void MoveEnemyDivisions(ERTSGameDifficulty GameDifficulty);
	void RefreshDivisionInfluence(ERTSGameDifficulty GameDifficulty);
	void CacheDivisionSaveState();

	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueMoveOrderToDivision(AWorldDivisionBase* WorldDivision, const FVector& TargetLocation);

	TArray<FWorldDivisionSaveData> BuildWorldDivisionSaveData() const;
	const TArray<TObjectPtr<AWorldDivisionBase>>& GetWorldDivisions() const { return M_WorldDivisions; }

private:
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TWeakObjectPtr<UWorldDataComponent> M_WorldDataComponent;

	UPROPERTY()
	TArray<TObjectPtr<AWorldDivisionBase>> M_WorldDivisions;

	EWorldTurnType M_ActiveMovingTurn = EWorldTurnType::None;
	ERTSGameDifficulty M_CurrentGameDifficulty = ERTSGameDifficulty::Normal;
	ERTSGameDifficulty M_ActiveMovementDifficulty = ERTSGameDifficulty::Normal;
	int32 M_ActiveTurnMovementCount = 0;

	bool CacheRuntimeReferences(AWorldPlayerController* WorldPlayerController,
	                            AGeneratorWorldCampaign* WorldGenerator);
	void DestroyRegisteredDivisions();
	void SpawnInitialDivisions(ERTSGameDifficulty GameDifficulty);
	void SpawnSavedDivisions(ERTSGameDifficulty GameDifficulty,
	                         const TArray<FWorldDivisionSaveData>& DivisionSaveData);
	AWorldDivisionBase* SpawnDivision(EWorldFieldDivisions DivisionType, const FTransform& SpawnTransform) const;
	void RegisterDivision(AWorldDivisionBase* WorldDivision);
	TSubclassOf<AWorldDivisionBase> ResolveDivisionClass(EWorldFieldDivisions DivisionType) const;
	void InitializeDivisionFromWorldData(AWorldDivisionBase& WorldDivision,
	                                     const FGuid& DivisionKey,
	                                     EWorldFieldDivisions DivisionType,
	                                     int32 OwningPlayer,
	                                     ERTSGameDifficulty GameDifficulty) const;
	void MoveDivisionsForOwner(int32 OwningPlayer, EWorldTurnType TurnType, ERTSGameDifficulty GameDifficulty);
	void OnDivisionMovementFinished(AWorldDivisionBase* WorldDivision);
	void OnDivisionStrengthChanged(AWorldDivisionBase* WorldDivision);
	void FinishActiveTurnMovement();
	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidWorldGenerator() const;
	bool GetIsValidWorldDataComponent() const;
};
