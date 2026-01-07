#include "FVerticalCollapseOnCrushed.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FVerticalCollapseOnCrushed::Init(UPrimitiveComponent* TargetPrimitive,
                                      const float CollapseSpeedScale,
                                      USoundBase* const CollapseSound,
                                      USoundAttenuation* const Attenuation,
                                      USoundConcurrency* const Concurrency)
{
	M_TargetPrimitive = TargetPrimitive;
	M_CollapseSpeedScale = FMath::Max(0.1f, CollapseSpeedScale);
	M_CollapseSound = CollapseSound;
	M_Attenuation = Attenuation;
	M_Concurrency = Concurrency;
	bM_CollapseQueued = false;
	bM_IsInitialized = true;
}
void FVerticalCollapseOnCrushed::QueueCollapseFromOverlap(UPrimitiveComponent* const /*OverlappedComponent*/,
                                                          UPrimitiveComponent* const OtherComp,
                                                          const bool bFromSweep,
                                                          const FHitResult& SweepResult,
                                                          UWorld* const World)
{
	if (bM_CollapseQueued)
	{
		return;
	}
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("FVerticalCollapseOnCrushed::QueueCollapseFromOverlap: World is null."));
		return;
	}
	if (not IsValid(M_TargetPrimitive))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse: target primitive is invalid."));
		return;
	}

	// Ensure we can move it explicitly (no physics drive).
	if (M_TargetPrimitive->Mobility != EComponentMobility::Movable)
	{
		M_TargetPrimitive->SetMobility(EComponentMobility::Movable);
	}
	if (M_TargetPrimitive->IsSimulatingPhysics())
	{
		M_TargetPrimitive->SetSimulatePhysics(false);
	}

	const FVector FallDir = ComputeFallDirection(M_TargetPrimitive, OtherComp, bFromSweep, SweepResult);
	if (FallDir.IsNearlyZero())
	{
		return;
	}

	const FVector AxisWS = ComputeAxisFromFallDir(FallDir);
	if (AxisWS.IsNearlyZero())
	{
		return;
	}

	// Duration scales inversely with speed scale.
	constexpr float KBaseDurationSeconds = 0.85f; // tuned base duration
	const float Duration = KBaseDurationSeconds / M_CollapseSpeedScale;

	// Snapshot initial rotation (world space).
	M_Anim.M_AxisWS = AxisWS;
	M_Anim.M_InitialWorldRotation = M_TargetPrimitive->GetComponentQuat();
	M_Anim.M_Duration = Duration;
	M_Anim.M_TargetAngleDeg = 90.f;

	// Small debug viz of the axis.
#if !UE_BUILD_SHIPPING
	if constexpr (DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
	{
		const FVector Loc = M_TargetPrimitive->GetComponentLocation();
		DrawDebugDirectionalArrow(World, Loc, Loc + AxisWS * 100.f, 12.f, FColor::Green, false, 1.5f, 0, 2.f);
	}
#endif
PlayCollapseSound(World, M_TargetPrimitive->GetComponentLocation());
	StartCollapseTimer(World,
	                   TWeakObjectPtr<UPrimitiveComponent>(M_TargetPrimitive),
	                   M_Anim.M_AxisWS,
	                   M_Anim.M_InitialWorldRotation,
	                   M_Anim.M_Duration,
	                   M_Anim.M_TargetAngleDeg,
	                   M_CollapseTickHandle);

	bM_CollapseQueued = true;
}

void FVerticalCollapseOnCrushed::QueueCollapseRandom(UWorld* const World)
{
	if (bM_CollapseQueued)
	{
		return;
	}
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("FVerticalCollapseOnCrushed::QueueCollapseRandom: World is null."));
		return;
	}
	if (not IsValid(M_TargetPrimitive))
	{
		RTSFunctionLibrary::ReportError(TEXT("VerticalCollapse(Random): target primitive is invalid."));
		return;
	}

	// Ensure manual rotation (no physics drive).
	if (M_TargetPrimitive->Mobility != EComponentMobility::Movable)
	{
		M_TargetPrimitive->SetMobility(EComponentMobility::Movable);
	}
	if (M_TargetPrimitive->IsSimulatingPhysics())
	{
		M_TargetPrimitive->SetSimulatePhysics(false);
	}

	// Random ground-plane direction (uniform over [0, 2π)).
	const float Theta = FMath::FRandRange(0.f, 2.f * PI);
	const FVector FallDir = FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0.f);

	FVector AxisWS = ComputeAxisFromFallDir(FallDir);
	if (AxisWS.IsNearlyZero())
	{
		// Fallback: rotate around world X if something degenerate happens.
		AxisWS = FVector::RightVector;
	}

	// Duration scales inversely with speed scale (same feel as overlap-driven collapse).
	constexpr float KBaseDurationSeconds = 0.85f;
	const float Duration = KBaseDurationSeconds / M_CollapseSpeedScale;

	// Snapshot initial rotation (world).
	M_Anim.M_AxisWS = AxisWS;
	M_Anim.M_InitialWorldRotation = M_TargetPrimitive->GetComponentQuat();
	M_Anim.M_Duration = Duration;
	M_Anim.M_TargetAngleDeg = 90.f;

#if !UE_BUILD_SHIPPING
	if constexpr (DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
	{
		const FVector Loc = M_TargetPrimitive->GetComponentLocation();
		DrawDebugDirectionalArrow(World, Loc, Loc + AxisWS * 100.f, 12.f, FColor::Cyan, false, 1.5f, 0, 2.f);
	}
#endif

	PlayCollapseSound(World, M_TargetPrimitive->GetComponentLocation());
	StartCollapseTimer(World,
	                   TWeakObjectPtr<UPrimitiveComponent>(M_TargetPrimitive),
	                   M_Anim.M_AxisWS,
	                   M_Anim.M_InitialWorldRotation,
	                   M_Anim.M_Duration,
	                   M_Anim.M_TargetAngleDeg,
	                   M_CollapseTickHandle);

	bM_CollapseQueued = true;
}


void FVerticalCollapseOnCrushed::PlayCollapseSound(UWorld* const World, const FVector& AtLocation) const
{
	if (World == nullptr || M_CollapseSound == nullptr)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAtLocation(
		World,
		M_CollapseSound,
		AtLocation,
		FRotator::ZeroRotator,
		1.f,   // VolumeMultiplier
		1.f,   // PitchMultiplier
		0.f,   // StartTime
		M_Attenuation,
		M_Concurrency
	);
}


FVector FVerticalCollapseOnCrushed::ComputeFallDirection(UPrimitiveComponent* const Target,
                                                         UPrimitiveComponent* const OtherComp,
                                                         const bool bFromSweep,
                                                         const FHitResult& SweepResult)
{
	// Natural direction: fall AWAY from the overlapping component (tank).
	if (IsValid(Target) && IsValid(OtherComp))
	{
		FVector Away = (Target->GetComponentLocation() - OtherComp->GetComponentLocation());
		Away.Z = 0.f;
		if (Away.Normalize())
		{
			return Away;
		}
	}

	// If we have a blocking sweep, use opposite of impact normal projected to ground plane.
	if (bFromSweep && SweepResult.bBlockingHit)
	{
		FVector Ground = (-SweepResult.ImpactNormal);
		Ground.Z = 0.f;
		if (Ground.Normalize())
		{
			return Ground;
		}
	}

	// Fallback: away from the tank's motion.
	if (IsValid(OtherComp))
	{
		FVector Vel = OtherComp->GetComponentVelocity();
		Vel.Z = 0.f;
		if (Vel.Normalize())
		{
			return Vel;
		}
	}

	return FVector::ZeroVector;
}

FVector FVerticalCollapseOnCrushed::ComputeAxisFromFallDir(const FVector& FallDir)
{
	// Tip toward FallDir by rotating around axis = Up x FallDir.
	return FVector::CrossProduct(FVector::UpVector, FallDir).GetSafeNormal();
}

void FVerticalCollapseOnCrushed::StartCollapseTimer(UWorld* const World,
                                                    const TWeakObjectPtr<UPrimitiveComponent> WeakTarget,
                                                    const FVector& AxisWS,
                                                    const FQuat& InitialWorldRotation,
                                                    const float DurationSeconds,
                                                    const float TargetAngleDeg,
                                                    FTimerHandle& OutHandle)
{
	if (World == nullptr || !WeakTarget.IsValid())
	{
		return;
	}

	// Fixed tick rate for consistent easing.
	constexpr float KTickInterval = 1.0f / 60.0f; // seconds
	const double StartSeconds = FPlatformTime::Seconds();

	FTimerDelegate TickDel;
	TickDel.BindLambda([WeakTarget, AxisWS, InitialWorldRotation, DurationSeconds, TargetAngleDeg, StartSeconds]()
	{
		if (!WeakTarget.IsValid())
		{
			return;
		}

		const double Now = FPlatformTime::Seconds();
		const float T = FMath::Clamp(static_cast<float>((Now - StartSeconds) / FMath::Max(0.001f, DurationSeconds)),
		                             0.f, 1.f);

		// Non-linear ease-in (quadratic): accelerates as it falls.
		const float Ease = T * T;

		const float AngleRad = FMath::DegreesToRadians(TargetAngleDeg) * Ease;
		const FQuat Delta = FQuat(AxisWS, AngleRad);

		WeakTarget->SetWorldRotation(Delta * InitialWorldRotation);

		if (T >= 1.f)
		{
			// Snap exactly to 90° at the end (avoid tiny drift).
			const FQuat Final = FQuat(AxisWS, FMath::DegreesToRadians(TargetAngleDeg)) * InitialWorldRotation;
			WeakTarget->SetWorldRotation(Final);
		}
	});

	// Run once per frame until we manually stop by reaching T>=1.
	World->GetTimerManager().SetTimer(OutHandle, TickDel, KTickInterval, true);

	// Also schedule a one-shot that stops the repeating timer exactly at DurationSeconds.
	FTimerHandle StopHandle;
	FTimerDelegate StopDel;
	StopDel.BindLambda([World, &OutHandle, WeakTarget, AxisWS, InitialWorldRotation, TargetAngleDeg]()
	{
		if (!WeakTarget.IsValid() || World == nullptr)
		{
			return;
		}
		World->GetTimerManager().ClearTimer(OutHandle);

		// Ensure exact final orientation.
		const FQuat Final = FQuat(AxisWS, FMath::DegreesToRadians(TargetAngleDeg)) * InitialWorldRotation;
		WeakTarget->SetWorldRotation(Final);
	});

	World->GetTimerManager().SetTimer(StopHandle, StopDel, FMath::Max(0.001f, DurationSeconds), false);
}
