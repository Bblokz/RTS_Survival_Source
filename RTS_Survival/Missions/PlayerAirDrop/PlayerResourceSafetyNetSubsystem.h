// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlayerResourceSafetyNetTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/ParachutedResourcePayload.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "PlayerResourceSafetyNetSubsystem.generated.h"

class AParachutedResourcePayload;
class ASupplyCratePickup;
class UMissionBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSafetyNetDropSent, const FVector& /*DropCentre*/);

/**
 * @brief Runs a mission's resource safety net (UMissionBase::StartPlayerResourceSafetyNet): from the moment the player
 * HQ stands, it compares the player's radixite and metal with the highest price of the required purchases not owned
 * yet and parachutes ASupplyCratePickup crates next to the HQ when the player stays short. Any player unit collects a
 * crate by touching it. Ends when every purchase is owned, the starting mission ends, or StopSafetyNet is called.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerResourceSafetyNetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UPlayerResourceSafetyNetSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * @brief Starts (or restarts) watching with these settings.
	 * @param Settings What must be bought and how crates are dropped.
	 * @param StartedBy The mission it belongs to; the net ends when that mission is gone.
	 */
	void StartSafetyNet(const FPlayerResourceSafetyNetSettings& Settings, UMissionBase* StartedBy);

	void StopSafetyNet();

	bool GetIsStartedBy(const UMissionBase* Mission) const { return bM_IsWatching && M_StartedBy.Get() == Mission; }

	// Broadcast with the centre of each drop (the mission pings it on the minimap).
	FOnResourceSafetyNetDropSent OnDropSent;

	UFUNCTION(BlueprintPure, Category = "Mission|Resource Safety Net",
		meta = (WorldContext = "WorldContextObject"))
	static UPlayerResourceSafetyNetSubsystem* GetPlayerResourceSafetyNetSubsystem(const UObject* WorldContextObject)
	{
		return Get(WorldContextObject);
	}

	UFUNCTION(BlueprintPure, Category = "Mission|Resource Safety Net")
	bool GetIsWatching() const { return bM_IsWatching; }

	UFUNCTION(BlueprintPure, Category = "Mission|Resource Safety Net")
	int32 GetDropCount() const { return M_DropCount; }

	UFUNCTION(BlueprintPure, Category = "Mission|Resource Safety Net")
	FVector GetLastDropLocation() const { return M_LastDropLocation; }

	/** @return Crates still falling or lying uncollected. */
	UFUNCTION(BlueprintPure, Category = "Mission|Resource Safety Net")
	int32 GetOutstandingCrateCount() const;

	/** @return What the player needs now: the highest price per resource of the purchases not owned yet. */
	UFUNCTION(BlueprintCallable, Category = "Mission|Resource Safety Net")
	TMap<ERTSResourceType, int32> GetRequiredResources() const;

protected:
	virtual void Deinitialize() override;

private:
	FPlayerResourceSafetyNetSettings M_Settings;

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_StartedBy;

	// Falling and landing looks taken from the supply drop ability; the kind and contents are set per drop.
	UPROPERTY()
	FResourceDropSettings M_DropSettings;

	UPROPERTY()
	TArray<TWeakObjectPtr<AParachutedResourcePayload>> M_FallingCrates;

	UPROPERTY()
	TArray<TWeakObjectPtr<ASupplyCratePickup>> M_LandedCrates;

	FTimerHandle M_CheckTimer;
	bool bM_IsWatching = false;
	int32 M_DropCount = 0;
	float M_LastDropWorldSeconds = -1.f;
	// World time the current shortfall began; negative while the player can afford everything.
	float M_ShortfallStartWorldSeconds = -1.f;
	FVector M_LastDropLocation = FVector::ZeroVector;

	// Reported once per start: a purchase without a valid cost leaves nothing to size crates with.
	bool bM_ReportedMissingCosts = false;

	void CheckResources();
	bool GetShouldStop() const;
	bool GetHasUnownedPurchase() const;
	bool GetIsPlayerHQStanding() const;
	bool GetIsPurchaseOwned(const FPlayerResourceSafetyNetPurchase& Purchase) const;
	bool GetIsUnitOwned(const FTrainingOption& Unit) const;
	TMap<ERTSResourceType, int32> GetPurchaseCost(const FPlayerResourceSafetyNetPurchase& Purchase) const;
	/**
	 * @param Required What the player needs now.
	 * @return Per resource how much the player lacks; empty when the player can afford everything.
	 */
	TMap<ERTSResourceType, int32> GetShortfall(const TMap<ERTSResourceType, int32>& Required) const;
	bool GetIsShortfallLongEnough(bool bIsShort);
	bool GetIsDropOnCooldown() const;
	bool CacheDropSettings();
	/**
	 * @param Required What the player needs now.
	 * @return One crate's contents: the top-up to Required plus ExtraFraction, split over CratesPerDrop.
	 */
	TMap<ERTSResourceType, int32> BuildCrateContents(const TMap<ERTSResourceType, int32>& Required) const;
	void SendDrop(const TMap<ERTSResourceType, int32>& Required);
	bool SpawnFallingCrate(UWorld& World, const FVector& GroundLocation);
	void OnCrateLanded(AParachutedResourcePayload* Payload, AActor* SpawnedResource);
	float GetWorldSeconds() const;
};
