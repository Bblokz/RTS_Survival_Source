// Copyright (C) Bas Blokzijl - All rights reserved.

#include "StealthTechComponent.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

UStealthTechComponent::UStealthTechComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UStealthTechComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStealthTimers();
	RestoreOriginalMaterialsAndClearCache();
	Super::EndPlay(EndPlayReason);
}

bool UStealthTechComponent::EnableStealth()
{
	if (M_StealthState != EStealthTechComponentState::Ready)
	{
		return false;
	}

	if (not GetAreStealthSettingsValid())
	{
		return false;
	}

	CacheOwnerMeshMaterials();
	if (not GetHasCachedMeshMaterials())
	{
		RTSFunctionLibrary::ReportError(
			"StealthTechComponent could not activate because no valid owner mesh materials were cached.");
		return false;
	}

	ApplyStealthMaterialToMeshes();
	M_StealthState = EStealthTechComponentState::Active;
	return StartStealthDurationTimer();
}

bool UStealthTechComponent::DisableStealth()
{
	if (M_StealthState != EStealthTechComponentState::Active)
	{
		return false;
	}

	ClearStealthTimers();
	RestoreOriginalMaterialsAndClearCache();
	M_StealthState = EStealthTechComponentState::Ready;
	return true;
}

bool UStealthTechComponent::GetCanEnableStealth() const
{
	const bool bHasStealthMaterial = M_StealthSettings.M_StealthMaterial != nullptr;
	const bool bHasValidDuration = M_StealthSettings.bM_IsPermanent || M_StealthSettings.M_StealthTime > 0.0f;
	return M_StealthState == EStealthTechComponentState::Ready && bHasStealthMaterial && bHasValidDuration;
}

void UStealthTechComponent::CacheOwnerMeshMaterials()
{
	M_MeshMaterials.Empty();
	if (not GetIsOwnerValidForMeshCaching())
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	GetOwner()->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		CacheMeshMaterials(MeshComponent);
	}
}

void UStealthTechComponent::CacheMeshMaterials(UMeshComponent* MeshComponent)
{
	if (not IsValid(MeshComponent))
	{
		return;
	}

	if (GetShouldSkipMeshComponent(MeshComponent))
	{
		return;
	}

	const int32 MaterialCount = MeshComponent->GetNumMaterials();
	if (MaterialCount <= 0)
	{
		return;
	}

	FStealthTechComponentMeshMaterials MeshMaterials;
	MeshMaterials.M_MeshComponent = MeshComponent;
	MeshMaterials.M_CachedMaterialSlotCount = MaterialCount;
	MeshMaterials.M_OriginalMaterials.Reserve(MaterialCount);
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		MeshMaterials.M_OriginalMaterials.Add(MeshComponent->GetMaterial(MaterialIndex));
	}
	M_MeshMaterials.Add(MeshMaterials);
}

void UStealthTechComponent::ApplyStealthMaterialToMeshes()
{
	for (const FStealthTechComponentMeshMaterials& MeshMaterials : M_MeshMaterials)
	{
		ApplyStealthMaterialToMesh(MeshMaterials);
	}
}

void UStealthTechComponent::ApplyStealthMaterialToMesh(const FStealthTechComponentMeshMaterials& MeshMaterials)
{
	UMeshComponent* MeshComponent = MeshMaterials.M_MeshComponent.Get();
	if (not IsValid(MeshComponent))
	{
		return;
	}

	const int32 MaterialCount = MeshComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		MeshComponent->SetMaterial(MaterialIndex, M_StealthSettings.M_StealthMaterial);
	}
}

void UStealthTechComponent::RestoreOriginalMaterialsAndClearCache()
{
	for (const FStealthTechComponentMeshMaterials& MeshMaterials : M_MeshMaterials)
	{
		RestoreOriginalMaterialsForMesh(MeshMaterials);
	}
	M_MeshMaterials.Empty();
}

void UStealthTechComponent::RestoreOriginalMaterialsForMesh(
	const FStealthTechComponentMeshMaterials& MeshMaterials) const
{
	UMeshComponent* MeshComponent = MeshMaterials.M_MeshComponent.Get();
	if (not IsValid(MeshComponent))
	{
		return;
	}

	const int32 CurrentMaterialCount = MeshComponent->GetNumMaterials();
	if (CurrentMaterialCount != MeshMaterials.M_CachedMaterialSlotCount)
	{
		RTSFunctionLibrary::ReportError(
			"StealthTechComponent material slot count changed while stealth was active."
			"\nMesh: " + MeshComponent->GetName());
	}

	const int32 MaterialCountToRestore = FMath::Min(
		MeshMaterials.M_OriginalMaterials.Num(),
		CurrentMaterialCount);
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCountToRestore; ++MaterialIndex)
	{
		MeshComponent->SetMaterial(MaterialIndex, MeshMaterials.M_OriginalMaterials[MaterialIndex]);
	}
}

bool UStealthTechComponent::StartStealthDurationTimer()
{
	if (M_StealthSettings.bM_IsPermanent)
	{
		return true;
	}

	if (not GetIsValidWorldForTimers("StartStealthDurationTimer"))
	{
		DisableStealth();
		return false;
	}

	GetWorld()->GetTimerManager().SetTimer(
		M_StealthDurationTimerHandle,
		this,
		&UStealthTechComponent::OnStealthDurationElapsed,
		M_StealthSettings.M_StealthTime,
		false);
	return true;
}

void UStealthTechComponent::StartCooldownTimer()
{
	if (not GetIsValidWorldForTimers("StartCooldownTimer"))
	{
		M_StealthState = EStealthTechComponentState::Ready;
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(M_StealthDurationTimerHandle);
	if (M_StealthSettings.M_CooldownTime <= 0.0f)
	{
		M_StealthState = EStealthTechComponentState::Ready;
		return;
	}

	M_StealthState = EStealthTechComponentState::Cooldown;
	GetWorld()->GetTimerManager().SetTimer(
		M_CooldownTimerHandle,
		this,
		&UStealthTechComponent::OnCooldownElapsed,
		M_StealthSettings.M_CooldownTime,
		false);
}

void UStealthTechComponent::ClearStealthTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_StealthDurationTimerHandle);
	World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
}

void UStealthTechComponent::OnStealthDurationElapsed()
{
	if (M_StealthState != EStealthTechComponentState::Active)
	{
		return;
	}

	RestoreOriginalMaterialsAndClearCache();
	StartCooldownTimer();
}

void UStealthTechComponent::OnCooldownElapsed()
{
	M_StealthState = EStealthTechComponentState::Ready;
}

bool UStealthTechComponent::GetAreStealthSettingsValid() const
{
	if (not GetIsValidStealthMaterial())
	{
		return false;
	}

	if (M_StealthSettings.bM_IsPermanent)
	{
		return true;
	}

	if (M_StealthSettings.M_StealthTime > 0.0f)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"StealthTechComponent has non-permanent stealth with M_StealthTime <= 0."
		"\nOwner: " + GetNameSafe(GetOwner()));
	return false;
}

bool UStealthTechComponent::GetIsValidStealthMaterial() const
{
	if (not IsValid(M_StealthSettings.M_StealthMaterial))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_StealthSettings.M_StealthMaterial",
			"GetIsValidStealthMaterial",
			this);
		return false;
	}

	return true;
}

bool UStealthTechComponent::GetIsOwnerValidForMeshCaching() const
{
	if (not IsValid(GetOwner()))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"Owner",
			"GetIsOwnerValidForMeshCaching",
			this);
		return false;
	}

	return true;
}

bool UStealthTechComponent::GetHasCachedMeshMaterials() const
{
	return not M_MeshMaterials.IsEmpty();
}

bool UStealthTechComponent::GetIsValidWorldForTimers(const FString& FunctionName) const
{
	if (not IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"World",
			FunctionName,
			this);
		return false;
	}

	return true;
}

bool UStealthTechComponent::GetShouldSkipMeshComponent(const UMeshComponent* MeshComponent) const
{
	if (not IsValid(MeshComponent))
	{
		return true;
	}

	for (const FName& ExcludedMeshTag : M_StealthSettings.M_ExcludedMeshTags)
	{
		if (MeshComponent->ComponentHasTag(ExcludedMeshTag))
		{
			return true;
		}
	}

	return false;
}
