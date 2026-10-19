// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "NomadicBuildingTurretComponent.generated.h"

class ACPPTurretsMaster;
class ANomadicVehicle;
class UChildActorComponent;

/**
 * @brief Added to a nomadic vehicle to give its expanded building an extra child-actor turret (the German HQ's
 * front MG42). The turret only exists for gameplay while the vehicle is a fully expanded building: it is hidden,
 * disabled and unregistered from the vehicle's turrets while it is a truck, expanding or packing up, so truck
 * commands never wake it, and it is registered and set to auto engage again once the building is expanded or
 * packing up is cancelled.
 * @note Name the child actor component with M_ChildActorComponentName; it may be parented to the building mesh.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UNomadicBuildingTurretComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNomadicBuildingTurretComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, Category = "Nomadic Building Turret")
	FName M_ChildActorComponentName = TEXT("FrontMG42");

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_NomadOwner;

	// Whether the turret is registered on the vehicle and engaging.
	bool bM_IsTurretActive = false;

	FDelegateHandle M_FinishedBuildingHandle;
	FDelegateHandle M_PackUpStartedHandle;
	FDelegateHandle M_PackUpCancelledHandle;

	UChildActorComponent* FindTurretChildActorComponent() const;
	ACPPTurretsMaster* GetTurret() const;
	bool GetIsValidNomadOwner() const;

	void HandleBuildingExpanded();
	void HandlePackUpStarted();
	void HandlePackUpCancelled();

	void ActivateTurret();
	void DeactivateTurret();
	void SetTurretPresent(const bool bPresent) const;
};
