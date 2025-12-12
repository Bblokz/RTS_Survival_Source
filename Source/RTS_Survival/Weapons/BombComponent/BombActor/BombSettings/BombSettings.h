#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "BombSettings.generated.h"

USTRUCT(BlueprintType)
struct FBombFXSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundAttenuation* M_LaunchAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundConcurrency* M_LaunchConcurrency = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundCue* LaunchSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundAttenuation* M_ImpactAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundConcurrency* M_ImpactConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector M_ImpactScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector M_BombScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> M_ImpactVfx;
	
	UPROPERTY()
	FPointDamageEvent M_DamageEvent;

	UPROPERTY()
	TSubclassOf<UDamageType> M_DamageType;
};
