// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_SpiesInfiltration.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
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

	/**
	 * @return Players whose hidden units the spies of this owner look for: every player the human fights; for an
	 * AI player only the human, if it fights the human, as the fog never hides AI players from each other.
	 */
	uint8 GetPlayersToSpyOn(const int32 OwningPlayer)
	{
		const uint8 HostileMask = FRTSPlayers::GetHostileMask(OwningPlayer);
		return FRTSPlayers::IsHumanPlayer(OwningPlayer)
			       ? HostileMask
			       : static_cast<uint8>(HostileMask & RTSPlayers::HumanPlayerBit);
	}

	// The fog hides AI units from the human; human units carry a tag while the AI players see them.
	bool GetIsHiddenFromOwner(const AActor& Unit, const bool bOwnedByHuman)
	{
		return bOwnedByHuman
			       ? Unit.IsHidden()
			       : not Unit.Tags.Contains(DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision);
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
	using SpiesInfiltrationPrivate::GetIsHiddenFromOwner;
	const bool bOwnedByHuman = FRTSPlayers::IsHumanPlayer(GetOwningPlayer());
	// Every opponent of the owner, not one fixed side: with more than two players several can hide units.
	const uint8 OpponentsMask = SpiesInfiltrationPrivate::GetPlayersToSpyOn(GetOwningPlayer());

	for (ATankMaster* Tank : GameUnitManager->GetTanksOfPlayers(OpponentsMask))
	{
		if (RTSFunctionLibrary::RTSIsValid(Tank) && GetIsHiddenFromOwner(*Tank, bOwnedByHuman))
		{
			HiddenUnits.Add(Tank);
		}
	}
	// A squad counts once, or one squad could take every pick; a soldier that is in sight shows the squad anyway.
	TSet<const AActor*> SquadsAlreadyAdded;
	for (ASquadUnit* SquadUnit : GameUnitManager->GetSquadUnitsOfPlayers(OpponentsMask))
	{
		if (not RTSFunctionLibrary::RTSIsValid(SquadUnit) || not GetIsHiddenFromOwner(*SquadUnit, bOwnedByHuman))
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
