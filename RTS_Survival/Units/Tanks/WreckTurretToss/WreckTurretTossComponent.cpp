#include "WreckTurretTossComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Sound/SoundCue.h"
#include "Engine/World.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisBurstComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckTurretToss, Log, All);

namespace WreckTurretTossConstants
{
	// Share of random direction mixed into the tumble axis so not every turret flips the same way.
	constexpr float TumbleAxisRandomness = 0.35f;
	constexpr float FullCircleDegrees = 360.f;
}

UWreckTurretTossComponent::UWreckTurretTossComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWreckTurretTossComponent::StartToss(const FWreckTurretTossSettings& Settings, const float LiveTurretYawOffset)
{
	M_Settings = Settings;
	UE_LOG(LogWreckTurretToss, Verbose, TEXT("Turret toss on %s, launch sound %s"), *GetNameSafe(GetOwner()),
	       *GetNameSafe(M_Settings.LaunchSound));
	if (not TryFindWreckMeshes())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_FireSubsystem = World->GetSubsystem<URTSFireSubsystem>();

	M_TurretRingLocation = GetTurretRingLocation();
	UWreckPartFlightComponent::SpawnWreckExplosion(this, M_Settings.LaunchExplosionType, M_TurretRingLocation,
	                                               M_Settings.LaunchSound);
	StartHullAmmoFire();
	StartDebrisBurst();
	// Casemate wrecks have no separate turret: they only get the explosion and the hull ammo fire.
	if (not M_TurretMesh.IsValid())
	{
		return true;
	}

	UWreckPartFlightComponent* TurretFlight = UWreckPartFlightComponent::AddToWreck(GetOwner());
	if (not IsValid(TurretFlight))
	{
		return true;
	}
	// Setup stores the turret's wreck collision before the ring placement makes it movable.
	TurretFlight->SetupPart(M_TurretMesh.Get(), GetTurretFlightSettings());
	PlaceTurretOnRing(LiveTurretYawOffset);
	LaunchTurret(*TurretFlight);
	StartTurretFire();
	return true;
}

void UWreckTurretTossComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFire(M_HullFireHandle);
	StopFire(M_TurretFireHandle);
	Super::EndPlay(EndPlayReason);
}

bool UWreckTurretTossComponent::TryFindWreckMeshes()
{
	const AActor* Wreck = GetOwner();
	if (not IsValid(Wreck))
	{
		return false;
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}
		if (MeshComponent->GetFName() == M_Settings.TurretComponentName)
		{
			// Some wrecks keep an empty Turret slot (trucks, variants that clear it); only a mesh with physics
			// collision can be thrown, anything else counts as a turretless wreck.
			if (UWreckPartFlightComponent::GetCanSimulatePart(*MeshComponent))
			{
				M_TurretMesh = MeshComponent;
			}
		}
		else if (MeshComponent->GetFName() == M_Settings.HullComponentName)
		{
			M_HullMesh = MeshComponent;
		}
	}

	// The turret is optional (casemates); without a hull there is no place for the ammo fire.
	if (M_HullMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("UWreckTurretTossComponent::TryFindWreckMeshes - wreck %s has no static mesh component named %s."),
		*Wreck->GetName(), *M_Settings.HullComponentName.ToString()));
	return false;
}

void UWreckTurretTossComponent::PlaceTurretOnRing(const float LiveTurretYawOffset) const
{
	if (not GetIsValidTurretMesh() || not GetIsValidHullMesh())
	{
		return;
	}

	// The wreck construction script may have dropped the turret next to the hull with a random tilt; the toss
	// starts from the ring instead, facing where the live gun pointed.
	const FQuat HullRotation = M_HullMesh->GetComponentQuat();
	const FQuat TurretRotation = HullRotation * FRotator(0.f, LiveTurretYawOffset, 0.f).Quaternion();
	// Wreck parts may be Static; a Static component ignores the move below.
	M_TurretMesh->SetMobility(EComponentMobility::Movable);
	M_TurretMesh->SetWorldLocationAndRotation(M_TurretRingLocation, TurretRotation, false, nullptr,
	                                          ETeleportType::TeleportPhysics);
}

FVector UWreckTurretTossComponent::GetTurretRingLocation() const
{
	if (not GetIsValidHullMesh())
	{
		return FVector::ZeroVector;
	}
	if (M_HullMesh->DoesSocketExist(M_Settings.TurretRingSocketName))
	{
		return M_HullMesh->GetSocketLocation(M_Settings.TurretRingSocketName);
	}
	if (M_TurretMesh.IsValid())
	{
		return M_TurretMesh->GetComponentLocation();
	}
	// No ring socket and no turret: burn from the top centre of the hull (casemate roof).
	const FBoxSphereBounds HullBounds = M_HullMesh->Bounds;
	return HullBounds.Origin + FVector(0.f, 0.f, HullBounds.BoxExtent.Z);
}

void UWreckTurretTossComponent::LaunchTurret(UWreckPartFlightComponent& TurretFlight) const
{
	const float UpSpeed = FMath::FRandRange(M_Settings.UpSpeedRange.X, M_Settings.UpSpeedRange.Y);
	const float SideSpeed = FMath::FRandRange(M_Settings.SideSpeedRange.X, M_Settings.SideSpeedRange.Y);
	const float SpinSpeed = FMath::FRandRange(M_Settings.SpinDegreesRange.X, M_Settings.SpinDegreesRange.Y);
	const FVector SideDirection = FRotator(
		0.f, FMath::FRandRange(0.f, WreckTurretTossConstants::FullCircleDegrees), 0.f).Vector();

	// Setting velocities directly keeps the arc the same for a light Panzer II turret and a heavy Tiger turret.
	const FVector LaunchVelocity = SideDirection * SideSpeed + FVector::UpVector * UpSpeed;
	// Tumble over the direction of travel, with some randomness so turrets also corkscrew.
	const FVector TumbleAxis = (FVector::CrossProduct(FVector::UpVector, SideDirection)
		+ FMath::VRand() * WreckTurretTossConstants::TumbleAxisRandomness).GetSafeNormal();
	TurretFlight.Launch(LaunchVelocity, TumbleAxis * SpinSpeed);
}

FWreckPartFlightSettings UWreckTurretTossComponent::GetTurretFlightSettings() const
{
	FWreckPartFlightSettings FlightSettings;
	FlightSettings.bPlayLandingImpact = M_Settings.bPlayLandingImpact;
	FlightSettings.LandingExplosionType = M_Settings.LandingExplosionType;
	FlightSettings.LandingSound = M_Settings.LandingSound;
	FlightSettings.SettleLinearSpeed = M_Settings.SettleLinearSpeed;
	FlightSettings.SettleAngularSpeed = M_Settings.SettleAngularSpeed;
	FlightSettings.SettleDuration = M_Settings.SettleDuration;
	FlightSettings.MaxFlightTime = M_Settings.MaxFlightTime;
	return FlightSettings;
}

void UWreckTurretTossComponent::StartHullAmmoFire()
{
	if (M_Settings.HullFireType == ERTSFireType::None || not GetIsValidFireSubsystem() || not GetIsValidHullMesh())
	{
		return;
	}

	const FVector RingOffset = M_HullMesh->GetComponentTransform().InverseTransformPosition(M_TurretRingLocation);
	M_HullFireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(
		GetOwner(),
		M_HullMesh.Get(),
		NAME_None,
		RingOffset + M_Settings.HullFireOffset,
		M_Settings.HullFireType,
		M_Settings.HullFireLifeTime,
		M_Settings.HullFireScale,
		M_Settings.HullFireEffectParams);
}

void UWreckTurretTossComponent::StartDebrisBurst() const
{
	AActor* Wreck = GetOwner();
	if (not M_Settings.bThrowDebris || not IsValid(Wreck) || not GetIsValidHullMesh())
	{
		return;
	}
	UWreckDebrisBurstComponent* DebrisBurst = NewObject<UWreckDebrisBurstComponent>(Wreck);
	Wreck->AddInstanceComponent(DebrisBurst);
	DebrisBurst->RegisterComponent();
	DebrisBurst->StartBurst(M_TurretRingLocation, M_HullMesh.Get(), M_Settings.DebrisCountScale,
	                        GetDefault<UWreckDebrisSettings>()->AmmoCookOffBurst);
}

void UWreckTurretTossComponent::StartTurretFire()
{
	if (not M_Settings.bBurnTurretWhileFlying || M_Settings.TurretFireType == ERTSFireType::None
		|| not GetIsValidFireSubsystem() || not GetIsValidTurretMesh())
	{
		return;
	}

	// The wreck turret pivot is often far from the mesh centre; burn from the middle of the turret.
	const FVector TurretCentreOffset = M_TurretMesh->GetComponentTransform().InverseTransformPosition(
		M_TurretMesh->Bounds.Origin);
	M_TurretFireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(
		GetOwner(),
		M_TurretMesh.Get(),
		NAME_None,
		TurretCentreOffset,
		M_Settings.TurretFireType,
		M_Settings.TurretFireLifeTime,
		M_Settings.TurretFireScale,
		FRTSFireEffectParams());
}

void UWreckTurretTossComponent::StopFire(int32& InOutFireHandle) const
{
	if (InOutFireHandle == INDEX_NONE)
	{
		return;
	}
	if (M_FireSubsystem.IsValid())
	{
		M_FireSubsystem->StopFireByHandle(InOutFireHandle);
	}
	InOutFireHandle = INDEX_NONE;
}

bool UWreckTurretTossComponent::GetIsValidTurretMesh() const
{
	if (M_TurretMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_TurretMesh", "GetIsValidTurretMesh", this);
	return false;
}

bool UWreckTurretTossComponent::GetIsValidHullMesh() const
{
	if (M_HullMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_HullMesh", "GetIsValidHullMesh", this);
	return false;
}

bool UWreckTurretTossComponent::GetIsValidFireSubsystem() const
{
	if (M_FireSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_FireSubsystem", "GetIsValidFireSubsystem", this);
	return false;
}
