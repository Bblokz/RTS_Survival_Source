// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "Projectile.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/Derived/Damage/RadixiteDamageBehaviour.h"
#include "Sound/SoundCue.h"
#include "Trace/Trace.h"
#include "DrawDebugHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourVehicleStunned/BehVehicleStunned.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Subsystems/CameraShakeSubsystem/RTSCameraShakeSubsystem.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

// --- local helpers (file-scope) ------------------------------------------------
namespace
{
	constexpr float MinCurvatureVelocityMultiplier = 0.25f;
	constexpr float MaxCurvatureVelocityMultiplier = 4.0f;
	constexpr float RocketSwingStrengthMin = 1.0f;
	constexpr float RocketSwingStrengthMax = 100.0f;
	constexpr float RocketSwingCurveDistanceMinPercent = 0.2f;
	constexpr float RocketSwingCurveDistanceMaxPercent = 0.65f;
	constexpr float RocketSwingMinStraightPercent = 0.15f;
	constexpr float RocketSwingMinSpeedMultiplier = 0.1f;
	constexpr float RocketSwingMinCurveDistance = 50.0f;

	namespace RadixiteDamageConstants
	{
		constexpr float MinCalibreMm = 20.f;
		constexpr float MaxCalibreMm = 160.f;
		constexpr float MinDurationSeconds = 5.f;
		constexpr float MaxDurationSeconds = 15.f;
	}

	struct FArcedLaunchSolution
	{
		FVector LaunchLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
		FVector ApexLocation = FVector::ZeroVector;
		FVector LaunchVelocity = FVector::ZeroVector;
		float TimeToTarget = 0.0f;
	};

	struct FRocketSwingLaunchSolution
	{
		FVector CurveDirection = FVector::ZeroVector;
		FVector CurveEndLocation = FVector::ZeroVector;
		float CurveSpeed = 0.0f;
		float StraightSpeed = 0.0f;
		float CurveTime = 0.0f;
		float StraightTime = 0.0f;
		bool bUseStraightOnly = false;
	};

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

	inline FVector CalculateApexLocation(const FVector& LaunchLocation,
	                                     const FVector& HorizontalDirection,
	                                     const float HorizontalSpeed,
	                                     const float InitialVerticalVelocity,
	                                     const float Gravity)
	{
		const float TimeToApex = InitialVerticalVelocity / Gravity;
		const FVector HorizontalDisplacement = HorizontalDirection * HorizontalSpeed * TimeToApex;
		const float VerticalDisplacement = (InitialVerticalVelocity * TimeToApex) - (0.5f * Gravity * TimeToApex *
			TimeToApex);
		return LaunchLocation + HorizontalDisplacement + FVector(0.0f, 0.0f, VerticalDisplacement);
	}

	inline void DebugDrawArcedLaunch(const AProjectile* Projectile, const FArcedLaunchSolution& Solution)
	{
		if constexpr (DeveloperSettings::Debugging::GArchProjectile_Compile_DebugSymbols)
		{
			if (const UWorld* World = Projectile ? Projectile->GetWorld() : nullptr)
			{
				DrawDebugString(World, Solution.TargetLocation, TEXT("Arc Target"), nullptr, FColor::Green, 2.0f, false,
				                1.5f);
				DrawDebugString(World, Solution.ApexLocation, TEXT("Arc Apex"), nullptr, FColor::Yellow, 2.0f, false,
				                1.5f);
				DrawDebugLine(World, Solution.LaunchLocation, Solution.ApexLocation, FColor::Yellow, false, 2.0f, 0,
				              1.5f);
				DrawDebugLine(World, Solution.ApexLocation, Solution.TargetLocation, FColor::Green, false, 2.0f, 0,
				              1.5f);
			}
		}
	}

	inline bool BuildRocketSwingLaunchSolution(const FVector& LaunchLocation,
	                                           const FVector& TargetLocation,
	                                           const float ProjectileSpeed,
	                                           const FRocketWeaponSettings& RocketSettings,
	                                           FRocketSwingLaunchSolution& OutSolution)
	{
		const FVector LaunchToTarget = TargetLocation - LaunchLocation;
		const float DistanceToTarget = LaunchToTarget.Size();
		if (DistanceToTarget <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float SafeProjectileSpeed = FMath::Max(ProjectileSpeed, KINDA_SMALL_NUMBER);
		OutSolution.CurveSpeed = SafeProjectileSpeed * FMath::Max(RocketSettings.InCurveSpeedMlt,
		                                                          RocketSwingMinSpeedMultiplier);
		OutSolution.StraightSpeed = SafeProjectileSpeed * FMath::Max(RocketSettings.StraightSpeedMlt,
		                                                             RocketSwingMinSpeedMultiplier);

		const float StrengthClamped = FMath::Clamp(RocketSettings.RandomSwingStrength, RocketSwingStrengthMin,
		                                           RocketSwingStrengthMax);
		const float StrengthAlpha = (StrengthClamped - RocketSwingStrengthMin) /
			(RocketSwingStrengthMax - RocketSwingStrengthMin);
		const float CurveDistancePercent = FMath::Lerp(RocketSwingCurveDistanceMinPercent,
		                                               RocketSwingCurveDistanceMaxPercent,
		                                               StrengthAlpha);
		const float MaxCurveDistance = DistanceToTarget * (1.0f - RocketSwingMinStraightPercent);
		const FVector BaseDirection = LaunchToTarget.GetSafeNormal();

		if (MaxCurveDistance <= RocketSwingMinCurveDistance)
		{
			OutSolution.bUseStraightOnly = true;
			OutSolution.CurveDirection = BaseDirection;
			OutSolution.CurveEndLocation = LaunchLocation;
			OutSolution.StraightTime = DistanceToTarget / OutSolution.StraightSpeed;
			return true;
		}

		const float CurveDistance = FMath::Clamp(DistanceToTarget * CurveDistancePercent,
		                                         RocketSwingMinCurveDistance,
		                                         MaxCurveDistance);

		const float SwingYawMin = FMath::Min(RocketSettings.RandomSwingYawDeviationMin,
		                                     RocketSettings.RandomSwingYawDeviationMax);
		const float SwingYawMax = FMath::Max(RocketSettings.RandomSwingYawDeviationMin,
		                                     RocketSettings.RandomSwingYawDeviationMax);
		const float SwingYawDeviation = FMath::RandRange(SwingYawMin, SwingYawMax);
		const float SwingYawSigned = FMath::RandBool() ? SwingYawDeviation : -SwingYawDeviation;
		OutSolution.CurveDirection = FRotator(0.0f, SwingYawSigned, 0.0f).RotateVector(BaseDirection);
		if (OutSolution.CurveDirection.IsNearlyZero())
		{
			OutSolution.CurveDirection = BaseDirection;
		}

		OutSolution.CurveEndLocation = LaunchLocation + (OutSolution.CurveDirection * CurveDistance);
		const float StraightDistance = FVector::Distance(OutSolution.CurveEndLocation, TargetLocation);
		OutSolution.CurveTime = CurveDistance / OutSolution.CurveSpeed;
		OutSolution.StraightTime = StraightDistance / OutSolution.StraightSpeed;
		return true;
	}
} // namespace

AProjectile::AProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), M_ProjectileOwner(nullptr), M_ProjectileSpawn(), M_ProjectileMovement(nullptr),
	  M_TraceChannel()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AProjectile::OverwriteGravityScale(const float NewGravityScale) const
{
	if (not M_ProjectileMovement)
	{
		return;
	}
	M_ProjectileMovement->ProjectileGravityScale = NewGravityScale;
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

void AProjectile::SetCameraShakeSubsystem(URTSCameraShakeSubsystem* CameraShakeSubsystem)
{
	M_CameraShakeSubsystem = CameraShakeSubsystem;
}

bool AProjectile::GetIsValidCameraShakeSubsystem() const
{
	if (M_CameraShakeSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_CameraShakeSubsystem",
		"GetIsValidCameraShakeSubsystem",
		this);
	return false;
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
	ProjectileVfxSettings, const EWeaponShellType ShellType, const TArray<AActor*>& ActorsToIgnore, const int32
	WeaponCalibre, const bool bCanArmorOverPenetrate, const float PostPenArmorPenCarryOver,
	const float FloorArmorPenPercentageNeededAllowOverpen)
{
	M_ShellType = ShellType;
	M_ProjectileOwner = NewProjectileOwner;
	OnNewDamageType(DamageType);
	M_OwningPlayer = OwningPlayer;
	// NOTE: this cannot be applied on the weapon itself as it would provide misleading info to the player
	// (turret range is limited)
	if (bCanArmorOverPenetrate)
	{
		M_Range = Range * DeveloperSettings::GameBalance::Weapons::RailGun::RailGunProjectileRangeMlt;
	}
	else
	{
		M_Range = Range * DeveloperSettings::GameBalance::Weapons::Projectiles::DefaultProjectileRangeMlt;
	}
	M_FullDamage = Damage;
	M_ArmorPen = ArmorPen;
	M_ArmorPenAtMaxRange = ArmorPenMaxRange;
	bM_CanArmorOverPenetrate = bCanArmorOverPenetrate;
	M_PostPenArmorPenCarryOver = FMath::Clamp(PostPenArmorPenCarryOver, 0.0f, 1.0f);
	M_FloorArmorPenPercentageNeededAllowOverpen = FMath::Clamp(FloorArmorPenPercentageNeededAllowOverpen, 0.0f, 1.0f);
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
	M_WeaponCalibre = WeaponCalibre;
	M_ProjectileTraceRadius = 0.0f;

	OnRestartProjectile(LaunchLocation, LaunchRotation, ProjectileSpeed);
	SetupNiagaraWithPrjVfxSettings(ProjectileVfxSettings);
	// Reset the amount of bounced allowed.
	M_MaxBounces = DeveloperSettings::GameBalance::Weapons::Projectiles::MaxBouncesPerProjectile;
	// Only notify on first bounce; reset as we have a new projectile fire.
	bM_AlreadyNotified = false;

	// Used to calculate armor pen at different ranges.
	M_ProjectileSpawn = GetActorLocation();
	SetupTraceChannel(OwningPlayer);

	StartFlightTimers(M_Range / ProjectileSpeed);
}


void AProjectile::SetupBarrageProjectileForNewLaunch(
	const FWeaponData& WeaponData,
	const FWeaponVFX& WeaponVfx,
	const FProjectileVfxSettings& ProjectileVfxSettings,
	const FBarrageProjectileMover& ProjectileMover,
	const int32 OwningPlayer,
	const FVector& LaunchLocation,
	const FRotator& LaunchRotation,
	const FVector& AimPoint,
	const TArray<AActor*>& ActorsToIgnore)
{
	const float SafeLinearSpeed = FMath::Max(ProjectileMover.ProjectileLinearSpeed, 1.0f);
	ApplyBarrageProjectileLaunchData(WeaponData, WeaponVfx, ActorsToIgnore, OwningPlayer);
	OnRestartProjectile(LaunchLocation, LaunchRotation, SafeLinearSpeed);
	M_ProjectileSpawn = GetActorLocation();
	SetupNiagaraWithPrjVfxSettings(ProjectileVfxSettings);
	SetupTraceChannel(OwningPlayer);
	SpawnBarrageLaunchEffects(WeaponVfx, LaunchLocation, LaunchRotation);
	StartFlightTimers(M_Range / SafeLinearSpeed);
	StartBarrageProjectileGuidance(LaunchLocation, AimPoint, ProjectileMover);
}

void AProjectile::ApplyBarrageProjectileLaunchData(
	const FWeaponData& WeaponData,
	const FWeaponVFX& WeaponVfx,
	const TArray<AActor*>& ActorsToIgnore,
	const int32 OwningPlayer)
{
	M_ShellType = WeaponData.ShellType;
	M_ProjectileOwner = nullptr;
	OnNewDamageType(WeaponData.DamageType);
	M_OwningPlayer = OwningPlayer;
	M_Range = WeaponData.Range * DeveloperSettings::GameBalance::Weapons::Projectiles::DefaultProjectileRangeMlt;
	M_FullDamage = WeaponData.BaseDamage;
	M_ArmorPen = WeaponData.ArmorPen;
	M_ArmorPenAtMaxRange = WeaponData.ArmorPenMaxRange;
	bM_CanArmorOverPenetrate = false;
	M_PostPenArmorPenCarryOver = 0.0f;
	M_FloorArmorPenPercentageNeededAllowOverpen = 0.0f;
	M_ShrapnelParticles = WeaponData.ShrapnelParticles;
	M_ShrapnelRange = WeaponData.ShrapnelRange;
	M_ShrapnelDamage = WeaponData.ShrapnelDamage;
	M_ShrapnelArmorPen = WeaponData.ShrapnelPen;
	M_ImpactVfx = WeaponVfx.SurfaceImpactEffects;
	M_ImpactScale = WeaponVfx.ImpactScale;
	M_BounceSound = WeaponVfx.BounceSound;
	M_BounceVfx = WeaponVfx.BounceEffect;
	M_BounceScale = WeaponVfx.BounceScale;
	M_ImpactAttenuation = WeaponVfx.ImpactAttenuation;
	M_ImpactConcurrency = WeaponVfx.ImpactConcurrency;
	M_ActorsToIgnore = ActorsToIgnore;
	M_WeaponCalibre = FMath::RoundToInt(WeaponData.WeaponCalibre);
	M_ProjectileTraceRadius = 0.0f;
	M_MaxBounces = DeveloperSettings::GameBalance::Weapons::Projectiles::MaxBouncesPerProjectile;
	bM_AlreadyNotified = false;
}

void AProjectile::SpawnBarrageLaunchEffects(
	const FWeaponVFX& WeaponVfx,
	const FVector& LaunchLocation,
	const FRotator& LaunchRotation) const
{
	if (IsValid(WeaponVfx.LaunchSound))
	{
		UGameplayStatics::SpawnSoundAtLocation(
			this,
			WeaponVfx.LaunchSound,
			LaunchLocation,
			LaunchRotation,
			1.0f,
			1.0f,
			0.0f,
			WeaponVfx.LaunchAttenuation,
			WeaponVfx.LaunchConcurrency);
	}
	if (not IsValid(WeaponVfx.LaunchEffectSettings.LaunchEffect))
	{
		return;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		WeaponVfx.LaunchEffectSettings.LaunchEffect,
		LaunchLocation,
		LaunchRotation,
		WeaponVfx.LaunchScale);
}

void AProjectile::StartBarrageProjectileGuidance(
	const FVector& LaunchLocation,
	const FVector& AimPoint,
	const FBarrageProjectileMover& ProjectileMover)
{
	M_BarrageProjectileRuntimeState.LaunchLocation = LaunchLocation;
	M_BarrageProjectileRuntimeState.AimPoint = AimPoint;
	M_BarrageProjectileRuntimeState.ProjectileMover = ProjectileMover;
	UpdateBarrageProjectileCourse();

	if (UWorld* World = GetWorld())
	{
		constexpr float BarrageUpdateIntervalSeconds = 0.05f;
		World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
		TWeakObjectPtr<AProjectile> WeakThis(this);
		World->GetTimerManager().SetTimer(
			M_BarrageProjectileTimerHandle,
			[WeakThis]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				WeakThis->UpdateBarrageProjectileCourse();
			},
			BarrageUpdateIntervalSeconds,
			true);
	}
}

void AProjectile::UpdateBarrageProjectileCourse()
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	const FVector LaunchLocation = M_BarrageProjectileRuntimeState.LaunchLocation;
	const FVector AimPoint = M_BarrageProjectileRuntimeState.AimPoint;
	const FVector LaunchToAim = AimPoint - LaunchLocation;
	const FVector CurrentFromLaunch = GetActorLocation() - LaunchLocation;
	const float PathLengthSquared = LaunchToAim.SizeSquared();
	if (PathLengthSquared <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float PathAlpha = FVector::DotProduct(CurrentFromLaunch, LaunchToAim) / PathLengthSquared;
	if (PathAlpha >= 1.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
		}
		return;
	}

	const float ClampedAlpha = FMath::Clamp(PathAlpha, 0.0f, 1.0f);
	const float ArcWeight = FMath::Sin(ClampedAlpha * PI);
	const float Speed = FMath::Lerp(
		M_BarrageProjectileRuntimeState.ProjectileMover.ProjectileLinearSpeed,
		M_BarrageProjectileRuntimeState.ProjectileMover.ProjectileArcSpeed,
		ArcWeight);
	const FVector StraightDirection = (AimPoint - GetActorLocation()).GetSafeNormal();
	const FVector ArcOffset = FVector::UpVector * M_BarrageProjectileRuntimeState.ProjectileMover.ArcStrength * ArcWeight;
	const FVector DesiredDirection = (StraightDirection + ArcOffset).GetSafeNormal();
	M_ProjectileMovement->Velocity = DesiredDirection * FMath::Max(Speed, 1.0f);
	SetActorRotation(M_ProjectileMovement->Velocity.Rotation());
}

void AProjectile::StartFlightTimers(const float ExpectedFlightTime)
{
	if (ExpectedFlightTime <= 0.0f)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
		World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
		World->GetTimerManager().ClearTimer(M_DescentSoundTimerHandle);

		ResetAsyncTraceState();

		World->GetTimerManager().SetTimer(M_ExplosionTimerHandle, this, &AProjectile::HandleTimedExplosion,
		                                  ExpectedFlightTime, false);
		const float LineTraceInterval = DeveloperSettings::Optimization::ProjectileTraceInterval;
		World->GetTimerManager().SetTimer(M_LineTraceTimerHandle, this, &AProjectile::PerformAsyncLineTrace,
		                                  LineTraceInterval, true);
		M_LastTraceTime = World->GetTimeSeconds() - LineTraceInterval;
		M_LastTraceLocation = GetActorLocation();
		bM_HasLastTraceLocation = true;
		PerformAsyncLineTrace();
	}
}

void AProjectile::SetupAttachedRocketMesh(UStaticMesh* RocketMesh)
{
	if (GetIsValidNiagara())
	{
		M_NiagaraComponent->SetVariableStaticMesh(*NiagaraRocketMeshName, RocketMesh);
	}
}

void AProjectile::PlayDescentSound(USoundBase* DescentSound,
                                   USoundAttenuation* DescentAttenuation,
                                   USoundConcurrency* DescentConcurrency)
{
	if (not DescentSound)
	{
		return;
	}

	if (UAudioComponent* DescentAudio = M_DescentAudioComponent.Get())
	{
		DescentAudio->Stop();
		DescentAudio->DestroyComponent();
		M_DescentAudioComponent.Reset();
	}

	if (USceneComponent* Root = GetRootComponent())
	{
		UAudioComponent* SpawnedAudio = UGameplayStatics::SpawnSoundAttached(
			DescentSound,
			Root,
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false,
			1.0f,
			1.0f,
			0.0f,
			DescentAttenuation,
			DescentConcurrency);
		M_DescentAudioComponent = SpawnedAudio;
		return;
	}

	PlaySoundAt(GetWorld(), DescentSound, GetActorLocation(), GetActorRotation(), DescentAttenuation,
	            DescentConcurrency);
}

bool AProjectile::CalculateArcedLaunchParameters(const FVector& LaunchLocation,
                                                 const FVector& TargetLocation,
                                                 const FArchProjectileSettings& ArchSettings,
                                                 FVector& OutLaunchVelocity,
                                                 FVector& OutApexLocation,
                                                 float& OutTimeToTarget,
                                                 FVector& OutHorizontalDirection,
                                                 float& OutGravity)
{
	const FVector LaunchToTarget = TargetLocation - LaunchLocation;
	const FVector FlatDelta(LaunchToTarget.X, LaunchToTarget.Y, 0.0f);
	const float HorizontalDistance = FlatDelta.Size();
	const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : 0.0f;
	if (HorizontalDistance <= KINDA_SMALL_NUMBER || FMath::IsNearlyZero(GravityZ))
	{
		return false;
	}
	const float Gravity = FMath::Abs(GravityZ);
	const float DesiredApexHeight = CalculateDesiredApexHeight(
		LaunchLocation,
		TargetLocation,
		HorizontalDistance,
		ArchSettings);
	if (DesiredApexHeight <= LaunchLocation.Z)
	{
		return false;
	}
	const float BaseVerticalVelocity = FMath::Sqrt(2.0f * Gravity * (DesiredApexHeight - LaunchLocation.Z));
	const float InitialVerticalVelocity = ApplyCurvatureToVerticalVelocity(
		BaseVerticalVelocity,
		ArchSettings.CurvatureVerticalVelocityMultiplier);
	const float DeltaZ = TargetLocation.Z - LaunchLocation.Z;
	const float Discriminant = (InitialVerticalVelocity * InitialVerticalVelocity) - (2.0f * Gravity * DeltaZ);
	if (Discriminant < 0.0f)
	{
		return false;
	}
	const float TimeToTarget = (InitialVerticalVelocity + FMath::Sqrt(Discriminant)) / Gravity;
	if (TimeToTarget <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutHorizontalDirection = FlatDelta.GetSafeNormal();
	const float HorizontalSpeed = HorizontalDistance / TimeToTarget;
	if (HorizontalSpeed <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutLaunchVelocity = (OutHorizontalDirection * HorizontalSpeed) + FVector(
		0.0f, 0.0f, InitialVerticalVelocity);
	OutGravity = Gravity;
	OutTimeToTarget = TimeToTarget;
	OutApexLocation = CalculateApexLocation(
		LaunchLocation,
		OutHorizontalDirection,
		HorizontalSpeed,
		InitialVerticalVelocity,
		Gravity);
	return true;
}

void AProjectile::SetupArcedLaunch(
	const FVector& LaunchLocation,
	const FVector& TargetLocation,
	const float ProjectileSpeed,
	const float Range,
	const FArchProjectileSettings& ArchSettings,
	USoundAttenuation* DescentAttenuation,
	USoundConcurrency* DescentConcurrency)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	StopDescentSound();

	const float SafeProjectileSpeed = FMath::Max(ProjectileSpeed, KINDA_SMALL_NUMBER);
	FVector LaunchVelocity = FVector::ZeroVector;
	FVector ApexLocation = FVector::ZeroVector;
	FVector HorizontalDirection = FVector::ZeroVector;
	float TimeToTarget = 0.0f;
	float Gravity = 0.0f;
	if (not CalculateArcedLaunchParameters(
		LaunchLocation,
		TargetLocation,
		ArchSettings,
		LaunchVelocity,
		ApexLocation,
		TimeToTarget,
		HorizontalDirection,
		Gravity))
	{
		LaunchStraightFallback(LaunchLocation, TargetLocation, SafeProjectileSpeed, Range, ArchSettings);
		return;
	}

	SetActorLocation(LaunchLocation);
	SetActorRotation(LaunchVelocity.Rotation());
	M_ProjectileMovement->Activate();
	M_ProjectileMovement->ProjectileGravityScale = 1.0f;
	M_ProjectileMovement->Velocity = LaunchVelocity;

	StartFlightTimers(TimeToTarget);

	ScheduleDescentSound(TimeToTarget, ArchSettings, DescentAttenuation, DescentConcurrency);

	if constexpr (DeveloperSettings::Debugging::GArchProjectile_Compile_DebugSymbols)
	{
		DebugDrawArcedLaunch(
			this,
			FArcedLaunchSolution{
				LaunchLocation,
				TargetLocation,
				ApexLocation,
				LaunchVelocity,
				TimeToTarget
			});
	}
}

void AProjectile::LaunchStraightFallback(const FVector& LaunchLocation,
                                         const FVector& TargetLocation,
                                         const float SafeProjectileSpeed,
                                         const float Range,
                                         const FArchProjectileSettings& ArchSettings)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	StopDescentSound();

	const FVector LaunchToTarget = TargetLocation - LaunchLocation;
	const FVector FlatDelta(LaunchToTarget.X, LaunchToTarget.Y, 0.0f);
	const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : 0.0f;
	const float Gravity = FMath::Abs(GravityZ);

	if (FlatDelta.IsNearlyZero() || FMath::IsNearlyZero(Gravity))
	{
		LaunchStraightFallbackDirect(LaunchLocation, LaunchToTarget, SafeProjectileSpeed, Range);
		return;
	}

	LaunchStraightFallbackStartAscent(
		LaunchLocation,
		TargetLocation,
		LaunchToTarget,
		FlatDelta,
		SafeProjectileSpeed,
		Range,
		Gravity,
		ArchSettings);
}

void AProjectile::SetupRocketSwingLaunch(const FVector& LaunchLocation,
                                         const FVector& TargetLocation,
                                         const float ProjectileSpeed,
                                         const FRocketWeaponSettings& RocketSettings)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	StopDescentSound();

	FRocketSwingLaunchSolution LaunchSolution;
	if (not BuildRocketSwingLaunchSolution(LaunchLocation, TargetLocation, ProjectileSpeed, RocketSettings,
	                                       LaunchSolution))
	{
		return;
	}

	SetActorLocation(LaunchLocation);
	M_ProjectileMovement->Activate();
	M_ProjectileMovement->ProjectileGravityScale = M_DefaultGravityScale;

	if (LaunchSolution.bUseStraightOnly)
	{
		if (LaunchSolution.CurveDirection.IsNearlyZero())
		{
			return;
		}
		SetActorRotation(LaunchSolution.CurveDirection.Rotation());
		M_ProjectileMovement->Velocity = LaunchSolution.CurveDirection * LaunchSolution.StraightSpeed;
		StartFlightTimers(LaunchSolution.StraightTime);
		return;
	}

	SetActorRotation(LaunchSolution.CurveDirection.Rotation());
	M_ProjectileMovement->Velocity = LaunchSolution.CurveDirection * LaunchSolution.CurveSpeed;

	StartFlightTimers(LaunchSolution.CurveTime + LaunchSolution.StraightTime);

	ScheduleRocketSwingTransition(TargetLocation, LaunchSolution.StraightSpeed, LaunchSolution.CurveTime);
}


void AProjectile::SetupVerticalRocketLaunch(const FVector& LaunchLocation,
                                            const FVector& ApexLocation,
                                            const FVector& TargetLocation,
                                            const float ProjectileSpeed,
                                            const FVerticalRocketWeaponSettings& VerticalRocketSettings)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	const float SafeProjectileSpeed = FMath::Max(ProjectileSpeed, KINDA_SMALL_NUMBER);
	const float Stage1Speed = SafeProjectileSpeed * FMath::Max(VerticalRocketSettings.Stage1SpeedMultiplier, 0.1f);
	const float Stage2ArcSpeed = SafeProjectileSpeed *
		FMath::Max(VerticalRocketSettings.Stage2ArcSpeedMultiplier, 0.1f);
	const float Stage2StraightSpeed = SafeProjectileSpeed * FMath::Max(
		VerticalRocketSettings.Stage2StraightSpeedMultiplier,
		0.1f);

	const FVector LaunchToApex = ApexLocation - LaunchLocation;
	if (LaunchToApex.IsNearlyZero())
	{
		return;
	}

	const float CurvatureAlpha = FMath::Clamp(VerticalRocketSettings.FirstStageBezierCurvature, 0.1f, 5.0f);
	FVector Stage1ControlPoint = FMath::Lerp(LaunchLocation, ApexLocation, 0.5f);
	Stage1ControlPoint += FVector(0.0f, 0.0f, FVector::Distance(LaunchLocation, ApexLocation) * 0.25f * CurvatureAlpha);
	FVector Stage1Direction = (Stage1ControlPoint - LaunchLocation).GetSafeNormal();
	if (Stage1Direction.IsNearlyZero())
	{
		Stage1Direction = LaunchToApex.GetSafeNormal();
	}

	const float Stage1Distance = FVector::Distance(LaunchLocation, Stage1ControlPoint) + FVector::Distance(
		Stage1ControlPoint,
		ApexLocation);
	const float Stage1Time = Stage1Distance / Stage1Speed;

	const float Stage2ArcDistanceSetting = FMath::Max(VerticalRocketSettings.Stage2ArcDistance, 0.0f);
	const FVector Stage2ArcHeightOffset = FVector(0.0f, 0.0f, VerticalRocketSettings.Stage2ArcHeight);
	const float Stage2ArcTime = (Stage2ArcDistanceSetting > KINDA_SMALL_NUMBER || FMath::Abs(
			                            VerticalRocketSettings.Stage2ArcHeight) >
		                            KINDA_SMALL_NUMBER)
		                            ? (FMath::Sqrt((Stage2ArcDistanceSetting * Stage2ArcDistanceSetting)
			                            + (VerticalRocketSettings.Stage2ArcHeight * VerticalRocketSettings.
				                            Stage2ArcHeight)) / Stage2ArcSpeed)
		                            : 0.0f;
	const float Stage2StraightTime = FVector::Distance(ApexLocation, TargetLocation) / Stage2StraightSpeed;

	SetActorLocation(LaunchLocation);
	SetActorRotation(Stage1Direction.Rotation());
	M_ProjectileMovement->Activate();
	M_ProjectileMovement->ProjectileGravityScale = 0.0f;
	M_ProjectileMovement->Velocity = Stage1Direction * Stage1Speed;
	StartFlightTimers(Stage1Time + Stage2ArcTime + Stage2StraightTime);

	ScheduleVerticalRocketTransitions(
		TargetLocation,
		Stage2ArcDistanceSetting,
		Stage2ArcHeightOffset,
		Stage2ArcSpeed,
		Stage2StraightSpeed,
		Stage1Time,
		Stage2ArcTime);
}

void AProjectile::TransitionVerticalRocketToArcOrStraight(const FVector& TargetLocation,
                                                          const float Stage2ArcDistanceSetting,
                                                          const FVector& Stage2ArcHeightOffset,
                                                          const float Stage2ArcSpeed,
                                                          const float Stage2StraightSpeed,
                                                          const float Stage2ArcTime)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	const FVector Stage2StartLocation = GetActorLocation();
	const FVector Stage2StartToTarget = TargetLocation - Stage2StartLocation;
	const FVector Stage2ArcAnchor = Stage2StartLocation
		+ (Stage2StartToTarget.GetSafeNormal() * Stage2ArcDistanceSetting)
		+ Stage2ArcHeightOffset;
	const FVector Stage2ArcDirection = (Stage2ArcAnchor - Stage2StartLocation).GetSafeNormal();
	if (Stage2ArcDirection.IsNearlyZero() || Stage2ArcTime <= 0.0f)
	{
		TransitionVerticalRocketToStraight(TargetLocation, Stage2StraightSpeed);
		return;
	}

	SetActorRotation(Stage2ArcDirection.Rotation());
	M_ProjectileMovement->Velocity = Stage2ArcDirection * Stage2ArcSpeed;
}

void AProjectile::TransitionVerticalRocketToStraight(const FVector& TargetLocation, const float Stage2StraightSpeed)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	const FVector StraightDirection = (TargetLocation - GetActorLocation()).GetSafeNormal();
	if (StraightDirection.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(StraightDirection.Rotation());
	M_ProjectileMovement->Velocity = StraightDirection * Stage2StraightSpeed;
}

void AProjectile::SetupHomingMissileLaunch(const FVector& LaunchLocation,
                                           AActor* TargetActor,
                                           const FVector& FallbackTargetLocation,
                                           const float ProjectileSpeed,
                                           const FHomingMissileWeaponSettings& HomingSettings)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	constexpr int32 MotionTypeCount = 3;
	M_HomingMissileRuntimeState.M_Settings = HomingSettings;
	M_HomingMissileRuntimeState.M_MotionType = static_cast<EHomingMissileMotionType>(
		FMath::RandRange(0, MotionTypeCount - 1));
	M_HomingMissileRuntimeState.M_Target = TargetActor;
	M_HomingMissileRuntimeState.M_FallbackTargetLocation = FallbackTargetLocation;
	M_HomingMissileRuntimeState.M_Speed = FMath::Max(
		ProjectileSpeed * HomingSettings.HomingSpeedMultiplier,
		KINDA_SMALL_NUMBER);
	M_HomingMissileRuntimeState.M_LaunchLocation = LaunchLocation;
	M_HomingMissileRuntimeState.M_ElapsedSeconds = 0.0f;
	M_HomingMissileRuntimeState.bM_UseDirectHoming = false;

	const FVector InitialTargetLocation = IsValid(TargetActor)
		                                      ? TargetActor->GetActorLocation()
		                                      : FallbackTargetLocation;
	const float InitialTargetDistance = FVector::Distance(LaunchLocation, InitialTargetLocation);
	M_HomingMissileRuntimeState.M_ExpectedFlightSeconds = InitialTargetDistance / M_HomingMissileRuntimeState.M_Speed;
	PrepareDirectHomingSwitchThreshold();

	const FVector InitialDirection = (InitialTargetLocation - LaunchLocation).GetSafeNormal();
	if (InitialDirection.IsNearlyZero())
	{
		return;
	}

	M_HomingMissileRuntimeState.M_BezierControlPoint = LaunchLocation
		+ InitialDirection * HomingSettings.BezierSettings.ControlPointForwardDistance
		+ FVector::CrossProduct(InitialDirection, FVector::UpVector).GetSafeNormal()
		* HomingSettings.BezierSettings.ControlPointSideOffset
		+ FVector::UpVector * HomingSettings.BezierSettings.ControlPointUpOffset;

	SetActorLocation(LaunchLocation);
	SetActorRotation(InitialDirection.Rotation());
	M_ProjectileMovement->Activate();
	M_ProjectileMovement->ProjectileGravityScale = 0.0f;
	M_ProjectileMovement->Velocity = InitialDirection * M_HomingMissileRuntimeState.M_Speed;
	constexpr float MinimumHomingMissileTraceRadius = 12.0f;
	constexpr float MillimetersToCentimeters = 0.1f;
	M_ProjectileTraceRadius = FMath::Max(
		MinimumHomingMissileTraceRadius,
		static_cast<float>(M_WeaponCalibre) * MillimetersToCentimeters);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float HomingMissileMaximumFlightSeconds = M_Range / M_HomingMissileRuntimeState.M_Speed;
	StartFlightTimers(FMath::Max(
		M_HomingMissileRuntimeState.M_ExpectedFlightSeconds,
		HomingMissileMaximumFlightSeconds));

	World->GetTimerManager().SetTimer(
		M_HomingMissileTimerHandle,
		this,
		&AProjectile::UpdateHomingMissileCourse,
		GetHomingMissileTimerIntervalSeconds(),
		true);
}

void AProjectile::UpdateHomingMissileCourse()
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	M_HomingMissileRuntimeState.M_ElapsedSeconds += GetHomingMissileTimerIntervalSeconds();
	AActor* HomingTarget = M_HomingMissileRuntimeState.M_Target.Get();
	const FVector TargetLocation = IsValid(HomingTarget)
		                               ? HomingTarget->GetActorLocation()
		                               : M_HomingMissileRuntimeState.M_FallbackTargetLocation;
	TrySwitchHomingMissileToDirectHoming(TargetLocation);
	const FVector DesiredDirection = BuildHomingMissileDesiredDirection(TargetLocation);
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	FVector NewDirection = DesiredDirection;
	if (not M_HomingMissileRuntimeState.bM_UseDirectHoming)
	{
		const FVector CurrentDirection = M_ProjectileMovement->Velocity.GetSafeNormal();
		NewDirection = FMath::Lerp(
			CurrentDirection,
			DesiredDirection,
			FMath::Clamp(M_HomingMissileRuntimeState.M_Settings.TurnResponsiveness, 0.0f, 1.0f)).GetSafeNormal();
	}

	SetActorRotation(NewDirection.Rotation());
	M_ProjectileMovement->Velocity = NewDirection * M_HomingMissileRuntimeState.M_Speed;
}

void AProjectile::PrepareDirectHomingSwitchThreshold()
{
	if (not GetCanHomingMissileSwitchToDirectHoming())
	{
		M_HomingMissileRuntimeState.M_DirectHomingSwitchPathAlpha = 1.0f;
		return;
	}

	constexpr float MidpointPathAlpha = 0.5f;
	constexpr float PercentToAlpha = 0.01f;
	const float WindowAlpha = FMath::Clamp(
		M_HomingMissileRuntimeState.M_Settings.DirectHomingSwitchMidpointWindowPercent,
		0.0f,
		50.0f) * PercentToAlpha;
	M_HomingMissileRuntimeState.M_DirectHomingSwitchPathAlpha = FMath::FRandRange(
		MidpointPathAlpha - WindowAlpha,
		MidpointPathAlpha + WindowAlpha);
}

void AProjectile::TrySwitchHomingMissileToDirectHoming(const FVector& TargetLocation)
{
	if (M_HomingMissileRuntimeState.bM_UseDirectHoming)
	{
		return;
	}

	if (not GetCanHomingMissileSwitchToDirectHoming())
	{
		return;
	}

	const float SafeExpectedFlightSeconds = FMath::Max(
		M_HomingMissileRuntimeState.M_ExpectedFlightSeconds,
		KINDA_SMALL_NUMBER);
	const float CurrentPathAlpha = M_HomingMissileRuntimeState.M_ElapsedSeconds / SafeExpectedFlightSeconds;
	if (CurrentPathAlpha < M_HomingMissileRuntimeState.M_DirectHomingSwitchPathAlpha)
	{
		return;
	}

	if (not GetIsHomingMissileTooFarOff(TargetLocation))
	{
		return;
	}

	M_HomingMissileRuntimeState.bM_UseDirectHoming = true;
}

bool AProjectile::GetCanHomingMissileSwitchToDirectHoming() const
{
	return M_HomingMissileRuntimeState.M_MotionType == EHomingMissileMotionType::Bezier
		|| M_HomingMissileRuntimeState.M_MotionType == EHomingMissileMotionType::Spherical;
}

bool AProjectile::GetIsHomingMissileTooFarOff(const FVector& TargetLocation) const
{
	const float RemainingFlightSeconds = FMath::Max(
		M_HomingMissileRuntimeState.M_ExpectedFlightSeconds - M_HomingMissileRuntimeState.M_ElapsedSeconds,
		0.0f);
	const float ExpectedRemainingDistance = M_HomingMissileRuntimeState.M_Speed * RemainingFlightSeconds;
	return FVector::DistSquared(GetActorLocation(), TargetLocation) > FMath::Square(ExpectedRemainingDistance);
}

FVector AProjectile::BuildHomingMissileDesiredDirection(const FVector& TargetLocation) const
{
	const FVector ToTarget = (TargetLocation - GetActorLocation()).GetSafeNormal();
	if (M_HomingMissileRuntimeState.bM_UseDirectHoming)
	{
		return ToTarget;
	}

	if (M_HomingMissileRuntimeState.M_MotionType == EHomingMissileMotionType::Bezier)
	{
		return BuildBezierHomingDesiredDirection(TargetLocation);
	}

	if (M_HomingMissileRuntimeState.M_MotionType == EHomingMissileMotionType::Wave)
	{
		const FVector SideVector = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
		const float WaveOffset = FMath::Sin(
			                         M_HomingMissileRuntimeState.M_ElapsedSeconds
			                         * M_HomingMissileRuntimeState.M_Settings.WaveSettings.Frequency)
			* M_HomingMissileRuntimeState.M_Settings.WaveSettings.Amplitude;
		const FVector WaveTargetLocation = TargetLocation + SideVector * WaveOffset;
		const FVector WaveDirection = (WaveTargetLocation - GetActorLocation()).GetSafeNormal();
		return FMath::Lerp(
			WaveDirection,
			ToTarget,
			M_HomingMissileRuntimeState.M_Settings.WaveSettings.ForwardBlend).GetSafeNormal();
	}

	const FVector OrbitAxis = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
	const float AngleRadians = FMath::DegreesToRadians(
		M_HomingMissileRuntimeState.M_ElapsedSeconds
		* M_HomingMissileRuntimeState.M_Settings.SphericalSettings.OrbitSpeedDegrees);
	const FVector OrbitOffset = (OrbitAxis * FMath::Cos(AngleRadians) + FVector::UpVector * FMath::Sin(AngleRadians))
		* M_HomingMissileRuntimeState.M_Settings.SphericalSettings.OrbitRadius;
	const FVector OrbitTargetLocation = TargetLocation + OrbitOffset;
	const FVector OrbitDirection = (OrbitTargetLocation - GetActorLocation()).GetSafeNormal();
	return FMath::Lerp(
		OrbitDirection,
		ToTarget,
		M_HomingMissileRuntimeState.M_Settings.SphericalSettings.ForwardBlend).GetSafeNormal();
}

FVector AProjectile::BuildBezierHomingDesiredDirection(const FVector& TargetLocation) const
{
	constexpr float BezierLookAheadAlpha = 0.08f;
	const float SafeExpectedFlightSeconds = FMath::Max(
		M_HomingMissileRuntimeState.M_ExpectedFlightSeconds,
		KINDA_SMALL_NUMBER);
	const float PathAlpha = FMath::Clamp(
		(M_HomingMissileRuntimeState.M_ElapsedSeconds / SafeExpectedFlightSeconds) + BezierLookAheadAlpha,
		0.0f,
		1.0f);
	const FVector FirstSegmentPoint = FMath::Lerp(
		M_HomingMissileRuntimeState.M_LaunchLocation,
		M_HomingMissileRuntimeState.M_BezierControlPoint,
		PathAlpha);
	const FVector SecondSegmentPoint = FMath::Lerp(
		M_HomingMissileRuntimeState.M_BezierControlPoint,
		TargetLocation,
		PathAlpha);
	const FVector DesiredPathPoint = FMath::Lerp(FirstSegmentPoint, SecondSegmentPoint, PathAlpha);
	return (DesiredPathPoint - GetActorLocation()).GetSafeNormal();
}

float AProjectile::GetHomingMissileTimerIntervalSeconds() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return KINDA_SMALL_NUMBER;
	}

	constexpr float MinimumExpectedFrameSeconds = 1.0f / 120.0f;
	const float SafeFrameSeconds = FMath::Max(World->GetDeltaSeconds(), MinimumExpectedFrameSeconds);
	return SafeFrameSeconds * FMath::Max(M_HomingMissileRuntimeState.M_Settings.RetargetEveryTicks, 1);
}

void AProjectile::ScheduleVerticalRocketTransitions(const FVector& TargetLocation,
                                                    const float Stage2ArcDistanceSetting,
                                                    const FVector& Stage2ArcHeightOffset,
                                                    const float Stage2ArcSpeed,
                                                    const float Stage2StraightSpeed,
                                                    const float Stage1Time,
                                                    const float Stage2ArcTime)
{
	if (Stage1Time <= 0.0f)
	{
		TransitionVerticalRocketToArcOrStraight(
			TargetLocation,
			Stage2ArcDistanceSetting,
			Stage2ArcHeightOffset,
			Stage2ArcSpeed,
			Stage2StraightSpeed,
			Stage2ArcTime);
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
	World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
	World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
	World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
	const TWeakObjectPtr<AProjectile> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_RocketSwingTimerHandle,
		[WeakThis, TargetLocation, Stage2ArcDistanceSetting, Stage2ArcHeightOffset, Stage2ArcSpeed,
			Stage2StraightSpeed, Stage2ArcTime]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->TransitionVerticalRocketToArcOrStraight(
				TargetLocation,
				Stage2ArcDistanceSetting,
				Stage2ArcHeightOffset,
				Stage2ArcSpeed,
				Stage2StraightSpeed,
				Stage2ArcTime);
		},
		Stage1Time,
		false);

	if (Stage2ArcTime <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_VerticalRocketStraightTimerHandle,
		[WeakThis, TargetLocation, Stage2StraightSpeed]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->TransitionVerticalRocketToStraight(TargetLocation, Stage2StraightSpeed);
		},
		Stage1Time + Stage2ArcTime,
		false);
}

void AProjectile::LaunchStraightFallbackStartAscent(const FVector& LaunchLocation,
                                                    const FVector& TargetLocation,
                                                    const FVector& LaunchToTarget,
                                                    const FVector& FlatDelta,
                                                    const float SafeProjectileSpeed,
                                                    const float Range,
                                                    const float Gravity,
                                                    const FArchProjectileSettings& ArchSettings)
{
	FVector LaunchVelocity = FVector::ZeroVector;
	FVector ApexLocation = FVector::ZeroVector;
	float TimeToApex = 0.0f;
	float TotalFlightTime = 0.0f;
	if (not CalculateFallbackArcParameters(
		LaunchLocation,
		TargetLocation,
		LaunchToTarget,
		FlatDelta,
		SafeProjectileSpeed,
		Range,
		Gravity,
		ArchSettings,
		LaunchVelocity,
		ApexLocation,
		TimeToApex,
		TotalFlightTime))
	{
		return;
	}

	SetActorLocation(LaunchLocation);
	SetActorRotation(LaunchVelocity.Rotation());
	M_ProjectileMovement->Activate();
	M_ProjectileMovement->ProjectileGravityScale = 1.0f;
	M_ProjectileMovement->Velocity = LaunchVelocity;

	StartFlightTimers(TotalFlightTime);

	if constexpr (DeveloperSettings::Debugging::GArchProjectile_Compile_DebugSymbols)
	{
		DebugDrawArcedLaunch(
			this,
			FArcedLaunchSolution{
				LaunchLocation,
				TargetLocation,
				ApexLocation,
				LaunchVelocity,
				TotalFlightTime
			});
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
		TWeakObjectPtr<AProjectile> WeakThis(this);
		World->GetTimerManager().SetTimer(
			M_ArcFallbackTimerHandle,
			[WeakThis, TargetLocation, SafeProjectileSpeed]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				WeakThis->TransitionArcFallbackToStraight(TargetLocation, SafeProjectileSpeed);
			},
			TimeToApex,
			false);
	}
}

bool AProjectile::CalculateFallbackArcParameters(const FVector& LaunchLocation,
                                                 const FVector& TargetLocation,
                                                 const FVector& LaunchToTarget,
                                                 const FVector& FlatDelta,
                                                 const float SafeProjectileSpeed,
                                                 const float Range,
                                                 const float Gravity,
                                                 const FArchProjectileSettings& ArchSettings,
                                                 FVector& OutLaunchVelocity,
                                                 FVector& OutApexLocation,
                                                 float& OutTimeToApex,
                                                 float& OutTotalFlightTime)
{
	const float HorizontalDistance = FlatDelta.Size();
	const float DesiredApexHeight = CalculateDesiredApexHeight(
		LaunchLocation,
		TargetLocation,
		HorizontalDistance,
		ArchSettings);
	if (DesiredApexHeight <= LaunchLocation.Z)
	{
		LaunchStraightFallbackDirect(LaunchLocation, LaunchToTarget, SafeProjectileSpeed, Range);
		return false;
	}

	const float VerticalVelocity = FMath::Sqrt(2.0f * Gravity * (DesiredApexHeight - LaunchLocation.Z));
	if (VerticalVelocity <= KINDA_SMALL_NUMBER)
	{
		LaunchStraightFallbackDirect(LaunchLocation, LaunchToTarget, SafeProjectileSpeed, Range);
		return false;
	}

	OutTimeToApex = VerticalVelocity / Gravity;
	const FVector HorizontalDirection = FlatDelta.GetSafeNormal();
	const float DesiredHorizontalSpeed = (HorizontalDistance * 0.5f) / FMath::Max(OutTimeToApex, KINDA_SMALL_NUMBER);
	const float HorizontalSpeed = FMath::Clamp(DesiredHorizontalSpeed, KINDA_SMALL_NUMBER, SafeProjectileSpeed);
	OutLaunchVelocity = (HorizontalDirection * HorizontalSpeed) + FVector(0.0f, 0.0f, VerticalVelocity);
	OutApexLocation = CalculateApexLocation(
		LaunchLocation,
		HorizontalDirection,
		HorizontalSpeed,
		VerticalVelocity,
		Gravity);
	const float StraightDistance = FVector::Dist(OutApexLocation, TargetLocation);
	const float StraightTime = StraightDistance / SafeProjectileSpeed;
	OutTotalFlightTime = OutTimeToApex + StraightTime;
	return true;
}

void AProjectile::LaunchStraightFallbackDirect(const FVector& LaunchLocation,
                                               const FVector& LaunchToTarget,
                                               const float SafeProjectileSpeed,
                                               const float Range)
{
	OnRestartProjectile(LaunchLocation, LaunchToTarget.Rotation(), SafeProjectileSpeed);
	StartFlightTimers(Range / SafeProjectileSpeed);
}

void AProjectile::TransitionArcFallbackToStraight(const FVector TargetLocation, const float SafeProjectileSpeed)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	M_ProjectileMovement->ProjectileGravityScale = 0.0f;
	M_ProjectileMovement->Velocity = Direction * SafeProjectileSpeed;
	SetActorRotation(Direction.Rotation());
}

void AProjectile::TransitionRocketSwingToStraight(const FVector& TargetLocation, const float StraightSpeed)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
	World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	M_ProjectileMovement->ProjectileGravityScale = M_DefaultGravityScale;
	M_ProjectileMovement->Velocity = Direction * StraightSpeed;
	SetActorRotation(Direction.Rotation());
}

void AProjectile::ScheduleRocketSwingTransition(const FVector& TargetLocation,
                                                const float StraightSpeed,
                                                const float CurveTime)
{
	if (CurveTime <= 0.0f)
	{
		TransitionRocketSwingToStraight(TargetLocation, StraightSpeed);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
		World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
		TWeakObjectPtr<AProjectile> WeakThis(this);
		World->GetTimerManager().SetTimer(
			M_RocketSwingTimerHandle,
			[WeakThis, TargetLocation, StraightSpeed]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				WeakThis->TransitionRocketSwingToStraight(TargetLocation, StraightSpeed);
			},
			CurveTime,
			false);
	}
}

void AProjectile::StopDescentSound()
{
	if (UAudioComponent* DescentAudio = M_DescentAudioComponent.Get())
	{
		DescentAudio->Stop();
		DescentAudio->DestroyComponent();
		M_DescentAudioComponent.Reset();
	}
}

float AProjectile::CalculateDesiredApexHeight(const FVector& LaunchLocation,
                                              const FVector& TargetLocation,
                                              const float HorizontalDistance,
                                              const FArchProjectileSettings& ArchSettings) const
{
	const float HighestPoint = FMath::Max(LaunchLocation.Z, TargetLocation.Z);
	const float BaseApexHeight = HighestPoint + (HorizontalDistance * ArchSettings.ApexHeightMultiplier) +
		ArchSettings.ApexHeightOffset;
	const float MinimumHeight = HighestPoint + ArchSettings.MinApexOffset;
	return FMath::Max(BaseApexHeight, MinimumHeight);
}

float AProjectile::ApplyCurvatureToVerticalVelocity(const float BaseVerticalVelocity,
                                                    const float CurvatureVerticalVelocityMultiplier) const
{
	const float ClampedMultiplier = FMath::Clamp(
		CurvatureVerticalVelocityMultiplier,
		MinCurvatureVelocityMultiplier,
		MaxCurvatureVelocityMultiplier);
	return BaseVerticalVelocity * ClampedMultiplier;
}

void AProjectile::ScheduleDescentSound(const float TimeToTarget,
                                       const FArchProjectileSettings& ArchSettings,
                                       USoundAttenuation* DescentAttenuation,
                                       USoundConcurrency* DescentConcurrency)
{
	if (TimeToTarget <= 0.0f || not ArchSettings.DescentSound)
	{
		return;
	}

	const float DescentSoundDuration = ArchSettings.DescentSound->GetDuration();
	if (DescentSoundDuration <= 0.0f)
	{
		return;
	}

	const float LeadTime = FMath::Max(TimeToTarget - DescentSoundDuration, 0.0f);
	if (LeadTime <= KINDA_SMALL_NUMBER)
	{
		PlayDescentSound(ArchSettings.DescentSound, DescentAttenuation, DescentConcurrency);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate DescentDelegate;
		TWeakObjectPtr<AProjectile> WeakThis(this);
		DescentDelegate.BindLambda([WeakThis, ArchSettings, DescentAttenuation, DescentConcurrency]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->PlayDescentSound(ArchSettings.DescentSound, DescentAttenuation, DescentConcurrency);
		});
		World->GetTimerManager().SetTimer(M_DescentSoundTimerHandle, DescentDelegate, LeadTime, false);
	}
}

void AProjectile::ApplyAccelerationFactor(const float Factor, const bool bUseGravityTArch, const float LowestZValue)
{
	if (not GetIsValidProjectileMovement())
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
	SpawnExplosionHandleAOE(GetActorLocation(), Rotation, ERTSSurfaceType::Air, nullptr);

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
		World->GetTimerManager().ClearTimer(M_DescentSoundTimerHandle);
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
	World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
	}
	ResetAsyncTraceState();
	StopDescentSound();
}

void AProjectile::BeginDestroy()
{
	Super::BeginDestroy();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
		World->GetTimerManager().ClearTimer(M_DescentSoundTimerHandle);
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
	World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
	}
	ResetAsyncTraceState();
	StopDescentSound();
}

void AProjectile::OnHitActor(
	AActor* HitActor,
	const FVector& HitLocation,
	const FRotator& HitRotation, const ERTSSurfaceType HitSurface, const float DamageMlt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PrjWp_OnHitActor);
	if (RTSFunctionLibrary::RTSIsValid(HitActor))
	{
		M_DamageEvent.HitInfo.Location = HitLocation;
		if (HitActor->TakeDamage(M_FullDamage * DamageMlt, M_DamageEvent, nullptr, this) == 0 && M_ProjectileOwner)
		{
			M_ProjectileOwner->OnProjectileKilledActor(HitActor);
		}
		ApplyRadixiteDamageBehaviour(HitActor);
		He_Heat_AttemptStunOnPen(HitActor);
		SpawnExplosionHandleAOE(HitLocation, HitRotation, HitSurface, HitActor);
		OnProjectileDormant();
	}
}

void AProjectile::DamageActorWithShrapnel(AActor* HitActor)
{
	HitActor->TakeDamage(M_ShrapnelDamage, M_DamageEvent, nullptr, this);
}

void AProjectile::ApplyRadixiteDamageBehaviour(AActor* HitActor)
{
	if (M_ShellType != EWeaponShellType::Shell_Radixite)
	{
		return;
	}
	if (not IsValid(HitActor))
	{
		return;
	}

	UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		return;
	}

	const float RadixiteDurationSeconds = GetRadixiteDamageDurationSeconds();
	BehaviourComponent->AddBehaviourWithDuration(URadixiteDamageBehaviour::StaticClass(), RadixiteDurationSeconds);
}

float AProjectile::GetRadixiteDamageDurationSeconds() const
{
	const float WeaponCalibre = static_cast<float>(M_WeaponCalibre);
	const float ClampedCalibre = FMath::Clamp(WeaponCalibre,
	                                          RadixiteDamageConstants::MinCalibreMm,
	                                          RadixiteDamageConstants::MaxCalibreMm);
	const float InterpolationFactor = (ClampedCalibre - RadixiteDamageConstants::MinCalibreMm)
		/ (RadixiteDamageConstants::MaxCalibreMm - RadixiteDamageConstants::MinCalibreMm);
	return FMath::Lerp(RadixiteDamageConstants::MinDurationSeconds,
	                   RadixiteDamageConstants::MaxDurationSeconds,
	                   InterpolationFactor);
}

bool AProjectile::GetIsValidWidgetPoolManager() const
{
	if (not M_AnimatedTextWidgetPoolManager.IsValid())
	{
		RTSFunctionLibrary::ReportError("Projectile attempted to get widget pool manager but it is invalid!"
			"\n projectile: " + GetName());
		return false;
	}
	return true;
}

void AProjectile::PostInit_SetupWidgetPoolManager()
{
	M_AnimatedTextWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
}


void AProjectile::SetProjectileDormant()
{
	OnProjectileDormant();
}

void AProjectile::OnProjectileDormant()
{
	ResetAsyncTraceState();
	if (not GetIsValidProjectileMovement() || not GetIsValidProjectileManager())
	{
		return;
	}
	SetActorHiddenInGame(true);
	M_ProjectileMovement->Velocity = FVector::ZeroVector;
	M_ProjectileMovement->Deactivate();
	M_ProjectileMovement->ProjectileGravityScale = M_DefaultGravityScale;
	StopDescentSound();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
		World->GetTimerManager().ClearTimer(M_DescentSoundTimerHandle);
		World->GetTimerManager().ClearTimer(M_ArcFallbackTimerHandle);
		World->GetTimerManager().ClearTimer(M_RocketSwingTimerHandle);
		World->GetTimerManager().ClearTimer(M_VerticalRocketStraightTimerHandle);
		World->GetTimerManager().ClearTimer(M_HomingMissileTimerHandle);
	World->GetTimerManager().ClearTimer(M_BarrageProjectileTimerHandle);
	}
	M_ProjectilePoolSettings.ProjectileManager->OnTankProjectileDormant(M_ProjectilePoolSettings.ProjectileIndex);
}

void AProjectile::OnRestartProjectile(const FVector& NewLocation, const FRotator& LaunchRotation,
                                      const float ProjectileSpeed)
{
	StopDescentSound();
	SetActorHiddenInGame(false);
	// Move projectile to new location.
	SetActorLocation(NewLocation);
	SetActorRotation(LaunchRotation);
	if (GetIsValidProjectileMovement())
	{
		M_ProjectileMovement->Activate();
		M_ProjectileMovement->ProjectileGravityScale = M_DefaultGravityScale;
		const FVector Direction = LaunchRotation.Vector();
		M_ProjectileMovement->Velocity = Direction * ProjectileSpeed;
		DebugProjectile("Projectile speed: " + FString::SanitizeFloat(ProjectileSpeed));
	}

	M_LastTraceLocation = NewLocation;
	bM_HasLastTraceLocation = true;
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
	if (not GetIsValidProjectileMovement())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	if (bM_TraceInFlight)
	{
		return;
	}

	if (not bM_HasLastTraceLocation)
	{
		M_LastTraceLocation = GetActorLocation();
		bM_HasLastTraceLocation = true;
		return;
	}

	// Sample game time; respect dilation.
	const double Now = World->GetTimeSeconds();
	M_LastTraceTime = Now;

	const FVector StartLocation = M_LastTraceLocation;
	const FVector CurrentLocation = GetActorLocation();
	const FVector Dir = M_ProjectileMovement->Velocity.GetSafeNormal();
	constexpr float ForwardTracePaddingUnits = 10.0f;
	const FVector EndLocation = CurrentLocation + (Dir * ForwardTracePaddingUnits);
	M_LastTraceLocation = CurrentLocation;

	FCollisionQueryParams TraceParams(FName(TEXT("ProjectileTrace")), false, this);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;
	TraceParams.AddIgnoredActors(M_ActorsToIgnore);

	const int32 TraceRequestId = ++M_TraceRequestId;
	bM_TraceInFlight = true;
	const TWeakObjectPtr<AProjectile> WeakThis(this);
	FTraceDelegate TraceDelegate;
	TraceDelegate.BindLambda(
		[WeakThis, TraceRequestId](const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->OnAsyncTraceComplete(TraceHandle, TraceDatum, TraceRequestId);
		});

	if (M_ProjectileTraceRadius > 0.0f)
	{
		World->AsyncSweepByChannel(
			EAsyncTraceType::Single,
			StartLocation,
			EndLocation,
			FQuat::Identity,
			M_TraceChannel,
			FCollisionShape::MakeSphere(M_ProjectileTraceRadius),
			TraceParams,
			FCollisionResponseParams::DefaultResponseParam,
			&TraceDelegate
		);
		return;
	}

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		StartLocation,
		EndLocation,
		M_TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void AProjectile::ResetAsyncTraceState()
{
	++M_TraceRequestId;
	bM_TraceInFlight = false;
	bM_HasLastTraceLocation = false;
	M_LastTraceLocation = FVector::ZeroVector;
}

void AProjectile::OnAsyncTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum,
                                       const int32 TraceRequestId)
{
	if (TraceRequestId != M_TraceRequestId)
	{
		return;
	}

	bM_TraceInFlight = false;
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
		MakeImpactOutwardRotationZ(HitResult), 1.f);
}


float AProjectile::CalculateImpactAngle(const FVector& Velocity, const FVector& ImpactNormal) const
{
	FVector ImpactDirection = Velocity.GetSafeNormal();
	float DotProduct = FVector::DotProduct(ImpactDirection, ImpactNormal);
	float ClampedDot = FMath::Clamp(DotProduct, -1.0f, 1.0f); // Prevent NaN from Acos
	float ImpactAngleRadians = FMath::Acos(ClampedDot);
	return FMath::RadiansToDegrees(ImpactAngleRadians);
}

void AProjectile::HandleProjectileBounce(const FHitResult& HitResult, const EArmorPlate PlateHit, AActor* HitActor,
                                         const FRotator& HitRotation)
{
	if (not GetIsValidProjectileMovement())
	{
		return;
	}
	if (OnBounce_HandleHeHeatModuleDamage(HitResult.Location, PlateHit, HitActor, HitRotation))
	{
		// The HE or HEAT module damage already spawned explosion, damaged actor and set to dormant.
		return;
	}
	ProjectileHitPropagateNotification(true);

	// Reflect the velocity based on the hit normal
	const FVector ReflectedVelocity = FVector::VectorPlaneProject(M_ProjectileMovement->Velocity,
	                                                              HitResult.ImpactNormal).
		GetSafeNormal();
	const float VelocityScale = M_ProjectileMovement->Velocity.Size();

	// Update projectile's position and rotation
	SetActorLocation(HitResult.ImpactPoint);
	SetActorRotation(ReflectedVelocity.Rotation());
	M_ProjectileMovement->Velocity = ReflectedVelocity * VelocityScale;

	// Spawn bounce effects
	SpawnBounce(HitResult.ImpactPoint, ReflectedVelocity.Rotation());

	// Reduce armor penetration
	const float Divider = DeveloperSettings::GameBalance::Weapons::Projectiles::AmorPenBounceDivider;
	M_ArmorPen /= Divider;
	M_ArmorPenAtMaxRange /= Divider;

	// Decrement remaining bounces
	M_MaxBounces--;
	if (M_MaxBounces <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
	}
}

void AProjectile::HandleHitActorAndClearTimer(AActor* HitActor, const FVector& HitLocation,
                                              const ERTSSurfaceType HitSurface, const FRotator& HitRotation,
                                              const float DamageMlt)
{
	OnHitActor(HitActor, HitLocation, HitRotation, HitSurface, DamageMlt);
	GetWorld()->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
}


void AProjectile::SpawnExplosionHandleAOE(const FVector& Location, const FRotator& HitRotation,
                                          const ERTSSurfaceType HitSurface, AActor* HitActor)
{
	if (GetIsValidCameraShakeSubsystem())
	{
		FRTSCameraShakeRequest ShakeRequest;
		ShakeRequest.M_EventType = ERTSCameraShakeEventType::Explosion;
		ShakeRequest.M_CalibreMm = M_WeaponCalibre;
		ShakeRequest.M_WorldLocation = Location;
		M_CameraShakeSubsystem->RequestExplosionShake(ShakeRequest);
	}

	// Resolve impact data once.
	const FRTSSurfaceImpactData* const Data = FindImpactData(M_ImpactVfx, HitSurface);
	if (!Data)
	{
		ReportMissingSurface(this, HitSurface);
		return;
	}
	HandleAoe(Location, HitActor);

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
	const bool bValidOwner = IsValid(M_ProjectileOwner);
	if (bValidOwner)
	{
		M_ProjectileOwner->CreateWeaponNonPenVfx(Location, BounceDirection, false);
	}

	if (UWorld* World = GetWorld())
	{
		if (not bValidOwner)
		{
			PlayNiagaraAt(World, M_BounceVfx, Location, BounceDirection, M_BounceScale);
		}
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

bool AProjectile::GetCanArmorOverPenetrate(const float EffectiveArmor, const float AdjustedArmorPen) const
{
	if (not bM_CanArmorOverPenetrate || AdjustedArmorPen <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ArmorPenPercentageNeededToPenetrate = EffectiveArmor / AdjustedArmorPen;
	return ArmorPenPercentageNeededToPenetrate <= M_FloorArmorPenPercentageNeededAllowOverpen;
}

void AProjectile::OnOverPenetratingArmorHit(AActor* HitActor, const FHitResult& HitResult,
                                            const ERTSSurfaceType HitSurface,
                                            const FRotator& HitRotation, const float DamageMlt)
{
	if (not RTSFunctionLibrary::RTSIsValid(HitActor))
	{
		return;
	}

	M_DamageEvent.HitInfo.Location = HitResult.Location;
	if (HitActor->TakeDamage(M_FullDamage * DamageMlt, M_DamageEvent, nullptr, this) == 0 && M_ProjectileOwner)
	{
		M_ProjectileOwner->OnProjectileKilledActor(HitActor);
	}
	ApplyRadixiteDamageBehaviour(HitActor);
	SpawnExplosionHandleAOE(HitResult.Location, HitRotation, HitSurface, HitActor);
	M_ActorsToIgnore.AddUnique(HitActor);
	M_ArmorPen *= M_PostPenArmorPenCarryOver;
	M_ArmorPenAtMaxRange *= M_PostPenArmorPenCarryOver;
}

void AProjectile::ArmorCalc_KineticProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
                                              AActor* HitActor)
{
	float RawArmorValue = 0.0f;
	float AdjustedArmorPen = GetArmorPenAtRange();
	const FVector Velocity = M_ProjectileMovement->Velocity;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	const float EffectiveArmor = ArmorCalculation->GetEffectiveArmorOnHit(
		HitResult.Component, HitResult.Location, Velocity,
		HitResult.ImpactNormal, RawArmorValue, AdjustedArmorPen, PlateHit);
	const bool bShouldBounce = (EffectiveArmor >= AdjustedArmorPen) &&
	(M_ArmorPen < DeveloperSettings::GameBalance::Weapons::Projectiles::AllowPenRegardlessOfAngleFactor *
		RawArmorValue);
	if (bShouldBounce)
	{
		HandleProjectileBounce(HitResult, PlateHit, HitActor, MakeImpactOutwardRotationZ(HitResult));
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GWeapon_ArmorPen_Compile_DebugSymbols)
	{
		DebugArmorHit(RawArmorValue, EffectiveArmor, AdjustedArmorPen, HitResult.Location, not bShouldBounce);
	}
	OnArmorPen_DisplayText(HitResult.Location, PlateHit);
	ProjectileHitPropagateNotification(false);

	const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	constexpr float DamageMlt = 1.f;
	if (GetCanArmorOverPenetrate(EffectiveArmor, AdjustedArmorPen))
	{
		OnOverPenetratingArmorHit(HitActor, HitResult, SurfaceTypeHit, MakeImpactOutwardRotationZ(HitResult),
		                          DamageMlt);
		OnArmorOverPen_DisplayText(HitResult.Location);
		if (M_ProjectileMovement)
		{
			M_ProjectileMovement->MaxSpeed *=
				DeveloperSettings::GameBalance::Weapons::RailGun::OverPenProjectileSpeedMlt;
		}
		return;
	}

	HandleHitActorAndClearTimer(
		HitActor,
		HitResult.Location,
		SurfaceTypeHit,
		MakeImpactOutwardRotationZ(HitResult), DamageMlt);
}

void AProjectile::ArmorCalc_FireProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
                                           AActor* HitActor)
{
	ProjectileHitPropagateNotification(false);

	const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	constexpr float DamageMlt = 1.f;
	HandleHitActorAndClearTimer(
		HitActor,
		HitResult.Location,
		SurfaceTypeHit,
		MakeImpactOutwardRotationZ(HitResult), DamageMlt);
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
		ScaleNiagaraSystemDependingOnType(NewSettings.ProjectileNiagaraSystem, NewSettings.WeaponCaliber);

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
	case EProjectileNiagaraSystem::RailGun:
		return true;
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
	case EWeaponShellType::Shell_Railgun:
		return ShellWidth * APShellWidthMlt;
	}
	RTSFunctionLibrary::ReportError("Invalid shell type for calculating width: " +
		FString::FromInt(static_cast<int32>(ShellType)));
	return ShellWidth;
}


void AProjectile::DebugProjectile(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GProjectilePooling_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message + "\n Projectile: " + GetName(), FColor::Magenta);
	}
}

void AProjectile::ScaleNiagaraSystemDependingOnType(const EProjectileNiagaraSystem Type,
                                                    const float WeaponCalibre) const
{
	if (Type == EProjectileNiagaraSystem::AttachedRocket)
	{
		M_NiagaraComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
		// Also restart to recreate rocket.
		M_NiagaraComponent->ReinitializeSystem();
	}
	else if (Type == EProjectileNiagaraSystem::RailGun)
	{
		using namespace DeveloperSettings::GamePlay::Projectile::ProjectilesVfx;

		constexpr float MinimumCalibreForUpscale = 30.0f;
		constexpr float MaximumCalibreForUpscale = 120.0f;
		constexpr float MinimumUpscaleFactor = 1.0f;
		constexpr float MaximumUpscaleFactor = 2.0f;
		constexpr int32 ScalingAxisIndex = 0; // 0=X, 1=Y, 2=Z.

		const float CalibreAlpha = FMath::Clamp(
			(WeaponCalibre - MinimumCalibreForUpscale) / (MaximumCalibreForUpscale - MinimumCalibreForUpscale),
			0.0f,
			1.0f);
		const float CalibreScaleFactor = FMath::Lerp(MinimumUpscaleFactor, MaximumUpscaleFactor, CalibreAlpha);

		FVector RailGunScale = RailGunProjectileScaleBase;
		RailGunScale[ScalingAxisIndex] *= CalibreScaleFactor;
		M_NiagaraComponent->SetWorldScale3D(RailGunScale);
	}
	else
	{
		M_NiagaraComponent->SetWorldScale3D(M_BasicScale);
	}
}

bool AProjectile::OnBounce_HandleHeHeatModuleDamage(const FVector& Location, const EArmorPlate ArmorPlateHit,
                                                    AActor* HitActor, const FRotator& HitRotator)
{
	if (M_WeaponCalibre <= 35 || not GetIsValidWidgetPoolManager())
	{
		return false;
	}
	if (M_ShellType != EWeaponShellType::Shell_HE && M_ShellType != EWeaponShellType::Shell_HEAT)
	{
		// Not HE or HEAT shell.
		return false;
	}
	EArmorPlateDamageType ArmorPlateDamage = EArmorPlateDamageType::DamageFront;
	// Throw die to see if we can damage this type of plate (front, sides or rear).
	if (CanHeHeatDamageOnBounce(ArmorPlateHit, ArmorPlateDamage))
	{
		const float* DamageMltPtr = DeveloperSettings::GameBalance::Weapons::ArmorAndModules::PlateTypeToHeHeatDamageMlt
			.Find(ArmorPlateDamage);
		const float DamageMlt = DamageMltPtr ? *DamageMltPtr : 0.1f;
		//  damage, explosion and dormant.
		HandleHitActorAndClearTimer(HitActor, Location, ERTSSurfaceType::Metal, HitRotator, DamageMlt);
		CreateHeHeatBounceDamageText(Location, ArmorPlateDamage);
		He_Heat_AttemptStunOnBounce(HitActor);
		return true;
	}
	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 75.f;
	TextSettings.VisibleDuration = 1.f;
	TextSettings.FadeOutDuration = 1.f;

	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		FRTSRichTextConverter::MakeRTSRich("Shell Shattered!", ERTSRichText::Text_Armor),
		Location, false,
		350, ETextJustify::Type::Left,
		TextSettings
	);
	SpawnBounce(Location, HitRotator);
	OnProjectileDormant();
	return true;
}

bool AProjectile::CanHeHeatDamageOnBounce(const EArmorPlate PlateHit,
                                          EArmorPlateDamageType& OutArmorPlateDamageType) const
{
	TMap<EArmorPlateDamageType, int32> HeHeatOnBounce_DamageChance =
		DeveloperSettings::GameBalance::Weapons::ArmorAndModules::PlateTypeToHeHeatDamageChance;
	OutArmorPlateDamageType = Global_GetDamageTypeFromPlate(PlateHit);
	const int32 Chance = FMath::RandRange(0, 100);
	return Chance < (HeHeatOnBounce_DamageChance.FindChecked(OutArmorPlateDamageType) + M_WeaponCalibre / 5);
}

void AProjectile::CreateHeHeatBounceDamageText(const FVector& Location, const EArmorPlateDamageType DamageType) const
{
	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 105.f;
	TextSettings.VisibleDuration = 1.25f;
	TextSettings.FadeOutDuration = 1.f;
	FString Text = FRTSRichTextConverter::MakeRTSRich("Non-Penetrating explosion", ERTSRichText::Text_Exp);
	Text += "\n";
	Text += FRTSRichTextConverter::MakeRTSRich(Global_GetArmorPlateDamageTypeText(DamageType) + " armor damaged!",
	                                           ERTSRichText::Text_Exp);
	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		Text,
		Location, true,
		300, ETextJustify::Type::Left,
		TextSettings
	);
}

void AProjectile::OnArmorPen_DisplayText(const FVector& Location, const EArmorPlate PlatePenetrated)
{
	if (M_WeaponCalibre <= 35 || not GetIsValidWidgetPoolManager())
	{
		return;
	}
	if (M_ShellType == EWeaponShellType::Shell_HE)
	{
		OnArmorPen_HeDisplayText(Location);
		return;
	}

	FString PlateName;
	switch (PlatePenetrated)
	{
	case EArmorPlate::Plate_SideLeft:
	case EArmorPlate::Plate_SideRight:
	case EArmorPlate::Plate_SideLowerLeft:
	case EArmorPlate::Plate_SideLowerRight:
		PlateName = "Side armor";
		break;
	case EArmorPlate::Turret_Rear:
	case EArmorPlate::Turret_SidesAndRear:
		PlateName = "Rear-turret armor";
		break;
	case EArmorPlate::Turret_SideLeft:
	case EArmorPlate::Turret_SideRight:
		PlateName = "Side-turret armor";
		break;
	case EArmorPlate::Plate_Rear:
	case EArmorPlate::Plate_RearLowerGlacis:
	case EArmorPlate::Plate_RearUpperGlacis:
		PlateName = "Rear armor";
		break;
	default:

		// No text for other plates.
		return;
	}

	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 75.f;
	TextSettings.VisibleDuration = 1.f;
	TextSettings.FadeOutDuration = 1.f;

	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		FRTSRichTextConverter::MakeRTSRich(PlateName + " hit!", ERTSRichText::Text_Exp),
		Location, false,
		350, ETextJustify::Type::Left,
		TextSettings
	);
}

void AProjectile::OnArmorOverPen_DisplayText(const FVector& Location)
{
	if (M_WeaponCalibre <= 19 || not GetIsValidWidgetPoolManager())
	{
		return;
	}

	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 75.f;
	TextSettings.VisibleDuration = 1.f;
	TextSettings.FadeOutDuration = 1.f;

	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		FRTSRichTextConverter::MakeRTSRich("Over-Pen!!", ERTSRichText::Text_BBeh),
		Location, false,
		350, ETextJustify::Type::Left,
		TextSettings
	);
}

void AProjectile::OnArmorPen_HeDisplayText(const FVector& Location)
{
	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 75.f;
	TextSettings.VisibleDuration = 1.f;
	TextSettings.FadeOutDuration = 1.f;

	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		FRTSRichTextConverter::MakeRTSRich("HE destroyed armor!!", ERTSRichText::Text_Bad14),
		Location, false,
		350, ETextJustify::Type::Left,
		TextSettings
	);
}

void AProjectile::HandleAoe(const FVector& HitLocation, AActor* HitActor)
{
	if (M_WeaponCalibre < 45 || M_ShellType == EWeaponShellType::Shell_APCR || M_ShellType ==
		EWeaponShellType::Shell_AP)
	{
		return;
	}
	TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
	ActorsToIgnore.Add(HitActor);
	const float MaxArmorDamaged = M_ShrapnelArmorPen * 1.5;
	const float DamageFallOff = FRTSWeaponHelpers::GetAoEFalloffExponentFromShrapnelParticles(
		M_ShrapnelParticles, 3, 0.5);
	if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(HitLocation, this, "AOE FallOff = " + FString::SanitizeFloat(DamageFallOff));
	}
	ETriggerOverlapLogic OverlapLogic = M_OwningPlayer == 1
		                                    ? ETriggerOverlapLogic::OverlapEnemy
		                                    : ETriggerOverlapLogic::OverlapPlayer;
	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		this,
		HitLocation,
		M_ShrapnelRange,
		M_ShrapnelDamage,
		DamageFallOff,
		M_ShrapnelArmorPen,
		1,
		MaxArmorDamaged,
		ERTSDamageType::Kinetic,
		OverlapLogic,
		ActorsToIgnore
	);
}

void AProjectile::DebugArmorHit(const float Armor, const float EffectiveArmor, const float AdjustedPen,
                                const FVector& HitLcation, const bool bArmorPenned)
{
	FString Message = "A" + FString::SanitizeFloat(Armor) + " EA" + FString::SanitizeFloat(EffectiveArmor) + " APen" +
		FString::SanitizeFloat(AdjustedPen);
	if (bArmorPenned)
	{
		Message = FRTSRichTextConverter::MakeRTSRich(Message, ERTSRichText::Text_Armor);
	}
	else
	{
		Message = FRTSRichTextConverter::MakeRTSRich(Message, ERTSRichText::Text_Bad14);
	}
	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = 100.f;
	TextSettings.VisibleDuration = 3.f;
	TextSettings.FadeOutDuration = 0.5f;
	URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAtLocation(
		this,
		Message,
		HitLcation,
		false, 400,
		ETextJustify::Type::Center,
		TextSettings
	);
}

void AProjectile::He_Heat_AttemptStunOnBounce(const AActor* HitActor) const
{
	if (M_WeaponCalibre < DeveloperSettings::GameBalance::Weapons::Projectiles::StunCalibreThreshold || not
		IsValid(HitActor))
	{
		return;
	}
	UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		return;
	}
	BehaviourComponent->AddBehaviour(UBehVehicleStunned::StaticClass());
}

void AProjectile::He_Heat_AttemptStunOnPen(const AActor* HitActor) const
{
	if (M_WeaponCalibre < DeveloperSettings::GameBalance::Weapons::Projectiles::StunCalibreThreshold)
	{
		return;
	}
	const bool bIsHe_Heat = M_ShellType == EWeaponShellType::Shell_HE || M_ShellType == EWeaponShellType::Shell_HEAT;
	if (not bIsHe_Heat || not IsValid(HitActor))
	{
		return;
	}
	UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		return;
	}
	BehaviourComponent->AddBehaviour(UBehVehicleStunned::StaticClass());
}

void AProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_ProjectileMovement = FindComponentByClass<UProjectileMovementComponent>();
	if (not IsValid(M_ProjectileMovement))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "ProjectileMovement", "PostInitComponents");
	}
	M_DefaultGravityScale = DeveloperSettings::GameBalance::Weapons::ProjectileGravityScale;
	M_NiagaraComponent = FindComponentByClass<UNiagaraComponent>();
	if (not IsValid(M_NiagaraComponent))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NiagaraComponent", "PostInitComponents");
	}
	else
	{
		M_BasicScale = M_NiagaraComponent->GetComponentScale();
	}
	PostInit_SetupWidgetPoolManager();
}

float AProjectile::GetArmorPenAtRange()
{
	const float DistanceTravelled = FVector::Dist(M_ProjectileSpawn, GetActorLocation());
	const float InterpolationFactor = FMath::Clamp(DistanceTravelled / M_Range, 0.0f, 1.0f);
	return FMath::Lerp(M_ArmorPen, M_ArmorPenAtMaxRange, InterpolationFactor);
}
