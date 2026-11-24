// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldEnemyTurnSettings.generated.h"

/**
 * @brief Roster and fallback strength of one kind of enemy reaction piece spawned during an enemy turn.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldEnemyReactionPieceSetup
{
	GENERATED_BODY()

	/** Roster for tank divisions; empty keeps the actor class defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TArray<ETankSubtype> TankSubtypes;

	/** Roster for squad divisions; empty keeps the actor class defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TArray<ESquadSubtype> SquadSubtypes;

	/** Strength used when WorldData has no field-division entry for this type, so the piece still matters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	int32 FallbackMaxStrengthPercentage = 0;
};

/**
 * @brief Tunable numbers of the enemy reaction that WorldCampaignRules does not own. Plain data so the planning
 * functions in WorldEnemyTurnPlanning can be tested without a world.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldEnemyTurnNumbers
{
	GENERATED_BODY()

	/** Most convoys plus divisions the enemy sends in one turn; extra points are banked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	int32 MaxReactionPiecesPerTurn = 3;

	/** Most reaction points the enemy can keep for later turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	int32 MaxBankedReactionPoints = 24;

	/** How many different targets one turn's reaction is spread over. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "1"))
	int32 MaxReactionTargetsPerTurn = 2;

	/** Division types the enemy picks from (seeded) when it buys a division. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TArray<EWorldFieldDivisions> ReactionDivisionTypes;

	/** Enemy items that can send pieces, next to the enemy HQ which always can. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TArray<EMapEnemyItem> ReactionSourceItems;

	/** A piece with no path left this close (XY) to its target has arrived; a convoy then delivers. Must exceed the
	 * division anchor avoidance radius, since pieces stop at a standoff point outside it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	float ArrivalRadius = 3000.f;

	/** Base strength a delivery adds when the target already has every fortification modifier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	int32 ConvoyFallbackDeliveryStrengthPercentage = 5;

	/** Distance from the source item at which a new piece appears, so it does not sit on the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn", meta = (ClampMin = "0"))
	float SpawnDistanceFromSource = 1600.f;

	/** Roster and fallback strength per piece type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Turn")
	TMap<EWorldFieldDivisions, FWorldEnemyReactionPieceSetup> PieceSetups;

	FWorldEnemyTurnNumbers();
};

/**
 * @brief Project settings page for the enemy reaction (Config/DefaultGame.ini). Read by UWorldEnemyTurnDirector and
 * by UWorldDivisionManager for the fallback strength of reaction pieces.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Enemy Turn"))
class RTS_SURVIVAL_API UWorldEnemyTurnSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "World Campaign Enemy Turn")
	FWorldEnemyTurnNumbers Numbers;

	static const FWorldEnemyTurnNumbers& GetNumbers();

	/**
	 * @brief Looks up the setup of one piece type.
	 * @param PieceType Division or convoy type.
	 * @return The setup, or nullptr when none is configured.
	 */
	static const FWorldEnemyReactionPieceSetup* FindPieceSetup(EWorldFieldDivisions PieceType);
};
