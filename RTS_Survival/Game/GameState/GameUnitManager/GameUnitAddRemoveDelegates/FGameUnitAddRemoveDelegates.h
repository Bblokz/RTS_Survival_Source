// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FGameUnitAddRemoveDelegates.generated.h"

class ASquadUnit;
class ATankMaster;
class AAircraftMaster;
class AActor;
class ABuildingExpansion;

/** Type-erased listener stored internally; callback takes (AActor*, bool). */
USTRUCT()
struct FListenerEntry
{
	GENERATED_BODY()

	/** The subscriber (weak for safety in async/timers). */
	UPROPERTY()
	TWeakObjectPtr<UObject> Listener;

	/** Erased/bridged callback; we cast AActor* back to the specific type. (Not a UPROPERTY) */
	TFunction<void(AActor*, bool)> Callback;

	[[nodiscard]] bool IsUsable() const
	{
		return Listener.IsValid() && static_cast<bool>(Callback);
	}
};

/** Wrapper so we can use it as a TMap value type under UPROPERTY. */
USTRUCT()
struct FListenerBucket
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FListenerEntry> Entries;
};

/**
 * @brief Central registry for per-player unit add/remove notifications.
 * Objects register once per type+player; they will be notified on both additions and removals.
 *
 * Why: decouple unit tracking from consumers without proliferating per-type delegate boilerplate.
 */
USTRUCT()
struct FGameUnitAddRemoveDelegates
{
	GENERATED_BODY()

public:
	/**
	 * @brief Register to be notified when squad units for a specific player are added/removed.
	 * @param Listener The object subscribing (held weakly; not kept alive).
	 * @param PlayerIndex Only events for this player index are forwarded.
	 * @param Handler A callable of signature void(ASquadUnit* Unit, bool bWasAdded).
	 * @return true if registered (or replaced), false on invalid args.
	 */
	bool RegisterListener(UObject* Listener,
	                      int32 PlayerIndex,
	                      TFunction<void(ASquadUnit*, bool)> Handler);

	/** @brief Register using a member method: void MyClass::OnSquadChanged(ASquadUnit*, bool). */
	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ASquadUnit*, bool));

	/** @brief Const-member version for squad units. */
	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ASquadUnit*, bool) const);

	/**
	 * @brief Register to be notified when tank units for a specific player are added/removed.
	 * @param Listener The object subscribing (held weakly; not kept alive).
	 * @param PlayerIndex Only events for this player index are forwarded.
	 * @param Handler A callable of signature void(ATankMaster* Unit, bool bWasAdded).
	 * @return true if registered (or replaced), false on invalid args.
	 */
	bool RegisterListener(UObject* Listener,
	                      int32 PlayerIndex,
	                      TFunction<void(ATankMaster*, bool)> Handler);

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ATankMaster*, bool));

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ATankMaster*, bool) const);


	/**
	 * @brief Register to be notified when aircraft units for a specific player are added/removed.
	 * @param Listener The object subscribing (held weakly; not kept alive).
	 * @param PlayerIndex Only events for this player index are forwarded.
	 * @param Handler A callable of signature void(AAircraftMaster* Unit, bool bWasAdded).
	 * @return true if registered (or replaced), false on invalid args.
	 */
	bool RegisterListener(UObject* Listener,
	                      int32 PlayerIndex,
	                      TFunction<void(AAircraftMaster*, bool)> Handler);

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(AAircraftMaster*, bool));

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(AAircraftMaster*, bool) const);

	/**
	 * @brief Register to be notified when generic actors for a specific player are added/removed.
	 * @param Listener The object subscribing (held weakly; not kept alive).
	 * @param PlayerIndex Only events for this player index are forwarded.
	 * @param Handler A callable of signature void(AActor* Actor, bool bWasAdded).
	 * @return true if registered (or replaced), false on invalid args.
	 */
	bool RegisterListener(UObject* Listener,
	                      int32 PlayerIndex,
	                      TFunction<void(AActor*, bool)> Handler);

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(AActor*, bool));

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(AActor*, bool) const);

	//  register BXP (callable)
	bool RegisterListener(UObject* Listener,
	                      int32 PlayerIndex,
	                      TFunction<void(ABuildingExpansion*, bool)> Handler);

	//  register BXP (member method)
	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ABuildingExpansion*, bool));

	template <typename TObject>
	bool RegisterListener(TObject* Listener,
	                      int32 PlayerIndex,
	                      void (TObject::*Method)(ABuildingExpansion*, bool) const);

	bool UnregisterBxpListener(UObject* Listener, int32 PlayerIndex);

	// NEW (protected): notify BXP listeners
	void NotifyBxp(int32 PlayerIndex, ABuildingExpansion* Bxp, bool bWasAdded);

	/**
	 * @brief Unregister a previously registered listener for Squad notifications at PlayerIndex.
	 * @param Listener The object to remove from the registry.
	 * @param PlayerIndex The player bucket to remove from.
	 * @return true if an entry was removed, false otherwise.
	 */
	bool UnregisterSquadListener(UObject* Listener, int32 PlayerIndex);

	/** @brief Unregister a previously registered listener for Tank notifications at PlayerIndex. */
	bool UnregisterTankListener(UObject* Listener, int32 PlayerIndex);

	/** @brief Unregister a previously registered listener for Aircraft notifications at PlayerIndex. */
	bool UnregisterAircraftListener(UObject* Listener, int32 PlayerIndex);

	/** @brief Unregister a previously registered listener for generic Actor notifications at PlayerIndex. */
	bool UnregisterActorListener(UObject* Listener, int32 PlayerIndex);

	/**
	 * @brief Convenience: remove this listener from all types for a given player.
	 * @param Listener The object to remove.
	 * @param PlayerIndex The player bucket to remove from.
	 * @return true if at least one entry was removed.
	 */
	bool UnregisterAllForPlayer(UObject* Listener, int32 PlayerIndex);

protected:
	// UGameUnitManager is the only writer that should fire notifications.
	friend class UGameUnitManager;

	/** @brief Notify all listeners for squad units of PlayerIndex. */
	void NotifySquad(int32 PlayerIndex, ASquadUnit* Unit, bool bWasAdded);

	/** @brief Notify all listeners for tank units of PlayerIndex. */
	void NotifyTank(int32 PlayerIndex, ATankMaster* Unit, bool bWasAdded);

	/** @brief Notify all listeners for aircraft units of PlayerIndex. */
	void NotifyAircraft(int32 PlayerIndex, AAircraftMaster* Unit, bool bWasAdded);

	/** @brief Notify all listeners for generic actors of PlayerIndex. */
	void NotifyActor(int32 PlayerIndex, AActor* Actor, bool bWasAdded);

private:
	// Per-player listener buckets per unit category.
	UPROPERTY()
	TMap<int32, FListenerBucket> M_SquadListenersPerPlayer;
	UPROPERTY()
	TMap<int32, FListenerBucket> M_TankListenersPerPlayer;
	UPROPERTY()
	TMap<int32, FListenerBucket> M_AircraftListenersPerPlayer;
	UPROPERTY()
	TMap<int32, FListenerBucket> M_ActorListenersPerPlayer;

	UPROPERTY()
	TMap<int32, FListenerBucket> M_BxpListenersPerPlayer;

private:
	// ---------- Internal helpers (templated; defined inline in the header) ----------

	template <typename TUnit>
	bool RegisterListenerInternal(TMap<int32, FListenerBucket>& PerPlayer,
	                              UObject* Listener,
	                              int32 PlayerIndex,
	                              TFunction<void(TUnit*, bool)>&& TypedHandler);

	template <typename TObject, typename TUnit, typename TMethodPtr>
	bool RegisterListenerMemberInternal(TMap<int32, FListenerBucket>& PerPlayer,
	                                    TObject* Listener,
	                                    int32 PlayerIndex,
	                                    TMethodPtr Method);


	template <typename TUnit>
	void NotifyImpl(TMap<int32, FListenerBucket>& PerPlayer,
	                int32 PlayerIndex,
	                TUnit* Unit,
	                bool bWasAdded);

	/** @brief Remove one listener from a single per-player bucket map. */
	bool UnregisterFrom(TMap<int32, FListenerBucket>& PerPlayer, UObject* Listener, int32 PlayerIndex);
};

// -------------------- Inline template implementations --------------------

template <typename TUnit>
bool FGameUnitAddRemoveDelegates::RegisterListenerInternal(TMap<int32, FListenerBucket>& PerPlayer,
                                                           UObject* Listener,
                                                           const int32 PlayerIndex,
                                                           TFunction<void(TUnit*, bool)>&& TypedHandler)
{
	if (not IsValid(Listener))
	{
		return false;
	}

	FListenerEntry NewEntry;
	NewEntry.Listener = Listener;
	// Bridge typed handler to the erased AActor* signature.
	NewEntry.Callback = [WeakListener = TWeakObjectPtr<UObject>(Listener),
			Handler = MoveTemp(TypedHandler)](AActor* AsActor, const bool bWasAdded)
		{
			if (not WeakListener.IsValid())
			{
				return;
			}
			TUnit* AsSpecific = Cast<TUnit>(AsActor);
			if (not IsValid(AsSpecific))
			{
				// Defensive: this should never happen if fired correctly.
				extern void RTS_ReportDelegateTypeMismatch(const UObject* Context, const TCHAR* TypeName);
				// defined in .cpp
				RTS_ReportDelegateTypeMismatch(WeakListener.Get(), TEXT("RegisterListenerInternal"));
				return;
			}
			Handler(AsSpecific, bWasAdded);
		};

	FListenerBucket& Bucket = PerPlayer.FindOrAdd(PlayerIndex);
	TArray<FListenerEntry>& Entries = Bucket.Entries;

	// Replace existing entry for the same listener (avoid duplicates).
	for (FListenerEntry& Entry : Entries)
	{
		if (Entry.Listener == Listener)
		{
			Entry.Callback = MoveTemp(NewEntry.Callback);
			return true;
		}
	}

	Entries.Add(MoveTemp(NewEntry));
	return true;
}

template <typename TObject, typename TUnit, typename TMethodPtr>
bool FGameUnitAddRemoveDelegates::RegisterListenerMemberInternal(TMap<int32, FListenerBucket>& PerPlayer,
                                                                 TObject* Listener,
                                                                 const int32 PlayerIndex,
                                                                 TMethodPtr Method)
{
	if (not IsValid(Listener))
	{
		return false;
	}

	// Capture weak; invoke member on valid object.
	TWeakObjectPtr<TObject> Weak = Listener;
	TFunction<void(TUnit*, bool)> TypedHandler = [Weak, Method](TUnit* Unit, const bool bWasAdded)
	{
		if (not Weak.IsValid())
		{
			return;
		}
		(Weak.Get()->*Method)(Unit, bWasAdded);
	};

	return RegisterListenerInternal<TUnit>(PerPlayer, Listener, PlayerIndex, MoveTemp(TypedHandler));
}

template <typename TUnit>
void FGameUnitAddRemoveDelegates::NotifyImpl(TMap<int32, FListenerBucket>& PerPlayer,
                                             const int32 PlayerIndex,
                                             TUnit* Unit,
                                             const bool bWasAdded)
{
	if (not IsValid(Unit))
	{
		return;
	}

	FListenerBucket* BucketPtr = PerPlayer.Find(PlayerIndex);
	if (not BucketPtr)
	{
		return;
	}

	TArray<FListenerEntry>& Entries = BucketPtr->Entries;

	// Compact invalid listeners while notifying.
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FListenerEntry& Entry = Entries[Index];
		if (not Entry.IsUsable())
		{
			Entries.RemoveAtSwap(Index);
			continue;
		}
		Entry.Callback(static_cast<AActor*>(Unit), bWasAdded);
	}
}

// ----- Member-function typed convenience overloads -----

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(ASquadUnit*, bool))
{
	return RegisterListenerMemberInternal<
		TObject, ASquadUnit>(M_SquadListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(ASquadUnit*, bool) const)
{
	return RegisterListenerMemberInternal<
		TObject, ASquadUnit>(M_SquadListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(ATankMaster*, bool))
{
	return RegisterListenerMemberInternal<
		TObject, ATankMaster>(M_TankListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(ATankMaster*, bool) const)
{
	return RegisterListenerMemberInternal<
		TObject, ATankMaster>(M_TankListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(AAircraftMaster*, bool))
{
	return RegisterListenerMemberInternal<TObject, AAircraftMaster>(M_AircraftListenersPerPlayer, Listener, PlayerIndex,
	                                                                Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(AAircraftMaster*, bool) const)
{
	return RegisterListenerMemberInternal<TObject, AAircraftMaster>(M_AircraftListenersPerPlayer, Listener, PlayerIndex,
	                                                                Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(AActor*, bool))
{
	return RegisterListenerMemberInternal<TObject, AActor>(M_ActorListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   const int32 PlayerIndex,
                                                   void (TObject::*Method)(AActor*, bool) const)
{
	return RegisterListenerMemberInternal<TObject, AActor>(M_ActorListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   int32 PlayerIndex,
                                                   void (TObject::*Method)(ABuildingExpansion*, bool))
{
	return RegisterListenerMemberInternal<TObject, ABuildingExpansion>(
		M_BxpListenersPerPlayer, Listener, PlayerIndex, Method);
}

template <typename TObject>
bool FGameUnitAddRemoveDelegates::RegisterListener(TObject* Listener,
                                                   int32 PlayerIndex,
                                                   void (TObject::*Method)(ABuildingExpansion*, bool) const)
{
	return RegisterListenerMemberInternal<TObject, ABuildingExpansion>(
		M_BxpListenersPerPlayer, Listener, PlayerIndex, Method);
}
