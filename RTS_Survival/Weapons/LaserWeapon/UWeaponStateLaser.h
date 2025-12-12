#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "UWeaponStateLaser.generated.h"

class UNiagaraComponent;

UCLASS()
class RTS_SURVIVAL_API UWeaponStateLaser : public UWeaponState
{
	GENERATED_BODY()

public:
	void InitLaserWeapon(
		int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		FLaserWeaponSettings LaserWeaponSettings);

	inline int32 GetLaserTicks() const { return M_LaserWeaponSettings.BeamIterations; }

protected:
	virtual void FireWeaponSystem() override;
	virtual void CreateLaunchVfx(const FVector& LaunchLocation, const FVector& ForwardVector,
	                             const bool bCreateShellCase) override;
	virtual void BeginDestroy() override;

	virtual void OnStopFire() override;

private:
	FLaserWeaponSettings M_LaserWeaponSettings;

	// ------------ Beam Niagara ------------
	bool EnsureLaserEffectIsValid() const;
	void SetupLaserBeam();
	void InitLaserBeamParameters() const;
	UPROPERTY()
	UNiagaraComponent* M_LaserBeamSystem = nullptr;
	bool EnsureLaserBeamSystemIsValid() const;

	void SetBeamHidden(const bool bHidden) const;

	// Hide-beam timer (unchanged behavior).
	FTimerHandle M_LaserHideTimerHandle;
	void SetTimerToHideBeamPostShot();

	// ------------ Iterative tracing within one pulse ------------
	float GetIterationTime() const;
	void StartBeamIterationSchedule();
	void DoBeamIteration(); // timer callback
	void FireTraceIteration(const int32 PulseSerial);

	// Track the active pulse to ignore late async results from prior pulses.
	int32 M_CurrentPulseSerial = 0;
	int32 M_IterationsToDo = 0;
	int32 M_IterationsDone = 0;
	float M_PulseEndTime = 0.f;
	FTimerHandle M_LaserIterationTimerHandle;

	// Async trace plumbing
	void PlayImpactSound(const FVector& ImpactLocation) const;
	void OnLaserAsyncTraceComplete(
		FTraceDatum& TraceDatum,
		float ProjectileLaunchTime,
		const FVector& LaunchLocation,
		const FVector& TraceEnd,
		const int32 PulseSerial);

	float GetDamageToDealAndVerifyHitActor(const FHitResult& TraceHit, AActor*& OutHitActor) const;

	// ------------ Niagara params ------------
	inline static const FName NsLaserLocationParamName = TEXT("Location");
	inline static const FName NsLaserColorParamName = TEXT("LaserColor");
	inline static const FName NsLaserScaleParamName = TEXT("Scale");

	void UpdateLaserParam_Location(const FVector& NewEndLocation) const;
	void UpdateLaserParam_Color(const FLinearColor& NewColor) const;
	void UpdateLaserParam_Scale(const float NewScale) const;
	void StartBeamPulse(const FVector& InitialEndLocation) const;
	void StopBeamPulse() const;

	static FORCEINLINE bool IsFiniteVector(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}
};
