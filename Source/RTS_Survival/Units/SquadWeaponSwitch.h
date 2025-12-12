// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SquadWeaponSwitch.generated.h"

class ASquadController;
class ASquadUnit;

USTRUCT()
/**
 * @brief Coordinates intra-squad weapon swapping when a squad unit dies.
 * @details Evaluates weapon values for the squad and assigns the strongest available weapon to the weakest surviving unit.
 */
struct FSquadWeaponSwitch
{
        GENERATED_BODY()

public:
        FSquadWeaponSwitch();

        void Init(ASquadController* InSquadController);

        /**
         * @brief Attempts to transfer the weapon from the dead unit to the weakest armed survivor.
         * @param UnitThatDied The unit that just died.
         */
        void HandleWeaponSwitchOnUnitDeath(ASquadUnit* UnitThatDied);

private:
        bool GetIsValidSquadController() const;
        bool TryFindUnitWithLowestWeaponValue(const ASquadUnit* UnitThatDied, ASquadUnit*& OutLowestValueUnit) const;
        bool TrySwapWeapons(ASquadUnit* UnitThatDied, ASquadUnit* TargetUnit) const;
        /**
         * @brief Determines whether the candidate should receive the dead unit's weapon.
         * @param UnitThatDied The unit that lost its weapon.
         * @param CandidateUnit The prospective recipient.
         * @param OutCandidateWeaponValue Weapon value for the candidate (output).
         * @param DeadUnitWeaponValue Weapon value of the dead unit.
         * @param OutBestValue The current best (lowest) weapon value being tracked.
         * @return True when the candidate should swap because it holds the lowest weapon value so far.
         */
        bool ShouldSwapWithCandidate(const ASquadUnit* UnitThatDied, const ASquadUnit* CandidateUnit,
                                     int32& OutCandidateWeaponValue, const int32 DeadUnitWeaponValue,
                                     int32& OutBestValue) const;
        int32 GetWeaponValueForUnit(const ASquadUnit* SquadUnit) const;
        bool GetIsValidWeaponState(const ASquadUnit* SquadUnit) const;

        UPROPERTY()
        TWeakObjectPtr<ASquadController> M_SquadController;
};
