#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"

#include "WreckEngineFireDeathSettings.generated.h"

class UMaterialInterface;
class USoundCue;

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
};
