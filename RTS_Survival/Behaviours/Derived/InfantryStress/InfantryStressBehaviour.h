// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "InfantryStressBehaviour.generated.h"

class ASquadUnit;
class UWeaponState;

/**
 * @brief The weapon side of infantry stress: a stressed soldier fires a lot, fast and badly.
 * One instance is made per stressed soldier, by the soldier itself, from the class set in
 * Project Settings > Infantry Cover Finder > Infantry Stress; it lasts as long as the soldier's stress does.
 * Derive a Blueprint from this class and set it there to change the multipliers or to add effects.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UInfantryStressBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UInfantryStressBehaviour();

	/**
	 * @brief Puts the behaviour on one soldier's weapon and tops up its magazine, so the spraying can start
	 * on the same frame the soldier's stress does.
	 * @param Soldier The soldier that just got stressed.
	 */
	void StartOnSoldier(ASquadUnit& Soldier);

	// Takes the behaviour off the soldier's weapon again; safe when the weapon or the soldier is already gone.
	void EndOnSoldier();

	// The soldier stops spraying from where he is and runs for cover further from his enemy.
	void NotifySoldierRelocates();

	/**
	 * @brief Repeats the behaviour's animated text or icon over the soldier when its next showing is due.
	 * The squad's behaviour component does this for its own behaviours; a stress behaviour sits on one soldier
	 * instead, so the soldier's stress tick calls this.
	 * @param WorldSeconds Current game time.
	 */
	void UpdateSoldierFeedback(float WorldSeconds);

	const ASquadUnit* GetStressedSoldier() const { return M_Soldier.Get(); }

	// False when the behaviour's calibre rules leave the stressed soldier's weapon as it is.
	bool GetIsOnWeapon() const { return M_StressedWeapon.IsValid(); }

	/** @return Whether the behaviour's calibre rules let it change this weapon. */
	bool GetAppliesToWeapon(UWeaponState* Weapon) const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Infantry Stress")
	void BP_OnSoldierStressed(ASquadUnit* Soldier);

	// Halfway: the soldier leaves his cover for one further from his enemy.
	UFUNCTION(BlueprintImplementableEvent, Category = "Infantry Stress")
	void BP_OnSoldierRelocates(ASquadUnit* Soldier);

	// Soldier is null when he did not survive his stress.
	UFUNCTION(BlueprintImplementableEvent, Category = "Infantry Stress")
	void BP_OnSoldierCalmedDown(ASquadUnit* Soldier);

private:
	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_Soldier;

	// The weapon the multipliers were applied to; a soldier may be handed another weapon while stressed.
	UPROPERTY()
	TWeakObjectPtr<UWeaponState> M_StressedWeapon;

	// When the animated text or icon is shown next, and how often after that: INDEX_NONE without end, zero
	// when nothing is left to show.
	float M_NextFeedbackWorldSeconds = 0.0f;
	float M_FeedbackRepeatSeconds = 0.0f;
	int32 M_RemainingFeedbackShowings = 0;

	// Shows the animated text or icon once, and works out how often it is to be repeated.
	void StartSoldierFeedback(const ASquadUnit& Soldier);

	/** @return False when the behaviour has neither an icon nor a text to show, or it could not be shown. */
	bool ShowFeedbackOverSoldier(ASquadUnit& Soldier) const;
};
