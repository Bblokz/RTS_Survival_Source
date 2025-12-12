// Copyright (C) Bas Blokzijl - All rights reserved. 

#include "RepairComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"


URepairComponent::URepairComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URepairComponent::SetSquadOwner(ASquadUnit* OwnerUnit)
{
	M_OwnerSquadUnit = OwnerUnit;
	// Debugging.
	(void)GetIsValidOwnerSquadUnit();	
}

void URepairComponent::ExecuteRepair(AActor* RepairableActor, const FVector& RepairOffset)
{
	if (not GetIsValidOwnerSquadUnit())
	{
		return;
	}
	if (not FRTSRepairHelpers::GetIsUnitValidForRepairs(RepairableActor))
	{
		OnRepairUnitNotValidForRepairs();
		return;
	}
	M_RepairTarget = RepairableActor;
	M_RepairOffset = RepairOffset;
	RepairAcceptanceRadius = EnsureValidAcceptanceRadius();
	// checkf(FMath::IsNearlyZero(M_RepairOffset.Dot(FVector::OneVector)), *CheckRepairOffsetValidText());
	// Sets status and calls back on this component once mvt is complete.
	MoveToRepairableActor_SetStatus(RepairableActor);
}

void URepairComponent::TerminateRepair()
{
	M_RepairTarget = nullptr;
	M_RepairOffset = FVector::ZeroVector;
	M_RepairState = ERepairState::None;
	StopRepairTimer();
	RemoveRepairEquipment();
	StopRepairAnimation();
}

void URepairComponent::OnFinishedMovementForRepairAbility()
{
	if (not FRTSRepairHelpers::GetIsUnitValidForRepairs(M_RepairTarget) || not GetIsValidOwnerSquadUnit())
	{
		OnRepairUnitNotValidForRepairs();
		return;
	}
	// Did we finish the move-to-actor command?
	if (M_RepairState == ERepairState::MovingToRepairableActor)
	{
		MoveToRepairLocation_SetStatus();
		return;
	}
	// If not, then we arrived at the repair location.
	FRTSRepairHelpers::Debug_Repair("repair unit finished moving to repair location (callback) and will now"
		" start repairing");
	StartRepairTarget();
}


void URepairComponent::BeginPlay()
{
	Super::BeginPlay();
}


void URepairComponent::AddRepairEquipment()
{
	if (!GetIsValidOwnerSquadUnit())
	{
		RTSFunctionLibrary::ReportError("Cannot add repair equipment: invalid owner unit.");
		return;
	}

	ASquadUnit* OwnerUnit = M_OwnerSquadUnit;

	// Hide infantry weapon if valid
	SetSquadUnitWeaponDisabled(true);

	// Create and attach the static mesh component
	UStaticMeshComponent* EquipComp = NewObject<UStaticMeshComponent>(OwnerUnit);
	if (not EquipComp)
	{
		RTSFunctionLibrary::ReportError("Failed to create repair equipment!");
		SetSquadUnitWeaponDisabled(false);
		return;
	}
	EquipComp->SetStaticMesh(RepairEquipment);
	EquipComp->AttachToComponent(
		OwnerUnit->GetMesh(),
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		RepairEquipmentSocketName
	);
	EquipComp->RegisterComponent();
	EquipComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipComp->SetCanEverAffectNavigation(false);

	M_RepairEquipment = EquipComp;

	// Spawn & attach the Niagara effect to the equipment mesh
	if (IsValid(RepairEffect))
	{
		UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			RepairEffect,
			EquipComp,
			RepairEffectSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true
		);
		if (FX)
		{
			FX->SetAutoDestroy(true);
			M_RepairEffectComponent = FX;
		}
	}
}

void URepairComponent::PlayRepairAnimation() const
{
	if(not IsValid(M_RepairTarget) || not IsValid(M_OwnerSquadUnit))
	{
		return;
	}
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
		M_OwnerSquadUnit->GetActorLocation(),
		M_RepairTarget->GetActorLocation()
	);
	LookAtRotation.Pitch = 0.0f;
	LookAtRotation.Roll = 0.0f;
	M_OwnerSquadUnit->SetActorRotation(LookAtRotation);
	USquadUnitAnimInstance* AnimInstance = M_OwnerSquadUnit->AnimBp_SquadUnit;
	if(AnimInstance)
	{
		AnimInstance->PlayWeldingMontage();
	}
	
}

void URepairComponent::StopRepairAnimation() const
{
	if(not IsValid(M_OwnerSquadUnit))
	{
		return;
	}
	
	USquadUnitAnimInstance* AnimInstance = M_OwnerSquadUnit->AnimBp_SquadUnit;
	if(AnimInstance)
	{
		AnimInstance->StopAllMontages();
	}
}

void URepairComponent::RemoveRepairEquipment()
{
	if (!GetIsValidOwnerSquadUnit())
	{
		RTSFunctionLibrary::ReportError("Cannot remove repair equipment: invalid owner unit.");
		return;
	}

	ASquadUnit* OwnerUnit = M_OwnerSquadUnit;

	// Show infantry weapon again
	SetSquadUnitWeaponDisabled(false);

	// Destroy Niagara effect
	if (M_RepairEffectComponent)
	{
		M_RepairEffectComponent->DestroyComponent();
		M_RepairEffectComponent = nullptr;
	}

	// Destroy equipment mesh
	if (M_RepairEquipment)
	{
		M_RepairEquipment->DestroyComponent();
		M_RepairEquipment = nullptr;
	}
}


void URepairComponent::MoveToRepairableActor_SetStatus(AActor* RepairableActor)
{
	M_RepairState = ERepairState::MovingToRepairableActor;
	// First move to the repairable actor; Offset is only used for individual repair locations post reach.
	// Calls back from the unit once movement is Finished; see OnFinishedMovementForRepairAbility.
	M_OwnerSquadUnit->MoveToActorAndBindOnCompleted(RepairableActor, RepairAcceptanceRadius, EAbilityID::IdRepair);
}

void URepairComponent::MoveToRepairLocation_SetStatus()
{
	FRTSRepairHelpers::Debug_Repair("Unit finished move to repairable actor!");
	bool bIsValidLocation = false;
	const FVector RepairLocation = GetRepairLocation(bIsValidLocation);
	if (not bIsValidLocation)
	{
		RTSFunctionLibrary::ReportError("Could not get a valid projected repair location;"
			"starting repairs from current location");
		StartRepairTarget();
	}
	else
	{
		FRTSRepairHelpers::Debug_Repair("Repair unit arrived at repairable actor and will now move to"
			"repair location");
		M_RepairState = ERepairState::MovingToRepairLocation;
		M_OwnerSquadUnit->ExecuteMoveToSelfPathFinding(RepairLocation,
		                                               EAbilityID::IdRepair, true);
	}
}

bool URepairComponent::GetIsValidOwnerSquadUnit() const
{
	if (not IsValid(M_OwnerSquadUnit))
	{
		RTSFunctionLibrary::ReportError("Invalid squad unit owner for repair component!");
		return false;
	}
	return true;
}

void URepairComponent::OnRepairUnitNotValidForRepairs() const
{
	FRTSRepairHelpers::Debug_Repair("Repair component found that unit target for repair is no longer valid"
		"and calls for termination on the squad controller");
	ASquadController* SquadController = M_OwnerSquadUnit->GetSquadControllerChecked();
	if (SquadController)
	{
		// Calls done with repair id command which in turn triggers the terminate on all squad units.
		SquadController->OnRepairUnitNotValidForRepairs();
	}
}

FString URepairComponent::CheckRepairOffsetValidText() const
{
	const FString OwnerName = IsValid(M_OwnerSquadUnit)
		                          ? M_OwnerSquadUnit->GetName()
		                          : "Unknown Squad Unit";
	const FString Message = "Repair offset is nearly zero for"
		"\n Squad unit: " + OwnerName;
	return Message;
}

float URepairComponent::EnsureValidAcceptanceRadius()
{
	if (RepairAcceptanceRadius <= KINDA_SMALL_NUMBER)
	{
		const FString OwenerName = IsValid(M_OwnerSquadUnit)
			                           ? M_OwnerSquadUnit->GetName()
			                           : "Unknown Squad Unit";
		RTSFunctionLibrary::ReportError(
			"Repair Acceptance Radius is not set or is too small for squad unit: " + OwenerName +
			"\n Setting to default value of 100.0f");
		RepairAcceptanceRadius = 100.0f;
	}
	return RepairAcceptanceRadius;
}

FVector URepairComponent::GetRepairLocation(bool& OutValidPosition) const
{
	OutValidPosition = false;
	const FVector RepairLocation = RTSFunctionLibrary::GetLocationProjected(this,
	                                                                        M_RepairTarget->GetActorLocation() +
	                                                                        M_RepairOffset, true,
	                                                                        OutValidPosition, 1);
	return RepairLocation;
}

void URepairComponent::StartRepairTarget()
{
	if (not FRTSRepairHelpers::GetIsUnitValidForRepairs(M_RepairTarget))
	{
		OnRepairUnitNotValidForRepairs();
		return;
	}
	if (M_RepairState == ERepairState::Repairing)
	{
		RTSFunctionLibrary::ReportError("Repair component already repairing target, ignoring start repair call");
		return;
	}
	AddRepairEquipment();
	PlayRepairAnimation();
	M_RepairState = ERepairState::Repairing;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RepairTickHandle);
		TWeakObjectPtr<URepairComponent> WeakThis(this);
		auto RepairLambda = [WeakThis]()-> void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->RepairTick();
		};
		const float Rate = DeveloperSettings::GameBalance::Repair::RepairTickInterval;
		World->GetTimerManager().SetTimer(M_RepairTickHandle, RepairLambda, Rate, true);
	}
}

void URepairComponent::RepairTick()
{
	if (not RTSFunctionLibrary::RTSIsValid(M_RepairTarget) || not GetIsValidOwnerSquadUnit())
	{
		OnRepairUnitNotValidForRepairs();
		return;
	}
	UHealthComponent* HealthComponent = M_RepairTarget->FindComponentByClass<UHealthComponent>();
	if (not IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportError("Repair target does not have a valid health component!"
			"\n On RepairTick");
		OnRepairUnitNotValidForRepairs();
		return;
	}
	if (not GetIsRepairTargetInRange())
	{
		OnRepairTargetOutOfRange();
		return;
	}
	const float RepairAmount = DeveloperSettings::GameBalance::Repair::HpRepairedPerTick * RepairMlt;
	FRTSRepairHelpers::Debug_Repair("Repairing for " + FString::SanitizeFloat(RepairAmount) + " HP");
	const bool bIsRepaired = HealthComponent->Heal(RepairAmount);
	if (bIsRepaired)
	{
		OnRepairTargetFullyRepaired();
	}
}

bool URepairComponent::GetIsRepairTargetInRange() const
{
	const float Distance = M_OwnerSquadUnit->GetDistanceTo(M_RepairTarget);
	bool bIsValid = false;
	const float RepairRadius = FRTSRepairHelpers::GetUnitRepairRadius(bIsValid, M_RepairTarget);
	if (Distance > RepairRadius + (1.5 * RepairAcceptanceRadius))
	{
		FRTSRepairHelpers::Debug_Repair("Repair target is out of range for repairs!"
			"\n Distance: " + FString::SanitizeFloat(Distance) +
			"\n Repair Radius: " + FString::SanitizeFloat(RepairRadius) +
			"\n Repair Acceptance Radius: " + FString::SanitizeFloat(RepairAcceptanceRadius));
		return false;
	}
	return true;
}

void URepairComponent::OnRepairTargetOutOfRange()
{
	StopRepairTimer();
	StopRepairAnimation();
	RemoveRepairEquipment();
	// calls back on this component once mvt is complete.
	MoveToRepairableActor_SetStatus(M_RepairTarget);
	FRTSRepairHelpers::Debug_Repair("Repair target is out of range for repairs!"
		"\n Repair target: " + M_RepairTarget->GetName() +
		"\n Repair Acceptance Radius: " + FString::SanitizeFloat(RepairAcceptanceRadius));
}

void URepairComponent::OnRepairTargetFullyRepaired()
{
	if (not GetIsValidOwnerSquadUnit())
	{
		StopRepairTimer();
		return;
	}
	ASquadController* SquadController = M_OwnerSquadUnit->GetSquadControllerChecked();
	if (not SquadController)
	{
		StopRepairTimer();
		return;
	}
	FRTSRepairHelpers::Debug_Repair("Repair target is fully repaired!");
	SquadController->OnRepairTargetFullyRepaired(M_RepairTarget);
}

void URepairComponent::StopRepairTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RepairTickHandle);
	}
}

void URepairComponent::SetSquadUnitWeaponDisabled(const bool bDisabled) const
{
	if (not GetIsValidOwnerSquadUnit())
	{
		return;
	}
	AInfantryWeaponMaster* InfantryWeapon = M_OwnerSquadUnit->M_InfantryWeapon;
	if (not InfantryWeapon)
	{
		return;
	}
	if (bDisabled)
	{
		InfantryWeapon->DisableWeaponSearch(true, true);
	}
	else
	{
		InfantryWeapon->SetAutoEngageTargets(true);
	}
}
