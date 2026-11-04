#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"

#include "WreckEngineFireDeathSettings.generated.h"

class UMaterialInterface;
class USoundCue;

/**
 * @brief How the wreck's own engine deck part is blown off on a fuel explosion. Only wrecks with a static mesh
 * component named EngineDeckComponentName (with simple collision) throw it; the others throw generic deck debris.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckEngineDeckThrowSettings
{
	GENERATED_BODY()

	// Name of the wreck's engine deck static mesh component (the rear deck plates cut out of the hull mesh).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck")
	FName EngineDeckComponentName = TEXT("EngineDeck");

	// Upward launch speed range in cm/s (mass independent).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck", meta=(ClampMin="0"))
	FVector2D UpSpeedRange = FVector2D(800.0, 1100.0);

	// Sideways launch speed range in cm/s, thrown away from the hull over the engine end.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck", meta=(ClampMin="0"))
	FVector2D SideSpeedRange = FVector2D(150.0, 320.0);

	// The sideways direction varies this many degrees either side of straight back over the engine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck", meta=(ClampMin="0", ClampMax="180"))
	float DirectionSpreadDegrees = 40.f;

	// Tumble speed range in degrees per second around a mostly horizontal axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck", meta=(ClampMin="0"))
	FVector2D SpinDegreesRange = FVector2D(120.0, 330.0);

	// Multiplies the generic deck debris thrown along with the real deck; 0 throws only the deck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engine Deck", meta=(ClampMin="0"))
	float DeckDebrisCountScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing")
	bool bPlayLandingImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(EditCondition="bPlayLandingImpact"))
	ERTS_ExplosionType LandingExplosionType = ERTS_ExplosionType::Small;

	// Plays instead of the landing explosion type's own sound; empty keeps that sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(EditCondition="bPlayLandingImpact"))
	TObjectPtr<USoundCue> LandingSound = nullptr;

	// Below this linear speed (cm/s) for the settle duration the deck counts as landed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleLinearSpeed = 25.f;

	// Below this angular speed (deg/s) for the settle duration the deck counts as landed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleAngularSpeed = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleDuration = 0.4f;

	// Physics is always switched off after this many seconds, even when the deck still slides.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0.5"))
	float MaxFlightTime = 9.f;
};

/**
 * @brief Tuning for the engine fire death where the fuel explodes: the engine deck is blown apart, burning fuel
 * spills next to the hull and the wreck is charred around the engine.
 * Set per tank on ATankMaster; the values are handed to the UWreckEngineFireDeathComponent on the spawned wreck.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckEngineFireDeathSettings
{
	GENERATED_BODY()

	FWreckEngineFireDeathSettings();

	// When off, this tank keeps its normal wreck when it dies during an engine fire.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	bool bExplodeFuelOnEngineFireDeath = true;

	// Name of the wreck's hull static mesh component; the deck fire burns on it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	FName HullComponentName = TEXT("Hull");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Explosion")
	ERTS_ExplosionType ExplosionType = ERTS_ExplosionType::Gas;

	// Plays instead of the explosion type's own sound; empty keeps that sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Explosion")
	TObjectPtr<USoundCue> ExplosionSound = nullptr;

	// A second, smaller blast a moment later as the remaining fuel goes up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Explosion")
	bool bPlaySecondaryExplosion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Explosion", meta=(EditCondition="bPlaySecondaryExplosion"))
	ERTS_ExplosionType SecondaryExplosionType = ERTS_ExplosionType::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Explosion",
		meta=(EditCondition="bPlaySecondaryExplosion", ClampMin="0"))
	float SecondaryExplosionDelay = 0.45f;

	// Deck plates, grilles, hatch lids and exhaust parts thrown off the engine (pieces in Project Settings > Wreck Debris).
	// Also gates throwing the wreck's own EngineDeck part.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Debris")
	bool bThrowDeckDebris = true;

	// Multiplies the piece count, which already grows with the hull's size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Debris", meta=(EditCondition="bThrowDeckDebris", ClampMin="0"))
	float DeckDebrisCountScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Fire")
	ERTSFireType DeckFireType = ERTSFireType::TankFire;

	// Seconds the open engine bay keeps burning on the wreck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Fire", meta=(ClampMin="0"))
	float DeckFireLifeTime = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Fire")
	FVector DeckFireScale = FVector(1.4);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Fire")
	FRTSFireEffectParams DeckFireEffectParams;

	// Burning fuel that ran out of the engine bay onto the ground beside the hull. Visual only: it deals no damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill")
	bool bSpillBurningFuel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel"))
	ERTSFireType FuelFireType = ERTSFireType::GroundFire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel", ClampMin="0"))
	float FuelFireLifeTime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel"))
	FVector FuelFireScale = FVector(1.0);

	// How far (cm) past the hull's edge behind the engine the fuel pool burns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel"))
	float FuelSpillDistancePastHull = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel", ClampMin="0"))
	int32 ScorchDecalCount = 2;

	// Min and max scorch decal size in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel"))
	FVector2D ScorchDecalSize = FVector2D(280.0, 420.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fuel Spill", meta=(EditCondition="bSpillBurningFuel", ClampMin="0"))
	float ScorchDecalLifeTime = 90.f;

	// Translucent overlay that chars the wreck around the engine and lets embers glow out; empty keeps the plain wreck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char")
	TSoftObjectPtr<UMaterialInterface> CharOverlayMaterial;

	// Char radius as a share of the hull's bounding radius; the whole wreck still gets a light soot layer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char", meta=(ClampMin="0"))
	float CharRadiusHullScale = 0.75f;

	// Seconds the embers in the charred metal take to cool down.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char", meta=(ClampMin="0.1"))
	float EmberCoolDownSeconds = 35.f;

	// Hero wrecks with their own EngineDeck part throw it instead of most of the generic deck debris.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Deck Debris")
	FWreckEngineDeckThrowSettings EngineDeckThrow;
};
