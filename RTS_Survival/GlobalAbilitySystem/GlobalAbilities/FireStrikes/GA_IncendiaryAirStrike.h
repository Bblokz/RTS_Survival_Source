// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IncendiaryCanister.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/StripFlight/GA_StripFlight.h"
#include "GA_IncendiaryAirStrike.generated.h"

USTRUCT(BlueprintType)
struct FIncendiaryAirStrikeSettings
{
	GENERATED_BODY()

	// The same number of canisters on any strip: a short strip burns as one wall, a long one as separate fires.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 CanisterCount = 6;

	// Fraction of the strip's half width the canisters stray to either side, alternating.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SideScatterFraction = 0.35f;

	// The canister leaves the aircraft this far before its spot, so it keeps flying forward while it falls.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ReleaseLeadDistance = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIncendiaryCanisterSettings CanisterSettings;
};

/**
 * @brief An aircraft (the Ju 87 for GA_GerIncendiaryJu87) flies the strip the player drew and drops incendiary
 * canisters along it whether or not enemies are there; each leaves burning ground through the fire subsystem
 * (URTSFireSubsystem::RequestGroundFire). The spots are fixed for a strip, so the aim preview shows them. An aircraft
 * shot down drops nothing more.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_IncendiaryAirStrike : public UGA_StripFlight
{
	GENERATED_BODY()

public:
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const override;

	const FIncendiaryAirStrikeSettings& GetIncendiarySettings() const { return M_IncendiarySettings; }

protected:
	virtual void OnStripFlightStarted(const FStripFlightRun& Run) override;
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance) override;
	virtual void OnStripFlightEnded(const FStripFlightRun& Run) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FIncendiaryAirStrikeSettings M_IncendiarySettings;

	struct FPendingCanister
	{
		FVector GroundLocation = FVector::ZeroVector;
		float DistanceOnStrip = 0.0f;
	};

	// Spots of each flight that the aircraft has not released over yet.
	TMap<int32, TArray<FPendingCanister>> M_PendingCanistersPerRun;

	void DropCanister(const FStripFlightRun& Run, const FPendingCanister& Canister) const;
};
