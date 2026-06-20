// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TankTargetAcquisition.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values for this component's properties
UTankTargetAcquisition::UTankTargetAcquisition()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UTankTargetAcquisition::BeginPlay()
{
	M_OwningTank = Cast<ATankMaster>(GetOwner());
	(void)EnsureIsValidOwningTank();
	Super::BeginPlay();
	

}

void UTankTargetAcquisition::IssueAttackClosestVisibleTargetInAggroRange(AActor* TargetActor)
{
	Super::IssueAttackClosestVisibleTargetInAggroRange(TargetActor);
}

ETargetPreference UTankTargetAcquisition::GetTargetPreference()
{
	
}

bool UTankTargetAcquisition::CanAggroEnemies() const
{
	if (not EnsureIsValidOwningTank() || not M_OwningTank->GetIsUnitIdle())
	{
		return false;
	}
	UDigInComponent* DigIn =  M_OwningTank->GetDigInComponent();
	if (not IsValid(DigIn))
	{
		return false;
	}
	if (not DigIn->GetIsMovable())
	{
		return false;
	}
	// Check for enum state == Aggressive.
	return Super::CanAggroEnemies();
}

bool UTankTargetAcquisition::EnsureIsValidOwningTank() const
{
	if (not M_OwningTank.IsValid())
	{
		RTSFunctionLibrary::ReportError("no valid owning tank for target acquistion tank component: " + GetName());
		return false;
	}
	return true;
}
