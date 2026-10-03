// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Commands.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// ----------------------------------------------------------------------------------------------------
// Ability suppression
// ----------------------------------------------------------------------------------------------------

FAbilitySuppressionHandle UCommandData::BeginAbilitySuppression(UObject* Source,
                                                                const TConstArrayView<EAbilityID> AbilityIds)
{
	FAbilitySuppressionHandle Handle;
	if (not IsValid(Source))
	{
		RTSFunctionLibrary::ReportError(TEXT("BeginAbilitySuppression: invalid suppression source."));
		return Handle;
	}

	ReleaseStaleAbilitySuppressions();
	const int32 SourceIndex = FindFreeSuppressionSourceIndex();
	if (SourceIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("BeginAbilitySuppression: no free suppression source slot for ")
			+ Source->GetName());
		return Handle;
	}

	FAbilitySuppressionSource& SuppressionSource = M_AbilitySuppressionSources[SourceIndex];
	SuppressionSource.Source = Source;
	SuppressionSource.AbilityCount = 0;
	for (const EAbilityID AbilityId : AbilityIds)
	{
		const bool bCanStore = SuppressionSource.AbilityCount < FAbilitySuppressionSource::MaxSuppressedAbilities;
		if (AbilityId == EAbilityID::IdNoAbility || SuppressionSource.GetSuppresses(AbilityId) || not bCanStore)
		{
			continue;
		}
		SuppressionSource.AbilityIds[SuppressionSource.AbilityCount] = AbilityId;
		++SuppressionSource.AbilityCount;
	}
	SuppressionSource.bIsActive = true;
	++SuppressionSource.Generation;
	ApplySuppressionSourceToCard(SourceIndex);

	Handle.SourceIndex = SourceIndex;
	Handle.Generation = SuppressionSource.Generation;
	return Handle;
}

void UCommandData::EndAbilitySuppression(FAbilitySuppressionHandle& InOutHandle)
{
	const int32 SourceIndex = InOutHandle.SourceIndex;
	const bool bIsKnownSource = SourceIndex >= 0 && SourceIndex < MaxAbilitySuppressionSources;
	if (bIsKnownSource && M_AbilitySuppressionSources[SourceIndex].bIsActive
		&& M_AbilitySuppressionSources[SourceIndex].Generation == InOutHandle.Generation)
	{
		EndSuppressionSource(SourceIndex);
	}
	InOutHandle.Reset();
}

void UCommandData::EndSuppressionSource(const int32 SourceIndex)
{
	const uint32 SourceBit = 1u << SourceIndex;
	for (int32 CardIndex = 0; CardIndex < DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI; ++CardIndex)
	{
		FAbilitySuppressionSlot& Slot = M_AbilitySuppressionSlots[CardIndex];
		if ((Slot.OwnerMask & SourceBit) == 0)
		{
			continue;
		}
		Slot.OwnerMask &= ~SourceBit;
		// Revealed only when no other source still hides this entry.
		if (Slot.OwnerMask == 0)
		{
			RestoreSuppressedCardSlot(CardIndex);
		}
	}

	FAbilitySuppressionSource& SuppressionSource = M_AbilitySuppressionSources[SourceIndex];
	SuppressionSource.bIsActive = false;
	SuppressionSource.Source.Reset();
	SuppressionSource.AbilityCount = 0;
	++SuppressionSource.Generation;
}

void UCommandData::ApplySuppressionSourceToCard(const int32 SourceIndex)
{
	const int32 NumCardSlots = FMath::Min(M_Abilities.Num(), DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI);
	for (int32 CardIndex = 0; CardIndex < NumCardSlots; ++CardIndex)
	{
		if (not GetIsReservedCrewRepairCardIndex(CardIndex))
		{
			HideCardSlotForSource(CardIndex, SourceIndex);
		}
	}
}

void UCommandData::HideCardSlotForSource(const int32 CardIndex, const int32 SourceIndex)
{
	const FAbilitySuppressionSource& SuppressionSource = M_AbilitySuppressionSources[SourceIndex];
	FAbilitySuppressionSlot& Slot = M_AbilitySuppressionSlots[CardIndex];
	const uint32 SourceBit = 1u << SourceIndex;
	if (Slot.OwnerMask != 0)
	{
		// Already hidden by another source: share ownership so both must release it.
		if (SuppressionSource.GetSuppresses(Slot.BackingEntry.AbilityId))
		{
			Slot.OwnerMask |= SourceBit;
		}
		return;
	}
	if (not SuppressionSource.GetSuppresses(M_Abilities[CardIndex].AbilityId))
	{
		return;
	}
	Slot.BackingEntry = M_Abilities[CardIndex];
	Slot.OwnerMask = SourceBit;
	M_Abilities[CardIndex] = FUnitAbilityEntry();
}

void UCommandData::RestoreSuppressedCardSlot(const int32 CardIndex)
{
	const FUnitAbilityEntry BackingEntry = M_AbilitySuppressionSlots[CardIndex].BackingEntry;
	M_AbilitySuppressionSlots[CardIndex] = FAbilitySuppressionSlot();
	// Entries revoked while hidden were already cleared and stay revoked.
	if (BackingEntry.AbilityId == EAbilityID::IdNoAbility)
	{
		return;
	}
	if (M_Abilities.IsValidIndex(CardIndex) && M_Abilities[CardIndex].AbilityId == EAbilityID::IdNoAbility)
	{
		M_Abilities[CardIndex] = BackingEntry;
		return;
	}
	// The original index was reused; keep the grant in the first free slot instead of overwriting it.
	(void)PlaceAbilityInFreeGenericSlot(BackingEntry);
}

void UCommandData::RebuildAbilitySuppressionsForNewCard()
{
	for (FAbilitySuppressionSlot& Slot : M_AbilitySuppressionSlots)
	{
		Slot = FAbilitySuppressionSlot();
	}
	ReleaseStaleAbilitySuppressions();
	for (int32 SourceIndex = 0; SourceIndex < MaxAbilitySuppressionSources; ++SourceIndex)
	{
		if (M_AbilitySuppressionSources[SourceIndex].bIsActive)
		{
			ApplySuppressionSourceToCard(SourceIndex);
		}
	}
}

void UCommandData::ReleaseStaleAbilitySuppressions()
{
	for (int32 SourceIndex = 0; SourceIndex < MaxAbilitySuppressionSources; ++SourceIndex)
	{
		const FAbilitySuppressionSource& SuppressionSource = M_AbilitySuppressionSources[SourceIndex];
		// A destroyed owner can no longer release its suppression; release it on its behalf.
		if (SuppressionSource.bIsActive && not SuppressionSource.Source.IsValid())
		{
			EndSuppressionSource(SourceIndex);
		}
	}
}

void UCommandData::HideNewlyAddedAbilityIfSuppressed(const int32 CardIndex)
{
	if (not M_Abilities.IsValidIndex(CardIndex) || CardIndex >= DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI
		|| GetIsReservedCrewRepairCardIndex(CardIndex))
	{
		return;
	}
	for (int32 SourceIndex = 0; SourceIndex < MaxAbilitySuppressionSources; ++SourceIndex)
	{
		if (M_AbilitySuppressionSources[SourceIndex].bIsActive)
		{
			HideCardSlotForSource(CardIndex, SourceIndex);
		}
	}
}

int32 UCommandData::FindFreeSuppressionSourceIndex() const
{
	for (int32 SourceIndex = 0; SourceIndex < MaxAbilitySuppressionSources; ++SourceIndex)
	{
		if (not M_AbilitySuppressionSources[SourceIndex].bIsActive)
		{
			return SourceIndex;
		}
	}
	return INDEX_NONE;
}

int32 UCommandData::FindSuppressedBackingIndex(const EAbilityID AbilityId, const int32 CustomType) const
{
	for (int32 CardIndex = 0; CardIndex < DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI; ++CardIndex)
	{
		const FAbilitySuppressionSlot& Slot = M_AbilitySuppressionSlots[CardIndex];
		const bool bMatchesCustomType = CustomType == INDEX_NONE || Slot.BackingEntry.CustomType == CustomType;
		if (Slot.OwnerMask != 0 && Slot.BackingEntry.AbilityId == AbilityId && bMatchesCustomType)
		{
			return CardIndex;
		}
	}
	return INDEX_NONE;
}

bool UCommandData::GetIsCardSlotSuppressed(const int32 CardIndex) const
{
	return CardIndex >= 0 && CardIndex < DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI
		&& M_AbilitySuppressionSlots[CardIndex].OwnerMask != 0;
}

bool UCommandData::GetIsReservedCrewRepairCardIndex(const int32 CardIndex) const
{
	return bM_IsCrewRepairSlotReserved && CardIndex == VehicleModuleBalance::CrewRepairAbilitySlotIndex;
}

bool UCommandData::GetIsCardSlotAvailableForGenericAbility(const int32 CardIndex) const
{
	return M_Abilities.IsValidIndex(CardIndex) && M_Abilities[CardIndex].AbilityId == EAbilityID::IdNoAbility
		&& not GetIsCardSlotSuppressed(CardIndex) && not GetIsReservedCrewRepairCardIndex(CardIndex);
}

bool UCommandData::GetHasDuplicateAbilityEntry(const FUnitAbilityEntry& NewAbility) const
{
	const bool bIsOnEffectiveCard = M_Abilities.ContainsByPredicate(
		[&NewAbility](const FUnitAbilityEntry& AbilityEntry)
		{
			return AbilityEntry.AbilityId == NewAbility.AbilityId && AbilityEntry.CustomType == NewAbility.CustomType;
		});
	return bIsOnEffectiveCard || FindSuppressedBackingIndex(NewAbility.AbilityId, NewAbility.CustomType) != INDEX_NONE;
}

int32 UCommandData::FindFreeGenericCardSlot() const
{
	for (int32 CardIndex = 0; CardIndex < M_Abilities.Num(); ++CardIndex)
	{
		if (GetIsCardSlotAvailableForGenericAbility(CardIndex))
		{
			return CardIndex;
		}
	}
	return INDEX_NONE;
}

bool UCommandData::PlaceAbilityInFreeGenericSlot(const FUnitAbilityEntry& NewAbility)
{
	const int32 FreeIndex = FindFreeGenericCardSlot();
	if (FreeIndex == INDEX_NONE)
	{
		return false;
	}
	M_Abilities[FreeIndex] = NewAbility;
	HideNewlyAddedAbilityIfSuppressed(FreeIndex);
	return true;
}

bool UCommandData::TryRemoveSuppressedBackingEntry(const EAbilityID AbilityToRemove, const int32 CustomType)
{
	const int32 CardIndex = FindSuppressedBackingIndex(AbilityToRemove, CustomType);
	if (CardIndex == INDEX_NONE)
	{
		return false;
	}
	M_AbilitySuppressionSlots[CardIndex] = FAbilitySuppressionSlot();
	return true;
}

bool UCommandData::TrySwapSuppressedBackingEntry(const EAbilityID OldAbility, const int32 OldCustomType,
                                                 const FUnitAbilityEntry& NewAbility)
{
	const int32 CardIndex = FindSuppressedBackingIndex(OldAbility, OldCustomType);
	if (CardIndex == INDEX_NONE)
	{
		return false;
	}
	M_AbilitySuppressionSlots[CardIndex].BackingEntry = NewAbility;
	return true;
}

bool UCommandData::GetHasSuppressedAbilityOnCooldown() const
{
	for (const FAbilitySuppressionSlot& Slot : M_AbilitySuppressionSlots)
	{
		if (Slot.OwnerMask != 0 && Slot.BackingEntry.CooldownRemaining > 0)
		{
			return true;
		}
	}
	return false;
}

bool UCommandData::TickSuppressedAbilityCooldowns()
{
	bool bHasAnyCooldown = false;
	for (FAbilitySuppressionSlot& Slot : M_AbilitySuppressionSlots)
	{
		if (Slot.OwnerMask == 0 || Slot.BackingEntry.CooldownRemaining <= 0)
		{
			continue;
		}
		Slot.BackingEntry.CooldownRemaining = FMath::Max(Slot.BackingEntry.CooldownRemaining - 1, 0);
		bHasAnyCooldown |= Slot.BackingEntry.CooldownRemaining > 0;
	}
	return bHasAnyCooldown;
}

// ----------------------------------------------------------------------------------------------------
// CrewRepair command card
// ----------------------------------------------------------------------------------------------------

bool UCommandData::ReserveCrewRepairAbilitySlot()
{
	if (bM_IsCrewRepairSlotReserved)
	{
		return true;
	}
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	if (M_Abilities.IsValidIndex(ReservedIndex) && M_Abilities[ReservedIndex].AbilityId != EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::ReportError(TEXT("ReserveCrewRepairAbilitySlot: the final command-card slot is used by ")
			+ Global_GetAbilityIDAsString(M_Abilities[ReservedIndex].AbilityId)
			+ TEXT("; CrewRepair is unavailable on ") + (M_Owner ? M_Owner->GetOwnerName() : FString("Unknown Owner")));
		return false;
	}
	// Sized once to the card capacity so the reserved index always exists; indices of other entries never move.
	if (M_Abilities.Num() < DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI)
	{
		M_Abilities.SetNum(DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI);
	}
	bM_IsCrewRepairSlotReserved = true;
	return true;
}

bool UCommandData::SetCrewRepairAbilityEntry(const FUnitAbilityEntry& NewEntry)
{
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	if (not bM_IsCrewRepairSlotReserved || not M_Abilities.IsValidIndex(ReservedIndex))
	{
		return false;
	}
	const bool bIsValidEntry = NewEntry.AbilityId == EAbilityID::IdNoAbility
		|| NewEntry.AbilityId == EAbilityID::IdCrewRepair;
	if (not bIsValidEntry)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetCrewRepairAbilityEntry: only CrewRepair may use the reserved slot."));
		return false;
	}
	const FUnitAbilityEntry& CurrentEntry = M_Abilities[ReservedIndex];
	if (CurrentEntry.AbilityId == NewEntry.AbilityId && CurrentEntry.CustomType == NewEntry.CustomType)
	{
		return false;
	}
	M_Abilities[ReservedIndex] = NewEntry;
	return true;
}

FUnitAbilityEntry UCommandData::GetCrewRepairAbilityEntry() const
{
	constexpr int32 ReservedIndex = VehicleModuleBalance::CrewRepairAbilitySlotIndex;
	if (not bM_IsCrewRepairSlotReserved || not M_Abilities.IsValidIndex(ReservedIndex))
	{
		return FUnitAbilityEntry();
	}
	return M_Abilities[ReservedIndex];
}

bool UCommandData::GetIsQueuedCrewRepairStillAllowed(const FQueueCommand& QueuedCommand)
{
	if (not QueuedCommand.GetHasValidCrewRepairAbilitySubtype())
	{
		return false;
	}
	// A queued Enable is skipped quietly once no damaged or destroyed modules remain (the card entry was cleared).
	const FUnitAbilityEntry* CrewRepairEntry = GetAbilityEntryOfCustomType(EAbilityID::IdCrewRepair,
	                                                                     QueuedCommand.CustomType);
	return CrewRepairEntry != nullptr && CrewRepairEntry->CooldownRemaining <= 0;
}

// ----------------------------------------------------------------------------------------------------
// ICommands CrewRepair API
// ----------------------------------------------------------------------------------------------------

ECommandQueueError ICommands::CrewRepair(const ECrewRepairAbilityType Subtype, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	// Requires the exact card entry; CrewRepair is not a command-card exception.
	if (UnitCommandData->GetAbilityEntryOfCustomType(EAbilityID::IdCrewRepair, static_cast<int32>(Subtype)) == nullptr)
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (Subtype == ECrewRepairAbilityType::DisableRepair)
	{
		// Immediate control action: never queued behind the long-running Enable command.
		ExecuteCrewRepairCommand(ECrewRepairAbilityType::DisableRepair);
		return ECommandQueueError::NoError;
	}
	if (Subtype != ECrewRepairAbilityType::EnableRepair || GetIsCrewRepairActive()
		|| UnitCommandData->GetHasCommandInQueue(EAbilityID::IdCrewRepair))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	return UnitCommandData->AddAbilityToTCommands(EAbilityID::IdCrewRepair, FVector::ZeroVector, nullptr,
	                                              FRotator::ZeroRotator, static_cast<int32>(Subtype));
}

void ICommands::ExecuteCrewRepairCommand(const ECrewRepairAbilityType Subtype)
{
	// Unsupported owners reject safely without stalling the queue.
	if (Subtype == ECrewRepairAbilityType::EnableRepair)
	{
		DoneExecutingCommand(EAbilityID::IdCrewRepair);
	}
}

void ICommands::TerminateCrewRepairCommand(const ECrewRepairAbilityType Subtype)
{
}

void ICommands::TerminateCrewRepairCommandForCurrentQueue()
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	const FQueueCommand* CurrentCommand = IsValid(UnitCommandData) ? UnitCommandData->GetCurrentQueuedCommand() : nullptr;
	const ECrewRepairAbilityType Subtype = CurrentCommand != nullptr
		                                       ? CurrentCommand->GetCrewRepairAbilitySubtype()
		                                       : ECrewRepairAbilityType::EnableRepair;
	TerminateCrewRepairCommand(Subtype);
}
