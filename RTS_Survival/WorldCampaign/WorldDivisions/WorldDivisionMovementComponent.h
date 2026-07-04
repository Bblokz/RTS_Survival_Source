// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldDivisionMovementComponent.generated.h"

class AWorldDivisionBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FWorldDivisionMovementFinishedDelegate, AWorldDivisionBase*);

/**
 * @brief Moves world divisions along precomputed XY path points during turn-end movement windows.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldDivisionMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDivisionMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,
	                           ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * @brief Starts visual interpolation along an already budgeted turn movement path.
	 * @param PathPoints Ordered points for this turn only; index 0 should be current location.
	 * @param Speed Units per second used for simple interpolation.
	 */
	void StartMovement(const TArray<FVector>& PathPoints, float Speed);

	/**
	 * @brief Stops active interpolation and broadcasts completion if movement was in progress.
	 */
	void StopMovement();

	bool GetIsMoving() const { return bM_IsMoving; }

	/**
	 * @brief Delegate used by the manager to wait for all turn-end movement animations.
	 * @return Mutable completion delegate owned by this component.
	 */
	FWorldDivisionMovementFinishedDelegate& OnMovementFinished() { return M_OnMovementFinished; }

private:
	UPROPERTY()
	TWeakObjectPtr<AWorldDivisionBase> M_WorldDivisionOwner;

	TArray<FVector> M_PathPoints;
	int32 M_TargetPathPointIndex = 1;
	float M_Speed = 0.f;
	bool bM_IsMoving = false;
	FWorldDivisionMovementFinishedDelegate M_OnMovementFinished;

	/**
	 * @brief Validates and reports missing owner setup for deferred movement callbacks.
	 * @return true when the owning actor is a valid AWorldDivisionBase.
	 */
	bool GetIsValidWorldDivisionOwner() const;

	/**
	 * @brief Disables ticking and notifies listeners that visual movement has ended.
	 */
	void FinishMovement();
};
