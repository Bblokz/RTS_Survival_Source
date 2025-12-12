// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SquadWeaponSwitch.generated.h"

class ASquadController;
class ASquadUnit;

USTRUCT()
/**
 * @brief Handles intra-squad weapon swapping logic when a squad unit dies.
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
        bool ShouldSwapWithCandidate(const ASquadUnit* UnitThatDied, const ASquadUnit* CandidateUnit,
                                     int32& OutCandidateWeaponValue, const int32 DeadUnitWeaponValue,
                                     int32& OutBestValue) const;
        int32 GetWeaponValueForUnit(const ASquadUnit* SquadUnit) const;
        bool GetIsValidWeaponState(const ASquadUnit* SquadUnit) const;

        UPROPERTY()
        TWeakObjectPtr<ASquadController> M_SquadController;
};
