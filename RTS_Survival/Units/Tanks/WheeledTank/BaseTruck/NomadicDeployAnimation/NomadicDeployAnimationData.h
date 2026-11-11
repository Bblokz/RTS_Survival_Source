// Copyright Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "NomadicDeployAnimationData.generated.h"

class UNiagaraSystem;
class UStaticMesh;

/**
 * One effect played when the deploy progress crosses Progress: on deploy while progress rises past it, on pack-up while
 * it falls past it. Bursts are authored per building by Source/Tools/NomadicDeploy (smoke screen + landing dust).
 */
USTRUCT(BlueprintType)
struct FNomadicDeployEffectBurst
{
	GENERATED_BODY()

	// Deploy progress (0 = packed truck, 1 = building) at which the burst plays.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float Progress = 0.f;

	// Location in building mesh space (cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector LocalLocation = FVector::ZeroVector;

	// Component scale; also sets User.ImpactSpriteScale for stateless sprite layers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraSystem> System = nullptr;

	// Plays one of the vehicle's construction sounds with the burst (heavy landings).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPlayConstructionSound = false;
};

/**
 * @brief Per nomadic building: the baked VAT deploy mesh and the effects that go with its deploy / pack-up.
 * Set on the nomadic vehicle blueprint (DeployAnimationData); without it the building simply appears after the
 * mesh animation time. Built by the pipeline in Source/Tools/NomadicDeploy.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UNomadicDeployAnimationData : public UDataAsset
{
	GENERATED_BODY()

public:
	// Copy of the building mesh whose deploy materials read the VAT textures; shown only while animating.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> DeployMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FNomadicDeployEffectBurst> EffectBursts;

	// Pieces fly well outside the rest bounds mid-animation; keeps them from being culled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float DeployMeshBoundsScale = 3.f;
};
