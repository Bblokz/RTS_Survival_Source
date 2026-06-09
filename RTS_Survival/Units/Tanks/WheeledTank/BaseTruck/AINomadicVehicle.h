// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/AI/AITrackTank.h"
#include "AINomadicVehicle.generated.h"

/**
 * @brief Controller used by nomadic vehicles to route tank movement completion into construction flow.
 */
UCLASS()
class RTS_SURVIVAL_API AAINomadicVehicle : public AAITrackTank
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAINomadicVehicle(const FObjectInitializer& ObjectInitializer);

	void StopBehaviourTree();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline FVector GetBuildingLocation() const {return M_BuildingLocation;};

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline void SetBuildingLocation(const FVector& NewBuildingLocation) {M_BuildingLocation = NewBuildingLocation;};

	inline float GetConstructionAcceptanceRad() const {return ConstructionAcceptanceRad;};

	bool GetIsControlledPawnAtBuildingLocation() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// How close the vehicle has to get to the building location to start construction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ConstructionAcceptanceRad = 100.0f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnQueuedMovementCompleted(const EAbilityID CompletedMovementAbility) override;
	virtual void OnQueuedMovementFailed(
		const EAbilityID FailedMovementAbility,
		const EPathFollowingResult::Type FailedMovementResultCode) override;
	virtual void OnQueuedMovementRequestFailed(const EAbilityID FailedMovementAbility) override;

private:
	// Location to place the building; needed in behaviour tree.
	UPROPERTY()
	FVector M_BuildingLocation;
};
 