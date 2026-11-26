// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SupplyCratePickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

namespace SupplyCratePickupConstants
{
	// Reach of the trigger: a vehicle or soldier that passes over or right next to the crate collects it.
	constexpr float TriggerRadius = 220.f;
}

ASupplyCratePickup::ASupplyCratePickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);

	M_CrateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrateMesh"));
	M_CrateMesh->SetupAttachment(M_Root);
	// Units drive through the crate to collect it; it never blocks or affects navigation.
	M_CrateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_CrateMesh->SetGenerateOverlapEvents(false);
	M_CrateMesh->SetCanEverAffectNavigation(false);

	M_CrateTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerCircle"));
	M_CrateTrigger->SetupAttachment(M_Root);
	M_CrateTrigger->InitSphereRadius(SupplyCratePickupConstants::TriggerRadius);
	M_CrateTrigger->SetCanEverAffectNavigation(false);
}

void ASupplyCratePickup::InitSupplyCrate(UStaticMesh* CrateMesh, const float CrateMeshScale,
                                         const TMap<ERTSResourceType, int32>& Resources, USoundBase* PickupSound,
                                         UNiagaraSystem* PickupEffect)
{
	M_CrateMesh->SetStaticMesh(CrateMesh);
	M_CrateMesh->SetRelativeScale3D(FVector(FMath::Max(CrateMeshScale, KINDA_SMALL_NUMBER)));
	PickupSettings.ResourceGain.ResourceCosts = Resources;
	PickupSettings.PickupSound = PickupSound;
	PickupSettings.PickupNiagaraSystem = PickupEffect;
}
