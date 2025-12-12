// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "SecondaryWeapon.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"


// Sets default values for this component's properties
USecondaryWeapon::USecondaryWeapon(): M_OwningSquadUnit(nullptr), M_WeaponClass(nullptr),
                                      M_WeaponName(EWeaponName::DEFAULT_WEAPON), M_OwnerSecondaryWeaponMesh(nullptr)

{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void USecondaryWeapon::InitializeOwner(const TObjectPtr<ASquadUnit> NewOwner)
{
	if (IsValid(NewOwner))
	{
		M_OwningSquadUnit = NewOwner;
		return;
	}
	RTSFunctionLibrary::ReportError("Cannot initialize secondary weapon on unit: " + M_OwningSquadUnit->GetName() +
		"\n as the provided owner is null.");
}


void USecondaryWeapon::SetupSecondaryWeaponMesh(UStaticMesh* WeaponMesh, const EWeaponName WeaponName,
                                                const TSoftClassPtr<AInfantryWeaponMaster>& WeaponClass)
{
	if (IsValid(WeaponMesh) && WeaponClass)
	{
		M_WeaponName = WeaponName;
		M_WeaponClass = WeaponClass;
		if (IsValid(M_OwnerSecondaryWeaponMesh))
		{
			M_OwnerSecondaryWeaponMesh->SetStaticMesh(WeaponMesh);
		}
		else
		{
			InitialiseSecondaryWeaponMeshComponent();
			if (IsValid(M_OwnerSecondaryWeaponMesh))
			{
				M_OwnerSecondaryWeaponMesh->SetStaticMesh(WeaponMesh);
			}
		}
		return;
	}
	RTSFunctionLibrary::ReportError("Cannot setup secondary weapon mesh on unit: " + M_OwningSquadUnit->GetName() +
		"\n as either the provided weapon mesh or the weapon class is null.");
}

void USecondaryWeapon::ManuallySetSecondaryWeapon(UStaticMeshComponent* OwnerSecondaryWeaponMesh,
                                                  const EWeaponName NewWeaponName, TSoftClassPtr<AInfantryWeaponMaster>
                                                  NewWeaponClass)
{
	if (IsValid(OwnerSecondaryWeaponMesh))
	{
		M_OwnerSecondaryWeaponMesh = OwnerSecondaryWeaponMesh;
		SetCollisionSettings();
		M_WeaponName = NewWeaponName;
		M_WeaponClass = NewWeaponClass;
		return;
	}
	RTSFunctionLibrary::ReportError("Cannot manually set owner static mesh component for secondary weapon on unit: " +
		M_OwningSquadUnit->GetName() + "\n as the provided mesh is null.");
}

// Called when the game starts
void USecondaryWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void USecondaryWeapon::InitialiseSecondaryWeaponMeshComponent()
{
	if (IsValid(M_OwningSquadUnit) && IsValid(M_OwningSquadUnit->GetMesh()))
		if (M_OwnerSecondaryWeaponMesh == nullptr)
		{
			M_OwnerSecondaryWeaponMesh = NewObject<UStaticMeshComponent>(this, TEXT("SecondaryWeaponMesh"));
			M_OwnerSecondaryWeaponMesh->RegisterComponent();
			M_OwnerSecondaryWeaponMesh->AttachToComponent(M_OwningSquadUnit->GetMesh(),
			                                              FAttachmentTransformRules::KeepRelativeTransform,
			                                              M_OwningSquadUnit->GetSecondaryWeaponSocketName());
			SetCollisionSettings();
			if (!IsValid(M_OwnerSecondaryWeaponMesh))
			{
				const FString SocketNameAsString = M_OwningSquadUnit->GetSecondaryWeaponSocketName().ToString();
				RTSFunctionLibrary::ReportError(
					"Failed to create secondary weapon for unit: " + M_OwningSquadUnit->GetName()
					+ "\n at socket: " + SocketNameAsString);
			}
		}
}

void USecondaryWeapon::SetCollisionSettings()
{
	if (IsValid(M_OwnerSecondaryWeaponMesh))
	{
		M_OwnerSecondaryWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_OwnerSecondaryWeaponMesh->SetGenerateOverlapEvents(false);
		M_OwnerSecondaryWeaponMesh->SetCanEverAffectNavigation(false);
	}
}
