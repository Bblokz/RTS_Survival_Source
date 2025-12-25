// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReinforcementPoint.generated.h"

class UMeshComponent;
class USphereComponent;
class UPrimitiveComponent;

/**
 * @brief Defines a socket-based reinforcement location on a mesh.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UReinforcementPoint : public UActorComponent
{
	GENERATED_BODY()

public:
	UReinforcementPoint();

	/**
	 * @brief Initialize the reinforcement mesh and socket.
	 * @param InMeshComponent Mesh component that owns the socket.
	 * @param InSocketName Socket name on the mesh to use for reinforcements.
	 * @param OwningPlayer Index of the player that owns the reinforcement point.
	 * @param ActivationRadius Radius used for reinforcement activation.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName,
	                            int32 OwningPlayer, float ActivationRadius);

	/**
	 * @brief Resolve the world-space location of the configured reinforcement socket.
	 * @param bOutHasValidLocation Set true when a valid mesh and socket were found.
	 * @return Socket world location or ZeroVector when invalid.
	 */
	FVector GetReinforcementLocation(bool& bOutHasValidLocation) const;

private:
	bool GetIsValidReinforcementMesh() const;
	void CreateReinforcementTriggerSphere(const float ActivationRadius);
	void HandleSquadUnitEnteredRadius(class ASquadUnit* OverlappingUnit) const;
	void HandleSquadUnitExitedRadius(class ASquadUnit* OverlappingUnit) const;

	UFUNCTION()
	void OnReinforcementOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                                 const FHitResult& SweepResult);

	UFUNCTION()
	void OnReinforcementOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_ReinforcementMeshComponent;

	UPROPERTY()
	FName M_ReinforcementSocketName;

	UPROPERTY()
	TObjectPtr<USphereComponent> M_ReinforcementTriggerSphere;

	UPROPERTY()
	int32 M_OwningPlayer;

	UPROPERTY()
	float M_ReinforcementActivationRadius;
};
