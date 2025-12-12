#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "NiagaraSystem.h"

#include "CollapseFXParameters.generated.h"

// Defines SFX and VFX for a Collapse.
USTRUCT(Blueprintable)
struct FCollapseFX
{
	GENERATED_BODY()

	// Niagara system to spawn with offset from geo component, can be left null.
	UPROPERTY(BlueprintReadWrite)
	UNiagaraSystem* CollapseVfx = nullptr;

	// Offset from the Geo component's location.
	UPROPERTY(BlueprintReadWrite)
	FVector FxLocationOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector VfxScale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite)
	FRotator VfxRotation = FRotator::ZeroRotator;

	// Sound to play when the collapse happens, can be left null.
	UPROPERTY(BlueprintReadWrite)
	USoundCue* CollapseSfx = nullptr;

	UPROPERTY(BlueprintReadWrite)
	USoundAttenuation* SfxAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite)
	USoundConcurrency* SfxConcurrency = nullptr;

	void CreateFX(const AActor* WorldContext) const;
};


USTRUCT(Blueprintable)
struct FCollapseForce
{
	GENERATED_BODY()
	// Offset added to the location of the GeoCollection component.
	UPROPERTY(BlueprintReadWrite)
	FVector ForceLocationOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	float Force = 4000.f;

	UPROPERTY(BlueprintReadWrite)
	float Radius = 10.f;
};

USTRUCT(Blueprintable)
struct FCollapseDuration
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	float CollapsedGeometryLifeTime = 8;

	UPROPERTY(BlueprintReadWrite)
	bool bDestroyOwningActorAfterCollapse = false;

	// If false the geo component is destroyed by the CollapseMesh function after life time.
	// Otherwise, the geometry is kept visible but physics is disabled.
	UPROPERTY(BlueprintReadWrite)
	bool bKeepGeometryVisibleAfterLifeTime = false;
};
