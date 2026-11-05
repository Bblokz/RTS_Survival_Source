// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/FireSubsystem/GroundFire/RTSGroundFireSettings.h"
#include "IncendiaryCanister.generated.h"

class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** @brief Looks, fall and impact of one incendiary canister; set on the incendiary air strike template. */
USTRUCT(BlueprintType)
struct FIncendiaryCanisterSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> FallingMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float MeshScale = 1.f;

	// Rotation of the mesh relative to its flight direction, so the nose leads whatever axis the mesh was modelled on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator MeshRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FallSeconds = 1.6f;

	// The fireball where the canister bursts; the burning ground itself is the pooled GroundFire of the fire subsystem.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> ImpactEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float ImpactEffectScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> ImpactSound = nullptr;

	// Fire damage of the burst itself; fills the fire buffer of infantry so the burn bites from its first tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ImpactDamage = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ImpactRadiusCm = 400.f;

	// The ground left burning; uses the same rear-armor window as the burst.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRTSGroundFireSettings GroundFire;
};

/**
 * @brief An incendiary canister on its way down from an aircraft. Spawned from code only (UGA_IncendiaryAirStrike):
 * it falls along a forward-arcing path, bursts into flame on the ground and asks the fire subsystem to burn the area.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AIncendiaryCanister : public AActor
{
	GENERATED_BODY()

public:
	AIncendiaryCanister();

	/**
	 * @brief Starts the fall; call once right after spawning.
	 * @param ReleaseLocation Where it leaves the aircraft.
	 * @param GroundLocation Where it bursts.
	 * @param OwningPlayer Player that dropped it; the fire hurts the other side.
	 * @param Settings Looks, timing, burst and burn.
	 */
	void StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation, const int32 OwningPlayer,
	               const FIncendiaryCanisterSettings& Settings);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_FallingMesh;

	UPROPERTY()
	FIncendiaryCanisterSettings M_Settings;

	FVector M_ReleaseLocation = FVector::ZeroVector;
	FVector M_GroundLocation = FVector::ZeroVector;
	float M_SecondsFalling = 0.f;
	int32 M_OwningPlayer = 0;

	FVector GetFallLocation(const float FallAlpha) const;
	void Burst();
	void DealImpactDamage();
	void RequestBurningGround() const;
};
