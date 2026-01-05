// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "FieldConstruction.generated.h"

/**
 * @brief Actor spawned when squads build a field construction; handles collision and build visuals.
 */
UCLASS()
class RTS_SURVIVAL_API AFieldConstruction : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFieldConstruction(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> FieldConstructionMesh;

	bool GetIsValidFieldConstructionMesh();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupCollision(const int32 OwningPlayer, const bool bBlockPlayerProjectiles);

public:
	/**
	 * @brief Copies preview material to this construction and restores originals over the build duration.
	 * @param PreviewMeshComponent Mesh component from the preview actor.
	 * @param ConstructionDuration Time span for restoring materials.
	 */
	void InitialiseFromPreview(UStaticMeshComponent* PreviewMeshComponent, float ConstructionDuration);

	void StopConstructionMaterialTimer();

private:
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> M_OriginalMaterials;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_PreviewMaterial;

	FTimerHandle M_ConstructionMaterialTimer;
	float M_ConstructionTime = 0.f;
	float M_ConstructionElapsed = 0.f;

	void ApplyPreviewMaterial();
	void RestoreMaterialsOverTime();
	void RestoreOriginalMaterialSlots(const float BuildProgress);
};
