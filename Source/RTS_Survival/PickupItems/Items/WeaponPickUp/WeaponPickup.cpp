// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "WeaponPickup.h"

#include "Components/BoxComponent.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/OnHoverDescription/W_WeaponDescription.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values
AWeaponPickup::AWeaponPickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	WeaponClass = nullptr;
	WeaponName = EWeaponName::DEFAULT_WEAPON;
}

bool AWeaponPickup::EnsurePickupInitialized() const
{
	if (WeaponName != EWeaponName::DEFAULT_WEAPON && WeaponClass != nullptr)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"This weapon pickup has not been initialized correctly. Please check the weapon class and weapon name."
		"\n See : " + GetName());
	return false;
}

void AWeaponPickup::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

// Called when the game starts or when spawned
void AWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	UBoxComponent* BoxComponent = Cast<UBoxComponent>(GetComponentByClass(UBoxComponent::StaticClass()));
	UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(GetComponentByClass(UStaticMeshComponent::StaticClass()));
	FRTS_CollisionSetup::SetupPickupCollision(BoxComponent, MeshComponent);
}

void AWeaponPickup::OnStartOverlap(AActor* OtherActor)
{
	if (!GetIsItemEnabled())
	{
		return;
	}
	// check if squad unit and if so get squad controller reference.
	ASquadUnit* SquadUnit = Cast<ASquadUnit>(OtherActor);
	if (IsValid(SquadUnit))
	{
		if (ASquadController* SquadController = SquadUnit->GetSquadControllerChecked())
		{
			if (SquadController->GetTargetPickupItem() == this)
			{
				if (SquadController->CanSquadPickupWeapon())
				{
					SquadController->StartPickupWeapon(this);
					return;
				}
				RTSFunctionLibrary::ReportError(
					"Squad has this weapon item as pickup target but cannot pickup any items!"
					"SquadController: " + SquadController->GetName() +
					"\n WeaponPickup: " + GetName());
			}
		}
	}
}
