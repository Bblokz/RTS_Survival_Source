// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteNukeDebrisRain.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RadixiteNukeCraterRim.h"

namespace RadixiteNukeDebrisRainConstants
{
	// cm/s^2, down.
	constexpr float GravityCm = 980.f;
	// Stones leave from the crater floor, spread over this part of the blast radius.
	constexpr float LaunchSpreadFraction = 0.12f;
	constexpr float LaunchHeightCm = 150.f;
	constexpr float MinSpinRadiansPerSecond = 2.f;
	constexpr float MaxSpinRadiansPerSecond = 9.f;
	// A landed stone sinks a little into the ground, as if it had dug in.
	constexpr float LandedSinkFraction = 0.2f;
}

ARadixiteNukeDebrisRain::ARadixiteNukeDebrisRain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARadixiteNukeDebrisRain::StartRain(const FRadixiteNukeDebrisRainSettings& DebrisRain, const float BlastRadius)
{
	M_DebrisRain = DebrisRain;
	UWorld* World = GetWorld();
	if (not IsValid(World) || M_DebrisRain.Count <= 0)
	{
		return;
	}
	CreateStoneMeshes();
	if (M_StoneMeshes.IsEmpty())
	{
		return;
	}
	const FRadixiteNukeGroundSampler Ground(*World, GetActorLocation());
	M_Stones.SetNum(M_DebrisRain.Count);
	for (int32 StoneIndex = 0; StoneIndex < M_Stones.Num(); ++StoneIndex)
	{
		LaunchStone(StoneIndex, BlastRadius, Ground);
	}
	SetActorTickEnabled(true);
}

void ARadixiteNukeDebrisRain::CreateStoneMeshes()
{
	for (const TSoftObjectPtr<UStaticMesh>& SoftMesh : M_DebrisRain.Meshes)
	{
		UStaticMesh* Mesh = SoftMesh.Get();
		if (not IsValid(Mesh))
		{
			continue;
		}
		UInstancedStaticMeshComponent* StoneMesh = NewObject<UInstancedStaticMeshComponent>(this);
		StoneMesh->SetupAttachment(RootComponent);
		StoneMesh->SetStaticMesh(Mesh);
		StoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StoneMesh->SetGenerateOverlapEvents(false);
		StoneMesh->SetCanEverAffectNavigation(false);
		// Instances move every frame while they fly.
		StoneMesh->SetMobility(EComponentMobility::Movable);
		StoneMesh->RegisterComponent();
		M_StoneMeshes.Add(StoneMesh);
	}
}

void ARadixiteNukeDebrisRain::LaunchStone(const int32 StoneIndex, const float BlastRadius,
                                          const FRadixiteNukeGroundSampler& Ground)
{
	using namespace RadixiteNukeDebrisRainConstants;
	FRadixiteNukeDebrisStone& Stone = M_Stones[StoneIndex];
	const FVector GroundZero = GetActorLocation();
	const float LaunchAngle = FMath::FRandRange(0.f, UE_TWO_PI);
	const FVector2D LaunchXY = FVector2D(GroundZero)
		+ FVector2D(FMath::Cos(LaunchAngle), FMath::Sin(LaunchAngle))
		* FMath::FRandRange(0.f, BlastRadius * LaunchSpreadFraction);
	Stone.LaunchLocation = FVector(LaunchXY.X, LaunchXY.Y, GroundZero.Z + LaunchHeightCm);

	// Even spread of headings with jitter, so the ring of landings has no gaps.
	const float Heading = UE_TWO_PI * (StoneIndex + FMath::FRandRange(0.f, 1.f)) / M_Stones.Num();
	const float Range = BlastRadius * FMath::FRandRange(M_DebrisRain.MinRangeFraction,
	                                                    FMath::Max(M_DebrisRain.MinRangeFraction,
	                                                               M_DebrisRain.MaxRangeFraction));
	const FVector2D LandingXY = FVector2D(GroundZero) + FVector2D(FMath::Cos(Heading), FMath::Sin(Heading)) * Range;
	Stone.LandingLocation = FVector(LandingXY.X, LandingXY.Y, Ground.GetGroundHeight(LandingXY));

	// Solve the arc for the flight time: x(t) = x0 + v t + g t^2 / 2 has to reach the landing at t = T.
	Stone.FlightSeconds = FMath::FRandRange(M_DebrisRain.MinFlightSeconds,
	                                        FMath::Max(M_DebrisRain.MinFlightSeconds,
	                                                   M_DebrisRain.MaxFlightSeconds));
	const FVector Gravity(0.f, 0.f, -GravityCm);
	const float FlightSeconds = Stone.FlightSeconds;
	Stone.LaunchVelocity = (Stone.LandingLocation - Stone.LaunchLocation
		- 0.5f * Gravity * FlightSeconds * FlightSeconds) / FlightSeconds;

	Stone.MeshIndex = FMath::RandRange(0, M_StoneMeshes.Num() - 1);
	Stone.StartRotation = FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f),
	                               FMath::FRandRange(-180.f, 180.f)).Quaternion();
	Stone.SpinAxis = FMath::VRand();
	Stone.SpinRadiansPerSecond = FMath::FRandRange(MinSpinRadiansPerSecond, MaxSpinRadiansPerSecond);
	Stone.Scale = FVector(FMath::FRandRange(M_DebrisRain.MinScale,
	                                        FMath::Max(M_DebrisRain.MinScale, M_DebrisRain.MaxScale)));
	Stone.InstanceIndex = M_StoneMeshes[Stone.MeshIndex]->AddInstance(GetFlyingTransform(Stone), true);
}

FTransform ARadixiteNukeDebrisRain::GetFlyingTransform(const FRadixiteNukeDebrisStone& Stone) const
{
	using namespace RadixiteNukeDebrisRainConstants;
	const float Seconds = FMath::Min(M_RainSeconds, Stone.FlightSeconds);
	const FVector Location = Stone.LaunchLocation + Stone.LaunchVelocity * Seconds
		+ 0.5f * FVector(0.f, 0.f, -GravityCm) * Seconds * Seconds;
	const FQuat Spin(Stone.SpinAxis, Stone.SpinRadiansPerSecond * Seconds);
	return FTransform(Spin * Stone.StartRotation, Location, Stone.Scale);
}

void ARadixiteNukeDebrisRain::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_RainSeconds += DeltaSeconds;
	for (FRadixiteNukeDebrisStone& Stone : M_Stones)
	{
		if (Stone.bHasLanded)
		{
			continue;
		}
		if (M_RainSeconds >= Stone.FlightSeconds)
		{
			LandStone(Stone);
			continue;
		}
		M_StoneMeshes[Stone.MeshIndex]->UpdateInstanceTransform(Stone.InstanceIndex, GetFlyingTransform(Stone), true,
		                                                        false, true);
	}
	for (UInstancedStaticMeshComponent* StoneMesh : M_StoneMeshes)
	{
		StoneMesh->MarkRenderStateDirty();
	}
	if (M_LandedStones >= M_Stones.Num())
	{
		SetActorTickEnabled(false);
	}
}

void ARadixiteNukeDebrisRain::LandStone(FRadixiteNukeDebrisStone& Stone)
{
	using namespace RadixiteNukeDebrisRainConstants;
	Stone.bHasLanded = true;
	++M_LandedStones;
	UInstancedStaticMeshComponent* StoneMesh = M_StoneMeshes[Stone.MeshIndex];
	const float StoneHeight = StoneMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * Stone.Scale.Z;
	// Lies flat-ish where it fell, turned at random about the vertical.
	const FQuat RestRotation = FRotator(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-180.f, 180.f),
	                                    FMath::FRandRange(-12.f, 12.f)).Quaternion();
	const FVector RestLocation = Stone.LandingLocation - FVector(0.f, 0.f, StoneHeight * LandedSinkFraction);
	StoneMesh->UpdateInstanceTransform(Stone.InstanceIndex, FTransform(RestRotation, RestLocation, Stone.Scale), true,
	                                   false, true);
	if (UNiagaraSystem* LandingEffect = M_DebrisRain.LandingEffect.Get())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LandingEffect, Stone.LandingLocation, FRotator::ZeroRotator,
		                                               FVector(M_DebrisRain.LandingEffectScale));
	}
	USoundBase* LandingSound = M_DebrisRain.LandingSound.Get();
	if (IsValid(LandingSound) && M_LandingSoundsPlayed < M_DebrisRain.MaxLandingSounds)
	{
		++M_LandingSoundsPlayed;
		UGameplayStatics::PlaySoundAtLocation(this, LandingSound, Stone.LandingLocation);
	}
}
