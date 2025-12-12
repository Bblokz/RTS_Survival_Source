// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadWeaponSwitch.h"

#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

FSquadWeaponSwitch::FSquadWeaponSwitch()
{
}

void FSquadWeaponSwitch::Init(ASquadController* InSquadController)
{
        M_SquadController = InSquadController;
}

void FSquadWeaponSwitch::HandleWeaponSwitchOnUnitDeath(ASquadUnit* UnitThatDied)
{
        if (not GetIsValidSquadController())
        {
                return;
        }

        if (not IsValid(UnitThatDied))
        {
                return;
        }

        ASquadUnit* LowestValueUnit = nullptr;
        if (not TryFindUnitWithLowestWeaponValue(UnitThatDied, LowestValueUnit))
        {
                return;
        }

        TrySwapWeapons(UnitThatDied, LowestValueUnit);
}

bool FSquadWeaponSwitch::GetIsValidSquadController() const
{
        if (M_SquadController.IsValid())
        {
                return true;
        }
        return false;
}

bool FSquadWeaponSwitch::TryFindUnitWithLowestWeaponValue(const ASquadUnit* UnitThatDied, ASquadUnit*& OutLowestValueUnit) const
{
        OutLowestValueUnit = nullptr;
        if (not GetIsValidSquadController())
        {
                return false;
        }

        const int32 DeadUnitWeaponValue = GetWeaponValueForUnit(UnitThatDied);
        if (DeadUnitWeaponValue < 0)
        {
                return false;
        }

        int32 LowestWeaponValue = DeadUnitWeaponValue;
        for (ASquadUnit* SquadUnit : M_SquadController->M_TSquadUnits)
        {
                if (SquadUnit == UnitThatDied)
                {
                        continue;
                }

                int32 CandidateWeaponValue = -1;
                if (ShouldSwapWithCandidate(UnitThatDied, SquadUnit, CandidateWeaponValue, DeadUnitWeaponValue,
                                            LowestWeaponValue))
                {
                        LowestWeaponValue = CandidateWeaponValue;
                        OutLowestValueUnit = SquadUnit;
                }
        }

        return IsValid(OutLowestValueUnit);
}

bool FSquadWeaponSwitch::TrySwapWeapons(ASquadUnit* UnitThatDied, ASquadUnit* TargetUnit) const
{
        if (not IsValid(UnitThatDied) || not IsValid(TargetUnit))
        {
                return false;
        }

        return UnitThatDied->SwapWeaponsWithUnit(TargetUnit);
}

bool FSquadWeaponSwitch::ShouldSwapWithCandidate(const ASquadUnit* UnitThatDied, const ASquadUnit* CandidateUnit,
                                                 int32& OutCandidateWeaponValue, const int32 DeadUnitWeaponValue,
                                                 int32& OutBestValue) const
{
        OutCandidateWeaponValue = -1;
        if (not GetIsValidSquadController())
        {
                return false;
        }

        if (not M_SquadController->GetIsValidSquadUnit(CandidateUnit))
        {
                return false;
        }

        OutCandidateWeaponValue = GetWeaponValueForUnit(CandidateUnit);
        if (OutCandidateWeaponValue < 0)
        {
                return false;
        }

        if (OutCandidateWeaponValue >= DeadUnitWeaponValue)
        {
                return false;
        }

        const bool bIsLowerThanCurrentBest = OutCandidateWeaponValue < OutBestValue;
        if (bIsLowerThanCurrentBest)
        {
                return true;
        }

        return false;
}

int32 FSquadWeaponSwitch::GetWeaponValueForUnit(const ASquadUnit* SquadUnit) const
{
        if (not GetIsValidWeaponState(SquadUnit))
        {
                return -1;
        }

        const UWeaponState* WeaponState = SquadUnit->GetWeaponState();
        if (not IsValid(WeaponState))
        {
                return -1;
        }

        return Global_GetWeaponValue(WeaponState->GetRawWeaponData().WeaponName);
}

bool FSquadWeaponSwitch::GetIsValidWeaponState(const ASquadUnit* SquadUnit) const
{
        if (not M_SquadController.IsValid())
        {
                return false;
        }

        if (not M_SquadController->GetIsValidSquadUnit(SquadUnit))
        {
                return false;
        }

        if (not IsValid(SquadUnit))
        {
                return false;
        }

        return IsValid(SquadUnit->GetWeaponState());
}
