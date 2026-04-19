// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ExperienceLevelBehaviour.h"

#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ExperienceLevelBehaviourConstants
{
	constexpr int32 DefaultExperienceBehaviourMaxStackCount = 10;
}

UExperienceLevelBehaviour::UExperienceLevelBehaviour()
{
	BehaviourStackRule = EBehaviourStackRule::Stack;
	M_MaxStackCount = ExperienceLevelBehaviourConstants::DefaultExperienceBehaviourMaxStackCount;
}

void UExperienceLevelBehaviour::OnAdded(AActor* BehaviourOwner)
{
	CacheOwnerComponents();
	ApplyOwnerComponentMultipliers();
	Super::OnAdded(BehaviourOwner);
}

void UExperienceLevelBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	RestoreOwnerComponentMultipliers();
	ClearCachedOwnerComponents();
	Super::OnRemoved(BehaviourOwner);
}

void UExperienceLevelBehaviour::OnStack(UBehaviour* StackedBehaviour)
{
	Super::OnStack(StackedBehaviour);
	CacheOwnerComponents();
	ApplyOwnerComponentMultipliers();
}

bool UExperienceLevelBehaviour::GetIsValidHealthComponent() const
{
	if (M_HealthComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_HealthComponent",
		"GetIsValidHealthComponent",
		GetOwningActor()
	);
	return false;
}

bool UExperienceLevelBehaviour::GetIsValidFowComponent() const
{
	if (M_FowComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_FowComponent",
		"GetIsValidFowComponent",
		GetOwningActor()
	);
	return false;
}

void UExperienceLevelBehaviour::CacheOwnerComponents()
{
	AActor* OwningActor = GetOwningActor();
	if (not IsValid(OwningActor))
	{
		return;
	}

	if (not M_HealthComponent.IsValid())
	{
		M_HealthComponent = OwningActor->FindComponentByClass<UHealthComponent>();
	}

	if (not M_FowComponent.IsValid())
	{
		M_FowComponent = OwningActor->FindComponentByClass<UFowComp>();
	}
}

void UExperienceLevelBehaviour::ApplyOwnerComponentMultipliers()
{
	ApplyHealthMultiplier();
	ApplyVisionRadiusMultiplier();
}

void UExperienceLevelBehaviour::ApplyHealthMultiplier()
{
	if (FMath::IsNearlyEqual(HealthMultiplier, 1.f))
	{
		return;
	}

	if (not GetIsValidHealthComponent())
	{
		return;
	}

	UHealthComponent* HealthComponent = M_HealthComponent.Get();
	if (not M_OwnerStats.bM_HasCachedBaseMaxHealth)
	{
		M_OwnerStats.M_BaseMaxHealth = HealthComponent->GetMaxHealth();
		M_OwnerStats.bM_HasCachedBaseMaxHealth = true;
	}

	M_OwnerStats.M_AppliedHealthMultiplier *= HealthMultiplier;
	HealthComponent->SetMaxHealth(M_OwnerStats.M_BaseMaxHealth * M_OwnerStats.M_AppliedHealthMultiplier);
}

void UExperienceLevelBehaviour::ApplyVisionRadiusMultiplier()
{
	if (FMath::IsNearlyEqual(VisionRadiusMultiplier, 1.f))
	{
		return;
	}

	if (not GetIsValidFowComponent())
	{
		return;
	}

	UFowComp* FowComponent = M_FowComponent.Get();
	if (not M_OwnerStats.bM_HasCachedBaseVisionRadius)
	{
		M_OwnerStats.M_BaseVisionRadius = FowComponent->GetVisionRadius();
		M_OwnerStats.bM_HasCachedBaseVisionRadius = true;
	}

	M_OwnerStats.M_AppliedVisionRadiusMultiplier *= VisionRadiusMultiplier;
	FowComponent->SetVisionRadius(M_OwnerStats.M_BaseVisionRadius * M_OwnerStats.M_AppliedVisionRadiusMultiplier);
}

void UExperienceLevelBehaviour::RestoreOwnerComponentMultipliers()
{
	RestoreHealthMultiplier();
	RestoreVisionRadiusMultiplier();
}

void UExperienceLevelBehaviour::RestoreHealthMultiplier()
{
	if (not M_OwnerStats.bM_HasCachedBaseMaxHealth)
	{
		return;
	}

	if (not GetIsValidHealthComponent())
	{
		return;
	}

	M_HealthComponent->SetMaxHealth(M_OwnerStats.M_BaseMaxHealth);
	M_OwnerStats.M_AppliedHealthMultiplier = 1.f;
	M_OwnerStats.bM_HasCachedBaseMaxHealth = false;
}

void UExperienceLevelBehaviour::RestoreVisionRadiusMultiplier()
{
	if (not M_OwnerStats.bM_HasCachedBaseVisionRadius)
	{
		return;
	}

	if (not GetIsValidFowComponent())
	{
		return;
	}

	M_FowComponent->SetVisionRadius(M_OwnerStats.M_BaseVisionRadius);
	M_OwnerStats.M_AppliedVisionRadiusMultiplier = 1.f;
	M_OwnerStats.bM_HasCachedBaseVisionRadius = false;
}

void UExperienceLevelBehaviour::ClearCachedOwnerComponents()
{
	M_HealthComponent = nullptr;
	M_FowComponent = nullptr;
}
