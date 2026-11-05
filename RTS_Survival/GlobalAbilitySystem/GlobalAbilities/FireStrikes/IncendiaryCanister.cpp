// Copyright (C) Bas Blokzijl - All rights reserved.

#include "IncendiaryCanister.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"

namespace IncendiaryCanisterConstants
{
	constexpr int32 HumanPlayer = 1;
	// The async burst damage still reads this actor as its causer, so it stays (hidden) a little after the burst.
	constexpr float LifeAfterBurstSeconds = 2.f;
	// Exponent of the height curve: the canister leaves the aircraft level and drops ever faster.
	constexpr float FallHeightExponent = 2.f;
	// Small step used to aim the nose along the path.
	constexpr float DirectionSampleAlpha = 0.02f;
}

AIncendiaryCanister::AIncendiaryCanister()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_FallingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingMesh"));
	SetRootComponent(M_FallingMesh);
	M_FallingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_FallingMesh->SetGenerateOverlapEvents(false);
	M_FallingMesh->SetCanEverAffectNavigation(false);
	M_FallingMesh->SetReceivesDecals(false);
}

void AIncendiaryCanister::StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation,
                                    const int32 OwningPlayer, const FIncendiaryCanisterSettings& Settings)
{
	M_ReleaseLocation = ReleaseLocation;
	M_GroundLocation = GroundLocation;
	M_OwningPlayer = OwningPlayer;
	M_Settings = Settings;
	M_FallingMesh->SetStaticMesh(Settings.FallingMesh);
	SetActorScale3D(FVector(Settings.MeshScale));
	SetActorLocation(ReleaseLocation);
	SetActorTickEnabled(true);
}

void AIncendiaryCanister::Tick(float DeltaSeconds)
{
	using namespace IncendiaryCanisterConstants;
	Super::Tick(DeltaSeconds);
	M_SecondsFalling += DeltaSeconds;
	const float FallAlpha = FMath::Clamp(M_SecondsFalling / FMath::Max(M_Settings.FallSeconds, 0.1f), 0.f, 1.f);
	if (FallAlpha >= 1.f)
	{
		Burst();
		return;
	}

	const FVector Location = GetFallLocation(FallAlpha);
	const FVector AheadLocation = GetFallLocation(FMath::Min(FallAlpha + DirectionSampleAlpha, 1.f));
	const FRotator PathRotation = (AheadLocation - Location).Rotation();
	SetActorLocationAndRotation(Location, (PathRotation.Quaternion() * M_Settings.MeshRotationOffset.Quaternion()));
}

FVector AIncendiaryCanister::GetFallLocation(const float FallAlpha) const
{
	// Forward speed is kept while gravity takes over: linear over the ground, accelerating downwards.
	FVector Location = FMath::Lerp(M_ReleaseLocation, M_GroundLocation, FallAlpha);
	const float HeightAlpha = FMath::Pow(FallAlpha, IncendiaryCanisterConstants::FallHeightExponent);
	Location.Z = FMath::Lerp(M_ReleaseLocation.Z, M_GroundLocation.Z, HeightAlpha);
	return Location;
}

void AIncendiaryCanister::Burst()
{
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	if (IsValid(M_Settings.ImpactEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, M_Settings.ImpactEffect, M_GroundLocation,
		                                               FRotator::ZeroRotator,
		                                               FVector(M_Settings.ImpactEffectScale));
	}
	if (IsValid(M_Settings.ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_Settings.ImpactSound, M_GroundLocation);
	}
	DealImpactDamage();
	RequestBurningGround();
	SetLifeSpan(IncendiaryCanisterConstants::LifeAfterBurstSeconds);
}

void AIncendiaryCanister::DealImpactDamage()
{
	if (M_Settings.ImpactDamage <= 0.f || M_Settings.ImpactRadiusCm <= 0.f)
	{
		return;
	}
	const ETriggerOverlapLogic OverlapLogic = M_OwningPlayer == IncendiaryCanisterConstants::HumanPlayer
		                                          ? ETriggerOverlapLogic::OverlapEnemy
		                                          : ETriggerOverlapLogic::OverlapPlayer;
	const FRTSGroundFireSettings& Burn = M_Settings.GroundFire;
	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		this,
		M_GroundLocation,
		M_Settings.ImpactRadiusCm,
		M_Settings.ImpactDamage,
		Burn.DamageFalloffExponent,
		Burn.FullArmorPen,
		Burn.ArmorPenFallOff,
		Burn.MaxArmorPen,
		ERTSDamageType::Fire,
		OverlapLogic,
		EShieldDamageSource::Flamethrower,
		TArray<TWeakObjectPtr<AActor>>(),
		EVehicleModuleSplashPolicy::Ignore,
		static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance::ClosestCollision);
}

void AIncendiaryCanister::RequestBurningGround() const
{
	UWorld* World = GetWorld();
	URTSFireSubsystem* FireSubsystem = IsValid(World) ? World->GetSubsystem<URTSFireSubsystem>() : nullptr;
	if (not IsValid(FireSubsystem))
	{
		return;
	}
	FireSubsystem->RequestGroundFire(M_Settings.GroundFire, M_GroundLocation, M_OwningPlayer);
}
