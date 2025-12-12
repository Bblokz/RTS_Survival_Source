#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"

#include "ATestTurretOwner.generated.h"

UCLASS()
class RTS_SURVIVAL_API ATestTurretOwner : public ASelectableActorObjectsMaster, public ITurretOwner
{
	GENERATED_BODY()
public:
	virtual int GetOwningPlayer() override;

	virtual void OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret) override;

	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override;


	virtual void OnMountedWeaponTargetDestroyed(ACPPTurretsMaster* CallingTurret, UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor, const bool
	                                            bWasDestroyedByOwnWeapons) override;

	virtual FRotator GetOwnerRotation() const override;

protected:
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddTurret(ACPPTurretsMaster* Turret);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetTurretsToAutoEngage();

	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	

private:
	UPROPERTY()
	UMeshComponent* M_MeshComponent;

	UPROPERTY()
	TArray<ACPPTurretsMaster*> M_Turrets;

};

