// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSPlayerRelationsSubsystem.generated.h"

/**
 * @brief Starts every world with the default player relations (free for all), so teams set in one map or test
 * never leak into the next. Maps and missions set their teams from BeginPlay on with URTSPlayerRelationsLibrary.
 */
UCLASS()
class RTS_SURVIVAL_API URTSPlayerRelationsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};

/**
 * @brief Blueprint access to FRTSPlayers: use these instead of comparing an owning player with 1 or 2.
 */
UCLASS()
class RTS_SURVIVAL_API URTSPlayerRelationsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The human is player 1; all other players are AI. */
	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static bool IsHumanPlayer(int32 Player);

	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static bool IsAIPlayer(int32 Player);

	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static bool ArePlayersHostile(int32 PlayerA, int32 PlayerB);

	/** True for the same player and for players on one team. */
	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static bool ArePlayersAllied(int32 PlayerA, int32 PlayerB);

	/** False for actors without an RTS component. */
	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static bool IsActorHostileToPlayer(const AActor* Actor, int32 Player);

	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static int32 GetPlayerTeam(int32 Player);

	/**
	 * @brief Puts a player on a team; players sharing a team stop fighting each other.
	 * @param Player Player to move, 1..4.
	 * @param Team Any team number; by default every player is alone on the team with its own number.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Players")
	static void SetPlayerTeam(int32 Player, int32 Team);

	UFUNCTION(BlueprintCallable, Category = "RTS|Players")
	static void ResetPlayerTeamsToFreeForAll();

	UFUNCTION(BlueprintPure, Category = "RTS|Players")
	static int32 GetMaxPlayers();
};
