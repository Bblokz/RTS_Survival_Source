// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FlareProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FlareProjectileConstants
{
	constexpr float MinAscentSeconds = 0.6f;
	constexpr float MinHangSeconds = 0.1f;
	constexpr float MinAscentSpeed = 100.f;
	// The launch tube may point sideways; the flight still has to start upwards.
	constexpr float MinLaunchDirectionUp = 0.35f;
	// How far along the launch tube the curve pulls, as a fraction of the straight distance to the apex.
	constexpr float ControlPointFraction = 0.6f;
	// The apex is at least this far above the launch socket, also when the target lies far below the unit.
	constexpr float MinApexAboveLaunch = 300.f;
	// The cartridge lies on the ground, burning, at this height when it lands.
	constexpr float LandingHeight = 20.f;

	constexpr float ParachuteOpenSeconds = 0.6f;
	constexpr float ParachuteClosedScale = 0.05f;
	constexpr float LightFadeInSeconds = 0.3f;
	constexpr float BurnOutFadeSeconds = 1.5f;
	// Smoke of the burn effect keeps drifting after the flare burnt out; the actor waits for it.
	constexpr float DestroyAfterBurnOutSeconds = 5.f;

	constexpr float SwayAmplitudeDegrees = 6.f;
	constexpr float SwayPeriodSeconds = 3.4f;
	constexpr float FlickerSpeed = 9.f;
	constexpr float FlickerDepth = 0.18f;
}

FFlareFlightPath FFlareFlightPath::Make(const FVector& LaunchLocation, const FVector& LaunchDirection,
                                        const FVector& GroundTarget, const float ApexHeight, const float AscentSpeed,
                                        const float HangSeconds)
{
	using namespace FlareProjectileConstants;
	FFlareFlightPath Path;
	Path.Start = LaunchLocation;

	FVector Direction = LaunchDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::UpVector;
	}
	if (Direction.Z < MinLaunchDirectionUp)
	{
		const FVector Horizontal = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
		const float HorizontalLength = FMath::Sqrt(1.f - FMath::Square(MinLaunchDirectionUp));
		Direction = Horizontal * HorizontalLength + FVector::UpVector * MinLaunchDirectionUp;
	}

	Path.Apex = GroundTarget + FVector(0.f, 0.f, FMath::Max(ApexHeight, 0.f));
	Path.Apex.Z = FMath::Max(Path.Apex.Z, LaunchLocation.Z + MinApexAboveLaunch);
	Path.Landing = GroundTarget + FVector(0.f, 0.f, LandingHeight);

	const float StraightDistance = FVector::Dist(Path.Start, Path.Apex);
	// The control point lies along the tube, so the flare leaves along it. It may not lie above the apex, or the
	// flare would climb higher than the height it was set to burn at.
	float ControlDistance = StraightDistance * ControlPointFraction;
	const float Rise = Path.Apex.Z - Path.Start.Z;
	if (Direction.Z > KINDA_SMALL_NUMBER)
	{
		ControlDistance = FMath::Min(ControlDistance, Rise / Direction.Z);
	}
	Path.Control = Path.Start + Direction * ControlDistance;

	// The length of a quadratic Bezier lies between its chord and its control polygon.
	const float ControlPolygonLength = FVector::Dist(Path.Start, Path.Control) + FVector::Dist(Path.Control, Path.Apex);
	const float ApproximateLength = 0.5f * (StraightDistance + ControlPolygonLength);
	Path.AscentSeconds = FMath::Max(MinAscentSeconds, ApproximateLength / FMath::Max(AscentSpeed, MinAscentSpeed));
	Path.HangSeconds = FMath::Max(MinHangSeconds, HangSeconds);
	return Path;
}

FVector FFlareFlightPath::GetLocationAtTime(const float SecondsSinceLaunch) const
{
	if (SecondsSinceLaunch <= 0.f)
	{
		return Start;
	}
	if (SecondsSinceLaunch < AscentSeconds)
	{
		// Ease out: the cartridge leaves the tube fast and slows down towards the apex.
		const float Alpha = SecondsSinceLaunch / AscentSeconds;
		const float CurveParameter = 1.f - FMath::Square(1.f - Alpha);
		const float Inverse = 1.f - CurveParameter;
		return Start * (Inverse * Inverse) + Control * (2.f * Inverse * CurveParameter) + Apex * (CurveParameter *
			CurveParameter);
	}
	const float SecondsBurning = SecondsSinceLaunch - AscentSeconds;
	if (SecondsBurning < HangSeconds)
	{
		// The parachute sinks at a steady speed.
		return FMath::Lerp(Apex, Landing, SecondsBurning / HangSeconds);
	}
	return Landing;
}

FVector FFlareFlightPath::GetDirectionAtTime(const float SecondsSinceLaunch) const
{
	if (SecondsSinceLaunch >= AscentSeconds)
	{
		return -FVector::UpVector;
	}
	const float Alpha = FMath::Clamp(SecondsSinceLaunch / AscentSeconds, 0.f, 1.f);
	const float CurveParameter = 1.f - FMath::Square(1.f - Alpha);
	const FVector Derivative = (Control - Start) * (2.f * (1.f - CurveParameter)) + (Apex - Control) * (2.f *
		CurveParameter);
	const FVector Direction = Derivative.GetSafeNormal();
	return Direction.IsNearlyZero() ? FVector::UpVector : Direction;
}

AFlareProjectile::AFlareProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);

	M_SwayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwayPivot"));
	M_SwayPivot->SetupAttachment(M_Root);

	auto SetupInertMesh = [](UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetCastShadow(false);
	};

	M_FlareMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlareMesh"));
	M_FlareMesh->SetupAttachment(M_SwayPivot);
	SetupInertMesh(M_FlareMesh);

	M_ParachuteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParachuteMesh"));
	M_ParachuteMesh->SetupAttachment(M_SwayPivot);
	SetupInertMesh(M_ParachuteMesh);
	M_ParachuteMesh->SetVisibility(false);

	M_TrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffect"));
	M_TrailEffect->SetupAttachment(M_SwayPivot);
	M_TrailEffect->SetAutoActivate(false);

	M_BurnEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BurnEffect"));
	M_BurnEffect->SetupAttachment(M_SwayPivot);
	M_BurnEffect->SetAutoActivate(false);

	M_Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	M_Light->SetupAttachment(M_SwayPivot);
	M_Light->SetCastShadows(false);
	M_Light->SetIntensity(0.f);
	M_Light->SetVisibility(false);

	M_FowComponent = CreateDefaultSubobject<UFowComp>(TEXT("FowComponent"));
}

void AFlareProjectile::LaunchFlare(const FVector& LaunchLocation, const FVector& LaunchDirection,
                                   const FVector& GroundTarget, const FFlareProjectileSettings& Settings,
                                   const uint8 OwningPlayer)
{
	if (M_Phase != EFlareProjectilePhase::Inactive)
	{
		RTSFunctionLibrary::ReportError("AFlareProjectile::LaunchFlare called twice on " + GetName());
		return;
	}
	M_Settings = Settings;
	M_OwningPlayer = OwningPlayer;
	M_FlightPath = FFlareFlightPath::Make(LaunchLocation, LaunchDirection, GroundTarget, Settings.ApexHeight,
	                                      Settings.AscentSpeed, Settings.HangTimeSeconds);
	SetupMeshes();
	StartAscent();
}

void AFlareProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_SecondsSinceLaunch += DeltaSeconds;
	switch (M_Phase)
	{
	case EFlareProjectilePhase::Ascending:
		if (M_SecondsSinceLaunch >= M_FlightPath.AscentSeconds)
		{
			Ignite();
			TickBurning(M_SecondsSinceLaunch - M_FlightPath.AscentSeconds);
			return;
		}
		TickAscent();
		return;
	case EFlareProjectilePhase::Burning:
		if (M_SecondsSinceLaunch >= M_FlightPath.GetTotalSeconds())
		{
			BurnOut();
			return;
		}
		TickBurning(M_SecondsSinceLaunch - M_FlightPath.AscentSeconds);
		return;
	case EFlareProjectilePhase::BurntOut:
		TickBurntOut(DeltaSeconds);
		return;
	case EFlareProjectilePhase::Inactive:
		return;
	}
}

void AFlareProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFowVision();
	Super::EndPlay(EndPlayReason);
}

void AFlareProjectile::SetupMeshes()
{
	const FFlareVisualSettings& Visuals = M_Settings.Visuals;
	const FVector MeshScale(Visuals.MeshScale);
	if (IsValid(Visuals.FlareMesh))
	{
		M_FlareMesh->SetStaticMesh(Visuals.FlareMesh);
	}
	M_FlareMesh->SetRelativeScale3D(MeshScale);
	if (IsValid(Visuals.ParachuteMesh))
	{
		M_ParachuteMesh->SetStaticMesh(Visuals.ParachuteMesh);
	}
	M_ParachuteMesh->SetRelativeScale3D(MeshScale * FlareProjectileConstants::ParachuteClosedScale);

	// The glow, sparks, trail and light sit at the burning bottom of the cartridge, which hangs along -Z.
	const FVector BurnPoint(0.f, 0.f, -Visuals.BurnPointDepth * Visuals.MeshScale);
	M_TrailEffect->SetRelativeLocation(BurnPoint);
	M_BurnEffect->SetRelativeLocation(BurnPoint);
	M_Light->SetRelativeLocation(BurnPoint);
	M_Light->SetLightColor(Visuals.LightColor);
	M_Light->SetAttenuationRadius(Visuals.LightAttenuationRadius);
}

void AFlareProjectile::StartAscent()
{
	M_Phase = EFlareProjectilePhase::Ascending;
	M_SecondsSinceLaunch = 0.f;
	TickAscent();
	if (IsValid(M_Settings.Visuals.TrailEffect))
	{
		M_TrailEffect->SetAsset(M_Settings.Visuals.TrailEffect);
		M_TrailEffect->Activate(true);
	}
	SetActorTickEnabled(true);
}

void AFlareProjectile::Ignite()
{
	M_Phase = EFlareProjectilePhase::Burning;
	M_TrailEffect->Deactivate();
	M_ParachuteMesh->SetVisibility(true);
	if (IsValid(M_Settings.Visuals.BurnEffect))
	{
		M_BurnEffect->SetAsset(M_Settings.Visuals.BurnEffect);
		M_BurnEffect->Activate(true);
	}
	M_Light->SetVisibility(true);
	if (IsValid(M_Settings.Visuals.IgnitionSound))
	{
		UGameplayStatics::SpawnSoundAttached(M_Settings.Visuals.IgnitionSound, M_SwayPivot);
	}
	StartFowVision();
}

void AFlareProjectile::BurnOut()
{
	M_Phase = EFlareProjectilePhase::BurntOut;
	M_SecondsSinceBurnOut = 0.f;
	SetActorLocation(M_FlightPath.Landing);
	StopFowVision();
	M_BurnEffect->Deactivate();
	M_ParachuteMesh->SetVisibility(false);
	SetLifeSpan(FlareProjectileConstants::DestroyAfterBurnOutSeconds);
}

void AFlareProjectile::TickAscent()
{
	const FVector Direction = M_FlightPath.GetDirectionAtTime(M_SecondsSinceLaunch);
	// The cartridge hangs along -Z under its parachute; while climbing that axis trails behind it.
	SetActorLocationAndRotation(M_FlightPath.GetLocationAtTime(M_SecondsSinceLaunch),
	                            FRotationMatrix::MakeFromZ(Direction).Rotator());
}

void AFlareProjectile::TickBurning(const float SecondsSinceIgnition)
{
	using namespace FlareProjectileConstants;
	const float OpenAlpha = FMath::Clamp(SecondsSinceIgnition / ParachuteOpenSeconds, 0.f, 1.f);
	const FQuat AscentEndRotation = FRotationMatrix::MakeFromZ(
		M_FlightPath.GetDirectionAtTime(M_FlightPath.AscentSeconds - KINDA_SMALL_NUMBER)).ToQuat();
	const FQuat HangingRotation = FQuat::Identity;
	SetActorLocationAndRotation(M_FlightPath.GetLocationAtTime(M_SecondsSinceLaunch),
	                            FQuat::Slerp(AscentEndRotation, HangingRotation, OpenAlpha));

	const float ParachuteScale = FMath::Lerp(ParachuteClosedScale, 1.f, FMath::InterpEaseOut(0.f, 1.f, OpenAlpha, 2.f));
	M_ParachuteMesh->SetRelativeScale3D(FVector(M_Settings.Visuals.MeshScale * ParachuteScale));

	const float SwayAngle = SwayAmplitudeDegrees * OpenAlpha * FMath::Sin(
		SecondsSinceIgnition * UE_TWO_PI / SwayPeriodSeconds);
	const float SwayCrossAngle = 0.5f * SwayAmplitudeDegrees * OpenAlpha * FMath::Cos(
		SecondsSinceIgnition * UE_TWO_PI / (SwayPeriodSeconds * 1.37f));
	M_SwayPivot->SetRelativeRotation(FRotator(SwayAngle, 0.f, SwayCrossAngle));

	const float FadeIn = FMath::Clamp(SecondsSinceIgnition / LightFadeInSeconds, 0.f, 1.f);
	const float Flicker = 1.f - FlickerDepth * 0.5f * (1.f + FMath::PerlinNoise1D(SecondsSinceIgnition * FlickerSpeed));
	SetLightIntensityScale(FadeIn * Flicker);
}

void AFlareProjectile::TickBurntOut(const float DeltaSeconds)
{
	M_SecondsSinceBurnOut += DeltaSeconds;
	const float Remaining = 1.f - FMath::Clamp(
		M_SecondsSinceBurnOut / FlareProjectileConstants::BurnOutFadeSeconds, 0.f, 1.f);
	SetLightIntensityScale(Remaining);
	if (Remaining <= 0.f)
	{
		M_Light->SetVisibility(false);
	}
}

void AFlareProjectile::StartFowVision()
{
	if (not GetIsValidFowComponent())
	{
		return;
	}
	// Like the units: the player's flares light up the fog, the AI's flares give the AI vision.
	const EFowBehaviour FowBehaviour = M_OwningPlayer == 1
		                                   ? EFowBehaviour::Fow_Active
		                                   : EFowBehaviour::Fow_PassiveEnemyVision;
	M_FowComponent->SetVisionRadius(M_Settings.VisionRadius);
	M_FowComponent->StartFow(FowBehaviour, true);
}

void AFlareProjectile::StopFowVision()
{
	if (not IsValid(M_FowComponent))
	{
		return;
	}
	M_FowComponent->StopFowParticipation();
}

void AFlareProjectile::SetLightIntensityScale(const float Scale) const
{
	M_Light->SetIntensity(M_Settings.Visuals.LightIntensity * FMath::Max(Scale, 0.f));
}

bool AFlareProjectile::GetIsValidFowComponent() const
{
	if (IsValid(M_FowComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_FowComponent",
	                                                             "AFlareProjectile::GetIsValidFowComponent", this);
	return false;
}
