// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourComp.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BehaviourCompModuleConstants
{
	// Bounds drain passes when removal/addition callbacks keep requesting newer module states;
	// anything left stays pending for the next safe boundary.
	constexpr int32 MaxModuleSlotDrainPasses = 8;
}

bool UBehaviourComp::IsModuleOwnedBehaviour(const UBehaviour* Behaviour) const
{
	const UVehicleModuleBehaviour* ModuleBehaviour = Cast<UVehicleModuleBehaviour>(Behaviour);
	return ModuleBehaviour != nullptr && ModuleBehaviour->GetIsModuleOwned();
}

bool UBehaviourComp::GetIsValidModuleSlotIndex(const int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < VehicleModuleBalance::MaxModuleInstances)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("UBehaviourComp: invalid vehicle module slot %d on %s."), SlotIndex, *GetNameSafe(GetOwner())));
	return false;
}

bool UBehaviourComp::GetCanApplyModuleBehaviourChangesNow() const
{
	return not bM_IsTickingBehaviours && not bM_IsApplyingModuleBehaviours;
}

void UBehaviourComp::SetModuleBehaviour(const int32 SlotIndex, const TSubclassOf<UVehicleModuleBehaviour> DesiredClass,
                                        const FVehicleModuleBehaviourContext& Context)
{
	if (not GetIsValidModuleSlotIndex(SlotIndex))
	{
		return;
	}
	FBehaviourCompModuleSlot& Slot = M_ModuleBehaviourSlots[SlotIndex];
	Slot.DesiredClass = DesiredClass;
	Slot.DesiredContext = Context;
	M_PendingModuleSlotMask |= 1u << SlotIndex;
}

void UBehaviourComp::RemoveModuleBehaviour(const int32 SlotIndex)
{
	if (not GetIsValidModuleSlotIndex(SlotIndex))
	{
		return;
	}
	SetModuleBehaviour(SlotIndex, nullptr, M_ModuleBehaviourSlots[SlotIndex].DesiredContext);
	CommitModuleBehaviourChanges();
}

bool UBehaviourComp::CommitModuleBehaviourChanges()
{
	if (not GetCanApplyModuleBehaviourChangesNow())
	{
		// Drained at the existing safe processing boundary after the behaviour tick.
		return false;
	}
	ApplyPendingModuleBehaviourSlots();
	return M_PendingModuleSlotMask == 0;
}

void UBehaviourComp::ApplyPendingModuleBehaviourSlots()
{
	if (M_PendingModuleSlotMask == 0 || bM_IsApplyingModuleBehaviours)
	{
		return;
	}

	{
		TGuardValue<bool> ApplyGuard(bM_IsApplyingModuleBehaviours, true);
		for (int32 DrainPass = 0; DrainPass < BehaviourCompModuleConstants::MaxModuleSlotDrainPasses
		     && M_PendingModuleSlotMask != 0; ++DrainPass)
		{
			ApplyModuleBehaviourSlotsInMask(M_PendingModuleSlotMask);
		}
	}
	UpdateComponentTickEnabled();
	// Behaviour-derived effects are committed; owners may now publish their deferred module callbacks.
	M_OnModuleBehavioursApplied.Broadcast();
}

void UBehaviourComp::ApplyModuleBehaviourSlotsInMask(const uint32 SlotMask)
{
	M_PendingModuleSlotMask &= ~SlotMask;
	for (int32 SlotIndex = 0; SlotIndex < VehicleModuleBalance::MaxModuleInstances; ++SlotIndex)
	{
		if ((SlotMask & (1u << SlotIndex)) != 0)
		{
			ApplyModuleBehaviourSlot(SlotIndex);
		}
	}
}

void UBehaviourComp::ApplyModuleBehaviourSlot(const int32 SlotIndex)
{
	FBehaviourCompModuleSlot& Slot = M_ModuleBehaviourSlots[SlotIndex];
	UVehicleModuleBehaviour* ExistingBehaviour = Slot.Instance;
	const bool bKeepsSameClass = IsValid(ExistingBehaviour) && Slot.DesiredClass != nullptr
		&& ExistingBehaviour->GetClass() == Slot.DesiredClass;
	if (bKeepsSameClass)
	{
		// Same class for the new state: update the context only; no remove/add or allocation.
		ExistingBehaviour->UpdateModuleContext(Slot.DesiredContext);
		return;
	}

	Slot.Instance = nullptr;
	if (IsValid(ExistingBehaviour))
	{
		// Exact instance; unrelated behaviours of the same class stay active.
		RemoveBehaviourInstance(ExistingBehaviour);
	}
	// OnRemoved may have requested a newer state for this slot; the drain loop applies that one instead.
	const bool bHasNewerRequest = (M_PendingModuleSlotMask & (1u << SlotIndex)) != 0;
	if (bHasNewerRequest || Slot.DesiredClass == nullptr)
	{
		return;
	}
	AddModuleBehaviourInstance(SlotIndex);
}

void UBehaviourComp::AddModuleBehaviourInstance(const int32 SlotIndex)
{
	FBehaviourCompModuleSlot& Slot = M_ModuleBehaviourSlots[SlotIndex];
	UVehicleModuleBehaviour* NewBehaviour = NewObject<UVehicleModuleBehaviour>(this, Slot.DesiredClass);
	if (not IsValid(NewBehaviour))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this, "NewBehaviour", "AddModuleBehaviourInstance", this);
		return;
	}
	// Module source identity and context are set before OnAdded.
	NewBehaviour->InitializeModuleContext(Slot.DesiredContext);
	Slot.Instance = NewBehaviour;
	AddInitialisedBehaviour(NewBehaviour);
	NotifyActionUIManagerOfBehaviourUpdate();
}

void UBehaviourComp::ClearModuleSlotForRemovedInstance(const UBehaviour* RemovedBehaviour)
{
	const UVehicleModuleBehaviour* ModuleBehaviour = Cast<UVehicleModuleBehaviour>(RemovedBehaviour);
	if (ModuleBehaviour == nullptr || not ModuleBehaviour->GetIsModuleOwned())
	{
		return;
	}
	const int32 SlotIndex = ModuleBehaviour->GetModuleContext().SlotIndex;
	if (SlotIndex < 0 || SlotIndex >= VehicleModuleBalance::MaxModuleInstances)
	{
		return;
	}
	if (M_ModuleBehaviourSlots[SlotIndex].Instance == ModuleBehaviour)
	{
		M_ModuleBehaviourSlots[SlotIndex].Instance = nullptr;
	}
}

void UBehaviourComp::RecreateModuleBehaviourSlots()
{
	TGuardValue<bool> ApplyGuard(bM_IsApplyingModuleBehaviours, true);
	for (int32 SlotIndex = 0; SlotIndex < VehicleModuleBalance::MaxModuleInstances; ++SlotIndex)
	{
		FBehaviourCompModuleSlot& Slot = M_ModuleBehaviourSlots[SlotIndex];
		UVehicleModuleBehaviour* ExistingBehaviour = Slot.Instance;
		if (not IsValid(ExistingBehaviour))
		{
			continue;
		}
		Slot.Instance = nullptr;
		RemoveBehaviourInstance(ExistingBehaviour);
		if (Slot.DesiredClass != nullptr)
		{
			AddModuleBehaviourInstance(SlotIndex);
		}
	}
	UpdateComponentTickEnabled();
}

void UBehaviourComp::ResetModuleBehaviourSlots()
{
	for (FBehaviourCompModuleSlot& Slot : M_ModuleBehaviourSlots)
	{
		Slot = FBehaviourCompModuleSlot();
	}
	M_PendingModuleSlotMask = 0;
}
