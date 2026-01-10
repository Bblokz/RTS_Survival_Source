#pragma once

#include "CoreMinimal.h"

#include "StrategicAIRequests.generated.h"

USTRUCT()
struct FWeakActorLocations
{
	GENERATED_BODY()

	FWeakActorLocations();

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	TArray<FVector> Locations;
};

USTRUCT()
struct FFindClosestFlankableEnemyHeavy
{
	GENERATED_BODY()

	FFindClosestFlankableEnemyHeavy();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	FVector StartSearchLocation;

	UPROPERTY()
	int32 MaxHeavyTanksToFlank;

	UPROPERTY()
	int32 MaxSuggestedFlankPositionsPerTank;

	UPROPERTY()
	float DeltaYawFromLeftRight;

	UPROPERTY()
	float MinDistanceToTank;

	UPROPERTY()
	float MaxDistanceToTank;

	UPROPERTY()
	float FlankingPositionsSpreadScaler;
};

USTRUCT()
struct FResultClosestFlankableEnemyHeavy
{
	GENERATED_BODY()

	FResultClosestFlankableEnemyHeavy();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	TArray<FWeakActorLocations> Locations;
};

USTRUCT()
struct FGetPlayerUnitCountsAndBase
{
	GENERATED_BODY()

	FGetPlayerUnitCountsAndBase();

	UPROPERTY()
	int32 RequestID;
};

USTRUCT()
struct FResultPlayerUnitCounts
{
	GENERATED_BODY()

	FResultPlayerUnitCounts();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	int32 PlayerLightTanks;

	UPROPERTY()
	int32 PlayerMediumTanks;

	UPROPERTY()
	int32 PlayerHeavyTanks;

	UPROPERTY()
	int32 PlayerSquads;

	UPROPERTY()
	int32 PlayerNomadicVehicles;

	UPROPERTY()
	FVector PlayerHQLocation;

	UPROPERTY()
	TArray<FVector> PlayerResourceBuildings;
};

USTRUCT()
struct FFindAlliedTanksToRetreat
{
	GENERATED_BODY()

	FFindAlliedTanksToRetreat();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	int32 MaxTanksToFind;

	UPROPERTY()
	float MaxDistanceFromOtherGroupMembers;

	UPROPERTY()
	int32 MaxIdleHazmatsToConsider;

	UPROPERTY()
	float MaxHealthRatioConsiderUnitToRetreat;

	UPROPERTY()
	float RetreatFormationDistance;

	UPROPERTY()
	int32 MaxFormationWidth;
};

USTRUCT()
struct FDamagedTanksRetreatGroup
{
	GENERATED_BODY()

	FDamagedTanksRetreatGroup();

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> DamagedTanks;

	UPROPERTY()
	TArray<FWeakActorLocations> HazmatsWithFormationLocations;
};

USTRUCT()
struct FResultAlliedTanksToRetreat
{
	GENERATED_BODY()

	FResultAlliedTanksToRetreat();

	UPROPERTY()
	int32 RequestID;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group1;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group2;

	UPROPERTY()
	FDamagedTanksRetreatGroup Group3;
};

USTRUCT()
struct FStrategicAIRequestBatch
{
	GENERATED_BODY()

	FStrategicAIRequestBatch();

	UPROPERTY()
	TArray<FFindClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyRequests;

	UPROPERTY()
	TArray<FGetPlayerUnitCountsAndBase> GetPlayerUnitCountsAndBaseRequests;

	UPROPERTY()
	TArray<FFindAlliedTanksToRetreat> FindAlliedTanksToRetreatRequests;
};

USTRUCT()
struct FStrategicAIResultBatch
{
	GENERATED_BODY()

	FStrategicAIResultBatch();

	UPROPERTY()
	TArray<FResultClosestFlankableEnemyHeavy> FindClosestFlankableEnemyHeavyResults;

	UPROPERTY()
	TArray<FResultPlayerUnitCounts> PlayerUnitCountsResults;

	UPROPERTY()
	TArray<FResultAlliedTanksToRetreat> AlliedTanksToRetreatResults;
};
