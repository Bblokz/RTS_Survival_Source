// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/ParachutedResourcePayload.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "PlayerResourceSafetyNetTypes.generated.h"

/** @brief One thing the player must buy to finish a build-up tutorial: a unit or nomadic, or a building expansion. */
USTRUCT(BlueprintType)
struct FPlayerResourceSafetyNetPurchase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net")
	bool bIsBuildingExpansion = false;

	// Done once the player owns one (a tank or harvester of this subtype, or a nomadic as truck or building).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net",
		meta = (EditCondition = "not bIsBuildingExpansion", EditConditionHides))
	FTrainingOption Unit;

	// Done once the player has one, built or still under construction (it is paid for when placed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net",
		meta = (EditCondition = "bIsBuildingExpansion", EditConditionHides))
	EBuildingExpansionType BuildingExpansion = EBuildingExpansionType::BXT_Invalid;
};

/**
 * @brief Mission setting: while the player still has required purchases left, supply crates are parachuted next to
 * the HQ when the player cannot afford them for a while, so overspending never strands a tutorial.
 */
USTRUCT(BlueprintType)
struct FPlayerResourceSafetyNetSettings
{
	GENERATED_BODY()

	// What the player still has to buy; the net needs the highest price of any purchase not owned yet and ends once
	// all are owned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net")
	TArray<FPlayerResourceSafetyNetPurchase> RequiredPurchases;

	// The player must be short this long before crates come, so a purchase that is about to be paid off by a
	// harvester trip (or a unit that is still training) does not trigger a drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float ShortfallSeconds = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float MinSecondsBetweenDrops = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "1"))
	int32 MaxDrops = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "1"))
	int32 CratesPerDrop = 3;

	// A drop tops the player up to the needed amount plus this fraction of it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float ExtraFraction = 0.25f;

	// Crate contents are rounded up to a multiple of this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "1"))
	int32 RoundCrateContentsTo = 25;

	// The crates come down around the player HQ plus this offset, each on a spot clear of buildings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net")
	FVector DropOffsetFromHQ = FVector(-800.f, 600.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float CrateBuildingClearance = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float CrateSpacing = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "100.0"))
	float ReleaseHeight = 3000.f;

	// Resource drop ability whose falling crate, parachute and landing effects are used (contents are replaced).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net")
	EGlobalAbility SupplyDropAbility = EGlobalAbility::GA_GerRadixiteMetalSupplyDrop;

	// Optional crate looks and feedback; empty keeps the supply drop ability's falling mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net")
	FResourceDropPickupSettings CrateLooks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.0"))
	float MiniMapPingSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource Safety Net", meta = (ClampMin = "0.5"))
	float CheckIntervalSeconds = 2.f;
};
