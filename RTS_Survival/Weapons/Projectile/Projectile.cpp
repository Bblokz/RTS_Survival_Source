// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "Projectile.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "Sound/SoundCue.h"
#include "Trace/Trace.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

// --- local helpers (file-scope) ------------------------------------------------
namespace
{
	// Builds a rotation whose +Z points outward along the impact normal.
	inline FRotator MakeImpactOutwardRotationZ(const FHitResult& Hit)
	{
		const FVector N = Hit.ImpactNormal.GetSafeNormal();
		return FRotationMatrix::MakeFromZ(N).Rotator();
	}

	inline void PlayNiagaraAt(
		UWorld* World,
		UNiagaraSystem* Vfx,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale)
	{
		if (World && Vfx)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Vfx, Location, Rotation, Scale);
		}
	}

	inline void PlaySoundAt(
		UWorld* World,
		USoundBase* Sound,
		const FVector& Location,
		const FRotator& Rotation,
		USoundAttenuation* Attenuation,
		USoundConcurrency* Concurrency)
	{
		if (World && Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, Rotation, 1.f, 1.f, 0.f,
			                                      Attenuation, Concurrency);
		}
	}

	inline const FRTSSurfaceImpactData* FindImpactData(
		const TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& Map,
		ERTSSurfaceType Surface)
	{
		return Map.Contains(Surface) ? &Map.FindChecked(Surface) : nullptr;
	}

	inline void ReportMissingSurface(const AProjectile* Self, ERTSSurfaceType Surface)
	{
		const FString SurfaceString = FRTS_PhysicsHelper::GetSurfaceTypeString(Surface);
		RTSFunctionLibrary::ReportError(
			"The impact vfx for projectile: " + (Self ? Self->GetName() : TEXT("<null>")) +
			" does not contain a surface: " + SurfaceString + " in the impact vfx map.");
	}
} // namespace

AProjectile::AProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), M_ProjectileOwner(nullptr), M_ProjectileSpawn(), M_ProjectileMovement(nullptr),
	  M_TraceChannel()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AProjectile::OnCreatedInPoolSetDormant()
{
	// Is this enough for the projectile to stay alive and hidden in game till it needs to be used?
	OnProjectileDormant();
}

void AProjectile::InitProjectilePoolSettings(const TWeakObjectPtr<ASmallArmsProjectileManager> Manager,
                                             const int32 Index)
{
	if (not Manager.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid projectile pool manager provided for tank projectile: " + GetName());
		return;
	}
	M_ProjectilePoolSettings.ProjectileManager = Manager;
	M_ProjectilePoolSettings.ProjectileIndex = Index;
}

void AProjectile::SetupProjectileForNewLaunch(
	UWeaponStateProjectile* NewProjectileOwner,
	const ERTSDamageType DamageType,
	const float Range,
	const float Damage,
	const float ArmorPen,
	const float ArmorPenMaxRange,
	const uint32 ShrapnelParticles,
	const float ShrapnelRange,
	const float ShrapnelDamage,
	const float ShrapnelArmorPen,
	const int32 OwningPlayer,
	const TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& ImpactSurfaceData,
	UNiagaraSystem* BounceEffect,
	USoundCue* BounceSound,
	const FVector& ImpactScale,
	const FVector& BounceScale,
	const float ProjectileSpeed, const FVector& LaunchLocation,
	const FRotator& LaunchRotation, USoundAttenuation* ImpactAttenuation, USoundConcurrency* ImpactConcurrency,
	const FProjectileVfxSettings&
	ProjectileVfxSettings, const EWeaponShellType ShellType, const TArray<AActor*>& ActorsToIgnore)
{
	M_ShellType = ShellType;
	M_ProjectileOwner = NewProjectileOwner;
	OnNewDamageType(DamageType);
	M_OwningPlayer = OwningPlayer;
	M_Range = Range;
	M_FullDamage = Damage;
	M_ArmorPen = ArmorPen;
	M_ArmorPenAtMaxRange = ArmorPenMaxRange;
	M_ShrapnelParticles = ShrapnelParticles;
	M_ShrapnelRange = ShrapnelRange;
	M_ShrapnelDamage = ShrapnelDamage;
	M_ShrapnelArmorPen = ShrapnelArmorPen;
	// SetupCollision(OwningPlayer);
	M_ImpactVfx = ImpactSurfaceData;
	M_ImpactScale = ImpactScale;
	M_BounceSound = BounceSound;
	M_BounceVfx = BounceEffect;
	M_BounceScale = BounceScale;
	M_ImpactAttenuation = ImpactAttenuation;
	M_ImpactConcurrency = ImpactConcurrency;
	M_ActorsToIgnore.Empty();
	M_ActorsToIgnore = ActorsToIgnore;

	OnRestartProjectile(LaunchLocation, LaunchRotation, ProjectileSpeed);
	SetupNiagaraWithPrjVfxSettings(ProjectileVfxSettings);
	// Reset the amount of bounced allowed.
	M_MaxBounces = DeveloperSettings::GameBalance::Weapons::Projectiles::MaxBouncesPerProjectile;
	// Only notify on first bounce; reset as we have a new projectile fire.
	bM_AlreadyNotified = false;

	// Used to calculate armor pen at different ranges.
	M_ProjectileSpawn = GetActorLocation();
	SetupTraceChannel(OwningPlayer);

	// Calculate time until explosion
	const float TimeToExplode = M_Range / ProjectileSpeed;

	// Set a timer to call the explosion function after TimeToExplode seconds
	GetWorld()->GetTimerManager().SetTimer(M_ExplosionTimerHandle, this, &AProjectile::HandleTimedExplosion,
	                                       TimeToExplode, false);
	const float LineTraceInterval = DeveloperSettings::Optimization::ProjectileTraceInterval;
	GetWorld()->GetTimerManager().SetTimer(M_LineTraceTimerHandle, this, &AProjectile::PerformAsyncLineTrace,
	                                       LineTraceInterval, true);
	// set up our last-trace timestamp; make sure we do not clip through nearby objects at the first trace!
	// Sample game time; respect dilation.
	M_LastTraceTime = GetWorld()->GetTimeSeconds() - LineTraceInterval;
	PerformAsyncLineTrace();
}

void AProjectile::SetupAttachedRocketMesh(UStaticMesh* RocketMesh)
{
	if (GetIsValidNiagara())
	{
		M_NiagaraComponent->SetVariableStaticMesh(*NiagaraRocketMeshName, RocketMesh);
	}
}

void AProjectile::ApplyAccelerationFactor(const float Factor, const bool bUseGravityTArch, const float LowestZValue)
{
	if (!GetIsValidProjectileMovement())
	{
		return;
	}

	// 1) Scale overall velocity by Factor to accelerate movement over time.
	if (FMath::IsNearlyZero(Factor))
	{
		RTSFunctionLibrary::ReportError(TEXT("ApplyAccelerationFactor: Factor is zero; cannot scale velocity."));
		return;
	}
	M_ProjectileMovement->Velocity *= Factor;

	// 2) If the gravity‐based trajectory adjustment is requested, compute and set a custom gravity scale
	if (bUseGravityTArch)
	{
		// Retrieve current position and velocity components
		const FVector CurrentLocation = GetActorLocation();
		const FVector CurrentVelocity = M_ProjectileMovement->Velocity;

		// Horizontal (XY) speed remains constant
		const FVector HorizontalVel = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
		const float HorizontalSpeed = HorizontalVel.Size();
		if (HorizontalSpeed <= SMALL_NUMBER)
		{
			RTSFunctionLibrary::ReportError(
				TEXT("ApplyAccelerationFactor: Horizontal speed is zero; cannot compute flight time."));
			return;
		}

		// We want the projectile to travel M_Range (which is the total horizontal distance) before exploding.
		// Flight time t = Range / horizontal speed.
		if (M_Range <= 0.0f)
		{
			RTSFunctionLibrary::ReportError(
				TEXT("ApplyAccelerationFactor: M_Range is not positive; cannot compute trajectory."));
			return;
		}
		const float FlightTime = M_Range / HorizontalSpeed;

		// Initial vertical position and velocity
		const float Z0 = CurrentLocation.Z;
		const float Vz = CurrentVelocity.Z;

		// Solve z(t) = Z0 + Vz * t + 0.5 * a_z * t^2 = LowestZValue
		// => a_z = 2 * (LowestZValue - Z0 - Vz * t) / (t^2)
		const float Numerator = LowestZValue - Z0 - Vz * FlightTime;
		const float Denominator = 0.5f * FlightTime * FlightTime;
		if (FMath::IsNearlyZero(Denominator))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("ApplyAccelerationFactor: FlightTime too small; cannot compute gravity."));
			return;
		}
		const float DesiredAccelZ = Numerator / Denominator;

		// Unreal’s default gravity is obtained from the world’s gravity Z (negative value)
		const float DefaultGravityZ = GetWorld()->GetGravityZ();
		if (FMath::IsNearlyZero(DefaultGravityZ))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("ApplyAccelerationFactor: Default gravity is zero; cannot adjust gravity scale."));
			return;
		}

		// GravityScale = DesiredAccelZ / DefaultGravityZ
		// If DesiredAccelZ and DefaultGravityZ are both negative (downward), the scale is positive.
		const float NewGravityScale = DesiredAccelZ / DefaultGravityZ;
		M_ProjectileMovement->ProjectileGravityScale = NewGravityScale;
	}
}

void AProjectile::SetupTraceChannel(const int NewOwningPlayer)
{
	if (NewOwningPlayer == 1)
	{
		M_TraceChannel = COLLISION_TRACE_ENEMY;
	}
	else
	{
		M_TraceChannel = COLLISION_TRACE_PLAYER;
	}
}


void AProjectile::HandleTimedExplosion()
{
	// Explode mid air.
	FRotator Rotation = FRotator::ZeroRotator;
	SpawnExplosion(GetActorLocation(), Rotation, ERTSSurfaceType::Air);
	if (M_ShrapnelParticles > 0)
	{
		// UAOE::CreateTraceAOEHitEnemy(this,
		//                              [this](AActor* Param) { DamageActorWithShrapnel(Param); },
		//                              GetActorLocation(), M_ShrapnelRange, M_ShrapnelParticles, {this});
	}
	OnProjectileDormant();
}

// Called when the game starts or when spawned
void AProjectile::BeginPlay()
{
	Super::BeginPlay();
}


void AProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
	}
}

void AProjectile::BeginDestroy()
{
	Super::BeginDestroy();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
	}
}

void AProjectile::OnHitActor(
	AActor* HitActor,
	const FVector& HitLocation,
	const FRotator& HitRotation, const ERTSSurfaceType HitSurface)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrjWp_OnHitActor);
	if (RTSFunctionLibrary::RTSIsValid(HitActor))
	{
		M_DamageEvent.HitInfo.Location = HitLocation;
		if (HitActor->TakeDamage(M_FullDamage, M_DamageEvent, nullptr, this) == 0 && M_ProjectileOwner)
		{
			M_ProjectileOwner->OnProjectileKilledActor(HitActor);
		}
		const TArray<AActor*> ActorsToIgnore = {HitActor, this};
		SpawnExplosion(HitLocation, HitRotation, HitSurface);
		if (M_ShrapnelParticles > 0)
		{
			// UAOE::CreateTraceAOEHitEnemy(this,
			//                              [this](AActor* Param) { DamageActorWithShrapnel(Param); },
			//                              HitLocation, M_ShrapnelRange, M_ShrapnelParticles, ActorsToIgnore);
		}
		OnProjectileDormant();
	}
}

void AProjectile::DamageActorWithShrapnel(AActor* HitActor)
{
	HitActor->TakeDamage(M_ShrapnelDamage, M_DamageEvent, nullptr, this);
}

void AProjectile::OnProjectileDormant()
{
	if (not GetIsValidProjectileMovement() || not GetIsValidProjectileManager())
	{
		return;
	}
	SetActorHiddenInGame(true);
	M_ProjectileMovement->Velocity = FVector::ZeroVector;
	M_ProjectileMovement->Deactivate();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
	}
	M_ProjectilePoolSettings.ProjectileManager->OnTankProjectileDormant(M_ProjectilePoolSettings.ProjectileIndex);
}

void AProjectile::OnRestartProjectile(const FVector& NewLocation, const FRotator& LaunchRotation,
                                      const float ProjectileSpeed)
{
	SetActorHiddenInGame(false);
	// Move projectile to new location.
	SetActorLocation(NewLocation);
	SetActorRotation(LaunchRotation);
	if (GetIsValidProjectileMovement())
	{
		M_ProjectileMovement->Activate();
		const FVector Direction = LaunchRotation.Vector();
		M_ProjectileMovement->Velocity = Direction * ProjectileSpeed;
		DebugProjectile("Projectile speed: " + FString::SanitizeFloat(ProjectileSpeed));
	}
}

bool AProjectile::GetIsValidProjectileManager() const
{
	if (M_ProjectilePoolSettings.ProjectileManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Projectile attempted to get pool manager but it is invalid!"
		"\n TANK projectile: " + GetName());
	return false;
}

void AProjectile::OnNewDamageType(const ERTSDamageType DamageType)
{
	M_RTSDamageType = DamageType;
	M_DamageEvent.DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(M_RTSDamageType);
}

void AProjectile::PerformAsyncLineTrace()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrjWp_AsyncLineTrace);
	if (!IsValid(M_ProjectileMovement))
	{
		return;
	}
	// Sample game time; respect dilation.
	const double Now = GetWorld()->GetTimeSeconds();
	const double Δtime = Now - M_LastTraceTime;
	M_LastTraceTime = Now;

	const FVector StartLocation = GetActorLocation();
	const FVector Dir = M_ProjectileMovement->Velocity.GetSafeNormal();
	const float Speed = M_ProjectileMovement->Velocity.Size();
	const FVector EndLocation = StartLocation + Dir * Speed * Δtime * 1.1;

	FCollisionQueryParams TraceParams(FName(TEXT("ProjectileTrace")), false, this);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;
	TraceParams.AddIgnoredActors(M_ActorsToIgnore);

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindUObject(this, &AProjectile::OnAsyncTraceComplete);

	GetWorld()->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		StartLocation,
		EndLocation,
		M_TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void AProjectile::OnAsyncTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
	if (TraceDatum.OutHits.Num() == 0)
	{
		return;
	}

	const FHitResult& HitResult = TraceDatum.OutHits[0];
	if (not HitResult.GetActor() || not HitResult.GetComponent())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(Projectile::SearchArmorCalc);

	AActor* HitActor = nullptr;
	UArmorCalculation* ArmorCalculationComp = FRTSWeaponHelpers::GetArmorAndActorOrParentFromHit(
		HitResult, HitActor);
	if (IsValid(ArmorCalculationComp))
	{
		OnHitArmorCalcComponent(ArmorCalculationComp, HitResult, HitActor);
		return;
	}
	// Handle propagation of succesful hit.
	ProjectileHitPropagateNotification(false);
	ERTSSurfaceType SurfaceType = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	// Always deals damage; no armor detected.
	HandleHitActorAndClearTimer(
		HitResult.GetActor(),
		HitResult.Location,
		SurfaceType,
		MakeImpactOutwardRotationZ(HitResult));
}


float AProjectile::CalculateImpactAngle(const FVector& Velocity, const FVector& ImpactNormal) const
{
	FVector ImpactDirection = Velocity.GetSafeNormal();
	float DotProduct = FVector::DotProduct(ImpactDirection, ImpactNormal);
	float ClampedDot = FMath::Clamp(DotProduct, -1.0f, 1.0f); // Prevent NaN from Acos
	float ImpactAngleRadians = FMath::Acos(ClampedDot);
	return FMath::RadiansToDegrees(ImpactAngleRadians);
}

void AProjectile::HandleProjectileBounce(const FHitResult& HitResult)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}
	ProjectileHitPropagateNotification(true);

	// Reflect the velocity based on the hit normal
	FVector ReflectedVelocity = FVector::VectorPlaneProject(M_ProjectileMovement->Velocity, HitResult.ImpactNormal).
		GetSafeNormal();
	float VelocityScale = M_ProjectileMovement->Velocity.Size();

	// Update projectile's position and rotation
	SetActorLocation(HitResult.ImpactPoint);
	SetActorRotation(ReflectedVelocity.Rotation());
	M_ProjectileMovement->Velocity = ReflectedVelocity * VelocityScale;

	// Spawn bounce effects
	SpawnBounce(HitResult.ImpactPoint, ReflectedVelocity.Rotation());

	if (M_ShellType == EWeaponShellType::Shell_HE || M_ShellType == EWeaponShellType::Shell_HEAT)
	{
		// If the projectile is HE or Heat it never bounced but explodes with the HE bounce effect.
		OnProjectileDormant();
		return;
	}

	// Reduce armor penetration
	float Divider = DeveloperSettings::GameBalance::Weapons::Projectiles::AmorPenBounceDivider;
	M_ArmorPen /= Divider;
	M_ArmorPenAtMaxRange /= Divider;

	// Decrement remaining bounces
	M_MaxBounces--;
	if (M_MaxBounces <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
	}
	return;
}

void AProjectile::HandleHitActorAndClearTimer(AActor* HitActor, const FVector& HitLocation,
                                              const ERTSSurfaceType HitSurface, const FRotator& HitRotation)
{
	OnHitActor(HitActor, HitLocation, HitRotation, HitSurface);
	GetWorld()->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
}


void AProjectile::SpawnExplosion(const FVector& Location, const FRotator& HitRotation,
                                 const ERTSSurfaceType HitSurface) const
{
	// Resolve impact data once.
	const FRTSSurfaceImpactData* const Data = FindImpactData(M_ImpactVfx, HitSurface);
	if (!Data)
	{
		ReportMissingSurface(this, HitSurface);
		return;
	}

	// Normal pooled path:
	if (M_ProjectileOwner)
	{
		M_ProjectileOwner->CreateWeaponImpact(Location, HitSurface, HitRotation);
		return;
	}

	// Fallback (no owner → non-pooled).
	if (UWorld* World = GetWorld())
	{
		PlayNiagaraAt(World, Data->ImpactEffect, Location, FRotator::ZeroRotator, M_ImpactScale);
		PlaySoundAt(World, Data->ImpactSound, Location, FRotator::ZeroRotator, M_ImpactAttenuation,
		            M_ImpactConcurrency);
	}
}

void AProjectile::SpawnBounce(const FVector& Location, const FRotator& BounceDirection) const
{
	// Forward to owner’s pooled bounce player (VFX + SFX).
	if (M_ProjectileOwner)
	{
		M_ProjectileOwner->CreateWeaponNonPenVfx(Location, BounceDirection);
		return;
	}

	// Fallback: keep the old behavior if owner is missing.
	if (UWorld* World = GetWorld())
	{
		PlayNiagaraAt(World, M_BounceVfx, Location, BounceDirection, M_BounceScale);
		PlaySoundAt(World, M_BounceSound, Location, FRotator::ZeroRotator, M_ImpactAttenuation, M_ImpactConcurrency);
	}
}


void AProjectile::OnHitArmorCalcComponent(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
                                          AActor* HitActor)
{
	if (M_RTSDamageType == ERTSDamageType::Kinetic)
	{
		ArmorCalc_KineticProjectile(ArmorCalculation, HitResult, HitActor);
	}
	else
	{
		ArmorCalc_FireProjectile(ArmorCalculation, HitResult, HitActor);
	}
}

void AProjectile::ArmorCalc_KineticProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
                                              AActor* HitActor)
{
	float RawArmorValue = 0.0f;
	float AdjustedArmorPen = GetArmorPenAtRange();
	const FVector Velocity = M_ProjectileMovement->Velocity;
	const float EffectiveArmor = ArmorCalculation->GetEffectiveArmorOnHit(
		HitResult.Component, HitResult.Location, Velocity,
		HitResult.ImpactNormal, RawArmorValue, AdjustedArmorPen);
	// RTSFunctionLibrary::PrintString("Effective Armor: " + FString::SanitizeFloat(EffectiveArmor), FColor::Red);
	// RTSFunctionLibrary::PrintString("Raw Armor: " + FString::SanitizeFloat(RawArmorValue), FColor::Blue);
	// RTSFunctionLibrary::PrintString("Adjusted Pen: " + FString::SanitizeFloat(AdjustedArmorPen), FColor::Green);
	const bool bShouldBounce = (EffectiveArmor >= AdjustedArmorPen) &&
	(M_ArmorPen < DeveloperSettings::GameBalance::Weapons::Projectiles::AllowPenRegardlessOfAngleFactor *
		RawArmorValue);
	if (bShouldBounce)
	{
		HandleProjectileBounce(HitResult);
		return;
	}
	ProjectileHitPropagateNotification(false);

	const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	HandleHitActorAndClearTimer(
		HitActor,
		HitResult.Location,
		SurfaceTypeHit,
		MakeImpactOutwardRotationZ(HitResult));
}

void AProjectile::ArmorCalc_FireProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
                                           AActor* HitActor)
{
	ProjectileHitPropagateNotification(false);

	const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	HandleHitActorAndClearTimer(
		HitActor,
		HitResult.Location,
		SurfaceTypeHit,
		MakeImpactOutwardRotationZ(HitResult));
}

void AProjectile::ProjectileHitPropagateNotification(const bool bBounced)
{
	if (M_OwningPlayer != 1 || bM_AlreadyNotified)
	{
		// no proagation and hence voice lines if owning player is 1.
		return;
	}
	bM_AlreadyNotified = true;
	if (M_ProjectileOwner)
	{
		M_ProjectileOwner->OnProjectileHit(bBounced);
	}
}

void AProjectile::SetupNiagaraWithPrjVfxSettings(const FProjectileVfxSettings& NewSettings)
{
	if (UNiagaraSystem* System = GetSystemFromType(NewSettings.ProjectileNiagaraSystem))
	{
		const bool bSetSystemCauseInvalid = !IsValid(M_NiagaraComponent->GetAsset());
		if (bSetSystemCauseInvalid || System != M_NiagaraComponent->GetAsset())
		{
			M_NiagaraComponent->SetAsset(System);
			DebugProjectile("Niagara system changed: " + System->GetName());
		}

		// For AttachedRockets this will make sure the effect is not too large.
		ScaleNiagaraSystemDependingOnType(NewSettings.ProjectileNiagaraSystem);

		if (GetCanSetParametersOnSystem(NewSettings.ProjectileNiagaraSystem))
		{
			const int32 ShellTypeAsInt = static_cast<int32>(NewSettings.ShellType);
			M_NiagaraComponent->SetColorParameter(*NiagaraShellColorName,
			                                      GetColorOfShell(NewSettings.ShellType,
			                                                      NewSettings.ProjectileNiagaraSystem));
			const float Width = GetWidthOfShell(NewSettings.WeaponCaliber, NewSettings.ShellType);
			DebugProjectile("Shell width: " + FString::SanitizeFloat(Width) +
				"\n Shell type: " + FString::FromInt(ShellTypeAsInt) +
				"\n Calibre: " + FString::SanitizeFloat(NewSettings.WeaponCaliber) +
				"\n Shell color: " + GetColorOfShell(NewSettings.ShellType,
				                                     NewSettings.ProjectileNiagaraSystem).ToString());
			M_NiagaraComponent->SetFloatParameter(*NiagaraShellWidthName,
			                                      Width);
		}
	}
	M_ProjectileVfxSettings = NewSettings;
}

UNiagaraSystem* AProjectile::GetSystemFromType(const EProjectileNiagaraSystem Type)
{
	if (not GetIsValidNiagara())
	{
		return nullptr;
	}
	if (UNiagaraSystem* System = ProjectileNiagaraSystems.FindChecked(Type))
	{
		return System;
	}
	return nullptr;
}

bool AProjectile::GetIsValidProjectileMovement()
{
	if (IsValid(M_ProjectileMovement))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid projectile movement on projectile: " + GetName());
	M_ProjectileMovement = FindComponentByClass<UProjectileMovementComponent>();
	return IsValid(M_ProjectileMovement);
}

bool AProjectile::GetIsValidNiagara()
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid niagara component on projectile: " + GetName());
	M_NiagaraComponent = FindComponentByClass<UNiagaraComponent>();
	return IsValid(M_NiagaraComponent);
}

bool AProjectile::GetCanSetParametersOnSystem(const EProjectileNiagaraSystem Type) const
{
	switch (Type)
	{
	case EProjectileNiagaraSystem::None:
		return false;
	case EProjectileNiagaraSystem::TankShell:
		return true;
	case EProjectileNiagaraSystem::AAShell:
		return true;
	case EProjectileNiagaraSystem::Bazooka:
		return false;
	case EProjectileNiagaraSystem::AttachedRocket:
		// Uses different settings!
		return false;
	}
	RTSFunctionLibrary::ReportError("Invalid projectile type for setting parameters: " +
		FString::FromInt(static_cast<int32>(Type)));
	return false;
}

FLinearColor AProjectile::GetColorOfShell(const EWeaponShellType ShellType,
                                          const EProjectileNiagaraSystem SystemType) const
{
	if (SystemType == EProjectileNiagaraSystem::AAShell)
	{
		return AAShellColor;
	}
	if (auto* Color = ShellColorMap.Find(ShellType))
	{
		return *Color;
	}
	RTSFunctionLibrary::ReportError(
		"No color found for shell type: " + FString::FromInt(static_cast<int32>(ShellType)));
	return FLinearColor::Red;
}

float AProjectile::GetWidthOfShell(const float WeaponCalibre, const EWeaponShellType ShellType) const
{
	using namespace DeveloperSettings::GamePlay::Projectile::ProjectilesVfx;

	float ShellWidth = BaseShellWidth;
	// Calculation base-off 75 mm shells:
	ShellWidth *= (WeaponCalibre / 75.f);
	switch (ShellType)
	{
	// Falls through.
	case EWeaponShellType::Shell_AP:
	case EWeaponShellType::Shell_APHE:
		return ShellWidth * APShellWidthMlt;
	case EWeaponShellType::Shell_APHEBC:
		return ShellWidth * APBCShellWidthMlt;
	case EWeaponShellType::Shell_APCR:
		return ShellWidth * APCRShellWidthMlt;
	case EWeaponShellType::Shell_HE:
		return ShellWidth * HEShellWidthMlt;
	case EWeaponShellType::Shell_HEAT:
		return ShellWidth * HEATShellWidthMlt;
	case EWeaponShellType::Shell_None:
		break;
	case EWeaponShellType::Shell_Fire:
		return ShellWidth * FireShellWidthMlt;
	}
	RTSFunctionLibrary::ReportError("Invalid shell type for calculating width: " +
		FString::FromInt(static_cast<int32>(ShellType)));
	return ShellWidth;
}

void AProjectile::DebugBounce(const FVector& Location) const
{
	if (DeveloperSettings::Debugging::GWeapon_ArmorPen_Compile_DebugSymbols)
	{
		DrawDebugString(
			GetWorld(), Location, TEXT("Bounce!"), 0, FColor::Purple, 5.f, false, 1.f);
	}
}

void AProjectile::DebugProjectile(const FString& Message) const
{
	if (DeveloperSettings::Debugging::GProjectilePooling_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message + "\n Projectile: " + GetName(), FColor::Magenta);
	}
}

void AProjectile::ScaleNiagaraSystemDependingOnType(const EProjectileNiagaraSystem Type) const
{
	if (Type == EProjectileNiagaraSystem::AttachedRocket)
	{
		M_NiagaraComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
		// Also restart to recreate rocket.
		M_NiagaraComponent->ReinitializeSystem();
	}
	else
	{
		M_NiagaraComponent->SetWorldScale3D(M_BasicScale);
	}
}

void AProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_ProjectileMovement = FindComponentByClass<UProjectileMovementComponent>();
	if (!IsValid(M_ProjectileMovement))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "ProjectileMovement", "PostInitComponents");
	}
	M_NiagaraComponent = FindComponentByClass<UNiagaraComponent>();
	if (!IsValid(M_NiagaraComponent))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NiagaraComponent", "PostInitComponents");
	}
	else
	{
		M_BasicScale = M_NiagaraComponent->GetComponentScale();
	}
}

float AProjectile::GetArmorPenAtRange()
{
	const float DistanceTravelled = FVector::Dist(M_ProjectileSpawn, GetActorLocation());
	const float InterpolationFactor = FMath::Clamp(DistanceTravelled / M_Range, 0.0f, 1.0f);
	return FMath::Lerp(M_ArmorPen, M_ArmorPenAtMaxRange, InterpolationFactor);
}
