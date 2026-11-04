// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTS_TeamColor.h"

#include "Components/ChildActorComponent.h"
#include "EngineUtils.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace TeamColorConstants
{
	// The owning player index of the human player.
	constexpr int32 PlayerIndex = 1;
}

void FRTS_TeamColor::ApplyPlayerTeamColorToActor(AActor* Actor)
{
	if (not IsValid(Actor) || not GetIsOwnedByPlayer(Actor))
	{
		return;
	}
	const FLinearColor TeamColor = GetPlayerTeamColor(Actor);
	// Without a colour the primitive data stays unset: the camo is untouched, exactly as before team colours existed.
	if (TeamColor.A <= 0.f)
	{
		return;
	}
	SetTeamColorOnActorAndChildren(Actor, TeamColor);
}

void FRTS_TeamColor::ApplyPlayerTeamColorToWorldVehicles(const UObject* WorldContextObject)
{
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return;
	}
	const FLinearColor TeamColor = GetPlayerTeamColor(WorldContextObject);
	for (TActorIterator<ATankMaster> TankIterator(World); TankIterator; ++TankIterator)
	{
		ATankMaster* const Tank = *TankIterator;
		if (IsValid(Tank) && GetIsOwnedByPlayer(Tank))
		{
			SetTeamColorOnActorAndChildren(Tank, TeamColor);
		}
	}
}

FLinearColor FRTS_TeamColor::GetPlayerTeamColor(const UObject* WorldContextObject)
{
	const URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(WorldContextObject);
	if (not IsValid(GameInstance))
	{
		return FLinearColor::Transparent;
	}
	return GameInstance->GetPlayerTeamColor();
}

void FRTS_TeamColor::SetTeamColorOnActorAndChildren(const AActor* TargetActor, const FLinearColor& TeamColor)
{
	if (not IsValid(TargetActor))
	{
		return;
	}
	const FVector4 TeamColorData(TeamColor.R, TeamColor.G, TeamColor.B, TeamColor.A);
	TInlineComponentArray<UActorComponent*> Components(TargetActor);
	for (UActorComponent* EachComponent : Components)
	{
		if (UPrimitiveComponent* const Primitive = Cast<UPrimitiveComponent>(EachComponent))
		{
			Primitive->SetCustomPrimitiveDataVector4(TeamColorPrimitiveDataIndex, TeamColorData);
			continue;
		}
		// Turrets are child actors; recursion also reaches child actors nested in those.
		const UChildActorComponent* const ChildActorComponent = Cast<UChildActorComponent>(EachComponent);
		if (IsValid(ChildActorComponent))
		{
			SetTeamColorOnActorAndChildren(ChildActorComponent->GetChildActor(), TeamColor);
		}
	}
}

bool FRTS_TeamColor::GetIsOwnedByPlayer(const AActor* Actor)
{
	const URTSComponent* const RTSComponent = Actor->FindComponentByClass<URTSComponent>();
	return IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TeamColorConstants::PlayerIndex;
}
