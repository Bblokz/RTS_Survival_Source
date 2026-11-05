// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "SonnengewehrBeam.generated.h"

class ASonnengewehrBeam;
class UAudioComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
struct FOverlapResult;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSonnengewehrSweepFinished, ASonnengewehrBeam*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSonnengewehrActorBurnt, ASonnengewehrBeam*, AActor*, float);

UENUM()
enum class ESonnengewehrPhase : uint8
{
	Inactive,
	// The strip glows for everyone; a faint aiming ray flickers at its start.
	Warning,
	// The beam comes down at the start of the strip and already burns.
	Igniting,
	// The beam travels from the start to the end of the strip.
	Sweeping,
	// The beam fades out at the end of the strip; nothing burns any more.
	Fading,
	// Only the scorch mark remains, cooling down.
	Spent
};

/** @brief Damage and timing of the Sonnengewehr; the beam width and strip length come from the aim area. */
USTRUCT(BlueprintType)
struct FSonnengewehrSettings
{
	GENERATED_BODY()

	// Seconds from the order to the beam coming down; the warning a player gets to move out of the strip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float WarningSeconds = 4.f;

	// The beam burns at the start of the strip this long before it starts to travel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float IgnitionSeconds = 0.6f;

	// Time the beam takes from the start to the end of the strip, whatever its length.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float SweepSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FadeSeconds = 0.8f;

	// Laser damage every unit or building the beam touches takes, once; laser resistances and armour sides apply.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BeamDamage = 1400.f;

	// How often the beam looks for what it touches.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.02"))
	float DamageIntervalSeconds = 0.1f;

	// Whose units the beam burns; the mirror in orbit does not ask.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETriggerOverlapLogic AffectedUnits = ETriggerOverlapLogic::OverlapBoth;

	// The scorch mark glows behind the beam and cools down over this time once the sweep is over.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float ScorchCoolSeconds = 8.f;

	// After this the scorch mark and the actor are removed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float ScorchLifeSeconds = 90.f;
};

/** @brief Assets and look of the Sonnengewehr beam, loaded when the ability fires. */
USTRUCT(BlueprintType)
struct FSonnengewehrVisuals
{
	GENERATED_BODY()

	// Additive beam shader on the engine cylinder; reads Color, Intensity, Strength and EdgePower.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> BeamMaterial;

	// Additive radial glow on a ground plane under the beam; reads Color, Intensity and Strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> ImpactMaterial;

	// Decal over the whole strip; reads the strip in world space and Head, the burnt length, and Glow.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> ScorchMaterial;

	// Same shader as the target area: it tells everyone where the beam will sweep.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UMaterialInterface> WarningAreaMaterial;

	// Plays once for everyone when the warning starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USoundBase> WarningSound;

	// Plays at the beam while it burns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USoundBase> BeamSound;

	// White-hot centre of the beam.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor CoreColor = FLinearColor(1.f, 0.85f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor GlowColor = FLinearColor(1.f, 0.72f, 0.2f);

	// Wide and faint around the glow.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor HaloColor = FLinearColor(1.f, 0.36f, 0.04f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float CoreDiameter = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float GlowDiameter = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float HaloDiameter = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float CoreIntensity = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float GlowIntensity = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float HaloIntensity = 1.5f;

	// Height of the beam above the ground; far above the RTS camera so it reads as coming from orbit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
	float BeamHeight = 60000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float ImpactGlowDiameter = 1900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ImpactIntensity = 12.f;

	// Bright blob where the beam touches the ground, on the engine sphere with the beam shader.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float ImpactFlashDiameter = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor LightColor = FLinearColor(1.f, 0.78f, 0.38f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float LightIntensity = 8000000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float LightRadius = 5000.f;
};

/**
 * @brief One Sonnengewehr strike from warning to scorch mark: shows the strip to everyone, brings the beam down at its
 * start, sweeps it to the end and burns every unit and building it touches once with laser damage. Spawned by
 * UGA_Sonnengewehr with its assets loaded; stays behind with the cooling scorch mark, then removes itself.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASonnengewehrBeam : public AActor
{
	GENERATED_BODY()

public:
	ASonnengewehrBeam();

	/**
	 * @brief Starts the warning; call once right after spawning.
	 * @param StripStart Ground location the beam comes down at.
	 * @param StripEnd Ground location the sweep ends at.
	 * @param BeamHalfWidth Half the width of the burnt path in cm.
	 * @param Settings Damage and timing.
	 * @param Visuals Assets; have to be loaded.
	 * @param OwningPlayer Player of the ability, credited with the kills.
	 */
	void StartStrike(const FVector& StripStart, const FVector& StripEnd, const float BeamHalfWidth,
	                 const FSonnengewehrSettings& Settings, const FSonnengewehrVisuals& Visuals,
	                 const uint8 OwningPlayer);

	ESonnengewehrPhase GetPhase() const { return M_Phase; }
	// Distance of the beam from the start of the strip; 0 before it moves.
	float GetBeamDistance() const { return M_BeamDistance; }
	FVector GetBeamGroundLocation() const;
	FVector GetStripStart() const { return M_StripStart; }
	FVector GetStripDirection() const { return M_StripDirection; }
	float GetStripLength() const { return M_StripLength; }
	float GetBeamHalfWidth() const { return M_BeamHalfWidth; }
	int32 GetBurntActorCount() const { return M_BurntActors.Num(); }

	// The beam stopped burning; the ability ends here, the scorch mark stays.
	FOnSonnengewehrSweepFinished OnSweepFinished;

	// An actor took the beam's damage: the actor and the damage after resistances.
	FOnSonnengewehrActorBurnt OnActorBurnt;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_WarningPlane;

	// Moves along the strip; holds the beam layers, the impact glow and the light.
	UPROPERTY()
	TObjectPtr<USceneComponent> M_BeamRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_BeamCore;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_BeamGlow;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_BeamHalo;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ImpactFlash;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ImpactGlow;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> M_BeamLight;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_ScorchDecal;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_BeamAudio;

	// One dynamic material per beam layer, in the order core, glow, halo, impact flash.
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> M_BeamLayerMaterials;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_ImpactMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_ScorchMaterial;

	UPROPERTY()
	FSonnengewehrSettings M_Settings;

	UPROPERTY()
	FSonnengewehrVisuals M_Visuals;

	// Every actor burns once; a unit that stays in the beam for several damage ticks is not burnt again.
	TSet<TWeakObjectPtr<AActor>> M_BurntActors;

	ESonnengewehrPhase M_Phase = ESonnengewehrPhase::Inactive;
	FVector M_StripStart = FVector::ZeroVector;
	FVector M_StripDirection = FVector::ForwardVector;
	float M_StripLength = 0.f;
	float M_BeamHalfWidth = 0.f;
	float M_BeamDistance = 0.f;
	// Distance along the strip up to which the beam has already burnt.
	float M_BurntUpToDistance = 0.f;
	float M_PhaseSeconds = 0.f;
	float M_SecondsSinceDamageTick = 0.f;
	float M_SecondsSinceSweepEnd = 0.f;
	uint8 M_OwningPlayer = 0;

	void SetupWarningArea();
	void SetupBeamLayers();
	UMaterialInstanceDynamic* SetupBeamLayer(UStaticMeshComponent* Layer, const FLinearColor& Color,
	                                         const float Intensity, const float EdgePower, const float Diameter,
	                                         const float Height);
	void SetupImpact();
	void SetupScorch();

	void TickWarning();
	void StartIgnition();
	void TickIgniting(const float DeltaSeconds);
	void StartSweep();
	void TickSweeping(const float DeltaSeconds);
	void FinishSweep();
	void TickFading();
	void TickSpent(const float DeltaSeconds);

	void MoveBeamTo(const float Distance);
	void SetBeamStrength(const float Strength) const;
	void UpdateScorch(const float Glow) const;
	void TickDamage(const float DeltaSeconds, const bool bForce);
	void BurnUpTo(const float Distance);
	void BurnOverlap(const FOverlapResult& Overlap, const FVector& BeamPoint, TSet<AActor*>& ActorsThisTick);
	void DealBeamDamage(AActor& Victim, const UPrimitiveComponent* HitComponent, const FVector& BeamPoint);

	/**
	 * @brief The laser's own damage path (UArmorCalculation with ERTSDamageType::Laser): the beam hits the armour
	 * plate of the registered mesh that faces it, so the plate's side resistance applies.
	 * @param Victim Actor that owns the armour component.
	 * @param HitComponent Component the beam overlapped; preferred when it is a registered armour mesh.
	 * @param BeamPoint Point on the beam axis at the victim's height.
	 * @return Damage after resistances; the beam damage itself when the victim has no armour component.
	 */
	float GetLaserDamageOnVictim(const AActor& Victim, const UPrimitiveComponent* HitComponent,
	                             const FVector& BeamPoint) const;

	FCollisionObjectQueryParams GetBeamObjectQueryParams() const;
	FVector GetGroundAt(const FVector& Location) const;
	AActor* GetDamageCauser() const;
};
