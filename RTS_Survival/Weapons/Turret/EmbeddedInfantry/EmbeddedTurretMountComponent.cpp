// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "EmbeddedTurretMountComponent.h"

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

UEmbeddedTurretMountComponent::UEmbeddedTurretMountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ACPPTurretsMaster* UEmbeddedTurretMountComponent::GetMountedTurret() const
{
	if (not IsValid(M_MountChildActorComponent))
	{
		return nullptr;
	}
	return Cast<ACPPTurretsMaster>(M_MountChildActorComponent->GetChildActor());
}

void UEmbeddedTurretMountComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_ScheduleMount();
}

void UEmbeddedTurretMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_MountTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UEmbeddedTurretMountComponent::BeginPlay_ScheduleMount()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (EmbeddedTurretClasses.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("UEmbeddedTurretMountComponent on " + GetNameSafe(GetOwner())
			+ " has no EmbeddedTurretClasses; no turret is mounted.");
		return;
	}

	const TWeakObjectPtr<UEmbeddedTurretMountComponent> WeakThis(this);
	M_MountTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->MountTurret();
	}));
}

void UEmbeddedTurretMountComponent::MountTurret()
{
	M_MountTimerHandle.Invalidate();
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return;
	}

	UMeshComponent* TargetMesh = FindMountMesh();
	if (not IsValid(TargetMesh))
	{
		ReportMountMeshNotFound();
		return;
	}

	const TSubclassOf<ACPPTurretsMaster> ChosenTurretClass = GetRandomEmbeddedTurretClass();
	if (not IsValid(ChosenTurretClass))
	{
		RTSFunctionLibrary::ReportError("UEmbeddedTurretMountComponent on " + GetNameSafe(Owner)
			+ " has no valid entry in EmbeddedTurretClasses; no turret is mounted.");
		return;
	}

	M_MountChildActorComponent = CreateMountChildActorComponent(TargetMesh, ChosenTurretClass);
	if (not GetIsValidMountChildActorComponent())
	{
		return;
	}

	ACPPTurretsMaster* Turret = GetMountedTurret();
	if (not IsValid(Turret))
	{
		RTSFunctionLibrary::ReportError("UEmbeddedTurretMountComponent on " + GetNameSafe(Owner)
			+ " failed to spawn its turret " + GetNameSafe(ChosenTurretClass.Get()));
		return;
	}
	RegisterTurretWithOwner(Turret);
}

TSubclassOf<ACPPTurretsMaster> UEmbeddedTurretMountComponent::GetRandomEmbeddedTurretClass() const
{
	TArray<TSubclassOf<ACPPTurretsMaster>> ValidTurretClasses;
	for (const TSubclassOf<ACPPTurretsMaster>& TurretClass : EmbeddedTurretClasses)
	{
		if (IsValid(TurretClass))
		{
			ValidTurretClasses.Add(TurretClass);
		}
	}

	if (ValidTurretClasses.IsEmpty())
	{
		return nullptr;
	}
	return ValidTurretClasses[FMath::RandRange(0, ValidTurretClasses.Num() - 1)];
}

UMeshComponent* UEmbeddedTurretMountComponent::FindMountMesh() const
{
	if (not bIsTankInfantryTurret)
	{
		return FindOwnerMeshWithSocket(UMeshComponent::StaticClass());
	}

	switch (MountMesh)
	{
	case EEmbeddedTurretMountMesh::TankTurretSkeletalMesh:
		return FindTankTurretMeshWithSocket(USkeletalMeshComponent::StaticClass());
	case EEmbeddedTurretMountMesh::TankTurretStaticMesh:
		return FindTankTurretMeshWithSocket(UStaticMeshComponent::StaticClass());
	case EEmbeddedTurretMountMesh::TankHullStaticMesh:
		return FindOwnerMeshWithSocket(UStaticMeshComponent::StaticClass());
	}
	return nullptr;
}

UMeshComponent* UEmbeddedTurretMountComponent::FindTankTurretMeshWithSocket(
	const TSubclassOf<UMeshComponent> MeshClass) const
{
	const AActor* Owner = GetOwner();
	TArray<UChildActorComponent*> ChildActorComponents;
	Owner->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (const UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (not IsValid(ChildActorComponent) || ChildActorComponent == M_MountChildActorComponent)
		{
			continue;
		}
		const ACPPTurretsMaster* HostTurret = Cast<ACPPTurretsMaster>(ChildActorComponent->GetChildActor());
		if (not IsValid(HostTurret))
		{
			continue;
		}

		TArray<UMeshComponent*> TurretMeshes;
		HostTurret->GetComponents<UMeshComponent>(TurretMeshes);
		for (UMeshComponent* TurretMesh : TurretMeshes)
		{
			if (GetMeshHasMountSocket(TurretMesh, MeshClass))
			{
				return TurretMesh;
			}
		}
	}
	return nullptr;
}

UMeshComponent* UEmbeddedTurretMountComponent::FindOwnerMeshWithSocket(const TSubclassOf<UMeshComponent> MeshClass) const
{
	TArray<UMeshComponent*> OwnerMeshes;
	GetOwner()->GetComponents<UMeshComponent>(OwnerMeshes);
	for (UMeshComponent* OwnerMesh : OwnerMeshes)
	{
		if (GetMeshHasMountSocket(OwnerMesh, MeshClass))
		{
			return OwnerMesh;
		}
	}
	return nullptr;
}

bool UEmbeddedTurretMountComponent::GetMeshHasMountSocket(
	const UMeshComponent* Mesh,
	const TSubclassOf<UMeshComponent> MeshClass) const
{
	return IsValid(Mesh) && Mesh->IsA(MeshClass) && Mesh->DoesSocketExist(MountSocketName);
}

UChildActorComponent* UEmbeddedTurretMountComponent::CreateMountChildActorComponent(
	UMeshComponent* TargetMesh,
	const TSubclassOf<ACPPTurretsMaster> TurretClass)
{
	AActor* Owner = GetOwner();
	UChildActorComponent* ChildActorComponent = NewObject<UChildActorComponent>(
		Owner, MakeUniqueObjectName(Owner, UChildActorComponent::StaticClass(), TEXT("EmbeddedTurretMount")));
	if (not IsValid(ChildActorComponent))
	{
		return nullptr;
	}

	ChildActorComponent->SetChildActorClass(TurretClass);
	ChildActorComponent->SetupAttachment(TargetMesh, MountSocketName);
	ChildActorComponent->SetRelativeLocation(MountLocalOffset);
	ChildActorComponent->SetCanEverAffectNavigation(false);
	Owner->AddInstanceComponent(ChildActorComponent);
	// Registering spawns the turret, which begins play at once.
	ChildActorComponent->RegisterComponent();
	return ChildActorComponent;
}

void UEmbeddedTurretMountComponent::RegisterTurretWithOwner(ACPPTurretsMaster* Turret) const
{
	AActor* Owner = GetOwner();
	if (ATankMaster* TankOwner = Cast<ATankMaster>(Owner))
	{
		TankOwner->SetupTurret(Turret);
	}
	else if (ABuildingExpansion* BxpOwner = Cast<ABuildingExpansion>(Owner))
	{
		BxpOwner->SetupTurret(Turret);
	}
	else
	{
		RTSFunctionLibrary::ReportError("UEmbeddedTurretMountComponent: owner " + GetNameSafe(Owner)
			+ " is neither a tank nor a building expansion; its turret cannot be commanded.");
	}
	Turret->SetAutoEngageTargets(false);
}

void UEmbeddedTurretMountComponent::ReportMountMeshNotFound() const
{
	const FString MeshDescription = not bIsTankInfantryTurret
		? TEXT("any mesh")
		: UEnum::GetValueAsString(MountMesh);
	RTSFunctionLibrary::ReportError("UEmbeddedTurretMountComponent on " + GetNameSafe(GetOwner())
		+ " found no " + MeshDescription + " with socket " + MountSocketName.ToString()
		+ "; no turret is mounted.");
}

bool UEmbeddedTurretMountComponent::GetIsValidMountChildActorComponent() const
{
	if (IsValid(M_MountChildActorComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_MountChildActorComponent",
		"GetIsValidMountChildActorComponent",
		GetOwner()
	);
	return false;
}
