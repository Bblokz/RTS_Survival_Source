// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourTurretRotation.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace TurretRotationBehaviourConstants
{
	constexpr int32 DefaultTurretRotationStackCount = 10;
}

UTurretRotationBehaviour::UTurretRotationBehaviour()
{
	BehaviourStackRule = EBehaviourStackRule::Stack;
	M_MaxStackCount = TurretRotationBehaviourConstants::DefaultTurretRotationStackCount;
}

void UTurretRotationBehaviour::OnAdded()
{
	bM_IsBehaviourActive = true;
	ApplyBehaviourToTurrets();
}

void UTurretRotationBehaviour::OnRemoved()
{
	RemoveBehaviourFromTurrets();
	ClearTrackedTurrets();
	bM_HasAppliedRotationAtLeastOnce = false;
	bM_IsBehaviourActive = false;
}

void UTurretRotationBehaviour::OnStack(UBehaviour* StackedBehaviour)
{
	static_cast<void>(StackedBehaviour);

	if (not bM_IsBehaviourActive)
	{
		return;
	}

	ApplyBehaviourToTurrets();
}

void UTurretRotationBehaviour::ApplyBehaviourToTurrets()
{
	ATankMaster* TankMaster = nullptr;
	if (not TryGetTankMaster(TankMaster))
	{
		GetIsValidTankMaster();
		return;
	}

	const TArray<ACPPTurretsMaster*> Turrets = TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		ApplyBehaviourToTurret(Turret);
	}

	bM_HasAppliedRotationAtLeastOnce = true;
}

void UTurretRotationBehaviour::ApplyBehaviourToTurret(ACPPTurretsMaster* Turret)
{
	if (Turret == nullptr)
	{
		return;
	}

	const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = Turret;
	if (M_AppliedRotationChanges.Contains(TurretPtr))
	{
		return;
	}

	const float BaseRotationSpeed = Turret->RotationSpeed;
	const float RotationChange = CalculateRotationSpeedChange(BaseRotationSpeed);

	Turret->RotationSpeed = BaseRotationSpeed + RotationChange;

	M_AppliedRotationChanges.Add(TurretPtr, RotationChange);
	M_AffectedTurrets.AddUnique(TurretPtr);
}

void UTurretRotationBehaviour::RemoveBehaviourFromTurrets()
{
	for (const TWeakObjectPtr<ACPPTurretsMaster>& TurretPtr : M_AffectedTurrets)
	{
		if (not TurretPtr.IsValid())
		{
			M_AppliedRotationChanges.Remove(TurretPtr);
			continue;
		}

		const float* CachedRotationChange = M_AppliedRotationChanges.Find(TurretPtr);
		if (CachedRotationChange == nullptr)
		{
			continue;
		}

		ACPPTurretsMaster* Turret = TurretPtr.Get();
		Turret->RotationSpeed -= *CachedRotationChange;
		M_AppliedRotationChanges.Remove(TurretPtr);
	}
}

void UTurretRotationBehaviour::ClearTrackedTurrets()
{
	M_AffectedTurrets.Empty();
	M_AppliedRotationChanges.Empty();
	M_TankMaster.Reset();
}

float UTurretRotationBehaviour::CalculateRotationSpeedChange(const float BaseRotationSpeed) const
{
	const float ScaledRotationSpeed = (BaseRotationSpeed * RotationSpeedMultiplier) + RotationSpeedAdd;
	return ScaledRotationSpeed - BaseRotationSpeed;
}

bool UTurretRotationBehaviour::TryGetTankMaster(ATankMaster*& OutTankMaster)
{
	OutTankMaster = M_TankMaster.Get();
	if (OutTankMaster != nullptr)
	{
		return true;
	}

	OutTankMaster = Cast<ATankMaster>(GetOwningActor());
	if (OutTankMaster == nullptr)
	{
		return false;
	}

	M_TankMaster = OutTankMaster;
	return true;
}

bool UTurretRotationBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_TankMaster"),
		TEXT("GetIsValidTankMaster")
	);

	return false;
}
