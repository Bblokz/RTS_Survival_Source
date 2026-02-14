// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "InstancedDestrucablesEnvActor.generated.h"

struct FRTSRoadInstanceSetup;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class USoundBase;
class UNiagaraSystem;
class USoundAttenuation;
class USoundConcurrency;

// todo revisit after epic implements PrimitiveIDs for HISMs right now we cannot remove any instances so we move their transforms -500 instead.
// When epic implements PrimitiveIDs we can remove the instance directly.
// todo bug: collision is off on destroyed instances but it seems that the moved hierarchical instances of the original
// non destroyed instances still have collision on them. 
// Keeps track of the spawned instances at post initialize components; expects all instances to be spawned in the constructor.
// Creates destroyed instances on the same transform as the intact instance that has no health left.
UCLASS()
class RTS_SURVIVAL_API AInstancedDestrucablesEnvActor : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AInstancedDestrucablesEnvActor(const FObjectInitializer& ObjectInitializer);

	// Casts the damage event to point and uses async programming to find the closest instance transform to the location
	// of the hit. Then will process damage and or spawning a destroyed instance on the game thread.
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, Category="SceneManipulation")
		void SetupHierarchicalInstanceBox(UInstancedStaticMeshComponent* InstStaticMesh,
    	FVector2D BoxBounds,
    	const float DistanceBetweenInstances);

	
	/**
	 * @brief Place instances along a virtual road forwards and backwards with offsets from an actor.
	 * @param RoadSetup Defines how long to place forwards and backwards and on which side(s).
	 * @param InstStaticMesh The instance component to place the instances on.
	 */
	UFUNCTION(BlueprintCallable, Category="SceneManipulation")
	void SetupHierarchicalInstanceAlongRoad(const FRTSRoadInstanceSetup RoadSetup,
                                                                 UInstancedStaticMeshComponent* InstStaticMesh) const;
	// The HISM or ISM component that keeps track of the intact meshes spawned.
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UInstancedStaticMeshComponent> InstancedMeshComponent;

	UPROPERTY(BlueprintReadWrite)
	float HealthPerInstance = 100.f;

	// The soft pointer for the destroyed mesh that is instanced on a reflected intance component that mimics the intact mesh component.
	// When an instance takes too much damage the Destroyed mesh will be spawned at its Position.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> DestroyedMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UNiagaraSystem* CollapseFX;

	// Changed to USoundBase to match the API.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* CollapseSound;

	// Added to the original location of the instanced mesh for the destroyed mesh if needed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ZOffsetForDestroyedMesh = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundAttenuation* CollapseSoundAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundConcurrency* CollapseSoundConcurrency;

	// FX offset for the collapse effects.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector FXOffset = FVector::ZeroVector;

private:
	// Keeps track of the health per instance.
	TArray<float> M_Health;
	
	// keeps track of the location for each instance.
	TArray<FVector> M_InstanceLocations;

	// Instanced mesh component for destroyed meshes.
	TObjectPtr<UInstancedStaticMeshComponent> M_DestroyedMeshComponent;

	/** @brief spawns a destroyed mesh instance on the transform of the regular instance that has no health left */
	void OnInstanceDestroyed(int32 InstanceIndex) const;

	// loads the destroyed mesh and sets it on the destroyed mesh component.
	void AsyncLoadDestroyedMesh();

	void OnFoundClosestInstance(const int32 InstanceIndex, const float DamageToDeal);

};
