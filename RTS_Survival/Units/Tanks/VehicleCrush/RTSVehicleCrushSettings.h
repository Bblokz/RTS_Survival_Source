// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSVehicleCrushSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
enum class ESquadSubtype : uint8;

/**
 * @brief Hostile vehicles running over soldiers (Project Settings > Game > Vehicle Crush).
 * Read by URTSVehicleCrushSubsystem and ASquadUnit::OnRunOverByVehicle; C++ defaults name the assets, so no ini edit is
 * needed. Only hostile owners are ever crushed: allies keep the overlap evasion they always had.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vehicle Crush"))
class RTS_SURVIVAL_API URTSVehicleCrushSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSVehicleCrushSettings();

	static const URTSVehicleCrushSettings* Get() { return GetDefault<URTSVehicleCrushSettings>(); }

	UPROPERTY(Config, EditAnywhere, Category="Crush")
	bool bEnableVehicleCrush = true;

	// Seconds between two checks of all moving vehicles against hostile soldiers.
	UPROPERTY(Config, EditAnywhere, Category="Crush", meta=(ClampMin="0.03", ClampMax="0.5"))
	float CheckIntervalSeconds = 0.1f;

	// Ground speed in cm/s below which a vehicle does not crush: a standing tank does not kill the soldier beside it.
	UPROPERTY(Config, EditAnywhere, Category="Crush", meta=(ClampMin="0"))
	float MinCrushSpeed = 60.f;

	// How far inside the hull rectangle (cm) a soldier's centre must be; the hull bodies overhang the tracks a little.
	UPROPERTY(Config, EditAnywhere, Category="Crush", meta=(ClampMin="0"))
	float HullInsetCm = 15.f;

	// A soldier more than this far below the vehicle origin is under it on another level (cellar, bridge).
	UPROPERTY(Config, EditAnywhere, Category="Crush", meta=(ClampMin="0"))
	float MaxDepthBelowVehicleCm = 200.f;

	// A soldier more than this far above the vehicle origin is on a roof or upper floor.
	UPROPERTY(Config, EditAnywhere, Category="Crush", meta=(ClampMin="0"))
	float MaxHeightAboveVehicleCm = 250.f;

	// Squads that are machines, not men: a tank drives into them, never over them.
	UPROPERTY(Config, EditAnywhere, Category="Crush")
	TArray<ESquadSubtype> NotCrushableSubtypes;

	// ---------------- Getting out of the way ----------------

	// Seconds of driving ahead of the hull in which a soldier notices the vehicle.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	float DodgeLookAheadSeconds = 1.75f;

	// The look-ahead never drops below this, so a slowly turning tank is still noticed.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	float MinDodgeLookAheadCm = 250.f;

	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	float MaxDodgeLookAheadCm = 900.f;

	// Extra width (cm) on each side of the hull that still counts as in the way.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	float DodgeSideMarginCm = 90.f;

	// Chance that a soldier reacts at all; the rest freeze, fight on or do not see it coming.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0", ClampMax="1"))
	float DodgeChance = 0.65f;

	// Seconds before a soldier that does react starts to move.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	FVector2D DodgeReactionSeconds = FVector2D(0.2f, 0.9f);

	// How far beside the hull edge (cm) the soldier aims to end up.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	FVector2D DodgeClearanceCm = FVector2D(80.f, 260.f);

	// Chance the soldier picks the wrong side, back across the vehicle's path.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0", ClampMax="1"))
	float WrongSideChance = 0.12f;

	// A soldier forgets a vehicle he rolled against after it left his surroundings this long; then he rolls again.
	UPROPERTY(Config, EditAnywhere, Category="Dodge", meta=(ClampMin="0"))
	float EncounterForgetSeconds = 4.f;

	// ---------------- Scream ----------------

	UPROPERTY(Config, EditAnywhere, Category="Scream")
	TArray<TSoftObjectPtr<USoundBase>> MaleScreams;

	UPROPERTY(Config, EditAnywhere, Category="Scream")
	TArray<TSoftObjectPtr<USoundBase>> FemaleScreams;

	// Squads whose soldiers scream with the female set.
	UPROPERTY(Config, EditAnywhere, Category="Scream")
	TArray<ESquadSubtype> FemaleScreamSubtypes;

	UPROPERTY(Config, EditAnywhere, Category="Scream")
	TSoftObjectPtr<USoundAttenuation> PlayerScreamAttenuation;

	UPROPERTY(Config, EditAnywhere, Category="Scream")
	TSoftObjectPtr<USoundAttenuation> EnemyScreamAttenuation;

	UPROPERTY(Config, EditAnywhere, Category="Scream", meta=(ClampMin="0"))
	FVector2D ScreamPitch = FVector2D(0.93f, 1.07f);

	// Several soldiers of one squad under one hull: only this many scream, the rest would only add noise.
	UPROPERTY(Config, EditAnywhere, Category="Scream", meta=(ClampMin="1"))
	int32 MaxScreamsPerVehiclePerSecond = 2;

	// ---------------- Blood ----------------

	// Splash thrown out from under the hull; not the bullet-hit blood of a regular death.
	UPROPERTY(Config, EditAnywhere, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> CrushBloodBurst;

	UPROPERTY(Config, EditAnywhere, Category="Blood", meta=(ClampMin="0"))
	float CrushBloodBurstScale = 1.6f;

	// Flat spray on the ground, laid along the drive direction.
	UPROPERTY(Config, EditAnywhere, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> CrushBloodGroundSpray;

	UPROPERTY(Config, EditAnywhere, Category="Blood", meta=(ClampMin="0"))
	float CrushBloodGroundSprayScale = 1.4f;

	// Smeared pool decal the tracks leave behind.
	UPROPERTY(Config, EditAnywhere, Category="Blood")
	TSoftObjectPtr<UMaterialInterface> CrushBloodDecal;

	// Decal half size in cm: X along the drive direction, Y across it, Z projection depth.
	UPROPERTY(Config, EditAnywhere, Category="Blood")
	FVector CrushBloodDecalHalfSize = FVector(115.f, 60.f, 80.f);

	// Seconds the decal stays before fading out; matches the corpse lifetime.
	UPROPERTY(Config, EditAnywhere, Category="Blood", meta=(ClampMin="0"))
	float CrushBloodDecalLifetime = 30.f;

	UPROPERTY(Config, EditAnywhere, Category="Blood", meta=(ClampMin="0"))
	float CrushBloodDecalFadeSeconds = 4.f;

	// Launch the body gets as a fraction of a full-damage hit: low, the hull rolls over him instead of throwing him.
	UPROPERTY(Config, EditAnywhere, Category="Blood", meta=(ClampMin="0", ClampMax="1"))
	float CorpsePushFraction = 0.08f;

	/** @return Whether this kind of squad is flesh a vehicle can run over. */
	bool GetIsCrushableSubtype(ESquadSubtype Subtype) const;

	/** @return A scream for this kind of soldier, loaded; nullptr when none is set. */
	USoundBase* PickScream(ESquadSubtype Subtype) const;
};
