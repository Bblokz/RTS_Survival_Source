// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "FlameThrowerSettings.h"
#include "UWeaponStateFlameThrower.generated.h"

class UAudioComponent;

/**
 * Aggregated iteration state for the flamethrower.
 */
struct FIterationState
{
	int32 CurrentSerial = 0;
	int32 IterationsToDo = 0;
	int32 IterationsDone = 0;
};

/**
 * Aggregated per-iteration async ray fan state.
 */
struct FPendingRaysState
{
	int32 Serial = 0;
	int32 Expected = 0;
	int32 Received = 0;
	FVector Launch = FVector::ZeroVector;
	TArray<FVector> RayEndPoints;
	TArray<TArray<FHitResult>> RayHits;
};

/**
 * A flamethrower weapon state.
 * - Always Single fire mode.
 * - Attaches a Niagara flame system to FireSocket; never destroyed, only toggled on/off.
 * - Starts a looping sound at fire start; stops and destroys it after the flame duration.
 * - No impact VFX or impact sounds.
 * - Performs timed “iterations” of damage as async multi line-traces fanning out in a cone.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateFlameThrower : public UWeaponState
{
	GENERATED_BODY()

public:
	void InitFlameThrowerWeapon(
		int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		FFlameThrowerSettings FlameSettings);

	FFlameThrowerSettings GetFlameSettings() const { return M_FlameSettings; }

protected:
	// Drives the timed trace schedule; CreateLaunchVfx handles visuals/audio start/stop.
	virtual void FireWeaponSystem() override;

	// Starts the Niagara flame + looping sound and schedules the stop after the duration.
	virtual void CreateLaunchVfx(const FVector& LaunchLocation, const FVector& ForwardVector,
	                             const bool bCreateShellCase) override;

	virtual void BeginDestroy() override;
	virtual void OnStopFire() override;
	virtual void OnDisableWeapon() override;

private:
	// ---------------- Settings / Params ----------------
	FFlameThrowerSettings M_FlameSettings;

	// Niagara parameter names (shared across all flame Niagara systems).
	inline static const FName NsFlameColorParamName = TEXT("Color");
	inline static const FName NsFlameDurationParamName = TEXT("Duration");
	inline static const FName NsFlameRangeParamName = TEXT("Range");
	inline static const FName NsFlameConeAngleParamName = TEXT("ConeAngle");

	// ---------------- Niagara system -------------------
	UPROPERTY()
	UNiagaraComponent* M_FlameSystem = nullptr;

	bool EnsureFlameEffectIsValid() const;
	bool EnsureFlameSystemIsValid() const;
	void SetupFlameSystem();
	void InitFlameParamsStatic() const; // Color/Range/ConeAngle static bits
	void StartFlameVfx(const float Duration) const;
	void StopFlameVfx() const;

	void UpdateFlameParam_Color(const FLinearColor& NewColor) const;
	void UpdateFlameParam_Range(const float NewRange) const;
	void UpdateFlameParam_Duration(const float NewDuration) const;
	void UpdateFlameParam_ConeAngle(const float NewConeAngle) const;

	// ---------------- Audio loop -----------------------
	UPROPERTY()
	UAudioComponent* M_LoopingAudio = nullptr;

	void StartLoopingSoundAt(const FVector& Location);
	void StopAndDestroyLoopingSound();

	// ---------------- Timing ---------------------------
	// Iteration scheduling (one trace fan each tick).
	FTimerHandle M_FlameIterationTimerHandle;
	// Visual/audio stop timer.
	FTimerHandle M_FlameStopTimerHandle;

	float GetIterationTime() const;
	float GetTotalDurationSeconds() const;

	// Iteration state
	FIterationState M_FlameIterationState;

	void StartIterationSchedule();
	void DoOneIteration();

	// ---------------- Async multi-ray aggregation -------
	// We aggregate all rays for a single iteration and then debug once.
	FPendingRaysState M_PendingRaysState;

	void DispatchIterationTraces(const FVector& LaunchLocation, const FVector& ForwardDir, const float Range);
	inline void EnqueueRayAsync(int32 RayIndex, const FVector& LaunchLocation, const FVector& End);
	void OnSingleRayAsyncComplete(const int32 Serial, const int32 RayIndex, FTraceDatum& TraceDatum);
	void OnIterationAllRaysComplete();
	void OnHitValidActors(const TArray<AActor*>& ValidHitActors);

	// Helpers to build the cone rays (up to spec’d patterns).
	void BuildRayYawOffsets(TArray<float>& OutYawDegrees) const;
	FVector ComputeRayEndOnPerpPlane(const FVector& Launch, const FVector& MidDir, const float Range,
	                                 const FVector& RayDir) const;
};
