// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapPlayerItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/FPlayerProfileSaveData.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "FWorldCampaignState.generated.h"

/**
 * @brief Persistent anchor data used to reconstruct generated campaign map nodes.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignAnchorSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid AnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FTransform Transform = FTransform::Identity;
};

/**
 * @brief Persistent connection data used to restore graph topology between saved anchors.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignConnectionSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FGuid> ConnectedAnchorKeys;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FVector JunctionLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	bool bM_IsThreeWayConnection = false;
};

/**
 * @brief Persistent map item data used to restore promoted world objects on anchors.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignMapItemSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid AnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapItemType MapItemType = EMapItemType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapEnemyItem EnemyItemType = EMapEnemyItem::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapMission MissionType = EMapMission::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapNeutralObjectType NeutralObjectType = EMapNeutralObjectType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapPlayerItem PlayerItemType = EMapPlayerItem::None;

	/**
	 * @brief Cached base fortification strength percentage for enemy/mission map items.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	float BaseFortificationStrength = 0.f;

	/**
	 * @brief Fortification modifier enums restored into UWorldFortificationModificationsComponent.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<EWorldFortificationStrength> FortificationStrengthModifiers;

	/**
	 * @brief Distinguishes a newly saved empty modifier list from an older save that did not serialize this array.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	bool bHasSavedFortificationStrengthModifiers = false;
};

/**
 * @brief Persistent mobile division data restored separately from generated anchors.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDivisionSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid DivisionKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EWorldFieldDivisions DivisionType = EWorldFieldDivisions::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 OwningPlayer = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentStrengthPercentage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FVector> PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentPathPointIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	bool bHasPendingMoveOrder = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ETankSubtype> TankSubtypes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ESquadSubtype> SquadSubtypes;
};

/**
 * @brief Save-only state for rebuilding the world campaign map independently from player UI/profile data.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentTurn = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 WorldGenerationSeed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 EnemyObjectProceduralMapIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ECampaignGenerationStep GenerationStep = ECampaignGenerationStep::Finished;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid PlayerHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid EnemyHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldCampaignAnchorSaveData> Anchors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldCampaignConnectionSaveData> Connections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldCampaignMapItemSaveData> MapItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldDivisionSaveData> WorldDivisions;

	bool GetIsValidForRestore() const;
};

/**
 * @brief Single save payload used so world state and player profile data are committed atomically.
 */
UCLASS()
class RTS_SURVIVAL_API USaveWorldCampaign : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWorldCampaignState WorldCampaignState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerProfileSaveData PlayerProfileSaveData;
};
