// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSMenuBattleTypes.h"

#include "RTSMenuBattleArena.generated.h"

class UArrowComponent;
class UBillboardComponent;

/**
 * @brief Place on open, navmesh-covered ground of the start map: one spot where the menu battle director stages a
 * battle. The Germans line up behind the actor's origin facing along its arrow, the Soviets face them from the front.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSMenuBattleArena : public AActor
{
	GENERATED_BODY()

public:
	ARTSMenuBattleArena();

	const FRTSMenuBattleArenaSettings& GetSettings() const { return M_Settings; }

	/** Unit vector from the German line toward the Soviet line. */
	FVector GetBattleForward() const;

	FVector GetLineCenter(const ERTSMenuBattleSide Side) const;

	/** Yaw that faces a side's units toward the other side. */
	float GetFacingYaw(const ERTSMenuBattleSide Side) const;

	/**
	 * @brief Where a unit enters the battle: vehicles in front, infantry behind, aircraft at the back; reinforcement
	 * waves arrive ReinforcementDepth further back.
	 * @param Entry The unit's side, kind, wave, slot (0 in the middle, then alternating right and left) and jitter.
	 * @return Location on the arena's plane; the director drops it onto the ground.
	 */
	FVector GetSlotLocation(const FRTSMenuBattleRosterEntry& Entry) const;

	/** Where a unit that drives in stops: its slot on the starting line, without the approach distance. */
	FVector GetFormUpLocation(const FRTSMenuBattleRosterEntry& Entry) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> M_ArenaRoot;

#if WITH_EDITORONLY_DATA
	// Points from the German line to the Soviet line.
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UArrowComponent> M_BattleDirectionArrow;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBillboardComponent> M_Billboard;
#endif

	UPROPERTY(EditAnywhere, Category = "MenuBattle")
	FRTSMenuBattleArenaSettings M_Settings;

	float GetRankDistance(const ERTSMenuBattleUnitKind Kind) const;
};
