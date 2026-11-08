// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParachutedResourcePayload.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/StripFlight/GA_StripFlight.h"
#include "GA_ResourceDrop.generated.h"

class UGA_ResourceDrop;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnResourceDropLanded, UGA_ResourceDrop* /*Ability*/,
                                     AActor* /*SpawnedResource*/);

/**
 * @brief Economy ability: a transport flies the strip the player draws and drops parachuted supplies along it. Each
 * one becomes a scavengeable container (radixite & metal or construction supplies) or a harvestable radixite
 * deposit where it lands. The drop points are fixed for a strip, so the aim preview shows the real spots; an aircraft
 * shot down loses the rest of its load. Enemy AI and missions give a point: the strip is centred on it.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_ResourceDrop : public UGA_StripFlight
{
	GENERATED_BODY()

public:
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const override;

	const FResourceDropSettings& GetResourceDropSettings() const { return M_ResourceDropSettings; }

	/** @return What one use delivers in total: container rewards times the count, or the deposit amount. */
	TMap<ERTSResourceType, int32> GetTotalDeliveredResources() const;

	// Broadcast for every payload that landed, with the container or deposit it left (null when spawning failed).
	FOnResourceDropLanded OnResourceLanded;

protected:
	virtual void OnStripFlightStarted(const FStripFlightRun& Run) override;
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance) override;
	virtual void OnStripFlightEnded(const FStripFlightRun& Run) override;
	virtual void GetAssetsToLoadBeforeFlight(TArray<FSoftObjectPath>& OutAssets) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FResourceDropSettings M_ResourceDropSettings;

	struct FPendingResourcePayload
	{
		FVector GroundLocation = FVector::ZeroVector;
		float DistanceOnStrip = 0.0f;
	};

	// Spots of each flight that the aircraft has not reached yet.
	TMap<int32, TArray<FPendingResourcePayload>> M_PendingPayloadsPerRun;

	FSoftObjectPath GetPayloadClassPath() const;
	void DropPayload(const FStripFlightRun& Run, const FPendingResourcePayload& Payload);
	void OnPayloadLanded(AParachutedResourcePayload* Payload, AActor* SpawnedResource);
};
