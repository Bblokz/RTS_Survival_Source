// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/AI/AITrackTank.h"
#include "AINomadicVehicle.generated.h"

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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// How close the vehicle has to get to the building location to start construction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ConstructionAcceptanceRad = 100.0f;

	virtual void OnPossess(APawn* InPawn) override;

private:
	// Location to place the building; needed in behaviour tree.
	UPROPERTY()
	FVector M_BuildingLocation;
};
 