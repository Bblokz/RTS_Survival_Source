// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSPlayerCollision.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

namespace RTSPlayerCollisionPrivate
{
	// On components every player's weapons must hit, allied shots included; they never carry an owner bit.
	const FName HitByAllPlayersTag = TEXT("RTSHitByAllPlayers");

	bool GetCanTakeOwnerBit(const UPrimitiveComponent* PrimitiveComponent)
	{
		// Components without collision get the bit too: one enabled later with a plain SetCollisionEnabled (after an
		// owner change) must not keep the bit of its previous owner. Without physics state the bit is only stored.
		return IsValid(PrimitiveComponent) && not PrimitiveComponent->ComponentHasTag(HitByAllPlayersTag);
	}
}

ECollisionChannel FRTSPlayerCollision::GetWeaponTraceChannel(const int32 OwningPlayer)
{
	return FRTSPlayers::IsHumanPlayer(OwningPlayer) ? COLLISION_TRACE_HUMAN_WEAPONS : COLLISION_TRACE_AI_WEAPONS;
}

uint8 FRTSPlayerCollision::GetWeaponIgnoreMask(const int32 OwningPlayer)
{
	return FRTSPlayers::GetFriendlyMask(OwningPlayer);
}

void FRTSPlayerCollision::ApplyWeaponIgnoreMask(FCollisionQueryParams& QueryParams, const int32 OwningPlayer)
{
	QueryParams.IgnoreMask = GetWeaponIgnoreMask(OwningPlayer);
}

void FRTSPlayerCollision::ApplyWeaponIgnoreMask(FCollisionObjectQueryParams& ObjectQueryParams,
                                                const int32 OwningPlayer)
{
	ObjectQueryParams.IgnoreMask = GetWeaponIgnoreMask(OwningPlayer);
}

ECollisionChannel FRTSPlayerCollision::GetOwnerObjectChannel(const int32 OwningPlayer)
{
	return FRTSPlayers::IsHumanPlayer(OwningPlayer) ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
}

void FRTSPlayerCollision::SetWeaponHitResponses(UPrimitiveComponent* Component, const int32 OwningPlayer,
                                                const ECollisionResponse Response)
{
	if (not IsValid(Component))
	{
		return;
	}
	if (FRTSPlayers::IsAIPlayer(OwningPlayer))
	{
		Component->SetCollisionResponseToChannel(COLLISION_TRACE_HUMAN_WEAPONS, Response);
	}
	// Any AI player can be hostile to any player, so AI shots reach every owned component; the ignore mask of the
	// query takes the allied ones out again. Neutral things keep what they had before four players: only the
	// human's weapons reach them.
	if (not FRTSPlayers::IsNeutralOwner(OwningPlayer))
	{
		Component->SetCollisionResponseToChannel(COLLISION_TRACE_AI_WEAPONS, Response);
	}
	Component->ComponentTags.Remove(RTSPlayerCollisionPrivate::HitByAllPlayersTag);
	ApplyMaskFilter(Component, FRTSPlayers::GetPlayerBit(OwningPlayer));
}

void FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(UPrimitiveComponent* Component,
                                                             const ECollisionResponse Response)
{
	if (not IsValid(Component))
	{
		return;
	}
	Component->SetCollisionResponseToChannel(COLLISION_TRACE_AI_WEAPONS, Response);
	Component->SetCollisionResponseToChannel(COLLISION_TRACE_HUMAN_WEAPONS, Response);
	// Keeps ApplyOwnerMaskToActor from giving the component an owner bit later (owner change, late BeginPlay).
	Component->ComponentTags.AddUnique(RTSPlayerCollisionPrivate::HitByAllPlayersTag);
	ApplyMaskFilter(Component, 0);
}

void FRTSPlayerCollision::SetOwnerMaskFilter(UPrimitiveComponent* Component, const int32 OwningPlayer)
{
	if (not IsValid(Component))
	{
		return;
	}
	ApplyMaskFilter(Component, FRTSPlayers::GetPlayerBit(OwningPlayer));
}

void FRTSPlayerCollision::ClearOwnerMaskFilter(UPrimitiveComponent* Component)
{
	if (not IsValid(Component))
	{
		return;
	}
	ApplyMaskFilter(Component, 0);
}

void FRTSPlayerCollision::ApplyOwnerMaskToActor(AActor* Actor, const int32 OwningPlayer)
{
	if (not IsValid(Actor))
	{
		return;
	}
	const uint8 OwnerBit = FRTSPlayers::GetPlayerBit(OwningPlayer);
	TArray<AActor*> ActorsToMark;
	Actor->GetAllChildActors(ActorsToMark, true);
	ActorsToMark.Add(Actor);
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	for (const AActor* ActorToMark : ActorsToMark)
	{
		if (not IsValid(ActorToMark))
		{
			continue;
		}
		PrimitiveComponents.Reset();
		ActorToMark->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (RTSPlayerCollisionPrivate::GetCanTakeOwnerBit(PrimitiveComponent))
			{
				ApplyMaskFilter(PrimitiveComponent, OwnerBit);
			}
		}
	}
}

void FRTSPlayerCollision::ApplySideChangeToActor(AActor* Actor, const int32 OldOwningPlayer,
                                                 const int32 NewOwningPlayer)
{
	const bool bIsNewOwnerHuman = FRTSPlayers::IsHumanPlayer(NewOwningPlayer);
	if (not IsValid(Actor) || FRTSPlayers::IsHumanPlayer(OldOwningPlayer) == bIsNewOwnerHuman)
	{
		return;
	}
	TArray<AActor*> ActorsToChange;
	Actor->GetAllChildActors(ActorsToChange, true);
	ActorsToChange.Add(Actor);
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	for (const AActor* ActorToChange : ActorsToChange)
	{
		if (not IsValid(ActorToChange))
		{
			continue;
		}
		PrimitiveComponents.Reset();
		ActorToChange->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			// Hit-by-everyone components block both weapon channels whoever owns them.
			if (RTSPlayerCollisionPrivate::GetCanTakeOwnerBit(PrimitiveComponent))
			{
				ApplySideChangeToComponent(*PrimitiveComponent, bIsNewOwnerHuman);
			}
		}
	}
}

void FRTSPlayerCollision::ApplySideChangeToComponent(UPrimitiveComponent& Component, const bool bIsNewOwnerHuman)
{
	const ECollisionChannel OldOwnChannel = bIsNewOwnerHuman ? COLLISION_OBJ_ENEMY : COLLISION_OBJ_PLAYER;
	const ECollisionChannel NewOwnChannel = bIsNewOwnerHuman ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
	// The weapons that were hostile to the old side: AI weapons for a human body, human weapons for an AI body.
	const ECollisionChannel OldHostileWeapons = bIsNewOwnerHuman
		                                            ? COLLISION_TRACE_HUMAN_WEAPONS
		                                            : COLLISION_TRACE_AI_WEAPONS;
	FCollisionResponseContainer Responses = Component.GetCollisionResponseToChannels();
	const ECollisionResponse ResponseToOldHostileWeapons = Responses.GetResponse(OldHostileWeapons);
	// Mirror: what the body did to its own side's objects it now does to the new own side's, and the reverse.
	const ECollisionResponse ResponseToOldOwnObjects = Responses.GetResponse(OldOwnChannel);
	Responses.SetResponse(OldOwnChannel, Responses.GetResponse(NewOwnChannel));
	Responses.SetResponse(NewOwnChannel, ResponseToOldOwnObjects);
	if (bIsNewOwnerHuman)
	{
		Responses.SetResponse(COLLISION_TRACE_AI_WEAPONS, ResponseToOldHostileWeapons);
		// The human's own shots never stop on human bodies; before four players they did not block this channel.
		if (ResponseToOldHostileWeapons == ECR_Block)
		{
			Responses.SetResponse(COLLISION_TRACE_HUMAN_WEAPONS, ECR_Ignore);
		}
	}
	else
	{
		Responses.SetResponse(COLLISION_TRACE_HUMAN_WEAPONS, ResponseToOldHostileWeapons);
		// Other AI players fire on the AI weapon channel; the owner's allies are filtered by the mask bit.
		if (ResponseToOldHostileWeapons == ECR_Block)
		{
			Responses.SetResponse(COLLISION_TRACE_AI_WEAPONS, ECR_Block);
		}
	}
	// Hull blocker boxes (FRTS_CollisionSetup::SetupVehicleHullBlockerCollision) sit on a hull channel per side and
	// block the other side's: the same mirror. Only boxes that block the other hull channel are hull blockers; other
	// components on ECC_Vehicle are left alone.
	const ECollisionChannel OldHullChannel = bIsNewOwnerHuman ? COLLISION_OBJ_ENEMY_HULL : ECC_Vehicle;
	const ECollisionChannel NewHullChannel = bIsNewOwnerHuman ? ECC_Vehicle : COLLISION_OBJ_ENEMY_HULL;
	const bool bIsHullBlocker = Component.IsA<UBoxComponent>()
		&& Component.GetCollisionObjectType() == OldHullChannel
		&& Responses.GetResponse(NewHullChannel) == ECR_Block;
	if (bIsHullBlocker)
	{
		const ECollisionResponse ResponseToOldOwnHulls = Responses.GetResponse(OldHullChannel);
		Responses.SetResponse(OldHullChannel, ECR_Block);
		Responses.SetResponse(NewHullChannel, ResponseToOldOwnHulls);
	}
	// One filter update for all responses instead of one per channel.
	Component.SetCollisionResponseToChannels(Responses);
	if (Component.GetCollisionObjectType() == OldOwnChannel)
	{
		Component.SetCollisionObjectType(NewOwnChannel);
	}
	if (bIsHullBlocker)
	{
		Component.SetCollisionObjectType(NewHullChannel);
	}
}

void FRTSPlayerCollision::CopyOwnerMaskFilter(const UPrimitiveComponent* SourceComponent,
                                              UPrimitiveComponent* TargetComponent)
{
	if (not IsValid(SourceComponent) || not IsValid(TargetComponent))
	{
		return;
	}
	// The tag travels with the bit, or the next ApplyOwnerMaskToActor would mark a hit-by-everyone copy as owned.
	if (SourceComponent->ComponentHasTag(RTSPlayerCollisionPrivate::HitByAllPlayersTag))
	{
		TargetComponent->ComponentTags.AddUnique(RTSPlayerCollisionPrivate::HitByAllPlayersTag);
	}
	else
	{
		TargetComponent->ComponentTags.Remove(RTSPlayerCollisionPrivate::HitByAllPlayersTag);
	}
	ApplyMaskFilter(TargetComponent, SourceComponent->BodyInstance.GetMaskFilter());
}

void FRTSPlayerCollision::RefreshOwnerMaskFilter(UPrimitiveComponent* Component)
{
	if (not IsValid(Component))
	{
		return;
	}
	ApplyMaskFilter(Component, Component->BodyInstance.GetMaskFilter());
}

int32 FRTSPlayerCollision::GetOwningPlayerOfHit(const AActor* HitActor, const UPrimitiveComponent* HitComponent)
{
	if (IsValid(HitComponent))
	{
		// Bodies marked with their owner answer without a component search; a mask is one player bit or zero.
		const uint32 OwnerBit = HitComponent->BodyInstance.GetMaskFilter() & RTSPlayers::AllPlayersMask;
		if (OwnerBit != 0 && FMath::IsPowerOfTwo(OwnerBit))
		{
			return static_cast<int32>(FMath::FloorLog2(OwnerBit)) + 1;
		}
	}
	const int32 ActorOwner = FRTSPlayers::GetOwningPlayerOfActor(HitActor);
	if (ActorOwner != INDEX_NONE || not IsValid(HitComponent))
	{
		return ActorOwner;
	}
	// Owned things without an RTS component (Blueprint colliders) are still on the object channel of their side.
	const ECollisionChannel ObjectChannel = HitComponent->GetCollisionObjectType();
	if (ObjectChannel == COLLISION_OBJ_PLAYER)
	{
		return RTSPlayers::HumanPlayer;
	}
	return ObjectChannel == COLLISION_OBJ_ENEMY ? RTSPlayers::FirstAIPlayer : INDEX_NONE;
}

bool FRTSPlayerCollision::IsHitHostileToPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent,
                                               const int32 Player)
{
	const int32 HitOwner = GetOwningPlayerOfHit(HitActor, HitComponent);
	return HitOwner != INDEX_NONE && FRTSPlayers::AreHostile(Player, HitOwner);
}

bool FRTSPlayerCollision::IsHitDamageableByPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent,
                                                  const int32 Player)
{
	const int32 HitOwner = GetOwningPlayerOfHit(HitActor, HitComponent);
	return HitOwner != INDEX_NONE && FRTSPlayers::CanPlayerDamageOwner(Player, HitOwner);
}

bool FRTSPlayerCollision::IsHitAlliedToPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent,
                                              const int32 Player)
{
	const int32 HitOwner = GetOwningPlayerOfHit(HitActor, HitComponent);
	return HitOwner != INDEX_NONE && FRTSPlayers::AreAllied(Player, HitOwner);
}

ETriggerOverlapLogic FRTSPlayerCollision::GetOverlapLogicForHostilesOf(const int32 OwningPlayer)
{
	const uint8 ActiveHostileMask = FRTSPlayers::GetActiveHostileMask(OwningPlayer);
	const bool bHumanIsHostile = (ActiveHostileMask & RTSPlayers::HumanPlayerBit) != 0;
	const bool bAnAIIsHostile = (ActiveHostileMask & RTSPlayers::AIPlayersMask) != 0;
	if (bHumanIsHostile && bAnAIIsHostile)
	{
		return ETriggerOverlapLogic::OverlapBoth;
	}
	// A player with no hostile human only has AI enemies (or none at all: the AI channel then finds nothing hostile).
	return bHumanIsHostile ? ETriggerOverlapLogic::OverlapPlayer : ETriggerOverlapLogic::OverlapEnemy;
}

ETriggerOverlapLogic FRTSPlayerCollision::GetOverlapLogicForAlliesOf(const int32 OwningPlayer)
{
	const uint8 ActiveFriendlyMask = FRTSPlayers::GetFriendlyMask(OwningPlayer) & FRTSPlayers::GetActivePlayersMask();
	const bool bHumanIsFriendly = (ActiveFriendlyMask & RTSPlayers::HumanPlayerBit) != 0;
	const bool bAnAIIsFriendly = (ActiveFriendlyMask & RTSPlayers::AIPlayersMask) != 0;
	if (bHumanIsFriendly && bAnAIIsFriendly)
	{
		return ETriggerOverlapLogic::OverlapBoth;
	}
	return bHumanIsFriendly ? ETriggerOverlapLogic::OverlapPlayer : ETriggerOverlapLogic::OverlapEnemy;
}

void FRTSPlayerCollision::AddObjectTypesForOverlapLogic(FCollisionObjectQueryParams& ObjectQueryParams,
                                                        const ETriggerOverlapLogic OverlapLogic)
{
	if (OverlapLogic == ETriggerOverlapLogic::OverlapPlayer || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	}
	if (OverlapLogic == ETriggerOverlapLogic::OverlapEnemy || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	}
}

void FRTSPlayerCollision::ApplyMaskFilter(UPrimitiveComponent* Component, const uint8 MaskFilter)
{
	Component->SetMaskFilterOnBodyInstance(MaskFilter);
	if (const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
	{
		// A skeletal mesh has one body per bone; each carries its own filter.
		for (FBodyInstance* BoneBody : SkeletalMesh->Bodies)
		{
			if (BoneBody != nullptr)
			{
				BoneBody->SetMaskFilter(MaskFilter);
			}
		}
	}
	else if (const UInstancedStaticMeshComponent* InstancedMesh = Cast<UInstancedStaticMeshComponent>(Component))
	{
		// Instances are hit through their own bodies, not the component's.
		for (FBodyInstance* InstanceBody : InstancedMesh->InstanceBodies)
		{
			if (InstanceBody != nullptr)
			{
				InstanceBody->SetMaskFilter(MaskFilter);
			}
		}
	}
	// FBodyInstance::SetMaskFilter cannot reach the shapes of a body welded to a parent; rebuilding the filter data
	// does. Only welded bodies need it, so unit BeginPlay and owner changes skip the rebuild for all others.
	if (Component->BodyInstance.WeldParent != nullptr)
	{
		Component->BodyInstance.UpdatePhysicsFilterData();
	}
}
