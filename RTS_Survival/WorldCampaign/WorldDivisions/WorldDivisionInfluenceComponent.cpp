// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionInfluenceComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"

UWorldDivisionInfluenceComponent::UWorldDivisionInfluenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldDivisionInfluenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideAllRadii();

	Super::EndPlay(EndPlayReason);
}

void UWorldDivisionInfluenceComponent::ShowHoverRadius()
{
	if (GetIsRadiusActive(M_SelectedRadiusId) || GetIsRadiusActive(M_HoverRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_HoverRadiusId = InvalidRadiusId;
	M_HoverRadiusId = CreateRadius();
}

void UWorldDivisionInfluenceComponent::HideHoverRadius()
{
	HideRadiusById(M_HoverRadiusId);
}

void UWorldDivisionInfluenceComponent::ShowSelectedRadius()
{
	HideHoverRadius();
	if (GetIsRadiusActive(M_SelectedRadiusId))
	{
		return;
	}

	M_SelectedRadiusId = InvalidRadiusId;
	M_SelectedRadiusId = CreateRadius();
}

void UWorldDivisionInfluenceComponent::HideSelectedRadius()
{
	HideRadiusById(M_SelectedRadiusId);
}

void UWorldDivisionInfluenceComponent::HideAllRadii()
{
	HideHoverRadius();
	HideSelectedRadius();
}

void UWorldDivisionInfluenceComponent::ShowHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDivisionInfluenceComponent*> InfluenceComponents;
	Actor->GetComponents<UWorldDivisionInfluenceComponent>(InfluenceComponents);
	for (UWorldDivisionInfluenceComponent* InfluenceComponent : InfluenceComponents)
	{
		if (not IsValid(InfluenceComponent))
		{
			continue;
		}

		InfluenceComponent->ShowHoverRadius();
	}
}

void UWorldDivisionInfluenceComponent::HideHoverRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDivisionInfluenceComponent*> InfluenceComponents;
	Actor->GetComponents<UWorldDivisionInfluenceComponent>(InfluenceComponents);
	for (UWorldDivisionInfluenceComponent* InfluenceComponent : InfluenceComponents)
	{
		if (not IsValid(InfluenceComponent))
		{
			continue;
		}

		InfluenceComponent->HideHoverRadius();
	}
}

void UWorldDivisionInfluenceComponent::ShowSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDivisionInfluenceComponent*> InfluenceComponents;
	Actor->GetComponents<UWorldDivisionInfluenceComponent>(InfluenceComponents);
	for (UWorldDivisionInfluenceComponent* InfluenceComponent : InfluenceComponents)
	{
		if (not IsValid(InfluenceComponent))
		{
			continue;
		}

		InfluenceComponent->ShowSelectedRadius();
	}
}

void UWorldDivisionInfluenceComponent::HideSelectedRadiiOnActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return;
	}

	TArray<UWorldDivisionInfluenceComponent*> InfluenceComponents;
	Actor->GetComponents<UWorldDivisionInfluenceComponent>(InfluenceComponents);
	for (UWorldDivisionInfluenceComponent* InfluenceComponent : InfluenceComponents)
	{
		if (not IsValid(InfluenceComponent))
		{
			continue;
		}

		InfluenceComponent->HideSelectedRadius();
	}
}

bool UWorldDivisionInfluenceComponent::GetIsValidRadiusPoolSubsystem()
{
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("World"),
			TEXT("UWorldDivisionInfluenceComponent::GetIsValidRadiusPoolSubsystem"),
			this);
		return false;
	}

	M_RadiusPoolSubsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_RadiusPoolSubsystem"),
		TEXT("UWorldDivisionInfluenceComponent::GetIsValidRadiusPoolSubsystem"),
		this);
	return false;
}

URTSRadiusPoolSubsystem* UWorldDivisionInfluenceComponent::GetRadiusPoolSubsystem()
{
	if (not GetIsValidRadiusPoolSubsystem())
	{
		return nullptr;
	}

	return M_RadiusPoolSubsystem.Get();
}

int32 UWorldDivisionInfluenceComponent::CreateRadius()
{
	const AWorldDivisionBase* DivisionOwner = Cast<AWorldDivisionBase>(GetOwner());
	if (not IsValid(DivisionOwner) || DivisionOwner->GetInfluenceAreaRadius() <= 0.f)
	{
		return InvalidRadiusId;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		return InvalidRadiusId;
	}

	AActor* Owner = GetOwner();
	const int32 RadiusId = RadiusPoolSubsystem->CreateRTSRadius(
		DivisionOwner->GetActorLocation(),
		DivisionOwner->GetInfluenceAreaRadius(),
		DivisionOwner->GetHoverRadiusType());
	if (RadiusId <= 0 || not IsValid(Owner))
	{
		return InvalidRadiusId;
	}

	RadiusPoolSubsystem->AttachRTSRadiusToActorYawOnly(RadiusId, Owner, FVector::ZeroVector);
	return RadiusId;
}

bool UWorldDivisionInfluenceComponent::GetIsRadiusActive(const int32 RadiusId)
{
	if (RadiusId <= 0)
	{
		return false;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		return false;
	}

	return RadiusPoolSubsystem->GetIsRTSRadiusIdActive(RadiusId);
}

void UWorldDivisionInfluenceComponent::HideRadiusById(int32& RadiusId)
{
	if (RadiusId <= 0)
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	URTSRadiusPoolSubsystem* RadiusPoolSubsystem = GetRadiusPoolSubsystem();
	if (not IsValid(RadiusPoolSubsystem))
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	if (not RadiusPoolSubsystem->GetIsRTSRadiusIdActive(RadiusId))
	{
		RadiusId = InvalidRadiusId;
		return;
	}

	RadiusPoolSubsystem->HideRTSRadiusById(RadiusId);
	RadiusId = InvalidRadiusId;
}
