// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "FieldConstruction.generated.h"

/**
 * @brief Actor spawned when squads build a field construction; handles collision and build visuals.
 */
class UCargo;

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
	virtual void OnUnitDies(const ERTSDeathType DeathType) override;
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> FieldConstructionMesh;

	bool GetIsValidFieldConstructionMesh();

	/**
	 * @brief Sets the collision of all meshes for the owner of the construction.
	 * @param OwningPlayer Side the Blueprint was authored for; only used when the construction's own RTS component
	 * has no player as owner (the builder's owner on that component wins).
	 * @param bBlockPlayerProjectiles Whether allied shots hit the construction too.
	 * @param bOverlapTanks Whether vehicles overlap (crush) the construction.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupCollision(const int32 OwningPlayer, const bool bBlockPlayerProjectiles, const bool bOverlapTanks);

	/** Bound to the RTS component: redoes what depends on the owner once the construction is handed to its builder. */
	virtual void OnOwningPlayerChanged(uint8 OldOwningPlayer, uint8 NewOwningPlayer);

public:
	/**
	 * @brief Copies preview material to this construction and restores originals over the build duration.
	 * @param PreviewMeshComponent Mesh component from the preview actor.
	 * @param ConstructionDuration Time span for restoring materials.
	 */
	void InitialiseFromPreview(UStaticMeshComponent* PreviewMeshComponent, float ConstructionDuration);

	void StopConstructionMaterialTimer();

private:
	// What the Blueprint last asked of SetupCollision; applied again with the new owner when the owner changes.
	struct FFieldConstructionCollisionRequest
	{
		bool bIsSet = false;
		int32 OwningPlayerFromBlueprint = 0;
		bool bBlockPlayerProjectiles = false;
		bool bOverlapTanks = false;
	};

	FFieldConstructionCollisionRequest M_CollisionRequest;

	void BeginPlay_BindToOwningPlayerChanges();
	void ApplyCollisionRequest();

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
	bool EnsureCargoComponentIsValid() const;

	void OnUnitDies_HandleGarrisonState();

	static const int32 DamagePercentage;

	UPROPERTY()
	TObjectPtr<UCargo> M_CargoComponent;
};
