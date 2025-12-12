// Copyright (c) Bas Blokzijl. All rights reserved.

#include "UWeaponStateFlameThrower.h"

#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"

using namespace DeveloperSettings::GameBalance::Weapons::FlameWeapons;

void UWeaponStateFlameThrower::InitFlameThrowerWeapon(
	int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	FFlameThrowerSettings FlameSettings)
{
	// Sanitize designer values (never 0; enforce maxima).
	FlameSettings.ConeAngleMlt = FMath::Clamp(FlameSettings.ConeAngleMlt <= 0 ? 1 : FlameSettings.ConeAngleMlt, 1, 5);
	FlameSettings.FlameIterations = FMath::Clamp(FlameSettings.FlameIterations <= 0 ? 1 : FlameSettings.FlameIterations,
	                                             1, 10);

	// Always Single fire mode.
	FWeaponVFX NotUsedFX;
	FWeaponShellCase NotUsedShell;
	constexpr EWeaponFireMode FireMode = EWeaponFireMode::Single;

	InitWeaponState(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		FireMode,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		NotUsedFX,
		NotUsedShell,
		/*BurstCooldown*/0.f, /*SingleBurstMax*/0, /*MinBurst*/0,
		/*bCreateShellEachRandomBurst*/false,
		true
	);

	// Now WeaponData is initialized; compute total duration and adjust cooldown.
	const float TotalDuration = GetTotalDurationSeconds();
	WeaponData.BaseCooldown += TotalDuration;

	M_FlameSettings = FlameSettings;

	SetupFlameSystem();
	InitFlameParamsStatic(); // Color/Range/ConeAngle (Duration is set at fire start)
}

bool UWeaponStateFlameThrower::EnsureFlameEffectIsValid() const
{
	if (!IsValid(M_FlameSettings.FlameEffect))
	{
		RTSFunctionLibrary::ReportError(TEXT("Flamethrower has invalid FlameEffect Niagara system!"));
		return false;
	}
	return true;
}

bool UWeaponStateFlameThrower::EnsureFlameSystemIsValid() const
{
	if (!IsValid(M_FlameSystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("Flamethrower Niagara component was not set up!"));
		return false;
	}
	return true;
}

void UWeaponStateFlameThrower::SetupFlameSystem()
{
	if (!EnsureFlameEffectIsValid())
	{
		return;
	}
	if (!IsValid(MeshComponent) || FireSocketName.IsNone() || !WeaponOwner)
	{
		RTSFunctionLibrary::ReportError(TEXT("Flamethrower cannot setup Niagara: invalid mesh/socket/owner."));
		return;
	}

	// Attach but do not auto-destroy; keep deactivated until firing.
	const FVector ZeroLoc = FVector::ZeroVector;
	const FRotator ZeroRot = FRotator::ZeroRotator;

	M_FlameSystem = UNiagaraFunctionLibrary::SpawnSystemAttached(
		M_FlameSettings.FlameEffect,
		MeshComponent,
		FireSocketName,
		ZeroLoc,
		ZeroRot,
		FVector::OneVector,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/false,
		ENCPoolMethod::None,
		/*bPreCullCheck*/false,
		/*bAutoActivate*/false
	);

	if (EnsureFlameSystemIsValid())
	{
		M_FlameSystem->SetHiddenInGame(true, false);
	}
}

void UWeaponStateFlameThrower::InitFlameParamsStatic() const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	// Color from settings.
	UpdateFlameParam_Color(M_FlameSettings.Color);

	// Range from weapon stats (already initialized by base InitWeaponState).
	UpdateFlameParam_Range(WeaponData.Range);

	// ConeAngle = FlameConeAngleUnit * ConeAngleMlt.
	const float ConeAngle = FlameConeAngleUnit * static_cast<float>(M_FlameSettings.ConeAngleMlt);
	UpdateFlameParam_ConeAngle(ConeAngle);
}

void UWeaponStateFlameThrower::StartFlameVfx(const float Duration) const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}

	// Update the runtime Duration every shot (depends on FlameIterations).
	UpdateFlameParam_Duration(Duration);

	// Reset/activate. We don't destroy—just toggle.
	M_FlameSystem->DeactivateImmediate();
	M_FlameSystem->SetHiddenInGame(false, false);
	M_FlameSystem->Activate(true);
}

void UWeaponStateFlameThrower::StopFlameVfx() const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	M_FlameSystem->Deactivate();
	M_FlameSystem->SetHiddenInGame(true, false);
}

void UWeaponStateFlameThrower::UpdateFlameParam_Color(const FLinearColor& NewColor) const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	M_FlameSystem->SetVariableLinearColor(NsFlameColorParamName, NewColor);
}

void UWeaponStateFlameThrower::UpdateFlameParam_Range(const float NewRange) const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	// We always add 8 percent to make the flame end points seem better engulfed in the VFX.
	M_FlameSystem->SetFloatParameter(NsFlameRangeParamName, NewRange * 1.08f);
}

void UWeaponStateFlameThrower::UpdateFlameParam_Duration(const float NewDuration) const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	M_FlameSystem->SetFloatParameter(NsFlameDurationParamName, NewDuration);
}

void UWeaponStateFlameThrower::UpdateFlameParam_ConeAngle(const float NewConeAngle) const
{
	if (!EnsureFlameSystemIsValid())
	{
		return;
	}
	M_FlameSystem->SetFloatParameter(NsFlameConeAngleParamName, NewConeAngle);
}

// ---------------- Audio ----------------

void UWeaponStateFlameThrower::StartLoopingSoundAt(const FVector& Location)
{
	if (!IsValid(World) || !IsValid(M_FlameSettings.LoopingSound))
	{
		return;
	}

	//  prevent multiple overlapping loops from this weapon
	if (IsValid(M_LoopingAudio))
	{
		M_LoopingAudio->Stop();
		M_LoopingAudio = nullptr;
	}

	//  attach to the muzzle so it follows and dies with the owner
	if (IsValid(MeshComponent))
	{
		M_LoopingAudio = UGameplayStatics::SpawnSoundAttached(
			M_FlameSettings.LoopingSound,
			MeshComponent,
			FireSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			/*bStopWhenAttachedToDestroyed*/ true,
			/*VolumeMultiplier*/ 1.f,
			/*PitchMultiplier*/ 1.f,
			/*StartTime*/ 0.f,
			M_FlameSettings.LaunchAttenuation,
			M_FlameSettings.LaunchConcurrency,
			/*bAutoDestroy*/ true
		);
	}
}

void UWeaponStateFlameThrower::StopAndDestroyLoopingSound()
{
	if (!IsValid(M_LoopingAudio))
	{
		return;
	}
	M_LoopingAudio->Stop();
	M_LoopingAudio = nullptr;
}

// ---------------- Timing ----------------

float UWeaponStateFlameThrower::GetIterationTime() const
{
	return FlameIterationTime;
}

float UWeaponStateFlameThrower::GetTotalDurationSeconds() const
{
	// Duration = Iterations * IterationTime
	const int32 Iters = FMath::Clamp(M_FlameSettings.FlameIterations, 1, 10);
	return static_cast<float>(Iters) * GetIterationTime();
}

void UWeaponStateFlameThrower::StartIterationSchedule()
{
	if (!IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_FlameIterationTimerHandle);

	auto& Iter = M_FlameIterationState;

	Iter.CurrentSerial++;
	Iter.IterationsDone = 0;
	Iter.IterationsToDo = FMath::Clamp(M_FlameSettings.FlameIterations, 1, 10);

	// Spec: If FlameIterations == 1, fire once *after* the first iteration finishes.
	// So we don't fire immediately—start a repeating timer that ticks every iteration.
	const float Tick = GetIterationTime();

	TWeakObjectPtr<UWeaponStateFlameThrower> WeakThis(this);
	FTimerDelegate IterDel;
	IterDel.BindLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->DoOneIteration();
	});

	World->GetTimerManager().SetTimer(M_FlameIterationTimerHandle, IterDel, Tick, /*bLoop*/true);
}

void UWeaponStateFlameThrower::DoOneIteration()
{
	if (!IsValid(World) || !IsValid(MeshComponent))
	{
		return;
	}

	auto& Iter = M_FlameIterationState;

	// Stop schedule when done.
	if (Iter.IterationsDone >= Iter.IterationsToDo)
	{
		World->GetTimerManager().ClearTimer(M_FlameIterationTimerHandle);
		return;
	}

	// Compute current muzzle and mid direction.
	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	const FVector LaunchLocation = LaunchAndForward.Key;
	const FVector ForwardVector = LaunchAndForward.Value.GetSafeNormal();

	DispatchIterationTraces(LaunchLocation, ForwardVector, WeaponData.Range);

	Iter.IterationsDone++;

	// Stop after last dispatch.
	if (Iter.IterationsDone >= Iter.IterationsToDo)
	{
		World->GetTimerManager().ClearTimer(M_FlameIterationTimerHandle);
	}
}

// ---------------- Fire / VFX entry points ----------------

void UWeaponStateFlameThrower::FireWeaponSystem()
{
	// Only the timed iteration schedule; visuals/audio are kicked by CreateLaunchVfx().
	StartIterationSchedule();
}

void UWeaponStateFlameThrower::CreateLaunchVfx(const FVector& LaunchLocation, const FVector& /*ForwardVector*/,
                                               const bool /*bCreateShellCase*/)
{
	if (!IsValid(World))
	{
		return;
	}

	// Audio
	StartLoopingSoundAt(LaunchLocation);

	// VFX
	const float TotalDuration = GetTotalDurationSeconds();
	StartFlameVfx(TotalDuration);

	// Schedule visual/audio stop exactly at the end of duration.
	TWeakObjectPtr<UWeaponStateFlameThrower> WeakThis(this);
	FTimerDelegate StopDel;
	StopDel.BindLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->StopFlameVfx();
		WeakThis->StopAndDestroyLoopingSound();
	});

	World->GetTimerManager().ClearTimer(M_FlameStopTimerHandle);
	World->GetTimerManager().SetTimer(M_FlameStopTimerHandle, StopDel, TotalDuration, /*bLoop*/false);
}

void UWeaponStateFlameThrower::BeginDestroy()
{
	// Clear timers
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_FlameIterationTimerHandle);
		World->GetTimerManager().ClearTimer(M_FlameStopTimerHandle);
	}

	// Sound must be destroyed manually
	StopAndDestroyLoopingSound();

	Super::BeginDestroy();
}

void UWeaponStateFlameThrower::OnStopFire()
{
	// Do not stop the flame; the base class already clears the main weapon timers; after the flame iteration is finished
	// the weapon stops automatically.
}

void UWeaponStateFlameThrower::OnDisableWeapon()
{
	// Hide/stop visuals & audio immediately
	StopFlameVfx();
	StopAndDestroyLoopingSound();

	// Invalidate any in-flight async results from the previous run
	auto& Iter    = M_FlameIterationState;
	auto& Pending = M_PendingRaysState;
	++Iter.CurrentSerial;
	Pending.Serial = -1;

	// Clear timers so nothing re-triggers post-stop
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_FlameIterationTimerHandle);
		World->GetTimerManager().ClearTimer(M_FlameStopTimerHandle);
	}

	Iter.IterationsDone = 0;
	Iter.IterationsToDo = 0;
}

// ---------------- Ray building / dispatch ----------------

void UWeaponStateFlameThrower::BuildRayYawOffsets(TArray<float>& OutYawDegrees) const
{
	// Spec’d patterns:
	// Mlt==1: {0}
	// Mlt==2: {0, ±unit}
	// Mlt==3: {0, ±unit, ±1.5*unit}
	// For >3 we keep sampling like Mlt==3 for trace purposes (Niagara still receives full ConeAngle).
	const float Unit = FlameConeAngleUnit;
	const int32 Mlt = FMath::Clamp(M_FlameSettings.ConeAngleMlt, 1, 3);

	OutYawDegrees.Reset();

	switch (Mlt)
	{
	case 1:
		OutYawDegrees = {0.f};
		break;
	case 2:
		OutYawDegrees = {0.f, -Unit, +Unit};
		break;
	default: // 3 or more: follow the spec with the extra ±1.5*unit
		OutYawDegrees = {0.f, -Unit, +Unit, -1.5f * Unit, +1.5f * Unit};
		break;
	}
}

FVector UWeaponStateFlameThrower::ComputeRayEndOnPerpPlane(const FVector& Launch, const FVector& MidDir,
                                                           const float Range, const FVector& RayDir) const
{
	// Intersect ray(Launch + t * RayDir) with plane through Pm (= Launch + MidDir*Range) having normal MidDir.
	const FVector N = MidDir.GetSafeNormal();
	const FVector Pm = Launch + N * Range;

	const float denom = FVector::DotProduct(RayDir, N);
	if (FMath::IsNearlyZero(denom))
	{
		// Parallel to plane; fallback to straight 'Range'.
		return Launch + RayDir * Range;
	}

	const float t = FVector::DotProduct(Pm - Launch, N) / denom;
	const float ClampedT = (t > 0.f) ? t : Range; // avoid negative
	return Launch + RayDir * ClampedT;
}

void UWeaponStateFlameThrower::DispatchIterationTraces(const FVector& LaunchLocation, const FVector& ForwardDir,
                                                       const float Range)
{
	if (!IsValid(World))
	{
		return;
	}

	// Build yaw offsets (at least one).
	TArray<float> Yaws;
	BuildRayYawOffsets(Yaws);
	const int32 BaseRayCount = Yaws.Num();
	const int32 TotalRays    = BaseRayCount * 2;

	// Prepare aggregation state for all rays this iteration.
	auto& Pending = M_PendingRaysState;
	const auto& Iter = M_FlameIterationState;

	Pending.Serial    = Iter.CurrentSerial;
	Pending.Expected  = TotalRays;
	Pending.Received  = 0;
	Pending.Launch    = LaunchLocation;
	Pending.RayEndPoints.SetNum(TotalRays);
	Pending.RayHits.SetNum(TotalRays);

	const FRotator BaseRot = ForwardDir.Rotation();

	// Each base ray emits two traces: the original and a lowered one.
	for (int32 i = 0; i < BaseRayCount; ++i)
	{
		const float    YawDeg = Yaws[i];
		const FRotator RayRot(0.f, BaseRot.Yaw + YawDeg, 0.f);
		const FVector  RayDir = RayRot.Vector();

		const FVector RayEnd   = ComputeRayEndOnPerpPlane(LaunchLocation, ForwardDir, Range, RayDir);
		const int32   UpperIdx = i * 2;
		const int32   LowerIdx = UpperIdx + 1;

		Pending.RayEndPoints[UpperIdx] = RayEnd;
		Pending.RayEndPoints[LowerIdx] = FVector(RayEnd.X, RayEnd.Y, RayEnd.Z - (Range * 0.2f));

		// Fire both the original and the lowered ray.
		EnqueueRayAsync(UpperIdx, LaunchLocation, Pending.RayEndPoints[UpperIdx]);
		EnqueueRayAsync(LowerIdx, LaunchLocation, Pending.RayEndPoints[LowerIdx]);
	}
}

void UWeaponStateFlameThrower::EnqueueRayAsync(const int32 RayIndex, const FVector& LaunchLocation, const FVector& End)
{
	if (!IsValid(World))
	{
		return;
	}

	auto& Pending = M_PendingRaysState;

	FCollisionQueryParams TraceParams(FName(TEXT("FlameRay")), /*bTraceComplex*/false, /*IgnoreActor*/nullptr);
	TraceParams.bReturnPhysicalMaterial = false;

	// Avoid self-hit from owner.
	if (AActor* OwnerActor = MeshComponent ? MeshComponent->GetOwner() : nullptr)
	{
		TraceParams.AddIgnoredActor(OwnerActor);
	}

	const int32 SerialCopy   = Pending.Serial;
	const int32 RayIndexCopy = RayIndex;

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindLambda(
		[this, SerialCopy, RayIndexCopy](const FTraceHandle& /*Handle*/, FTraceDatum& TraceDatum)
		{
			this->OnSingleRayAsyncComplete(SerialCopy, RayIndexCopy, TraceDatum);
		});

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Multi,
		LaunchLocation,
		End,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void UWeaponStateFlameThrower::OnSingleRayAsyncComplete(const int32 Serial, const int32 RayIndex,
                                                        FTraceDatum& TraceDatum)
{
	auto& Pending = M_PendingRaysState;

	// Late results from older serials are ignored.
	if (Serial != Pending.Serial)
	{
		return;
	}
	if (RayIndex < 0 || RayIndex >= Pending.RayHits.Num())
	{
		return;
	}

	Pending.RayHits[RayIndex] = TraceDatum.OutHits;

	Pending.Received++;
	if (Pending.Received >= Pending.Expected)
	{
		OnIterationAllRaysComplete();
	}
}

void UWeaponStateFlameThrower::OnIterationAllRaysComplete()
{
	// Debug all hits: red spheres; endpoints: blue sphere.
	if (!IsValid(World))
	{
		return;
	}

	const auto& Pending = M_PendingRaysState;

	// Allow duplicates: each contact counts (later overlaps included).
	TArray<AActor*> ValidHitActors;

	for (int32 i = 0; i < Pending.RayHits.Num(); ++i)
	{
		for (const FHitResult& Hit : Pending.RayHits[i])
		{

			if (AActor* HitActor = FRTSWeaponHelpers::GetRTSValidHitActor(Hit))
			{
				// NOTE: no AddUnique — we want later “overlaps” / multiple contacts to count
				ValidHitActors.Add(HitActor);
			}
		}

		const FVector End = Pending.RayEndPoints.IsValidIndex(i) ? Pending.RayEndPoints[i] : Pending.Launch;
	}

	OnHitValidActors(ValidHitActors);

	// Clear aggregation containers for the next iteration.
	M_PendingRaysState.RayHits.Reset();
	M_PendingRaysState.RayEndPoints.Reset();
}

void UWeaponStateFlameThrower::OnHitValidActors(const TArray<AActor*>& ValidHitActors)
{
	if (ValidHitActors.Num() == 0)
	{
		return;
	}
	bool bKilledAnActor = false;
	AActor* LastKilledActor = nullptr;
	const float DamageWithFlux = FRTSWeaponHelpers::GetDamageWithFlux(WeaponData.BaseDamage, WeaponData.DamageFlux);
	for (auto eachValidHitActor : ValidHitActors)
	{
		if (FluxDamageHitActor_DidActorDie(eachValidHitActor, DamageWithFlux))
		{
			bKilledAnActor = true;
			LastKilledActor = eachValidHitActor;
		}
	}
	if (bKilledAnActor)
	{
		OnActorKilled(LastKilledActor);
	}
}
