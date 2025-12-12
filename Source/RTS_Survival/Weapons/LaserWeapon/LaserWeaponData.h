#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"

#include "LaserWeaponData.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FLaserWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraSystem* LaserEffect = nullptr;

	/**
	 * Designer control: number of iteration steps the laser stays visible.
	 * Actual duration = (Global Iteration Time) * (BeamIterations).
	 * This guarantees BeamTime is always an exact multiple of the Developer setting.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Laser", meta=(ClampMin="1", UIMin="1"))
	int32 BeamIterations = 2;

	/**
	 * Derived read-only: the actual beam lifetime in seconds.
	 * Computed from BeamIterations * DeveloperSettings::GameBalance::Weapons::LaserIterationTimeInOnePulse.
	 */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Laser", meta=(ForceUnits="s"))
	float BeamTime = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLinearColor BeamColor = FLinearColor::Blue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BeamScale = 1.f;

	// The offset from the impact point to the launch location to put as the actual end-hit-location.
	// Because some lasers have spherical impacts this can be used to make that sphere more visible.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LaserImpactOffset = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* LaunchSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* ImpactSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundAttenuation* ImpactAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundConcurrency* ImpactConcurrency = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundAttenuation* LaunchAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundConcurrency* LaunchConcurrency = nullptr;
};

USTRUCT(Blueprintable, BlueprintType)
struct FChargeUpLaserSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraSystem* ChargeUpEffect = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector ChargeUpOffset = FVector(0.f, 0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ChargeUpTime = 2.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* ChargeUpSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLaserWeaponSettings LaserWeaponSettings;
};
