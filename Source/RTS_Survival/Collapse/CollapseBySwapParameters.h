#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"

#include "CollapseBySwapParameters.generated.h"

USTRUCT(BlueprintType)
struct FSwapToDestroyedMesh
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMeshComponent* ComponentToSwapOn = nullptr;

	// The sound to play when the mesh is swapped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* SwapSound = nullptr;

	// Niagara system to spawn when the mesh is swapped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UNiagaraSystem* SwapFX = nullptr;

	// The offset from the origin to play the FX at.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector FXOffset = FVector::ZeroVector;

	// The SoftPointer mesh to load and swap to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> MeshToSwapTo = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundAttenuation* Attenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundConcurrency* SoundConcurrency = nullptr;
};
