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
	 * @param bStartEnabled Toggle to start with trigger overlaps enabled.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName,
	                            int32 OwningPlayer, float ActivationRadius, bool bStartEnabled = true);

	/**
	 * @brief Toggle the reinforcement trigger overlaps without destroying the sphere.
	 * @param bEnable True to enable overlaps, false to disable.
	 */
	void SetReinforcementEnabled(bool bEnable);

	/** @return True when the reinforcement trigger is currently enabled. */
	bool GetIsReinforcementEnabled() const;

	/**
	 * @brief Resolve the world-space location of the configured reinforcement socket.
	 * @param bOutHasValidLocation Set true when a valid mesh and socket were found.
	 * @return Socket world location or ZeroVector when invalid.
	 */
	FVector GetReinforcementLocation(bool& bOutHasValidLocation) const;

private:
	bool GetIsDebugEnabled() const;
	bool GetIsValidReinforcementMesh() const;
	bool GetIsValidReinforcementTriggerSphere(bool bReportIfMissing = true) const;
	bool CreateReinforcementTriggerSphere(const float ActivationRadius);
	void SetTriggerOverlapEnabled(bool bEnable) const;
	void DrawDebugStatusString(const FString& DebugText, const FVector& DrawLocation) const;
	void HandleSquadUnitEnteredRadius(class ASquadUnit* OverlappingUnit);
	void HandleSquadUnitExitedRadius(class ASquadUnit* OverlappingUnit) const;

	UFUNCTION()
	void OnReinforcementOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                                 const FHitResult& SweepResult);

	UFUNCTION()
	void OnReinforcementOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Either remove or add overlap depending on enable state.
	void OverlapOnReinforcementEnabled(float Radius, int32 OwningPlayer, bool bEnable);

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

	UPROPERTY()
	bool bM_ReinforcementEnabled = false;
};
