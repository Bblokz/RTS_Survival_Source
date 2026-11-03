// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "GA_GliderTankDrop.generated.h"

class AGliderTankDelivery;
class ATankMaster;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FGliderTankDropSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AGliderTankDelivery> DeliveryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	// Spawns exactly this class, for a drop that promises one variant whatever the player picked for TankSubtype.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<ATankMaster> TankClassOverride;

	// Longest horizontal distance the tow flies before the release point; the map edge usually comes first.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxApproachDistance = 30000.0f;
};

/**
 * @brief A super heavy tank delivered by a towed glider: the player draws the landing run as a strip (touchdown at
 * the first click, stop at the second), the glider skids along it and the tank drives out of the nose at the end.
 * Enemy AI and missions give a point; the run then ends on that point, coming in from the aircraft spawn side.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_GliderTankDrop : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd) override;
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FGliderTankDropSettings M_GliderDropSettings;

	// Keeps async class loads alive until their callbacks spawn the delivery.
	TArray<TSharedPtr<FStreamableHandle>> M_LoadHandles;

	UPROPERTY(Transient)
	int32 M_ActiveDeliveries = 0;

	/**
	 * @brief Loads the delivery class and starts one landing run on the ground below the two points.
	 * @param Touchdown Where the glider touches down.
	 * @param Stop Where its skid ends.
	 */
	void StartDelivery(const FVector& Touchdown, const FVector& Stop);
	void OnDeliveryClassLoaded(FVector Touchdown, FVector Stop);
	FVector GetDirectionForPointExecution(const FVector& TargetLocation) const;
	FVector GetGroundLocation(const FVector& Location) const;
	float GetApproachDistance(const FVector& Touchdown, const FVector& Direction) const;
	void OnTankDelivered(AGliderTankDelivery* Delivery, ATankMaster* Tank);
	void BroadcastDropFinishedIfNeeded();
};
