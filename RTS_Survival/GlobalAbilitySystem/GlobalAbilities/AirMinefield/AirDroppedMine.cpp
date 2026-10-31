// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AirDroppedMine.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldMine.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace AirDroppedMineConstants
{
	constexpr uint8 HumanPlayer = 1;
	constexpr float ParachuteOpenFraction = 0.15f;
	constexpr float ParachuteClosedScale = 0.05f;
	constexpr float SwayAmplitudeDegrees = 9.f;
	constexpr float SwayPeriodSeconds = 1.7f;
}

AAirDroppedMine::AAirDroppedMine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_FallingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingMesh"));
	SetRootComponent(M_FallingMesh);
	M_ParachuteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParachuteMesh"));
	M_ParachuteMesh->SetupAttachment(M_FallingMesh);

	for (UStaticMeshComponent* Mesh : {M_FallingMesh.Get(), M_ParachuteMesh.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetCastShadow(false);
	}
}

void AAirDroppedMine::StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation, UClass* MineClass,
                                const uint8 OwningPlayer, const FAirDroppedMineSettings& Settings)
{
	M_ReleaseLocation = ReleaseLocation;
	M_GroundLocation = GroundLocation;
	M_MineClass = MineClass;
	M_OwningPlayer = OwningPlayer;
	M_Settings = Settings;
	M_FallingMesh->SetStaticMesh(Settings.FallingMesh);
	M_ParachuteMesh->SetStaticMesh(Settings.ParachuteMesh);
	SetActorScale3D(FVector(Settings.MeshScale));
	SetActorLocation(ReleaseLocation);
	SetActorTickEnabled(true);
}

void AAirDroppedMine::Tick(float DeltaSeconds)
{
	using namespace AirDroppedMineConstants;
	Super::Tick(DeltaSeconds);
	M_SecondsFalling += DeltaSeconds;
	const float FallAlpha = FMath::Clamp(M_SecondsFalling / M_Settings.FallSeconds, 0.f, 1.f);
	if (FallAlpha >= 1.f)
	{
		Land();
		return;
	}

	// The canopy brakes the fall early on: most of the height goes in the first part of the time.
	const float HeightAlpha = FMath::InterpEaseOut(0.f, 1.f, FallAlpha, 1.6f);
	SetActorLocation(FMath::Lerp(M_ReleaseLocation, M_GroundLocation, HeightAlpha));

	const float OpenAlpha = FMath::Clamp(FallAlpha / ParachuteOpenFraction, 0.f, 1.f);
	M_ParachuteMesh->SetRelativeScale3D(FVector(FMath::Lerp(ParachuteClosedScale, 1.f, OpenAlpha)));
	const float SwayAngle = SwayAmplitudeDegrees * OpenAlpha * FMath::Sin(
		M_SecondsFalling * UE_TWO_PI / SwayPeriodSeconds);
	SetActorRotation(FRotator(SwayAngle, GetActorRotation().Yaw, 0.f));
}

void AAirDroppedMine::Land()
{
	SetActorTickEnabled(false);
	if (IsValid(M_Settings.LandingEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, M_Settings.LandingEffect, M_GroundLocation);
	}
	if (IsValid(M_Settings.LandingSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_Settings.LandingSound, M_GroundLocation);
	}
	PlaceMine();
	Destroy();
}

void AAirDroppedMine::PlaceMine() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(M_MineClass))
	{
		return;
	}
	const FTransform MineTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), M_GroundLocation);
	AFieldMine* Mine = Cast<AFieldMine>(
		FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, M_MineClass, MineTransform, M_OwningPlayer));
	if (IsValid(Mine) && M_OwningPlayer != AirDroppedMineConstants::HumanPlayer)
	{
		Mine->ConcealMine();
	}
}
