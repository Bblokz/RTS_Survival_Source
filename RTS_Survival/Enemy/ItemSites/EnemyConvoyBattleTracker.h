// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyItemSiteTypes.h"
#include "EnemyConvoyBattleTracker.generated.h"

class AConvoy;
class AEnemyConvoyRoute;
struct FConvoyCargoOutcome;
struct FConvoyEvent;

DECLARE_MULTICAST_DELEGATE(FOnEnemyConvoyBattleReady);

/**
 * @brief Used by AEnemyItemSiteDirector on field maps where the attacked supply convoy drives a placed
 * AEnemyConvoyRoute: StartConvoyBattle runs the route's single convoy with the world map's truck count, and
 * GetBattleState feeds EnemyItemSiteRules::GetConvoyBattleObjectiveState (any escape loses).
 */
UCLASS(ClassGroup = (Enemy))
class RTS_SURVIVAL_API UEnemyConvoyBattleTracker : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyConvoyBattleTracker();

	/**
	 * @brief Starts the route's one convoy and follows it until it finishes; runs once.
	 * @param Route The placed route the convoy drives (Start On Begin Play off).
	 * @param TruckCount Trucks the world map convoy still has.
	 * @return False when the route could not start; the battle then counts as failed and is ready at once.
	 */
	bool StartConvoyBattle(AEnemyConvoyRoute* Route, const int32 TruckCount);

	// The followed convoy's state: what was recorded when it finished, or its live truck fates while it drives.
	FEnemyConvoyBattleState GetBattleState() const;

	// Spawned trucks of the convoy that are alive and still hostile to the player (a captured truck is gone).
	TArray<AActor*> GetLivingTrucks() const;

	// Trucks that spawned once the convoy finished; the living trucks before that.
	int32 GetSpawnedTruckCount() const;

	bool GetIsBattleReady() const { return bM_IsBattleReady; }

	// Once: the column spawned and formed, or the battle ended before that (failed route, convoy finished early).
	FOnEnemyConvoyBattleReady OnBattleReady;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyConvoyRoute> M_Route;

	UPROPERTY()
	TWeakObjectPtr<AConvoy> M_Convoy;

	// Recorded from the route's events; live truck fates of the driving convoy are added in GetBattleState.
	FEnemyConvoyBattleState M_BattleState;

	FDelegateHandle M_ConvoyEventHandle;
	bool bM_HasStartedBattle = false;
	bool bM_IsBattleReady = false;

	UFUNCTION()
	void HandleConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy);

	UFUNCTION()
	void HandleConvoyCargoOutcome(AEnemyConvoyRoute* Route, AConvoy* Convoy, const FConvoyCargoOutcome& Outcome);

	UFUNCTION()
	void HandleRouteFinished(AEnemyConvoyRoute* Route);

	void HandleConvoyEvent(const FConvoyEvent& Event);
	void MarkBattleReady();
	void BindRoute(AEnemyConvoyRoute& Route);
	void UnbindRoute();
	void UnbindConvoy();
	void AddLiveTruckFates(FEnemyConvoyBattleState& InOutBattleState) const;
};
