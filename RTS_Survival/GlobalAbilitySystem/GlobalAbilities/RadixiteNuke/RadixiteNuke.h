// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RadixiteNuke.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProceduralMeshComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
struct FAoeInstigator;

UENUM()
enum class ERadixiteNukePhase : uint8
{
	Inactive,
	// The target area glows for everyone; the warhead is still on its way.
	Warning,
	// The warhead comes down on the target.
	Falling,
	// The blast is over; the ground poisons whatever stays on it, if the settings have radiation.
	Radiating,
	// Only the scar remains.
	Spent
};

/** @brief Damage and timing of the radixite nuke; the radii come from the aim area of the ability. */
USTRUCT(BlueprintType)
struct FRadixiteNukeSettings
{
	GENERATED_BODY()

	// Seconds from the order to the blast; the warning a player gets to move out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float WarningSeconds = 8.f;

	// Last part of the warning, during which the warhead is visible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FallSeconds = 2.5f;

	// Height above the target at which the warhead appears; above the RTS camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
	float FallStartHeight = 9000.f;

	// Above 0 the warhead flies in on a low arc from this far away instead of falling from above, like a V2 rocket.
	// It crosses the screen from the side, so set FallSeconds to WarningSeconds to show the whole flight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ArcApproachDistance = 0.f;

	// Height above the target at which the arc starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ArcStartHeight = 1500.f;

	// Highest point of the arc above the target; at least ArcStartHeight. Keep it under the RTS camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ArcApexHeight = 4000.f;

	// Damage at the centre of the core zone, which ignores armor; falls off towards the edge of the core.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float CoreDamage = 4000.f;

	// Damage at the centre of the blast, which reaches AreaRadius of the aim area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BlastDamage = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BlastFalloffExponent = 1.3f;

	// Rear armor that still takes the full blast.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BlastFullDamageArmor = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BlastArmorFalloff = 1.2f;

	// Rear armor that stops the blast completely.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BlastMaxArmor = 400.f;

	// Damage of every radiation pulse at the centre of the blast.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float RadiationDamagePerPulse = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.25"))
	float RadiationPulseSeconds = 2.f;

	// 0 leaves no radiation: the scar is plain burnt ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float RadiationSeconds = 30.f;

	// Whose units the blast and the radiation hurt; a nuke does not ask. Seen from the firing player: Player = its own
	// and allied units, Enemy = the units of its hostile players.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETriggerOverlapLogic AffectedUnits = ETriggerOverlapLogic::OverlapBoth;
};

/**
 * @brief One more Niagara effect layered into the detonation (ground burst, thrown earth, later fires, smoke).
 * Every copy picks its own delay and distance from ground zero in the ranges and lands on the ground there.
 */
USTRUCT(BlueprintType)
struct FRadixiteNukeDetonationLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "32"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	// Seconds after the blast.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinDelaySeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxDelaySeconds = 0.f;

	// Distance from ground zero as part of the blast radius; the copies spread evenly around it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinRadiusFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxRadiusFraction = 0.f;
};

/**
 * @brief The crater the warhead leaves: a deep-looking parallax pit decal ringed by a raised earth rim that follows the
 * ground. The landscape cannot be cut at runtime, so the pit is painted and only the rim is geometry. Pathing never sees
 * it; vehicles can ride over the rim.
 */
USTRUCT(BlueprintType)
struct FRadixiteNukeCrater
{
	GENERATED_BODY()

	// Radius of the rim crest in cm; 0 leaves no crater.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float RimRadius = 0.f;

	// Pit decal with its own bowl and rim painted in; spun to a random angle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> PitDecalMaterial;

	// Where the painted rim crest lies in the decal, as part of its half size; lines the decal up with the rim mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "1"))
	float DecalRimFraction = 0.6f;

	// Material of the earth rim; vertex colour R = fresh spoil on the crest, G = foot that blends into the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> RimMaterial;

	// Height of the crest above the ground; units drive and walk at ground level inside the pit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float RimHeight = 160.f;

	// Run of the inner wall from the crest toward the centre, as part of RimRadius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float RimInnerRunFraction = 0.25f;

	// Run of the outer ejecta slope from the crest out to the ground, as part of RimRadius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float RimOuterRunFraction = 0.5f;

	// How much the crest height and radius wander around the ring, so the rim does not read as a perfect torus.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "0.5"))
	float RimIrregularity = 0.15f;

	// Vehicles ride over the rim like over the trench banks (physics only); infantry still walk through it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRimCarriesVehicles = true;
};

/** @brief Assets of the nuke, loaded when the ability fires. */
USTRUCT(BlueprintType)
struct FRadixiteNukeVisuals
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> WarheadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float WarheadScale = 1.f;

	// Rotation of the warhead mesh relative to its flight, so the nose leads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator WarheadRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> WarheadTrailEffect;

	// Place of the trail effect relative to the warhead mesh, e.g. at the nozzle of a rocket.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WarheadTrailOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator WarheadTrailRotation = FRotator::ZeroRotator;

	// Scale of the trail and smoke effects: exhausts made for small rockets scale with their component, and a 14 m
	// rocket needs a bigger flame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float WarheadTrailScale = 1.f;

	// Optional smoke left behind along the flight, at the same place as the trail effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> WarheadSmokeTrailEffect;

	// The fireball and mushroom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> DetonationEffect;

	// Scale of the detonation effect; match it to the blast radius of the ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float DetonationEffectScale = 1.f;

	// Lingers over the ground during the radiation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> RadiationEffect;

	// Same shader as the target area: it tells everyone where the warhead will land.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> WarningAreaMaterial;

	// Ground ring running out from the blast; the shader reads the scalar Progress, 0 to 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> ShockwaveMaterial;

	// Burnt glowing ground decal; the shader reads the scalar Glow, 1 down to 0 during the radiation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> ScarMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USoundBase> WarningSound;

	// Plays the warning sound for everyone at full volume instead of at the target, where the camera may not be.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWarningSound2D = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USoundBase> DetonationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FlashColor = FLinearColor(0.25f, 1.f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float FlashIntensity = 40000000.f;

	// Seconds the flash takes to fade out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FlashFadeSeconds = 2.5f;

	// Extra effects layered into the detonation after DetonationEffect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRadixiteNukeDetonationLayer> DetonationLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRadixiteNukeCrater Crater;

	// Camera shakes of the blast; the shake system caps one shake, so a bigger blast rumbles longer instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "8"))
	int32 BlastShakePulses = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float BlastShakePulseIntervalSeconds = 0.4f;
};

/**
 * @brief One radixite warhead from warning to scar: shows the target area to everyone, brings the warhead down, deals
 * the core and blast damage, then keeps poisoning the ground for a while. Spawned by UGA_RadixiteNuke with its assets
 * loaded; stays behind with the scar decal and, if the visuals have one, the crater when it is done.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ARadixiteNuke : public AActor
{
	GENERATED_BODY()

public:
	ARadixiteNuke();

	/**
	 * @brief Starts the countdown; call once right after spawning.
	 * @param GroundZero Ground location of the blast.
	 * @param BlastRadius Reach of the blast in cm.
	 * @param CoreRadius Reach of the core zone in cm.
	 * @param Settings Damage and timing.
	 * @param Visuals Assets; have to be loaded.
	 * @param OwningPlayer Player of the ability, credited with the kills.
	 */
	void StartNuke(const FVector& GroundZero, const float BlastRadius, const float CoreRadius,
	               const FRadixiteNukeSettings& Settings, const FRadixiteNukeVisuals& Visuals,
	               const uint8 OwningPlayer);

	ERadixiteNukePhase GetPhase() const { return M_Phase; }
	FVector GetWarheadLocation() const;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_WarningPlane;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ShockwavePlane;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_WarheadMesh;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_TrailEffect;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_SmokeTrailEffect;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> M_FlashLight;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_ScarDecal;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_CraterDecal;

	// Raised earth ring around the crater pit, built at the blast to follow the ground under it.
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> M_CraterRim;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_ShockwaveMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_ScarMaterial;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_RadiationEffect;

	UPROPERTY()
	FRadixiteNukeSettings M_Settings;

	UPROPERTY()
	FRadixiteNukeVisuals M_Visuals;

	ERadixiteNukePhase M_Phase = ERadixiteNukePhase::Inactive;
	FVector M_GroundZero = FVector::ZeroVector;
	// Start and control point of the flight; a quadratic Bezier that ends at ground zero.
	FVector M_FlightStart = FVector::ZeroVector;
	FVector M_FlightControl = FVector::ZeroVector;
	float M_BlastRadius = 0.f;
	float M_CoreRadius = 0.f;
	float M_PhaseSeconds = 0.f;
	float M_NextRadiationPulseSeconds = 0.f;
	uint8 M_OwningPlayer = 0;

	void SetupWarningArea();
	void SetupShockwave();
	void SetupScar();

	void StartFalling();
	void SetupFlightPath();
	FVector GetArcApproachDirection() const;
	void TickFalling();
	void Detonate();
	void DealBlastDamage() const;
	void RequestBlastCameraShake() const;
	void ScheduleBlastShakePulses();
	void ShowCrater();
	void SpawnDetonationLayers();
	void SpawnDetonationLayerCopy(const FRadixiteNukeDetonationLayer& Layer, const int32 CopyIndex);
	void TickRadiating(const float DeltaSeconds);
	void DealRadiationPulse() const;
	void EndRadiation();
	void TickBlastVisuals() const;

	AActor* GetDamageCauser() const;

	// The authored AffectedUnits read from the firing player's point of view.
	FAoeInstigator GetAffectedUnits() const;
};
