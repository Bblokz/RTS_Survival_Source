// Copyright ...
#pragma once

#include "CoreMinimal.h"

class UAircraftOwnerComp;
class AAircraftMaster;

/**
 * @brief Centralised repair controller for an airfield/airbase.
 *        Holds its own timer/state and heals *landed* aircraft at fixed ticks.
 *
 * The owner (UAircraftOwnerComp) only forwards lifecycle events:
 *  - InitAircraftRepair()
 *  - OnAnyAircraftLanded()
 *  - OnAnyAircraftVTO()
 *  - OnAirfieldDies()
 *  - OnAircraftDies()
 */
struct FAirfieldRepairManager
{
public:
	FAirfieldRepairManager() = default;

	/**
	 * @brief Initialise the manager with repair strength and owner.
	 * @param RepairStrengthMlt Multiplier applied on DeveloperSettings Repair::HpRepairedPerTick.
	 * @param Owner Owning AircraftOwner component.
	 */
	void InitAircraftRepair(const float RepairStrengthMlt, UAircraftOwnerComp* Owner);

	/** @brief Start ticking repairs if not already ticking (expects ≥1 landed aircraft). */
	void OnAnyAircraftLanded();

	/**
	 * @brief Stop ticking when any aircraft VTOs (design choice per spec).
	 * We will automatically restart on the next landed notification.
	 */
	void OnAnyAircraftVTO();

	/** @brief Stop ticking because the airfield/owner is going away. */
	void OnAirfieldDies();

	/**
	 * @brief When an aircraft dies: if we are ticking and none remain landed, stop ticking.
	 * If we are not ticking, do nothing.
	 */
	void OnAircraftDies();

private:
	// ---------- Core ticking ----------
	void StartRepairTick();
	void StopRepairTick();
	void OnRepairTick();

	// ---------- Helpers ----------
	bool GetIsValidOwner() const;
	bool HasAnyLanded() const;
	void HealAllLandedOnce() const;

private:
	/** Owning aircraft owner; required for world/timer/sockets access. */
	TWeakObjectPtr<UAircraftOwnerComp> M_Owner;

	/** Timer for periodic repair ticks. */
	FTimerHandle M_RepairTickHandle;

	/** Strength multiplier applied per aircraft, per tick. */
	float M_RepairStrengthMlt = 1.0f;

	/** True when our repeating timer is armed. */
	bool bM_IsTicking = false;
};
