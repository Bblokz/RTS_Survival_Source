// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleTurretBehaviour.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

UVehicleModuleTurretBehaviour::UVehicleModuleTurretBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::None;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Damaged Turret");
	M_DisplayText = TEXT("Turret traverse is slowed or locked; aligned guns can still fire.");
}

void UVehicleModuleTurretBehaviour::OnAdded(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	ApplyTraverseRestriction();
	Super::OnAdded(BehaviourOwner);
}

void UVehicleModuleTurretBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	RemoveTraverseRestriction();
	M_TankMaster.Reset();
	Super::OnRemoved(BehaviourOwner);
}

void UVehicleModuleTurretBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	RemoveTraverseRestriction();
	ApplyTraverseRestriction();
	Super::OnModuleContextUpdated(PreviousContext);
}

void UVehicleModuleTurretBehaviour::ApplyTraverseRestriction()
{
	if (not GetIsValidTankMaster() || GetModuleContext().Type != EVehicleModuleTypes::Turret)
	{
		return;
	}
	UMeshComponent* BoundMesh = GetBoundMesh();
	if (not IsValid(BoundMesh))
	{
		return;
	}
	for (ACPPTurretsMaster* Turret : M_TankMaster->GetTurrets())
	{
		if (not IsValid(Turret) || Turret->GetModuleBindingMesh() != BoundMesh)
		{
			continue;
		}
		const float Multiplier = GetModuleContext().State == EVehicleModuleState::Destroyed
			? VehicleModuleBalance::DefaultBehaviours::TurretDestroyedTraverseMultiplier
			: VehicleModuleBalance::DefaultBehaviours::TurretYellowTraverseMultiplier;
		Turret->SetModuleTraverseMultiplier(this, Multiplier);
		M_RestrictedTurret = Turret;
		bM_HasRegisteredTraverseRestriction = true;
		return;
	}
}

void UVehicleModuleTurretBehaviour::RemoveTraverseRestriction()
{
	if (bM_HasRegisteredTraverseRestriction && GetIsValidRestrictedTurret())
	{
		M_RestrictedTurret->ClearModuleTraverseMultiplier(this);
	}
	M_RestrictedTurret.Reset();
	bM_HasRegisteredTraverseRestriction = false;
}

bool UVehicleModuleTurretBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_TankMaster", "GetIsValidTankMaster", this);
	return false;
}

bool UVehicleModuleTurretBehaviour::GetIsValidRestrictedTurret() const
{
	if (M_RestrictedTurret.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_RestrictedTurret", "GetIsValidRestrictedTurret", this);
	return false;
}
