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

	void StartMovement(const TArray<FVector>& PathPoints, float Speed);
	void StopMovement();
	bool GetIsMoving() const { return bM_IsMoving; }
	FWorldDivisionMovementFinishedDelegate& OnMovementFinished() { return M_OnMovementFinished; }

private:
	UPROPERTY()
	TWeakObjectPtr<AWorldDivisionBase> M_WorldDivisionOwner;

	TArray<FVector> M_PathPoints;
	int32 M_TargetPathPointIndex = 1;
	float M_Speed = 0.f;
	bool bM_IsMoving = false;
	FWorldDivisionMovementFinishedDelegate M_OnMovementFinished;

	bool GetIsValidWorldDivisionOwner() const;
	void FinishMovement();
};
