// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSystemTypes.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthLevels/HealthLevels.h"

#include "GeometryDamageSettings.generated.h"

class UCurveFloat;
class UNiagaraSystem;
class USoundAttenuation;
class USoundConcurrency;
class USoundCue;

/** @brief One world-space impact, normalised away from the engine damage event types. */
USTRUCT(BlueprintType)
struct FGeometryDamageHit
{
	GENERATED_BODY()

	// Raw damage dealt before health-relative normalisation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DamageAmount = 0.f;

	// World-space centre of every field produced by this impact.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldHitLocation = FVector::ZeroVector;

	// Surface normal at the impact. Zero is tolerated by the direction fallback chain.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ImpactNormal = FVector::ZeroVector;

	// Projectile travel direction. Zero is tolerated by the direction fallback chain.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ShotDirection = FVector::ZeroVector;

	// Negative means that max health is read live from the health component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealthOverride = -1.f;

	// Negative means that post-impact health is read live from the health component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthPercentAfterOverride = -1.f;
};

/** @brief Selects the field graph used to shape a geometry damage reaction. */
UENUM(BlueprintType)
enum class EGeometryDamageForceArchetype : uint8
{
	RadialBurst UMETA(DisplayName = "Radial Burst"),
	DirectionalPunch UMETA(DisplayName = "Directional Punch"),
	Implosion UMETA(DisplayName = "Implosion"),
	ChaoticJitter UMETA(DisplayName = "Chaotic Jitter"),
	TorqueTwist UMETA(DisplayName = "Torque Twist")
};

/** @brief Designer controls for turning raw damage into the severity shared by every reaction channel. */
USTRUCT(BlueprintType)
struct FGeometryDamageScalingSettings
{
	GENERATED_BODY()

	// Damage below this fraction of max health is ignored without opening a simulation window.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadzoneFraction = 0.02f;

	// Damage at this fraction of max health earns the maximum response.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float FullResponseFraction = 0.25f;

	// Shapes the response when no response curve is assigned.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite,
		meta = (ClampMin = "0.05", EditCondition = "ResponseCurve == nullptr"))
	float ResponseExponent = 0.6f;

	// Replaces ResponseExponent when assigned; X and Y are expected in the normalised 0..1 range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> ResponseCurve = nullptr;

	// Final per-unit severity trim applied after the curve or exponent.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float GlobalSeverityMultiplier = 1.0f;
};

/** @brief A complete physical reaction profile with force, radius, strain, falloff, and decay controls. */
USTRUCT(BlueprintType)
struct FGeometryDamageForceProfile
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EGeometryDamageForceArchetype Archetype = EGeometryDamageForceArchetype::DirectionalPunch;

	// Zero is the selected archetype and one is a pure radial burst.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RadialVsDirectionalBlend = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinForce = 25000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxForce = 750000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinRadius = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxRadius = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinStrain = 0.f;

	// Must exceed the collection asset's authored threshold before any kinematic cluster can break free.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxStrain = 400000.f;

	// Limits cluster breaking to a tighter area than the force field.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxStrainRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<EFieldFalloffType> FalloffType = EFieldFalloffType::Field_Falloff_Squared;

	// Replaces linear force decay over the impulse window when assigned.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> ForceDecayCurve = nullptr;
};

/** @brief Spatial SFX and VFX fired at an impact point when a health threshold is crossed. */
USTRUCT(BlueprintType)
struct FGeometryDamageFX
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USoundCue> ImpactSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USoundAttenuation> SoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USoundConcurrency> SoundConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinVolumeMultiplier = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector2D PitchRange = FVector2D(0.95f, 1.05f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> ImpactVfx = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector VfxScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAlignVfxToImpactNormal = true;
};

/** @brief One health crossing and the extra physical and audiovisual reaction it arms. */
USTRUCT(BlueprintType)
struct FGeometryDamageThresholdStage
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EHealthLevel TriggerLevel = EHealthLevel::Level_50Percent;

	// Multiplies ordinary-hit force while health remains at or below this stage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float SustainedForceMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bFireCrossingBurst = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (EditCondition = "bFireCrossingBurst"))
	FGeometryDamageForceProfile CrossingBurst;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGeometryDamageFX CrossingFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bRearmOnHeal = false;
};

/** @brief Global performance controls shared by every force profile on the component. */
USTRUCT(BlueprintType)
struct FGeometryDamageSimulationBudget
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float SimulateAfterImpactSeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ImpulseWindowSeconds = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MaxFieldDispatchesPerTick = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxSimulationDistanceFromView = 12000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float QuiesceSleepThreshold = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinSecondsBetweenThresholdFX = 0.25f;
};

/** @brief Internal impact state retained only while its short force window remains open. */
USTRUCT()
struct FGeometryDamageActiveImpact
{
	GENERATED_BODY()

	FVector WorldLocation = FVector::ZeroVector;
	FVector ResolvedShotDirection = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;
	float Severity = 0.f;
	float ImpactTime = 0.f;
	float SustainedForceMultiplier = 1.f;
	bool bForceRadialFallback = false;
};
