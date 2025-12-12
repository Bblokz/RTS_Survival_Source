// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "FGameUnitAddRemoveDelegates.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

namespace
{
	static void ReportInvalidArgs(const TCHAR* Where)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Invalid args in %s: Listener or Handler invalid."), Where));
	}
}

// Small helper referenced from the header inline template (link target must exist in a TU).
void RTS_ReportDelegateTypeMismatch(const UObject* Context, const TCHAR* TypeName)
{
	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("FGameUnitAddRemoveDelegates: Type cast failed while notifying (%s). Context: %s"),
		                TypeName,
		                IsValid(Context) ? *Context->GetName() : TEXT("null")));
}

// ---------------- Registration (non-templated bridges) ----------------

bool FGameUnitAddRemoveDelegates::RegisterListener(UObject* Listener,
                                                   const int32 PlayerIndex,
                                                   TFunction<void(ASquadUnit*, bool)> Handler)
{
	if (not IsValid(Listener) || !Handler)
	{
		ReportInvalidArgs(TEXT("RegisterListener(ASquadUnit)"));
		return false;
	}
	return RegisterListenerInternal<ASquadUnit>(M_SquadListenersPerPlayer, Listener, PlayerIndex, MoveTemp(Handler));
}

bool FGameUnitAddRemoveDelegates::RegisterListener(UObject* Listener,
                                                   const int32 PlayerIndex,
                                                   TFunction<void(ATankMaster*, bool)> Handler)
{
	if (not IsValid(Listener) || !Handler)
	{
		ReportInvalidArgs(TEXT("RegisterListener(ATankMaster)"));
		return false;
	}
	return RegisterListenerInternal<ATankMaster>(M_TankListenersPerPlayer, Listener, PlayerIndex, MoveTemp(Handler));
}

bool FGameUnitAddRemoveDelegates::RegisterListener(UObject* Listener,
                                                   const int32 PlayerIndex,
                                                   TFunction<void(AAircraftMaster*, bool)> Handler)
{
	if (not IsValid(Listener) || !Handler)
	{
		ReportInvalidArgs(TEXT("RegisterListener(AAircraftMaster)"));
		return false;
	}
	return RegisterListenerInternal<AAircraftMaster>(M_AircraftListenersPerPlayer, Listener, PlayerIndex, MoveTemp(Handler));
}

bool FGameUnitAddRemoveDelegates::RegisterListener(UObject* Listener,
                                                   const int32 PlayerIndex,
                                                   TFunction<void(AActor*, bool)> Handler)
{
	if (not IsValid(Listener) || !Handler)
	{
		ReportInvalidArgs(TEXT("RegisterListener(AActor)"));
		return false;
	}
	return RegisterListenerInternal<AActor>(M_ActorListenersPerPlayer, Listener, PlayerIndex, MoveTemp(Handler));
}

bool FGameUnitAddRemoveDelegates::RegisterListener(UObject* Listener,
                                                   const int32 PlayerIndex,
                                                   TFunction<void(ABuildingExpansion*, bool)> Handler)
{
	if (not IsValid(Listener) || !Handler)
	{
		RTSFunctionLibrary::ReportError(TEXT("Invalid args in RegisterListener(ABuildingExpansion)."));
		return false;
	}
	return RegisterListenerInternal<ABuildingExpansion>(M_BxpListenersPerPlayer, Listener, PlayerIndex, MoveTemp(Handler));
}


// ---------------- Unregistration ----------------

bool FGameUnitAddRemoveDelegates::UnregisterFrom(TMap<int32, FListenerBucket>& PerPlayer,
                                                 UObject* Listener,
                                                 const int32 PlayerIndex)
{
	if (not IsValid(Listener))
	{
		return false;
	}

	FListenerBucket* BucketPtr = PerPlayer.Find(PlayerIndex);
	if (not BucketPtr)
	{
		return false;
	}

	TArray<FListenerEntry>& Entries = BucketPtr->Entries;

	// Remove all entries for this listener (should be 0 or 1 due to replace-on-register).
	int32 RemovedCount = 0;
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Entries[Index].Listener == Listener)
		{
			Entries.RemoveAtSwap(Index);
			++RemovedCount;
		}
	}
	return RemovedCount > 0;
}

bool FGameUnitAddRemoveDelegates::UnregisterBxpListener(UObject* Listener, const int32 PlayerIndex)
{
	return UnregisterFrom(M_BxpListenersPerPlayer, Listener, PlayerIndex);
}

void FGameUnitAddRemoveDelegates::NotifyBxp(const int32 PlayerIndex, ABuildingExpansion* Bxp, const bool bWasAdded)
{
	NotifyImpl<ABuildingExpansion>(M_BxpListenersPerPlayer, PlayerIndex, Bxp, bWasAdded);
}


bool FGameUnitAddRemoveDelegates::UnregisterSquadListener(UObject* Listener, const int32 PlayerIndex)
{
	return UnregisterFrom(M_SquadListenersPerPlayer, Listener, PlayerIndex);
}

bool FGameUnitAddRemoveDelegates::UnregisterTankListener(UObject* Listener, const int32 PlayerIndex)
{
	return UnregisterFrom(M_TankListenersPerPlayer, Listener, PlayerIndex);
}

bool FGameUnitAddRemoveDelegates::UnregisterAircraftListener(UObject* Listener, const int32 PlayerIndex)
{
	return UnregisterFrom(M_AircraftListenersPerPlayer, Listener, PlayerIndex);
}

bool FGameUnitAddRemoveDelegates::UnregisterActorListener(UObject* Listener, const int32 PlayerIndex)
{
	return UnregisterFrom(M_ActorListenersPerPlayer, Listener, PlayerIndex);
}

bool FGameUnitAddRemoveDelegates::UnregisterAllForPlayer(UObject* Listener, const int32 PlayerIndex)
{
	bool bAny = false;
	bAny = UnregisterFrom(M_SquadListenersPerPlayer,    Listener, PlayerIndex) || bAny;
	bAny = UnregisterFrom(M_TankListenersPerPlayer,     Listener, PlayerIndex) || bAny;
	bAny = UnregisterFrom(M_AircraftListenersPerPlayer, Listener, PlayerIndex) || bAny;
	bAny = UnregisterFrom(M_ActorListenersPerPlayer,    Listener, PlayerIndex) || bAny;
	// NEW:
	bAny = UnregisterFrom(M_BxpListenersPerPlayer,      Listener, PlayerIndex) || bAny;
	return bAny;
}

// ---------------- Protected notifiers (thin wrappers over the generic implementation) ----------------

void FGameUnitAddRemoveDelegates::NotifySquad(const int32 PlayerIndex, ASquadUnit* Unit, const bool bWasAdded)
{
	NotifyImpl<ASquadUnit>(M_SquadListenersPerPlayer, PlayerIndex, Unit, bWasAdded);
}

void FGameUnitAddRemoveDelegates::NotifyTank(const int32 PlayerIndex, ATankMaster* Unit, const bool bWasAdded)
{
	NotifyImpl<ATankMaster>(M_TankListenersPerPlayer, PlayerIndex, Unit, bWasAdded);
}

void FGameUnitAddRemoveDelegates::NotifyAircraft(const int32 PlayerIndex, AAircraftMaster* Unit, const bool bWasAdded)
{
	NotifyImpl<AAircraftMaster>(M_AircraftListenersPerPlayer, PlayerIndex, Unit, bWasAdded);
}

void FGameUnitAddRemoveDelegates::NotifyActor(const int32 PlayerIndex, AActor* Actor, const bool bWasAdded)
{
	NotifyImpl<AActor>(M_ActorListenersPerPlayer, PlayerIndex, Actor, bWasAdded);
}
