// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PanzerwerferReloadBonusSubsystem.generated.h"

class ATankMaster;

// One built expansion that speeds up its owner's Panzerwerfer reloads (FBxpData::PanzerwerferReloadMlt). Weak: an
// expansion that is gone without unregistering counts as gone.
USTRUCT()
struct FPanzerwerferReloadBonusSource
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const AActor> Source;

	int32 OwningPlayer = 0;
};

/**
 * @brief Keeps the Wurfrahmen shop's Panzerwerfer reload bonus on every Panzerwerfer of a player while at least one of
 * that player's shops stands: expansions register / unregister themselves, live Panzerwerfers get or lose
 * UBehaviourWeaponWurfrahmenShop at once, and new ones get it from URTSComponent when they finish BeginPlay.
 */
UCLASS()
class RTS_SURVIVAL_API UPanzerwerferReloadBonusSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief A built expansion starts granting the bonus; the first source of a player gives it to all their Panzerwerfers.
	 * @param Source The expansion; registering it twice is harmless.
	 * @param OwningPlayer The expansion's owner; only this player's Panzerwerfers profit.
	 */
	void RegisterBonusSource(const AActor* Source, const int32 OwningPlayer);

	/** @brief The expansion stops granting it (packed up, died, destroyed); the last one takes the bonus back. */
	void UnregisterBonusSource(const AActor* Source);

	/** @brief Gives a newly spawned Panzerwerfer the bonus when its owner has a shop. Other tanks are ignored. */
	void OnTankReady(ATankMaster* Tank);

	bool GetIsBonusActiveForPlayer(const int32 Player) const;

	/**
	 * @return True for the tanks the shop speeds up: the Panzerwerfer subtype and the Pz 38(t) R, whose rocket turret
	 * (Ger_Turret_PanzerWerfer, weapon Panzerwerfer_Small) is the Panzerwerfer that exists as a unit today.
	 */
	static bool GetIsPanzerwerfer(const ATankMaster& Tank);

private:
	UPROPERTY()
	TArray<FPanzerwerferReloadBonusSource> M_Sources;

	void RemoveStaleSources();
	void AddBonusToPlayerPanzerwerfers(const int32 Player) const;
	void RemoveBonusFromPlayerPanzerwerfers(const int32 Player) const;
	TArray<ATankMaster*> GetPlayerPanzerwerfers(const int32 Player) const;

	static void AddBonusBehaviour(ATankMaster& Tank);
	static void RemoveBonusBehaviour(ATankMaster& Tank);
};
