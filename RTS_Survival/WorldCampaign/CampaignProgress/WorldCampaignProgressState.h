// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitTypes.h"
#include "WorldCampaignProgressState.generated.h"

/**
 * @brief Buildings the player can place on a country's build nodes. Append only: saved by value.
 */
UENUM(BlueprintType)
enum class EWorldCampaignBuilding : uint8
{
	None,
	/** Reveals world-map fog in a radius. */
	WatchPost,
	/** Pays radixite and blueprints at the start of every player turn. */
	SupplyDepot,
	/** Lowers enemy strength for operations in range (air support). */
	Airfield,
	/** Lowers enemy strength for operations in range (artillery support). */
	Battery
};

/**
 * @brief What kind of target the player's operation for this turn was launched on.
 */
UENUM(BlueprintType)
enum class EWorldOperationTarget : uint8
{
	None,
	/** A story or side mission on an anchor. */
	Mission,
	/** An enemy map item on an anchor. */
	EnemyItem,
	/** A moving enemy division or convoy. */
	EnemyDivision
};

/**
 * @brief The two currencies the player spends on the world map between operations.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldMapResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Radixite = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 Blueprints = 0;

	bool GetCanAfford(const FWorldMapResources& Cost) const
	{
		return Radixite >= Cost.Radixite && Blueprints >= Cost.Blueprints;
	}

	void Add(const FWorldMapResources& Amount)
	{
		Radixite += Amount.Radixite;
		Blueprints += Amount.Blueprints;
	}

	void Subtract(const FWorldMapResources& Amount)
	{
		Radixite = FMath::Max(0, Radixite - Amount.Radixite);
		Blueprints = FMath::Max(0, Blueprints - Amount.Blueprints);
	}
};

/**
 * @brief The operation the player launched and has not yet returned from. Saved before the mission level opens so a
 * returning mission result can be matched to the map target it belongs to.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldPendingOperation
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	bool bIsPending = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EWorldOperationTarget Target = EWorldOperationTarget::None;

	/** Anchor of the mission or enemy item; invalid for a division target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid AnchorKey;

	/** Division key when Target is EnemyDivision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FGuid DivisionKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EMapMission MissionType = EMapMission::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LaunchedOnTurn = 0;
};

/**
 * @brief One building standing on a build node. Build nodes are hand-placed and found again by their id.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldBuildNodeSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FName BuildNodeId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	EWorldCampaignBuilding Building = EWorldCampaignBuilding::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 BuiltOnTurn = 0;
};

/**
 * @brief Enemy-side campaign state: how alarmed the enemy is and how far the superweapon has come.
 * @note Owned by UWorldEnemyTurnDirector; fields may be appended there.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldEnemyTurnState
{
	GENERATED_BODY()

	/** 0 = the enemy ignores the player, 1 = every point goes to reaction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	float Alert = 0.f;

	/** Points paid into the freeze so far. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 FreezeProgressPoints = 0;

	/** Finished freeze stages; what a stage does is not designed yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 FreezeStage = 0;

	/** Reaction points the enemy could not spend yet (no convoy or division was affordable). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 BankedReactionPoints = 0;

	/** Anchors the player struck since the last enemy turn; the reaction is sent toward them. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FGuid> RecentlyStruckAnchorKeys;

	/** Enemy turns played, used for deterministic enemy choices. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 EnemyTurnsPlayed = 0;

	/** Target anchor of every convoy or division the enemy sent, keyed by the piece's division key. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TMap<FGuid, FGuid> ReactionTargetAnchorByPieceKey;

	/** MissionsCompleted when the last enemy turn ended; a higher count means the player struck since then. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 MissionsCompletedAtLastEnemyTurn = 0;

	/** Where the player destroyed enemy divisions or convoys since the last enemy turn; pieces have no anchor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FVector> RecentlyStruckLocations;

	/** Last enemy turn, kept for the UI summary. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnReactionPoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnFreezePoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnConvoysSpawned = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnDivisionsSpawned = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnConvoysDelivered = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 LastTurnFreezeStagesGained = 0;
};

/**
 * @brief The commander's world-map side: who was picked and when the map ability is ready again.
 * @note Owned by UWorldCommanderMapKit; fields may be appended there.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderMapState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSCommander Commander = ERTSCommander::NoCommander;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 MapAbilityCooldownTurnsRemaining = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 MapAbilityUses = 0;

	/** Map ability effects that last several turns; counted down at the start of every player turn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldCommanderTimedEffect> ActiveTimedEffects;
};

/**
 * @brief Everything the campaign remembers beyond the generated map: what was won, what was bought and what the
 * enemy is up to. Lives inside FWorldCampaignState so it is saved in the same write.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignProgressState
{
	GENERATED_BODY()

	/** Won operations of any kind; drives rank and the global ability tier cap. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 MissionsCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 MissionsLost = 0;

	/** Anchors whose mission was won or whose enemy item was destroyed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FGuid> ClearedAnchorKeys;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<EMapMission> CompletedMissions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWorldMapResources Resources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ETechnology> ResearchedTechnologies;

	/** Nodes bought on the tech wheel, minor nodes included; a technology node also adds to ResearchedTechnologies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FName> BoughtTechNodeIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FWorldBuildNodeSaveData> Buildings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWorldEnemyTurnState EnemyTurn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWorldCommanderMapState CommanderMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FWorldPendingOperation PendingOperation;

	/** Difficulty picked at campaign start, so a campaign continued after a restart plays on the same setting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FRTSGameDifficulty Difficulty;

	bool GetIsAnchorCleared(const FGuid& AnchorKey) const
	{
		return ClearedAnchorKeys.Contains(AnchorKey);
	}

	EWorldCampaignBuilding GetBuildingOnNode(const FName BuildNodeId) const
	{
		for (const FWorldBuildNodeSaveData& BuildNodeSaveData : Buildings)
		{
			if (BuildNodeSaveData.BuildNodeId == BuildNodeId)
			{
				return BuildNodeSaveData.Building;
			}
		}
		return EWorldCampaignBuilding::None;
	}
};
