// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"

#include "AircraftDeathTypes.generated.h"

class UGeometryCollection;
class UNiagaraSystem;
class USoundBase;

/** Where on a shot-down aircraft the fire starts. */
UENUM(BlueprintType)
enum class EAircraftFireOrigin : uint8
{
	// The nose engine when the aircraft has a single propeller, otherwise one of the wing engines.
	Automatic,
	NoseEngine,
	WingEngine
};

/** Which side of the aircraft a wing fire burns on; the burning wing drops, so the wreck spirals that way. */
UENUM(BlueprintType)
enum class EAircraftDamagedSide : uint8
{
	None,
	Left,
	Right
};

UENUM(BlueprintType)
enum class EAircraftDeathPhase : uint8
{
	Alive,
	// Burning and falling with the speed the aircraft had when it was hit.
	Crashing,
	// Hit the ground; the wreck broke up and the aircraft actor is on its way out.
	Impacted
};

/**
 * @brief One effect of a crash and the values of its user parameters (names without "User."); the shared fire and
 * smoke systems were made for burning tanks and are sized to an aircraft through them.
 */
USTRUCT(BlueprintType)
struct FAircraftDeathEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TObjectPtr<UNiagaraSystem> System = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TMap<FName, float> FloatParameters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TMap<FName, FVector> VectorParameters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect")
	TMap<FName, FLinearColor> ColorParameters;

	// Size of this effect on top of the aircraft's effect scale.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effect", meta=(ClampMin="0.01"))
	float Scale = 1.f;
};

/**
 * @brief How a shot-down aircraft burns, falls and breaks up. Defaults suit every aircraft; a Blueprint changes them
 * on its death component only where its aircraft must differ (a socket for the fire, a heavier bomber's fall).
 */
USTRUCT(BlueprintType)
struct FAircraftDeathSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire")
	EAircraftFireOrigin FireOrigin = EAircraftFireOrigin::Automatic;

	// Optional socket or bone on the aircraft mesh to start the fire at instead of an engine found by its bones.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fire")
	FName FireSocketOverride = NAME_None;

	// Fraction of earth gravity pulling the wreck down; the wings still carry a little of it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(ClampMin="0.1", UIMin="0.1"))
	float GravityScale = 0.85f;

	// How fast the nose drops while the aircraft is still level; a nose fire stalls the aircraft faster.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="DegreesPerSecond"))
	float NoseDropDegreesPerSecond = 16.f;

	// The steepest dive the wreck settles into.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(ClampMin="10", ClampMax="89", Units="Degrees"))
	float MaxDiveDegrees = 62.f;

	// Roll speed a burning wing reaches as it loses lift; the wreck spirals towards it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="DegreesPerSecond"))
	float WingFireRollDegreesPerSecond = 85.f;

	// How fast the wreck turns towards the burning wing while it spirals down.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="DegreesPerSecond"))
	float WingFireTurnDegreesPerSecond = 22.f;

	// Rocking of a wreck that burns at the nose: no wing is lost, so it only wobbles.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="Degrees"))
	float NoseFireWobbleDegrees = 12.f;

	// Air drag per second on the falling wreck; keeps the speed it had without letting gravity run away with it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(ClampMin="0", ClampMax="1"))
	float DragPerSecond = 0.08f;

	// A slow or hovering aircraft (taking off, landing) still falls forward with at least this speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="CentimetersPerSecond"))
	float MinCrashSpeed = 900.f;

	// Safety: a wreck that never meets the ground (fell off the map) is removed after this long.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fall", meta=(Units="Seconds"))
	float MaxCrashSeconds = 20.f;

	// Optional pieces the wreck breaks into; without one the crash collection built for the aircraft's meshes is used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breakup")
	TSoftObjectPtr<UGeometryCollection> CrashCollectionOverride;

	// Share of the impact velocity the pieces keep: they skid on in the direction the aircraft flew.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breakup", meta=(ClampMin="0", ClampMax="1"))
	float PieceVelocityRetained = 0.45f;

	// Outward kick of the impact explosion on the pieces.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breakup", meta=(Units="CentimetersPerSecond"))
	float PieceScatterSpeed = 420.f;

	// How long the pieces stay before they are cleared away.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Breakup", meta=(Units="Seconds"))
	float WreckLifeSeconds = 30.f;

	// Scale of the effects; a heavy bomber burns and explodes bigger than a fighter.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects", meta=(ClampMin="0.1"))
	float EffectScale = 1.f;

	// Flames on the burning engine; they ride with the aircraft.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	FAircraftDeathEffect EngineFire;

	// Smoke puffs dropped into the world behind the burning engine; they stay where the wreck was, so they form its
	// trail whatever space the system simulates in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	FAircraftDeathEffect SmokeTrail;

	// Distance the wreck falls between two smoke puffs.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects", meta=(ClampMin="20", Units="Centimeters"))
	float SmokeTrailSpacing = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	FAircraftDeathEffect ImpactExplosion;

	// Burning fuel where the wreck came down; it goes with the wreck.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	FAircraftDeathEffect GroundFire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> EngineHitSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Effects")
	TObjectPtr<USoundBase> ImpactSound = nullptr;
};

/** Where the falling wreck is and how it moves; advanced by FAircraftDeathMath::StepCrashFlight. */
struct FAircraftCrashFlightState
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	// Roll around the direction of flight, in degrees; right wing down is positive.
	float RollDegrees = 0.f;
	float RollRateDegreesPerSecond = 0.f;
	float SecondsFalling = 0.f;
	EAircraftDamagedSide DamagedSide = EAircraftDamagedSide::None;
};
