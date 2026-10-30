// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "SurvivalWaveTypes.generated.h"

/**
 * @brief Whether a wave pool entry spawns a vehicle or an infantry/team-weapon squad.
 */
UENUM(BlueprintType)
enum class ESurvivalWaveUnitKind : uint8
{
	Tank,
	Squad
};

/**
 * @brief Tactical role of a pool entry; special waves bias the composition by role.
 */
UENUM(BlueprintType)
enum class ESurvivalWaveUnitRole : uint8
{
	Infantry,
	AntiTankInfantry,
	EliteInfantry,
	ArmoredCar,
	LightTank,
	MediumTank,
	HeavyTank,
	TankDestroyer,
	Artillery,
	AntiAir,
	SuperHeavy
};

/**
 * @brief What a wave is; Regular and Milestone are scheduled, the rest are rolled as special waves.
 */
UENUM(BlueprintType)
enum class ESurvivalWaveKind : uint8
{
	Regular,
	// Every N survival minutes: bigger budget, an extra edge.
	Milestone,
	// Armoured cars and light tanks from more edges.
	Blitz,
	// Mass infantry with a higher unit count.
	InfantryRush,
	// Self-propelled guns and rocket trucks plus barrages on the base.
	Artillery,
	// Heavy and super-heavy tanks with tank destroyers.
	HeavyArmour,
	// Strafing runs / carpet bombing on the base with a small ground escort.
	AirRaid,
	// Paratroopers and tank drops near the base with a small ground escort.
	Airborne
};

static inline FString Global_GetSurvivalWaveKindAsString(const ESurvivalWaveKind Kind)
{
	switch (Kind)
	{
	case ESurvivalWaveKind::Regular:
		return TEXT("Regular");
	case ESurvivalWaveKind::Milestone:
		return TEXT("Milestone");
	case ESurvivalWaveKind::Blitz:
		return TEXT("Blitz");
	case ESurvivalWaveKind::InfantryRush:
		return TEXT("InfantryRush");
	case ESurvivalWaveKind::Artillery:
		return TEXT("Artillery");
	case ESurvivalWaveKind::HeavyArmour:
		return TEXT("HeavyArmour");
	case ESurvivalWaveKind::AirRaid:
		return TEXT("AirRaid");
	case ESurvivalWaveKind::Airborne:
		return TEXT("Airborne");
	}
	return TEXT("Unknown");
}

/**
 * @brief One Soviet unit the wave composer may buy; cost defaults to the strategic AI training-point price
 * (EnemyTrainingHelpers, 10 points = one light tank) so waves and the strategic AI share one economy.
 */
USTRUCT(BlueprintType)
struct FSurvivalWaveUnitEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit")
	ESurvivalWaveUnitKind Kind = ESurvivalWaveUnitKind::Tank;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit",
		meta = (EditCondition = "Kind == ESurvivalWaveUnitKind::Tank", EditConditionHides))
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit",
		meta = (EditCondition = "Kind == ESurvivalWaveUnitKind::Squad", EditConditionHides))
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

	// The entry joins the pool once the survival tech stage reaches this level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit")
	EEnemyAITechLevel TechLevel = EEnemyAITechLevel::BasicInfantry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit")
	ESurvivalWaveUnitRole Role = ESurvivalWaveUnitRole::Infantry;

	// Relative pick weight inside the pool before role and fade multipliers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// 0 uses EnemyTrainingHelpers' training-point cost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit", meta = (ClampMin = "0"))
	int32 CostOverride = 0;

	// Survival minute (Normal pace) after which the weight is multiplied by FadeWeightMultiplier; 0 = never fades.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit", meta = (ClampMin = "0.0"))
	float FadeStartMinute = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeWeightMultiplier = 0.35f;

	FTrainingOption GetTrainingOption() const;
	int32 GetCost() const;
	bool GetIsValidEntry() const;
	FString GetDebugName() const;
};

/**
 * @brief Survival minute (at Normal tech pace) at which a tech stage's pool entries unlock.
 */
USTRUCT(BlueprintType)
struct FSurvivalTechStageUnlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	EEnemyAITechLevel TechLevel = EEnemyAITechLevel::BasicInfantry;

	// Divided by the difficulty profile's TechPaceMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech", meta = (ClampMin = "0.0"))
	float UnlockMinute = 0.f;

	// Shown by the HUD when the stage is reached, e.g. "The Soviets field heavy armour".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	FText AnnouncementText;
};

USTRUCT(BlueprintType)
struct FSurvivalRoleWeightMultiplier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	ESurvivalWaveUnitRole Role = ESurvivalWaveUnitRole::Infantry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role", meta = (ClampMin = "0.0"))
	float Multiplier = 1.f;
};

/**
 * @brief How a wave kind changes the composition, size and edge count of a wave, plus optional global abilities.
 */
USTRUCT(BlueprintType)
struct FSurvivalWaveKindDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind")
	ESurvivalWaveKind Kind = ESurvivalWaveKind::Regular;

	// "Heavy armour", "Air raid"; used in the HUD announcement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind")
	FText DisplayName;

	// Relative chance among the special waves that are allowed at this minute.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// Survival minute (Normal pace) from which the kind may roll; divided by the profile's GrowthPaceMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0.0"))
	float MinMinute = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0.0"))
	float BudgetMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0.0"))
	float MaxUnitsMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0"))
	int32 ExtraEdges = 0;

	// Roles not listed use DefaultRoleMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind")
	TArray<FSurvivalRoleWeightMultiplier> RoleWeightMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind", meta = (ClampMin = "0.0"))
	float DefaultRoleMultiplier = 1.f;

	// Cast on the player base through the enemy global ability manager (must be in the AEnemyController loadout).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind|Global Abilities")
	TArray<EGlobalAbility> GlobalAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind|Global Abilities", meta = (ClampMin = "0"))
	int32 GlobalAbilityCasts = 0;

	// When true and none of GlobalAbilities is ready, the wave falls back to a Regular wave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind|Global Abilities")
	bool bRequiresGlobalAbility = false;

	// Abilities land this far from the base centre (random direction) so drops do not hit the HQ itself.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Kind|Global Abilities", meta = (ClampMin = "0.0"))
	float GlobalAbilityTargetOffset = 0.f;

	float GetRoleMultiplier(const ESurvivalWaveUnitRole Role) const;
};

/**
 * @brief The growth model of one difficulty. Every curve is keyed by survival minute (time since the first wave)
 * and can point at an external UCurveFloat instead of its inline keys.
 */
USTRUCT(BlueprintType)
struct FSurvivalWaveDifficultyProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;

	// Seconds between two wave launches.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve WaveIntervalSeconds;

	// Training points spent on one wave (10 = one light tank).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve WaveBudget;

	// Edges used at once; the fraction is the chance of one more edge.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve EdgesPerWave;

	// Chance (0-1) that a non-milestone wave is a special wave.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve SpecialWaveChance;

	// Upper bound of units in one wave; leftover budget upgrades units instead.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve MaxUnitsPerWave;

	// Strategic AI training income (Soviet bases), applied to the enemy controller's training state.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Curves")
	FRuntimeFloatCurve StrategicTrainingPointsPerMinute;

	// Divides the tech stage unlock minutes: 1.6 reaches Experimental at minute 20 instead of 32.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile", meta = (ClampMin = "0.1"))
	float TechPaceMultiplier = 1.f;

	// Divides special wave minimum minutes (the curves above already contain the pace).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile", meta = (ClampMin = "0.1"))
	float GrowthPaceMultiplier = 1.f;

	// Max enemy unit load alive at once (tanks count 1, soldiers SoldierLoadWeight); waves shrink or wait above it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile", meta = (ClampMin = "1.0"))
	float AliveEnemyLoadCap = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Strategic AI", meta = (ClampMin = "1"))
	int32 StrategicMinPickedUnitsForAction = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile|Strategic AI", meta = (ClampMin = "1"))
	int32 StrategicMaxPickedUnitsForAction = 8;
};

/**
 * @brief Payload of USurvivalWaveDirectorComponent::OnWaveIncoming, broadcast when a wave is launched
 * (SecondsUntilSpawn before its units appear at the edges).
 */
USTRUCT(BlueprintType)
struct FSurvivalWaveAnnouncement
{
	GENERATED_BODY()

	// 1-based.
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	ESurvivalWaveKind Kind = ESurvivalWaveKind::Regular;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	FText KindDisplayName;

	// One per edge, e.g. "North", "the eastern ridge".
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	TArray<FText> EdgeNames;

	// World locations of the edges' spawn centres (for minimap pings).
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	TArray<FVector> EdgeLocations;

	// Normalised 2D directions from the targeted base to each edge (for HUD arrows).
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	TArray<FVector> EdgeDirectionsFromBase;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	int32 UnitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	int32 Budget = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	EEnemyAITechLevel TechStage = EEnemyAITechLevel::BasicInfantry;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	float SecondsUntilSpawn = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	bool bIsSpecial = false;

	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	int32 GlobalAbilitiesCast = 0;

	// Ready-to-show line, e.g. "Wave 7 (Heavy armour) incoming from the North and East".
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	FText Message;
};

/** @brief Broadcast by USurvivalWaveDirectorComponent when a wave launches; the HUD shows the warning. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurvivalWaveIncoming, const FSurvivalWaveAnnouncement& /*Announcement*/);

/** @brief Broadcast when the Soviets reach a new tech stage (AnnouncementText from the settings). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSurvivalTechStageReached, EEnemyAITechLevel /*NewStage*/,
                                     const FText& /*AnnouncementText*/);

/**
 * @brief Where the current wave (round) is: launching groups, waiting for its last enemies, or between waves.
 */
UENUM(BlueprintType)
enum class ESurvivalRoundState : uint8
{
	Idle,
	// Groups keep entering from the edges until the round's active time is over; later rounds last longer.
	Active,
	// No new groups; the round is survived once its enemies are dead (or the clear timeout runs out).
	Clearing,
	// Short grace between waves; the player picks a reward.
	Intermission
};

/** @brief Broadcast when the player survived a round (its active time is over and its enemies are gone). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurvivalRoundSurvived, int32 /*RoundNumber*/);
