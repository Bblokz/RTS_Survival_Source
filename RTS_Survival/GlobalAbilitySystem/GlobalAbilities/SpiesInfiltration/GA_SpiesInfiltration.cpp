// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_SpiesInfiltration.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SpiesInfiltrationPrivate
{
	void Shuffle(TArray<AActor*>& Actors)
	{
		for (int32 Index = Actors.Num() - 1; Index > 0; --Index)
		{
			Actors.Swap(Index, FMath::RandRange(0, Index));
		}
	}
}

void UGA_SpiesInfiltration::ActivateAbility()
{
	// Nothing to aim: the click path pays, executes and starts the cooldown in one go.
	OnClickedAbilityLocation(FVector::ZeroVector);
}

void UGA_SpiesInfiltration::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	Super::ExecuteAbilityAtLocation(TargetLocation);
	const TArray<AActor*> UnitsToReveal = PickUnitsToReveal(GetHiddenOpponentUnits());
	M_PreviouslyRevealed.Reset();
	for (AActor* Unit : UnitsToReveal)
	{
		AttachVisionBubble(Unit);
		M_PreviouslyRevealed.Add(Unit);
	}
}

TArray<AActor*> UGA_SpiesInfiltration::GetHiddenOpponentUnits() const
{
	TArray<AActor*> HiddenUnits;
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return HiddenUnits;
	}
	const bool bOwnedByPlayer = GetOwningPlayer() == 1;
	const uint8 Opponent = bOwnedByPlayer ? 2 : 1;
	// The fog hides enemy units from the player; player units carry a tag while the enemy sees them.
	auto IsHiddenFromOwner = [bOwnedByPlayer](const AActor* Unit)
	{
		return bOwnedByPlayer
			       ? Unit->IsHidden()
			       : not Unit->Tags.Contains(DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision);
	};

	for (ATankMaster* Tank : GameUnitManager->GetPlayerTanks(Opponent))
	{
		if (RTSFunctionLibrary::RTSIsValid(Tank) && IsHiddenFromOwner(Tank))
		{
			HiddenUnits.Add(Tank);
		}
	}
	// A squad counts once, or one squad could take every pick; a soldier that is in sight shows the squad anyway.
	TSet<const AActor*> SquadsAlreadyAdded;
	for (ASquadUnit* SquadUnit : GameUnitManager->GetSquadUnitsOfPlayer(Opponent))
	{
		if (not RTSFunctionLibrary::RTSIsValid(SquadUnit) || not IsHiddenFromOwner(SquadUnit))
		{
			continue;
		}
		const AActor* Squad = SquadUnit->GetSquadControllerChecked();
		if (IsValid(Squad) && SquadsAlreadyAdded.Contains(Squad))
		{
			continue;
		}
		SquadsAlreadyAdded.Add(Squad);
		HiddenUnits.Add(SquadUnit);
	}
	return HiddenUnits;
}

TArray<AActor*> UGA_SpiesInfiltration::PickUnitsToReveal(TArray<AActor*> Candidates) const
{
	// Fresh units first, the ones shown last time after them; both groups in a random order.
	TArray<AActor*> RevealedLastTime;
	for (int32 Index = Candidates.Num() - 1; Index >= 0; --Index)
	{
		if (M_PreviouslyRevealed.Contains(Candidates[Index]))
		{
			RevealedLastTime.Add(Candidates[Index]);
			Candidates.RemoveAtSwap(Index);
		}
	}
	SpiesInfiltrationPrivate::Shuffle(Candidates);
	SpiesInfiltrationPrivate::Shuffle(RevealedLastTime);
	Candidates.Append(RevealedLastTime);

	// Spread the bubbles first so a clump of units does not take every pick, then fill up with what is left.
	const int32 UnitCount = M_SpiesSettings.UnitsToReveal;
	const float MinDistanceSquared = FMath::Square(M_SpiesSettings.RevealRadius);
	TArray<AActor*> Picked;
	for (AActor* Candidate : Candidates)
	{
		if (Picked.Num() >= UnitCount)
		{
			return Picked;
		}
		const FVector Location = Candidate->GetActorLocation();
		const bool bTooClose = Picked.ContainsByPredicate([&Location, MinDistanceSquared](const AActor* Other)
		{
			return FVector::DistSquared2D(Location, Other->GetActorLocation()) < MinDistanceSquared;
		});
		if (not bTooClose)
		{
			Picked.Add(Candidate);
		}
	}
	for (AActor* Candidate : Candidates)
	{
		if (Picked.Num() >= UnitCount)
		{
			break;
		}
		Picked.AddUnique(Candidate);
	}
	return Picked;
}

void UGA_SpiesInfiltration::AttachVisionBubble(AActor* Unit) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(Unit))
	{
		return;
	}
	AGlobalAbilityVisionBubble* Bubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
		AGlobalAbilityVisionBubble::StaticClass(), Unit->GetActorLocation(), FRotator::ZeroRotator);
	if (not IsValid(Bubble))
	{
		return;
	}
	// Follows the unit; if the unit dies first the bubble stays where it fell until its time is up.
	Bubble->AttachToActor(Unit, FAttachmentTransformRules::KeepWorldTransform);
	Bubble->StartVision(M_SpiesSettings.RevealRadius, M_SpiesSettings.RevealHoldSeconds,
	                    static_cast<uint8>(GetOwningPlayer()));
}
