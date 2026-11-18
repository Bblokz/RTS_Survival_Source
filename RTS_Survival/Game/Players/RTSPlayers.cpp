// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSPlayers.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"

// Free for all: team of player N is N, so everyone but the player itself is hostile. Slot 0 is the neutral owner.
uint8 FRTSPlayers::M_TeamByPlayer[RTSPlayers::PlayerArraySize] = {0, 1, 2, 3, 4};
uint8 FRTSPlayers::M_HostileMaskByPlayer[RTSPlayers::PlayerArraySize] = {0, 0x0E, 0x0D, 0x0B, 0x07};
uint8 FRTSPlayers::M_ActivePlayersMask = RTSPlayers::DefaultActivePlayersMask;
uint32 FRTSPlayers::M_RelationsVersion = 0;

static_assert(RTSPlayers::MaxPlayers == 4, "Update the default team and hostile mask tables for the new player count.");
// The collision filter of a body has NumExtraFilterBits bits; every player needs its own.
static_assert(RTSPlayers::MaxPlayers <= NumExtraFilterBits, "More players than collision mask filter bits.");

namespace RTSPlayersPrivate
{
	// Unit -> turret -> weapon is the deepest child actor chain in the game; one spare level.
	constexpr int32 MaxParentActorsToAsk = 4;

	// The unit base classes keep a pointer to their RTS component; only other actors need the component search.
	const URTSComponent* FindRTSComponent(const AActor* Actor)
	{
		if (const AHpPawnMaster* HpPawn = Cast<AHpPawnMaster>(Actor))
		{
			return HpPawn->GetRTSComponent();
		}
		if (const ACharacterObjectsMaster* HpCharacter = Cast<ACharacterObjectsMaster>(Actor))
		{
			return HpCharacter->GetRTSComponent();
		}
		if (const AHPActorObjectsMaster* HpActor = Cast<AHPActorObjectsMaster>(Actor))
		{
			return HpActor->GetRTSComponent();
		}
		return Actor->FindComponentByClass<URTSComponent>();
	}
}

void FRTSPlayers::NotifyUnitOwnerChanged()
{
	++M_RelationsVersion;
}

void FRTSPlayers::MarkPlayerActive(const int32 Player)
{
	M_ActivePlayersMask |= GetPlayerBit(Player);
}

int32 FRTSPlayers::GetPlayerTeam(const int32 Player)
{
	return M_TeamByPlayer[NormalizePlayer(Player)];
}

void FRTSPlayers::SetPlayerTeam(const int32 Player, const int32 Team)
{
	if (not IsValidPlayer(Player))
	{
		return;
	}
	M_TeamByPlayer[Player] = static_cast<uint8>(Team);
	RebuildHostileMasks();
}

void FRTSPlayers::ResetToFreeForAll()
{
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		M_TeamByPlayer[Player] = static_cast<uint8>(Player);
	}
	RebuildHostileMasks();
}

void FRTSPlayers::ResetForNewWorld()
{
	ResetToFreeForAll();
	M_ActivePlayersMask = RTSPlayers::DefaultActivePlayersMask;
}

int32 FRTSPlayers::GetOwningPlayerOfActor(const AActor* Actor)
{
	const AActor* ActorToAsk = Actor;
	for (int32 Depth = 0; Depth < RTSPlayersPrivate::MaxParentActorsToAsk && IsValid(ActorToAsk); ++Depth)
	{
		const URTSComponent* RTSComponent = RTSPlayersPrivate::FindRTSComponent(ActorToAsk);
		if (IsValid(RTSComponent))
		{
			return RTSComponent->GetOwningPlayer();
		}
		ActorToAsk = ActorToAsk->GetParentActor();
	}
	return INDEX_NONE;
}

bool FRTSPlayers::IsActorHostileToPlayer(const AActor* Actor, const int32 Player)
{
	const int32 ActorOwner = GetOwningPlayerOfActor(Actor);
	return ActorOwner != INDEX_NONE && AreHostile(Player, ActorOwner);
}

bool FRTSPlayers::IsActorAlliedToPlayer(const AActor* Actor, const int32 Player)
{
	const int32 ActorOwner = GetOwningPlayerOfActor(Actor);
	return ActorOwner != INDEX_NONE && AreAllied(Player, ActorOwner);
}

void FRTSPlayers::RebuildHostileMasks()
{
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		uint8 HostileMask = 0;
		for (int32 Other = RTSPlayers::HumanPlayer; Other <= RTSPlayers::MaxPlayers; ++Other)
		{
			if (M_TeamByPlayer[Other] != M_TeamByPlayer[Player])
			{
				HostileMask |= static_cast<uint8>(1 << (Other - 1));
			}
		}
		M_HostileMaskByPlayer[Player] = HostileMask;
	}
	++M_RelationsVersion;
}
