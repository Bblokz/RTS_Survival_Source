// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "SupplyPointSubsystem.generated.h"

class ASupplyPoint;
class USupplyTruckComponent;
class USoundBase;
struct FSupplyAnnouncementLines;

/** The supply run of one captured point: its truck on the road or the timer until the next one leaves. */
USTRUCT()
struct FSupplyPointRun
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASupplyPoint> SupplyPoint;

	UPROPERTY()
	TWeakObjectPtr<USupplyTruckComponent> SupplyTruck;

	FTimerHandle DispatchTimerHandle;
};

/**
 * @brief Deals the lines of one announcer moment like a deck of cards: every line plays once before any repeats,
 * and a fresh deck never opens with the line that closed the last one.
 */
USTRUCT()
struct FSupplyAnnouncementDeck
{
	GENERATED_BODY()

	/** @return Index of the next line to play, or INDEX_NONE when there are no lines. */
	int32 DrawNextLineIndex(int32 LineCount);

private:
	TArray<int32> M_RemainingLineIndices;
	int32 M_LastPlayedLineIndex = INDEX_NONE;
};

/**
 * @brief Runs the supply runs of every supply point in the world: when a point is captured it announces it, has
 * the owner's base send a supply truck, pays the cargo out on arrival and sends the next truck.
 * Everything is keyed by owning player; only the "base of a player" and "pay a player" steps know the human
 * player today, so AI supply runs only need those two filled in (see GetSupplyBaseTransform and DeliverCargo).
 */
UCLASS()
class RTS_SURVIVAL_API USupplyPointSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USupplyPointSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	void RegisterSupplyPoint(ASupplyPoint* SupplyPoint);
	void UnregisterSupplyPoint(ASupplyPoint* SupplyPoint);

	/**
	 * @brief Where a player's trucks start and where they bring the cargo: a navmesh spot at the player's base.
	 * @param OwningPlayer Player whose base is wanted.
	 * @param OutBaseTransform Spawn / delivery transform beside the base.
	 * @return False when that player has no base right now (HQ destroyed, not delivered yet, or an AI player).
	 */
	bool GetSupplyBaseTransform(uint8 OwningPlayer, FTransform& OutBaseTransform) const;

	/**
	 * @brief Gives a player a fixed truck base instead of the HQ: missions without an HQ, AI players and tests.
	 * @param OwningPlayer Player the base belongs to.
	 * @param BaseTransform Navmesh spot where trucks spawn and deliver.
	 */
	void SetSupplyBaseOverride(uint8 OwningPlayer, const FTransform& BaseTransform);
	void ClearSupplyBaseOverride(uint8 OwningPlayer);

	/** Adds the cargo to the owner's storage; whatever does not fit is lost. */
	void DeliverCargo(uint8 OwningPlayer, const FSupplyTruckCargo& Cargo) const;

	/** Plays the next portrait line of this moment for the human player; other owners are silent. */
	void PlaySupplyAnnouncement(ESupplyAnnouncement Announcement, uint8 OwningPlayer);

	USupplyTruckComponent* GetSupplyTruckOfPoint(const ASupplyPoint* SupplyPoint) const;

private:
	void OnSupplyPointOwnerChanged(ASupplyPoint* SupplyPoint, uint8 PreviousOwner);
	void ScheduleTruckDispatch(ASupplyPoint* SupplyPoint, float Delay);
	void DispatchSupplyTruck(ASupplyPoint* SupplyPoint);
	USupplyTruckComponent* SpawnSupplyTruck(uint8 OwningPlayer, const FTransform& BaseTransform) const;
	UClass* LoadSupplyTruckClass(uint8 OwningPlayer) const;
	void BindSupplyTruck(USupplyTruckComponent* SupplyTruck);
	void OnSupplyTruckLoaded(USupplyTruckComponent* SupplyTruck);
	void OnSupplyTruckRunEnded(USupplyTruckComponent* SupplyTruck, bool bDelivered);
	FSupplyPointRun* FindRun(const ASupplyPoint* SupplyPoint);
	bool GetPlayerBaseTransform(FTransform& OutBaseTransform) const;
	const FSupplyAnnouncementLines& GetAnnouncementLines(ESupplyAnnouncement Announcement) const;

	UPROPERTY()
	TArray<FSupplyPointRun> M_SupplyPointRuns;

	TMap<ESupplyAnnouncement, FSupplyAnnouncementDeck> M_AnnouncementDecks;

	TMap<uint8, FTransform> M_SupplyBaseOverrides;
};
