// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSPlayerRelationsSubsystem.h"

#include "RTSPlayers.h"

void URTSPlayerRelationsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UWorld* World = GetWorld();
	// Editor and preview worlds come and go while a game world plays; only a new game world resets the players.
	if (World != nullptr && World->IsGameWorld())
	{
		FRTSPlayers::ResetForNewWorld();
	}
}

bool URTSPlayerRelationsLibrary::IsHumanPlayer(const int32 Player)
{
	return FRTSPlayers::IsHumanPlayer(Player);
}

bool URTSPlayerRelationsLibrary::IsAIPlayer(const int32 Player)
{
	return FRTSPlayers::IsAIPlayer(Player);
}

bool URTSPlayerRelationsLibrary::ArePlayersHostile(const int32 PlayerA, const int32 PlayerB)
{
	return FRTSPlayers::AreHostile(PlayerA, PlayerB);
}

bool URTSPlayerRelationsLibrary::ArePlayersAllied(const int32 PlayerA, const int32 PlayerB)
{
	return FRTSPlayers::AreAllied(PlayerA, PlayerB);
}

bool URTSPlayerRelationsLibrary::IsActorHostileToPlayer(const AActor* Actor, const int32 Player)
{
	return FRTSPlayers::IsActorHostileToPlayer(Actor, Player);
}

int32 URTSPlayerRelationsLibrary::GetPlayerTeam(const int32 Player)
{
	return FRTSPlayers::GetPlayerTeam(Player);
}

void URTSPlayerRelationsLibrary::SetPlayerTeam(const int32 Player, const int32 Team)
{
	FRTSPlayers::SetPlayerTeam(Player, Team);
}

void URTSPlayerRelationsLibrary::ResetPlayerTeamsToFreeForAll()
{
	FRTSPlayers::ResetToFreeForAll();
}

int32 URTSPlayerRelationsLibrary::GetMaxPlayers()
{
	return RTSPlayers::MaxPlayers;
}
