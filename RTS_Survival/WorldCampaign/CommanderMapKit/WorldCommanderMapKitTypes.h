// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

// Kept free of other project headers: WorldCampaignProgressState.h includes this file to save the timed effects.
#include "CoreMinimal.h"
#include "WorldCommanderMapKitTypes.generated.h"

/**
 * @brief What a commander's map ability does. Generic so several commanders can share one effect with their own
 * numbers. Append only: saved in the campaign and in config by value.
 */
UENUM(BlueprintType)
enum class EWorldCommanderMapAbility : uint8
{
	None,
	/** Lowers one anchor target's enemy strength by Magnitude percent for DurationTurns. */
	WeakenTarget,
	/** Reveals the fog in Radius around a world location for DurationTurns (0 = permanently). */
	RevealRegion,
	/** Removes Magnitude fortifications from one anchor target. */
	RemoveFortification,
	/** Pays Magnitude radixite per blueprint, up to Resources.Blueprints blueprints per use. */
	ConvertRadixiteToBlueprints,
	/** Grants Resources at once. */
	GrantResources,
	/** Deals Magnitude damage to one enemy division. */
	DamageDivision,
	/** Shows one anchor target's strength in full detail for DurationTurns. */
	RevealTargetStrength,
	/** Stops enemy strength growth in Radius around an anchor target for DurationTurns. */
	StallStrengthGrowth,
	/** The operation of this turn may target anchors up to Magnitude steps outside the fog. */
	ForcedMarch,
	/** The building on one build node cannot be destroyed for DurationTurns. */
	ProtectBuilding,
	/** The operation of this turn starts with Magnitude extra infantry squads. */
	ReinforceNextMission,
	/** Takes Magnitude points back from the enemy superweapon progress. */
	StallFreeze
};

/**
 * @brief Which field of FWorldCommanderMapAbilityTarget an ability reads.
 */
UENUM(BlueprintType)
enum class EWorldCommanderAbilityTargetKind : uint8
{
	None,
	Anchor,
	Division,
	BuildNode,
	WorldLocation
};

/**
 * @brief Where the player aimed a map ability. Only the field the ability needs is read; see
 * WorldCommanderMapKitRules::GetRequiredTargetKind.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderMapAbilityTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Commander")
	FGuid AnchorKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Commander")
	FGuid DivisionKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Commander")
	FName BuildNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Commander")
	FVector WorldLocation = FVector::ZeroVector;

	// FVector has no unset value, so a location target says explicitly that it was aimed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Commander")
	bool bHasWorldLocation = false;
};

/**
 * @brief A map ability effect that lasts several turns. Saved so it survives save and load; the systems that apply
 * the effect read the active list from UWorldCommanderMapKit::GetActiveTimedEffects.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCommanderTimedEffect
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Commander")
	EWorldCommanderMapAbility Ability = EWorldCommanderMapAbility::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Commander")
	FWorldCommanderMapAbilityTarget Target;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Commander")
	float Magnitude = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Commander")
	float Radius = 0.f;

	/** Player turns the effect still lasts, counting the current one; removed at the start of the turn it hits 0. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Commander")
	int32 TurnsRemaining = 0;
};
