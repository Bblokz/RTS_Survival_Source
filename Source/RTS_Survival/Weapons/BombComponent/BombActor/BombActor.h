#pragma once

#include "CoreMinimal.h"
#include "BombSettings/BombSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "BombActor.generated.h"

struct FBombFXSettings;
class UBombComponent;

UCLASS()
class RTS_SURVIVAL_API ABombActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABombActor();

	// Call this to “drop” the bomb: starts movement + trace timer
	void ActivateBomb(const FTransform& LaunchTransform, TWeakObjectPtr<AActor> TargetActor);

	// Initialize mesh, owner, gravity scale, etc.
	void InitBombActor(const FWeaponData& NewWeaponData,
	                   uint8 OwningPlayer,
	                   UStaticMesh* BombMesh,
	                   UBombComponent* OwningComponent,
	                   float GravitySpeedMlt, const float InitialSpeed, const FBombFXSettings& BombSettings);

	// Puts the bomb back to hidden/inactive state
	void SetBombDormant();

protected:
	virtual void BeginPlay() override;

private:
	// --- components ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* M_BombMeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UProjectileMovementComponent* M_ProjectileMovementComp;

	// Computes velocity blended toward target in XY, preserving Z from base velocity
	FVector ComputeNudgedVelocityXY(const FVector& BaseVel, const FVector& StartLoc,
	                                TWeakObjectPtr<AActor> TargetActor) const;

	// Distance/accuracy → blend factor [0..1]
	float ComputeNudgeAlpha(float PlanarDist) const;

	// --- state ---
	UPROPERTY()
	bool M_bIsDormant = false;

	// who to notify on hit
	TWeakObjectPtr<UBombComponent> M_Owner;

	// weapon data (assumes it contains e.g. InitialSpeed and TraceChannel)
	FWeaponData M_WeaponData;

	uint8 M_OwningPlayer = 0;

	// gravity multiplier
	float M_GravityFallSpeedMlt = 1.0f;
	float M_InitialSpeed = 100.f;

	ECollisionChannel M_TraceChannel = ECC_Visibility;

	float M_LastTraceTime = 0.f;
	FTimerHandle M_TraceTimerHandle;

	// helpers
	bool EnsureOwnerIsValid() const;

	// every 0.2 do a line‐trace from previous location to new location
	void PreformAsyncLineTrace();
	void OnAsyncTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	void OnHitActor(
		AActor* HitActor,
		const FVector& HitLocation,
		const ERTSSurfaceType HitSurface);

	UPROPERTY()
	FBombFXSettings M_BombSettings;

	UPROPERTY()
	FPointDamageEvent M_DamageEvent;

	UPROPERTY()
	TSubclassOf<UDamageType> M_DamageType;

	void SpawnExplosion(const FVector& Location, const ERTSSurfaceType HitSurface) const;

	// ----------------------------- BALISTICS CALCULATIONS -----------------------------------
	// Try to compute a ballistic v0 that hits target using a fixed speed (M_InitialSpeed).
	// Returns ZeroVector if unsolvable with that speed.
	FVector SolveBallisticVelocity_FixedSpeed(
		const FVector& Start, const FVector& Target,
		float Speed, float GravityZ, bool bPreferLowArc, bool& bOutSolved) const;

	// Exact solution for a chosen flight time t: v0 = (Δ - 0.5*g*t^2)/t
	FVector SolveBallisticVelocity_ByTime(
		const FVector& Start, const FVector& Target,
		float Time, float GravityZ) const;

	// Pick a reasonable flight time from planar range and base speed
	float SuggestFlightTime(const FVector& Start, const FVector& Target, float BaseSpeed) const;

	// Blend velocity directions and speeds using alpha
	FVector BlendVelocityByAlpha(const FVector& BaseVel, const FVector& IdealVel, float Alpha) const;
};
