// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleModuleBalance.h"

/**
 * @brief Fixed storage for the state transitions of one committed mutation (damage, repair, load).
 * Large enough to restore every module in one call; copied by value before dispatch so re-entrant
 * mutations never write into a batch that is being published.
 */
struct FModuleChangeBatch
{
	FModuleStateChange Changes[VehicleModuleBalance::MaxModuleInstances];
	int32 Count = 0;

	/**
	 * @brief Merges repeated transitions of one instance so observers only see its final state.
	 * @param Change Transition to record; its PreviousState is kept from the first record of the slot.
	 */
	void AddOrMerge(const FModuleStateChange& Change)
	{
		for (int32 ChangeIndex = 0; ChangeIndex < Count; ++ChangeIndex)
		{
			FModuleStateChange& Existing = Changes[ChangeIndex];
			if (Existing.SlotIndex != Change.SlotIndex)
			{
				continue;
			}
			Existing.NewState = Change.NewState;
			Existing.CurrentHp = Change.CurrentHp;
			Existing.MaxHp = Change.MaxHp;
			Existing.Cause = Change.Cause;
			if (Existing.PreviousState == Existing.NewState)
			{
				RemoveAtSwap(ChangeIndex);
			}
			return;
		}
		if (Count >= VehicleModuleBalance::MaxModuleInstances)
		{
			return;
		}
		Changes[Count] = Change;
		++Count;
	}

	void Reset()
	{
		Count = 0;
	}

	bool IsEmpty() const
	{
		return Count == 0;
	}

	TConstArrayView<FModuleStateChange> GetChanges() const
	{
		return MakeArrayView(Changes, Count);
	}

private:
	void RemoveAtSwap(const int32 ChangeIndex)
	{
		const int32 LastIndex = Count - 1;
		if (ChangeIndex != LastIndex)
		{
			Changes[ChangeIndex] = Changes[LastIndex];
		}
		Count = LastIndex;
	}
};

/** @brief Worst installed state per module type (Destroyed > Damaged > Healthy), indexed by EVehicleModuleTypes. */
struct FModuleIconStates
{
	EVehicleModuleState States[VehicleModuleBalance::ModuleTypeCount] = {};

	bool GetHasAnyNonHealthyType() const
	{
		for (const EVehicleModuleState State : States)
		{
			if (State != EVehicleModuleState::Healthy)
			{
				return true;
			}
		}
		return false;
	}
};

/** @brief Aggregate icon states plus a bitmask of the module types whose aggregate state changed. */
struct FModuleIconDeltaBatch
{
	FModuleIconStates IconStates;
	uint32 ChangedTypeMask = 0;

	bool IsEmpty() const
	{
		return ChangedTypeMask == 0;
	}
};
