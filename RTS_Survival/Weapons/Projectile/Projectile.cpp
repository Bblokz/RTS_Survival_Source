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
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

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
	WeaponCalibre)
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
	M_WeaponCalibre = WeaponCalibre;

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
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
		World->GetTimerManager().ClearTimer(M_LineTraceTimerHandle);
		World->GetTimerManager().ClearTimer(M_DescentSoundTimerHandle);

		World->GetTimerManager().SetTimer(M_ExplosionTimerHandle, this, &AProjectile::HandleTimedExplosion,
		                                  ExpectedFlightTime, false);
		const float LineTraceInterval = DeveloperSettings::Optimization::ProjectileTraceInterval;
		World->GetTimerManager().SetTimer(M_LineTraceTimerHandle, this, &AProjectile::PerformAsyncLineTrace,
		                                  LineTraceInterval, true);
		M_LastTraceTime = World->GetTimeSeconds() - LineTraceInterval;
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
	}
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
	}
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


void AProjectile::OnProjectileDormant()
{
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
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	const float EffectiveArmor = ArmorCalculation->GetEffectiveArmorOnHit(
		HitResult.Component, HitResult.Location, Velocity,
		HitResult.ImpactNormal, RawArmorValue, AdjustedArmorPen, PlateHit);
	// RTSFunctionLibrary::PrintString("Effective Armor: " + FString::SanitizeFloat(EffectiveArmor), FColor::Red);
	// RTSFunctionLibrary::PrintString("Raw Armor: " + FString::SanitizeFloat(RawArmorValue), FColor::Blue);
	// RTSFunctionLibrary::PrintString("Adjusted Pen: " + FString::SanitizeFloat(AdjustedArmorPen), FColor::Green);
	const bool bShouldBounce = (EffectiveArmor >= AdjustedArmorPen) &&
	(M_ArmorPen < DeveloperSettings::GameBalance::Weapons::Projectiles::AllowPenRegardlessOfAngleFactor *
		RawArmorValue);
	if (bShouldBounce)
	{
		HandleProjectileBounce(HitResult, PlateHit, HitActor, MakeImpactOutwardRotationZ(HitResult));
		return;
	}
	OnArmorPen_DisplayText(HitResult.Location, PlateHit);
	ProjectileHitPropagateNotification(false);

	const ERTSSurfaceType SurfaceTypeHit = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	constexpr float DamageMlt = 1.f;
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
	if constexpr (DeveloperSettings::Debugging::GWeapon_ArmorPen_Compile_DebugSymbols)
	{
		DrawDebugString(
			GetWorld(), Location, TEXT("Bounce!"), 0, FColor::Purple, 5.f, false, 1.f);
	}
}

void AProjectile::DebugProjectile(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GProjectilePooling_Compile_DebugSymbols)
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
	if (M_WeaponCalibre < 45 || M_ShellType == EWeaponShellType::Shell_APCR || M_ShellType == EWeaponShellType::Shell_AP)
	{
		return;
	}
	TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
	ActorsToIgnore.Add(HitActor);
	const float MaxArmorDamaged = M_ShrapnelArmorPen * 1.5;
	const float DamageFallOff = FRTSWeaponHelpers::GetAoEFalloffExponentFromShrapnelParticles(M_ShrapnelParticles, 3, 0.5);
	if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(HitLocation, this,"AOE FallOff = " + FString::SanitizeFloat(DamageFallOff));
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
