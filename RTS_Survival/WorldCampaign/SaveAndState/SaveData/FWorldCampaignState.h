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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	float BaseDifficulty = 0.f;
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
