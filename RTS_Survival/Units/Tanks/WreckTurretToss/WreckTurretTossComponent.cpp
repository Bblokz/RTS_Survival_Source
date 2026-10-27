#include "WreckTurretTossComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace WreckTurretTossConstants
{
	constexpr float LandingCheckInterval = 0.1f;
	// Ignore the first moments of flight: the turret still touches the ring and has no velocity before the impulse.
	constexpr float MinFlightTimeBeforeLanding = 0.5f;
	// Turret angular damping while flying so a rolling turret comes to rest instead of rocking on a curved face.
	constexpr float FlyingAngularDamping = 0.6f;
	// A turret this far below the ring when it settles fell through the world and is hidden.
	constexpr float FellThroughWorldDepth = 5000.f;
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
	SpawnExplosion(M_Settings.LaunchExplosionType, M_TurretRingLocation);
	StartHullAmmoFire();
	// Casemate wrecks have no separate turret: they only get the explosion and the hull ammo fire.
	if (not M_TurretMesh.IsValid())
	{
		return true;
	}

	StoreTurretCollision();
	PlaceTurretOnRing(LiveTurretYawOffset);
	PrepareTurretForFlight();
	// The body only turns dynamic once physics has processed the simulate switch; a velocity set now is lost.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		LaunchTurret();
	}));
	StartTurretFire();
	StartLandingChecks();
	return true;
}

void UWreckTurretTossComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandingCheckTimerHandle);
	}
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
			if (GetCanSimulateTurret(*MeshComponent))
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

bool UWreckTurretTossComponent::GetCanSimulateTurret(UStaticMeshComponent& TurretMesh)
{
	// Non-const: the engine's GetBodySetup is not const.
	const UBodySetup* TurretBodySetup = TurretMesh.GetBodySetup();
	return IsValid(TurretMesh.GetStaticMesh()) && IsValid(TurretBodySetup)
		&& TurretBodySetup->AggGeom.GetElementCount() > 0;
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

void UWreckTurretTossComponent::StoreTurretCollision()
{
	if (not GetIsValidTurretMesh())
	{
		return;
	}
	M_TurretCollisionResponses = M_TurretMesh->GetCollisionResponseToChannels();
	M_TurretObjectType = M_TurretMesh->GetCollisionObjectType();
	M_TurretCollisionEnabled = M_TurretMesh->GetCollisionEnabled();
	M_TurretMobility = M_TurretMesh->Mobility;
}

void UWreckTurretTossComponent::PrepareTurretForFlight()
{
	if (not GetIsValidTurretMesh())
	{
		return;
	}

	M_TurretMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	M_TurretMesh->SetMobility(EComponentMobility::Movable);
	// Only the world blocks the flying turret: it must not shove units or bounce off its own hull.
	M_TurretMesh->SetCollisionObjectType(ECC_PhysicsBody);
	M_TurretMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	M_TurretMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_TurretMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	M_TurretMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	M_TurretMesh->SetNotifyRigidBodyCollision(true);
	M_TurretMesh->OnComponentHit.AddDynamic(this, &UWreckTurretTossComponent::OnTurretHit);
	M_TurretMesh->SetAngularDamping(WreckTurretTossConstants::FlyingAngularDamping);
	// Wreck parts often have gravity off since they never simulate; without it the turret never comes down.
	M_TurretMesh->SetEnableGravity(true);
	M_TurretMesh->SetSimulatePhysics(true);
	bM_IsFlying = true;
}

void UWreckTurretTossComponent::LaunchTurret() const
{
	if (not GetIsValidTurretMesh())
	{
		return;
	}

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
	M_TurretMesh->WakeRigidBody();
	M_TurretMesh->SetPhysicsLinearVelocity(LaunchVelocity);
	M_TurretMesh->SetPhysicsAngularVelocityInDegrees(TumbleAxis * SpinSpeed);
}

void UWreckTurretTossComponent::SpawnExplosion(const ERTS_ExplosionType ExplosionType, const FVector& Location) const
{
	UGameExplosionsManager* ExplosionsManager = FRTS_Statics::GetGameExplosionsManager(this);
	if (not IsValid(ExplosionsManager))
	{
		return;
	}
	ExplosionsManager->SpawnRTSExplosionAtLocation(ExplosionType, Location, true);
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

void UWreckTurretTossComponent::StartLandingChecks()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_LaunchWorldTime = World->GetTimeSeconds();
	M_TimeBelowSettleSpeed = 0.f;
	FTimerDelegate LandingCheckDelegate;
	LandingCheckDelegate.BindWeakLambda(this, [this]()
	{
		CheckIfTurretLanded();
	});
	World->GetTimerManager().SetTimer(M_LandingCheckTimerHandle, LandingCheckDelegate,
	                                  WreckTurretTossConstants::LandingCheckInterval, true);
}

void UWreckTurretTossComponent::CheckIfTurretLanded()
{
	const UWorld* World = GetWorld();
	if (not bM_IsFlying || not IsValid(World) || not GetIsValidTurretMesh())
	{
		return;
	}

	const float FlightTime = World->GetTimeSeconds() - M_LaunchWorldTime;
	if (FlightTime >= M_Settings.MaxFlightTime)
	{
		SettleTurret();
		return;
	}
	if (FlightTime < WreckTurretTossConstants::MinFlightTimeBeforeLanding)
	{
		return;
	}
	if (not M_TurretMesh->IsAnyRigidBodyAwake())
	{
		SettleTurret();
		return;
	}

	const bool bIsSlow = M_TurretMesh->GetPhysicsLinearVelocity().Size() <= M_Settings.SettleLinearSpeed
		&& M_TurretMesh->GetPhysicsAngularVelocityInDegrees().Size() <= M_Settings.SettleAngularSpeed;
	M_TimeBelowSettleSpeed = bIsSlow ? M_TimeBelowSettleSpeed + WreckTurretTossConstants::LandingCheckInterval : 0.f;
	if (M_TimeBelowSettleSpeed >= M_Settings.SettleDuration)
	{
		SettleTurret();
	}
}

void UWreckTurretTossComponent::SettleTurret()
{
	bM_IsFlying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandingCheckTimerHandle);
	}
	if (not GetIsValidTurretMesh())
	{
		return;
	}

	// From here on the turret is an ordinary static wreck part: no simulation, the wreck's own collision again.
	M_TurretMesh->SetSimulatePhysics(false);
	M_TurretMesh->SetNotifyRigidBodyCollision(false);
	M_TurretMesh->OnComponentHit.RemoveDynamic(this, &UWreckTurretTossComponent::OnTurretHit);
	M_TurretMesh->SetCollisionObjectType(M_TurretObjectType);
	M_TurretMesh->SetCollisionResponseToChannels(M_TurretCollisionResponses);
	M_TurretMesh->SetCollisionEnabled(M_TurretCollisionEnabled);
	if (M_TurretMobility != EComponentMobility::Movable)
	{
		M_TurretMesh->SetMobility(M_TurretMobility);
	}

	if (M_TurretMesh->GetComponentLocation().Z < M_TurretRingLocation.Z - WreckTurretTossConstants::FellThroughWorldDepth)
	{
		M_TurretMesh->SetHiddenInGame(true);
		RTSFunctionLibrary::ReportError(
			TEXT("UWreckTurretTossComponent::SettleTurret - tossed turret fell through the world and was hidden."));
	}
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

void UWreckTurretTossComponent::OnTurretHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                                            const FHitResult& Hit)
{
	if (bM_HasPlayedLandingImpact || not M_Settings.bPlayLandingImpact)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World)
		|| World->GetTimeSeconds() - M_LaunchWorldTime < WreckTurretTossConstants::MinFlightTimeBeforeLanding)
	{
		return;
	}

	bM_HasPlayedLandingImpact = true;
	SpawnExplosion(M_Settings.LandingExplosionType, Hit.ImpactPoint);
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
