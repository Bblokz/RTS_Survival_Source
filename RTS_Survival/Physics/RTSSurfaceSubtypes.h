#pragma once
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"

#include "RTSSurfaceSubtypes.generated.h"

UENUM(BlueprintType)
enum class ERTSSurfaceType : uint8
{
	Sand UMETA(DisplayName = "Sand"),
	Metal UMETA(DisplayName = "Metal"),
	Flesh UMETA(DisplayName = "Flesh"),
	Air UMETA(DisplayName = "Air"),
	Stone UMETA(DisplayName = "Stone"),
};

// Defines a pair of Niagara system and sound cue to play when a surface is hit.
USTRUCT(BlueprintType)
struct FRTSSurfaceImpactData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UNiagaraSystem* ImpactEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundCue* ImpactSound = nullptr;
};

