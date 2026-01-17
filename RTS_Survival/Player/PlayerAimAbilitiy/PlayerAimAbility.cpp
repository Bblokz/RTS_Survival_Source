// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerAimAbility.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


APlayerAimAbility::APlayerAimAbility()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Create default subobject for the aim mesh component.
	AimMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimMeshComponent"));
	if (AimMeshComponent)
	{
		// Disable all collisions and physics
		AimMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimMeshComponent->SetGenerateOverlapEvents(false);
		AimMeshComponent->SetSimulatePhysics(false);
		// no decal effects
		AimMeshComponent->SetReceivesDecals(false);
		AimMeshComponent->SetCanEverAffectNavigation(false);
	}
	SetCanBeDamaged(false);
}

void APlayerAimAbility::ShowAimRadius(const float Radius, const EPlayerAimAbilityTypes AimType)
{
	if (not GetIsValidAimMeshComponent())
	{
		return;
	}
	bM_IsPlayerAimActive = true;
	SetActorHiddenInGame(false);
	const float ScaleValue = Radius / UnitsPerScale;
	SetActorScale3D(FVector(ScaleValue, ScaleValue, 1));
	UMaterialInterface* AimMaterial = nullptr;
	if (not GetIsValidMaterialForAimType(AimType, AimMaterial))
	{
		return;
	}
	AimMeshComponent->SetMaterial(0, AimMaterial);
}

void APlayerAimAbility::HideRadius()
{
	bM_IsPlayerAimActive = false;
	SetActorHiddenInGame(true);
}

// Called when the game starts or when spawned
void APlayerAimAbility::BeginPlay()
{
	Super::BeginPlay();

	if (AimMeshComponent)
	{
		// Disable all collisions and physics
		AimMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimMeshComponent->SetGenerateOverlapEvents(false);
		AimMeshComponent->SetSimulatePhysics(false);
		// no decal effects
		AimMeshComponent->SetReceivesDecals(false);
		AimMeshComponent->SetCanEverAffectNavigation(false);
	}
}

bool APlayerAimAbility::GetIsValidAimMeshComponent() const
{
	if (not IsValid(AimMeshComponent))
	{
		RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidAimMeshComponent"
			"AimMeshComponent is not valid!");
		return false;
	}
	return true;
}

bool APlayerAimAbility::GetIsValidMaterialForAimType(const EPlayerAimAbilityTypes AimType,
                                                     UMaterialInterface*& OutMaterial)
{
	if (not MaterialPerAbilityAim.Contains(AimType))
	{
		const FString TypeName = UEnum::GetValueAsString(AimType);
		RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidMaterialForAimType"
			"No material found for given AimType!"
			"\n type: " + TypeName);
		return false;
	}
	OutMaterial = MaterialPerAbilityAim[AimType];
	return true;
}
