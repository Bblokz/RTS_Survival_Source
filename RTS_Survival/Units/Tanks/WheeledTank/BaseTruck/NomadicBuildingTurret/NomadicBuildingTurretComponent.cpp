// Copyright (C) Bas Blokzijl - All rights reserved.

#include "NomadicBuildingTurretComponent.h"

#include "Components/ChildActorComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

UNomadicBuildingTurretComponent::UNomadicBuildingTurretComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNomadicBuildingTurretComponent::BeginPlay()
{
	Super::BeginPlay();

	M_NomadOwner = Cast<ANomadicVehicle>(GetOwner());
	if (not GetIsValidNomadOwner())
	{
		return;
	}
	if (not IsValid(GetTurret()))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Nomadic building turret on %s found no turret child actor named %s."),
			*GetNameSafe(GetOwner()), *M_ChildActorComponentName.ToString()));
		return;
	}

	ANomadicVehicle* NomadOwner = M_NomadOwner.Get();
	M_FinishedBuildingHandle = NomadOwner->OnNomadFinishedConvertToBuilding.AddUObject(
		this, &UNomadicBuildingTurretComponent::HandleBuildingExpanded);
	M_PackUpStartedHandle = NomadOwner->OnNomadConvertToVehicle.AddUObject(
		this, &UNomadicBuildingTurretComponent::HandlePackUpStarted);
	M_PackUpCancelledHandle = NomadOwner->OnNomadCancelledConvertToVehicle.AddUObject(
		this, &UNomadicBuildingTurretComponent::HandlePackUpCancelled);

	// A vehicle that starts as a building runs the normal expansion flow, which broadcasts the expansion.
	if (NomadOwner->GetNomadicStatus() == ENomadStatus::Building)
	{
		ActivateTurret();
		return;
	}
	bM_IsTurretActive = true;
	DeactivateTurret();
}

void UNomadicBuildingTurretComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANomadicVehicle* NomadOwner = M_NomadOwner.Get())
	{
		NomadOwner->OnNomadFinishedConvertToBuilding.Remove(M_FinishedBuildingHandle);
		NomadOwner->OnNomadConvertToVehicle.Remove(M_PackUpStartedHandle);
		NomadOwner->OnNomadCancelledConvertToVehicle.Remove(M_PackUpCancelledHandle);
	}
	Super::EndPlay(EndPlayReason);
}

UChildActorComponent* UNomadicBuildingTurretComponent::FindTurretChildActorComponent() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return nullptr;
	}
	TArray<UChildActorComponent*> ChildActorComponents;
	Owner->GetComponents(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (IsValid(ChildActorComponent) && ChildActorComponent->GetFName() == M_ChildActorComponentName)
		{
			return ChildActorComponent;
		}
	}
	return nullptr;
}

ACPPTurretsMaster* UNomadicBuildingTurretComponent::GetTurret() const
{
	const UChildActorComponent* TurretComponent = FindTurretChildActorComponent();
	return IsValid(TurretComponent) ? Cast<ACPPTurretsMaster>(TurretComponent->GetChildActor()) : nullptr;
}

bool UNomadicBuildingTurretComponent::GetIsValidNomadOwner() const
{
	if (M_NomadOwner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_NomadOwner"),
		TEXT("UNomadicBuildingTurretComponent::GetIsValidNomadOwner"),
		GetOwner());
	return false;
}

void UNomadicBuildingTurretComponent::HandleBuildingExpanded()
{
	ActivateTurret();
}

void UNomadicBuildingTurretComponent::HandlePackUpStarted()
{
	DeactivateTurret();
}

void UNomadicBuildingTurretComponent::HandlePackUpCancelled()
{
	ActivateTurret();
}

void UNomadicBuildingTurretComponent::ActivateTurret()
{
	ACPPTurretsMaster* Turret = GetTurret();
	if (bM_IsTurretActive || not IsValid(Turret) || not GetIsValidNomadOwner())
	{
		return;
	}
	bM_IsTurretActive = true;
	SetTurretPresent(true);
	// Registered only while expanded, so truck-mode attack and stop commands never reach it.
	M_NomadOwner->SetBuildingTurretRegistered(Turret, true);
	Turret->SetAutoEngageTargets(false);
}

void UNomadicBuildingTurretComponent::DeactivateTurret()
{
	ACPPTurretsMaster* Turret = GetTurret();
	if (not bM_IsTurretActive || not IsValid(Turret))
	{
		return;
	}
	bM_IsTurretActive = false;
	Turret->DisableTurret();
	if (ANomadicVehicle* NomadOwner = M_NomadOwner.Get())
	{
		NomadOwner->SetBuildingTurretRegistered(Turret, false);
	}
	SetTurretPresent(false);
}

void UNomadicBuildingTurretComponent::SetTurretPresent(const bool bPresent) const
{
	UChildActorComponent* TurretComponent = FindTurretChildActorComponent();
	if (IsValid(TurretComponent))
	{
		TurretComponent->SetVisibility(bPresent, true);
	}
	AActor* Turret = GetTurret();
	if (not IsValid(Turret))
	{
		return;
	}
	Turret->SetActorHiddenInGame(not bPresent);
	Turret->SetActorEnableCollision(bPresent);
}
