#include "PitAdditionalTurretComponent.h"

#include "Components/ChildActorComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

UPitAdditionalTurretComponent::UPitAdditionalTurretComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPitAdditionalTurretComponent::BeginPlay()
{
	Super::BeginPlay();

	ABuildingExpansion* PitOwner = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(PitOwner))
	{
		return;
	}

	UChildActorComponent* TurretComponent = FindAdditionalTurretComponent();
	if (not IsValid(TurretComponent))
	{
		return;
	}

	ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(TurretComponent->GetChildActor());
	if (not IsValid(Turret))
	{
		return;
	}

	PitOwner->SetupTurret(Turret);
	PitOwner->OnBxpConstructed.AddUObject(this, &UPitAdditionalTurretComponent::HandlePitConstructed);
	PitOwner->OnBxpPackingUp.AddUObject(this, &UPitAdditionalTurretComponent::HandlePitPackingUp);
	PitOwner->OnBxpCancelPackingUp.AddUObject(this, &UPitAdditionalTurretComponent::HandlePitPackingCancelled);

	const bool bIsPitBuilt = PitOwner->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_Built;
	SetAdditionalTurretVisibility(bIsPitBuilt);
}

void UPitAdditionalTurretComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ABuildingExpansion* PitOwner = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(PitOwner))
	{
		Super::EndPlay(EndPlayReason);
		return;
	}

	PitOwner->OnBxpConstructed.RemoveAll(this);
	PitOwner->OnBxpPackingUp.RemoveAll(this);
	PitOwner->OnBxpCancelPackingUp.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

UChildActorComponent* UPitAdditionalTurretComponent::FindAdditionalTurretComponent() const
{
	const AActor* PitOwner = GetOwner();
	if (not IsValid(PitOwner))
	{
		return nullptr;
	}

	TArray<UChildActorComponent*> ChildActorComponents;
	PitOwner->GetComponents(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (not IsValid(ChildActorComponent))
		{
			continue;
		}

		if (ChildActorComponent->GetFName() == M_ChildActorComponentName)
		{
			return ChildActorComponent;
		}
	}

	return nullptr;
}

void UPitAdditionalTurretComponent::HandlePitConstructed()
{
	SetAdditionalTurretVisibility(true);
}

void UPitAdditionalTurretComponent::HandlePitPackingUp()
{
	SetAdditionalTurretVisibility(false);
}

void UPitAdditionalTurretComponent::HandlePitPackingCancelled()
{
	SetAdditionalTurretVisibility(true);
}

void UPitAdditionalTurretComponent::SetAdditionalTurretVisibility(const bool bVisible) const
{
	UChildActorComponent* TurretComponent = FindAdditionalTurretComponent();
	if (not IsValid(TurretComponent))
	{
		return;
	}

	TurretComponent->SetVisibility(bVisible, false);
}
