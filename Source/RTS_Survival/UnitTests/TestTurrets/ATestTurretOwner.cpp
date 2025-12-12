#include "ATestTurretOwner.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

int ATestTurretOwner::GetOwningPlayer()
{
	if(IsValid(RTSComponent))
	{
		return RTSComponent->GetOwningPlayer();
	}
	return int();
}

void ATestTurretOwner::OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret)
{
}

void ATestTurretOwner::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
}

void ATestTurretOwner::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
}

void ATestTurretOwner::OnProjectileHit(const bool bBounced)
{
}

void ATestTurretOwner::OnMountedWeaponTargetDestroyed(ACPPTurretsMaster* CallingTurret, UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor, const bool
                                                      bWasDestroyedByOwnWeapons)
{
}

FRotator ATestTurretOwner::GetOwnerRotation() const
{
	if(IsValid(M_MeshComponent))
	{
		return M_MeshComponent->GetComponentRotation();
	}
	return FRotator(0, 0, 0);
}

void ATestTurretOwner::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_MeshComponent = FindComponentByClass<UMeshComponent>();
	if(!IsValid(M_MeshComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_MeshComponent", "PostInitializeComponents");
	}
}

void ATestTurretOwner::AddTurret(ACPPTurretsMaster* Turret)
{
	if(IsValid(Turret))
	{
		M_Turrets.Add(Turret);
	}
}

void ATestTurretOwner::SetTurretsToAutoEngage()
{
	for(const auto EachTurret : M_Turrets)
	{
		if(IsValid(EachTurret))
		{
			EachTurret->SetAutoEngageTargets(false);
		}
	}
}
