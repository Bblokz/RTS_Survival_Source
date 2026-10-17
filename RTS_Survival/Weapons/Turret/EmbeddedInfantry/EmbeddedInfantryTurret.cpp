// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "EmbeddedInfantryTurret.h"

#include "EmbeddedInfantryTurretAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EmbeddedInfantryTurretConstants
{
	// The infantry master skeleton socket in the right hand that squad units hold their weapon at.
	const FName WeaponSocketName = TEXT("GunSocket");

	// Same weapon offset from the hand socket as the Weapon child actor of RTS_BP_SquadUnit.
	const FVector WeaponRelativeLocation = FVector(-16.25f, 4.16f, 2.2f);
	const FRotator WeaponRelativeRotation = FRotator(-1.81f, -179.92f, 2.0f);

	// Infantry master meshes face +Y.
	constexpr float SoldierMeshRelativeYaw = -90.f;
}

AEmbeddedInfantryTurret::AEmbeddedInfantryTurret()
{
	TurretRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretRoot"));
	SetRootComponent(TurretRoot);

	SoldierMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SoldierMesh"));
	SoldierMesh->SetupAttachment(TurretRoot);
	SoldierMesh->SetRelativeRotation(FRotator(0.f, EmbeddedInfantryTurretConstants::SoldierMeshRelativeYaw, 0.f));
	SoldierMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	DisableCollisionNavigationAndDecals(SoldierMesh);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SoldierMesh, EmbeddedInfantryTurretConstants::WeaponSocketName);
	WeaponMesh->SetRelativeLocation(EmbeddedInfantryTurretConstants::WeaponRelativeLocation);
	WeaponMesh->SetRelativeRotation(EmbeddedInfantryTurretConstants::WeaponRelativeRotation);
	DisableCollisionNavigationAndDecals(WeaponMesh);

	// The turret master reads its mesh unchecked in GetTurretTransform; never leave it to a Blueprint.
	SceneSkeletalMesh = SoldierMesh;
}

void AEmbeddedInfantryTurret::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode,
                                                  const int32 WeaponCalibre)
{
	Super::PlayWeaponAnimation(WeaponIndex, FireMode, WeaponCalibre);
	if (not GetIsValidInfantryAnimInstance())
	{
		return;
	}
	M_InfantryAnimInstance->PlayFireMontage(FireMode);
}

void AEmbeddedInfantryTurret::PostInitializeComponents()
{
	InitTurretsMaster(SoldierMesh);
	InitChildTurret(
		InfantryTurretSettings.RotationSpeed,
		InfantryTurretSettings.MaxPitch,
		InfantryTurretSettings.MinPitch,
		InfantryTurretSettings.IdleRotationType);

	// Calls BP_PostInit, which may still adjust the turret.
	Super::PostInitializeComponents();
}

void AEmbeddedInfantryTurret::BeginPlay()
{
	// The Blueprint BeginPlay sets up the weapon.
	Super::BeginPlay();

	// Also covers meshes a derived Blueprint added.
	BeginPlay_DisableCollisionNavigationAndDecals();
	BeginPlay_CacheInfantryAnimInstance();
}

FTransform AEmbeddedInfantryTurret::GetTurretTransform() const
{
	FTransform TurretTransform = GetActorTransform();
	if (not GetIsValidSoldierMesh())
	{
		return TurretTransform;
	}
	TurretTransform.SetLocation(SoldierMesh->GetSocketLocation(EmbeddedInfantryTurretConstants::WeaponSocketName));
	return TurretTransform;
}

void AEmbeddedInfantryTurret::UpdateTargetPitchCPP(const float NewPitch)
{
	Super::UpdateTargetPitchCPP(NewPitch);
	if (not GetIsValidInfantryAnimInstance())
	{
		return;
	}
	M_InfantryAnimInstance->SetTargetAimPitch(TargetPitch);
}

void AEmbeddedInfantryTurret::BeginPlay_DisableCollisionNavigationAndDecals() const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		DisableCollisionNavigationAndDecals(PrimitiveComponent);
	}
}

void AEmbeddedInfantryTurret::BeginPlay_CacheInfantryAnimInstance()
{
	if (not GetIsValidSoldierMesh())
	{
		return;
	}
	M_InfantryAnimInstance = Cast<UEmbeddedInfantryTurretAnimInstance>(SoldierMesh->GetAnimInstance());
	// Reports a soldier without an infantry turret anim blueprint once at spawn.
	(void)GetIsValidInfantryAnimInstance();
}

void AEmbeddedInfantryTurret::DisableCollisionNavigationAndDecals(UPrimitiveComponent* PrimitiveComponent)
{
	if (not IsValid(PrimitiveComponent))
	{
		return;
	}
	PrimitiveComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimitiveComponent->SetGenerateOverlapEvents(false);
	PrimitiveComponent->SetCanEverAffectNavigation(false);
	PrimitiveComponent->SetReceivesDecals(false);
	PrimitiveComponent->CanCharacterStepUpOn = ECB_No;
}

bool AEmbeddedInfantryTurret::GetIsValidSoldierMesh() const
{
	if (IsValid(SoldierMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"SoldierMesh",
		"GetIsValidSoldierMesh",
		this
	);
	return false;
}

bool AEmbeddedInfantryTurret::GetIsValidInfantryAnimInstance() const
{
	if (M_InfantryAnimInstance.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_InfantryAnimInstance",
		"GetIsValidInfantryAnimInstance",
		this
	);
	return false;
}
