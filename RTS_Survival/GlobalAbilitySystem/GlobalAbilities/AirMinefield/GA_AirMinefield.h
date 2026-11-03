// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AirDroppedMine.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/StripFlight/GA_StripFlight.h"
#include "GA_AirMinefield.generated.h"

class AFieldMine;

USTRUCT(BlueprintType)
struct FAirMinefieldSettings
{
	GENERATED_BODY()

	// Dropped in turn along the strip, so a mix of anti-tank and anti-infantry mines is laid evenly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftClassPtr<AFieldMine>> MineClasses;

	// The same number of mines goes down on any strip: a short strip is dense, a long one covers more ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MineCount = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAirDroppedMineSettings DropSettings;
};

/**
 * @brief An aircraft flies the strip and scatters mines that stay as area denial. The scatter is fixed for a given
 * strip, so the aim preview (GetStripDropPoints) shows the real spots; spots off the navmesh, which no vehicle could
 * reach, are skipped. An aircraft shot down leaves the rest of its mines undropped.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_AirMinefield : public UGA_StripFlight
{
	GENERATED_BODY()

public:
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const override;

protected:
	virtual void OnStripFlightStarted(const FStripFlightRun& Run) override;
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance) override;
	virtual void OnStripFlightEnded(const FStripFlightRun& Run) override;
	virtual void GetAssetsToLoadBeforeFlight(TArray<FSoftObjectPath>& OutAssets) const override;

	// A field across the approach blocks a road; one along it would only clip the edge of a column.
	virtual FVector GetDirectionForPointExecution(const FVector& TargetLocation) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FAirMinefieldSettings M_MinefieldSettings;

	struct FPendingMine
	{
		FVector GroundLocation = FVector::ZeroVector;
		float DistanceOnStrip = 0.0f;
		int32 MineIndex = 0;
	};

	// Spots of each flight that the aircraft has not reached yet.
	TMap<int32, TArray<FPendingMine>> M_PendingMinesPerRun;

	void DropMine(const FStripFlightRun& Run, const FPendingMine& Mine);
};
