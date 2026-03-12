#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Weapons/Projectile/ProjectileVfxSettings/ProjectileVfxSettings.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "ICBMTypes.generated.h"

USTRUCT(BlueprintType)
struct FICBMStagePrepSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> PrepFireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraSystem> PrepFireVfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PrepFallbackTime = 1.0f;
};

USTRUCT(BlueprintType)
struct FICBMStageLaunchSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> LaunchSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraSystem> LaunchVfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LaunchHeight = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LaunchAcceleration = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LaunchMaxSpeed = 3200.0f;
};

USTRUCT(BlueprintType)
struct FICBMStageArcSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ArcCurvature = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ArcLength = 2000.0f;
};

USTRUCT(BlueprintType)
struct FICBMImpactSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> ImpactBySurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector ImpactVfxScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundAttenuation> ImpactAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundConcurrency> ImpactConcurrency = nullptr;
};

USTRUCT(BlueprintType)
struct FICBMFireStageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FICBMStagePrepSettings PrepStage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FICBMStageLaunchSettings LaunchStage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FICBMStageArcSettings ArcStage;
};

USTRUCT(BlueprintType)
struct FICBMLaunchSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FWeaponData WeaponData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> SocketNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinTimeBeforeReady = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxTimeBeforeReady = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SpawnNegativeZOffset = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SpawnVerticalMoveDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FICBMImpactSettings ImpactSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FICBMFireStageSettings FireStages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TraceSocketName = "TraceSocket";
};
