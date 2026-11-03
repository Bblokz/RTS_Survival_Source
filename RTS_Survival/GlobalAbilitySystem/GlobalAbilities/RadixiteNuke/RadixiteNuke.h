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
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class ERadixiteNukePhase : uint8
{
	Inactive,
	// The target area glows for everyone; the warhead is still on its way.
	Warning,
	// The warhead comes down on the target.
	Falling,
	// The blast is over; the ground poisons whatever stays on it.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float RadiationSeconds = 30.f;

	// Whose units the blast and the radiation hurt; a nuke does not ask.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETriggerOverlapLogic AffectedUnits = ETriggerOverlapLogic::OverlapBoth;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<USoundBase> DetonationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor FlashColor = FLinearColor(0.25f, 1.f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float FlashIntensity = 40000000.f;
};

/**
 * @brief One radixite warhead from warning to scar: shows the target area to everyone, brings the warhead down, deals
 * the core and blast damage, then keeps poisoning the ground for a while. Spawned by UGA_RadixiteNuke with its assets
 * loaded; stays behind with the scar decal when it is done.
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
	TObjectPtr<UPointLightComponent> M_FlashLight;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_ScarDecal;

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
	float M_BlastRadius = 0.f;
	float M_CoreRadius = 0.f;
	float M_PhaseSeconds = 0.f;
	float M_NextRadiationPulseSeconds = 0.f;
	uint8 M_OwningPlayer = 0;

	void SetupWarningArea();
	void SetupShockwave();
	void SetupScar();

	void StartFalling();
	void TickFalling();
	void Detonate();
	void DealBlastDamage() const;
	void RequestBlastCameraShake() const;
	void TickRadiating(const float DeltaSeconds);
	void DealRadiationPulse() const;
	void EndRadiation();
	void TickBlastVisuals() const;

	AActor* GetDamageCauser() const;
};
