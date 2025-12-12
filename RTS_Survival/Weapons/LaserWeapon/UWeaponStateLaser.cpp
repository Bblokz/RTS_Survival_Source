#include "UWeaponStateLaser.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"


void UWeaponStateLaser::InitLaserWeapon(
	int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	FLaserWeaponSettings LaserWeaponSettings)
{
	FWeaponVFX NotUsedFX;
	FWeaponShellCase NotUsedShellCaseData;
	EWeaponFireMode FireMode = EWeaponFireMode::Single;
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
		NotUsedShellCaseData,
		0, 0, 0,
		false,
		true
	);
	LaserWeaponSettings.BeamTime = LaserWeaponSettings.BeamIterations *
		DeveloperSettings::GameBalance::Weapons::LaserWeapons::LaserIterationTimeInOnePulse;
	// Make sure that the cooldown between shots is adjusted to only start after the beam has finished.
	WeaponData.BaseCooldown += LaserWeaponSettings.BeamTime;
	M_LaserWeaponSettings = LaserWeaponSettings;
	SetupLaserBeam();
	InitLaserBeamParameters();
}

void UWeaponStateLaser::FireWeaponSystem()
{
	// Note that we do not get the fire direction from the owner as the laser weapon needs to start from the actual
	// location on the mesh-socket.
	// We do NOT call StartBeamPulse here—the base firing flow will call CreateLaunchVfx()
	// which triggers StartBeamPulse + SetTimerToHideBeamPostShot. We only schedule traces.
	StartBeamIterationSchedule();
}

void UWeaponStateLaser::CreateLaunchVfx(const FVector& LaunchLocation, const FVector& ForwardVector,
                                        const bool /*bCreateShellCase*/)
{
	if (!World)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		M_LaserWeaponSettings.LaunchSound,
		LaunchLocation,
		FRotator::ZeroRotator,
		1.f, 1.f, 0.f,
		M_LaserWeaponSettings.LaunchAttenuation,
		M_LaserWeaponSettings.LaunchConcurrency);

	// Show the beam immediately for the pulse. Give it an initial reasonable length.
	StartBeamPulse(LaunchLocation + ForwardVector * 100.f);

	// Hide after full beam time (visual lifetime), independent from iteration scheduling.
	SetTimerToHideBeamPostShot();
}


void UWeaponStateLaser::BeginDestroy()
{
	Super::BeginDestroy();

	if (World)
	{
		World->GetTimerManager().ClearTimer(M_LaserHideTimerHandle);
		World->GetTimerManager().ClearTimer(M_LaserIterationTimerHandle);
	}

	if (IsValid(M_LaserBeamSystem))
	{
		M_LaserBeamSystem->DestroyComponent();
	}
}

void UWeaponStateLaser::OnStopFire()
{
	StopBeamPulse();
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_LaserHideTimerHandle);
		World->GetTimerManager().ClearTimer(M_LaserIterationTimerHandle);
	}
}


bool UWeaponStateLaser::EnsureLaserEffectIsValid() const
{
	if (!IsValid(M_LaserWeaponSettings.LaserEffect))
	{
		RTSFunctionLibrary::ReportError(TEXT("Invalid laser effect on laser weapon!"));
		return false;
	}
	return true;
}

void UWeaponStateLaser::SetupLaserBeam()
{
	if (!EnsureLaserEffectIsValid())
	{
		return;
	}
	if (!MeshComponent || FireSocketName.IsNone() || !WeaponOwner)
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot setup the laser beam: invalid mesh, socket, or owner!"));
		return;
	}

	const FVector DirFire = WeaponOwner->GetFireDirection(WeaponIndex);
	const FRotator FireDirection = DirFire.Rotation();

	M_LaserBeamSystem = UNiagaraFunctionLibrary::SpawnSystemAttached(
		M_LaserWeaponSettings.LaserEffect,
		MeshComponent,
		FireSocketName,
		FVector::ZeroVector,
		FireDirection,
		FVector::OneVector,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy=*/false,
		ENCPoolMethod::None,
		/*bPreCullCheck=*/false,
		/*bAutoActivate=*/false
	);

	(void)EnsureLaserBeamSystemIsValid();
	SetBeamHidden(true);
}


void UWeaponStateLaser::InitLaserBeamParameters() const
{
	UpdateLaserParam_Color(M_LaserWeaponSettings.BeamColor);
	UpdateLaserParam_Scale(M_LaserWeaponSettings.BeamScale);
}

bool UWeaponStateLaser::EnsureLaserBeamSystemIsValid() const
{
	if (!IsValid(M_LaserBeamSystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("No laser beam system was set up for the laser weapon!"));
		return false;
	}
	return true;
}


void UWeaponStateLaser::SetBeamHidden(const bool bHidden) const
{
	if (not EnsureLaserBeamSystemIsValid())
	{
		return;
	}
	if (bHidden)
	{
		if (M_LaserBeamSystem->bHiddenInGame)
		{
			RTSFunctionLibrary::ReportError("Attempted to hide laser beam but was already hidden!");
			return;
		}
		M_LaserBeamSystem->SetHiddenInGame(true, false);
		return;
	}
	if (not M_LaserBeamSystem->bHiddenInGame)
	{
		RTSFunctionLibrary::ReportError("Attempted to UN-hide laser beam but the beam is already visible!");
		return;
	}
	M_LaserBeamSystem->SetHiddenInGame(false, false);
}

void UWeaponStateLaser::SetTimerToHideBeamPostShot()
{
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<UWeaponStateLaser> WeakThis(this);
	const FTimerDelegate Del = FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}

		WeakThis->StopBeamPulse();

		// Also stop further iteration ticks for this pulse
		if (WeakThis->World)
		{
			WeakThis->World->GetTimerManager().ClearTimer(WeakThis->M_LaserIterationTimerHandle);
		}
	});

	World->GetTimerManager().ClearTimer(M_LaserHideTimerHandle);
	World->GetTimerManager().SetTimer(M_LaserHideTimerHandle, Del, M_LaserWeaponSettings.BeamTime, false);
}

float UWeaponStateLaser::GetIterationTime() const
{
	return DeveloperSettings::GameBalance::Weapons::LaserWeapons::LaserIterationTimeInOnePulse;
}

void UWeaponStateLaser::StartBeamIterationSchedule()
{
	if (!World)
	{
		return;
	}

	// Reset any previous schedule (e.g. rapid-fire weapons)
	World->GetTimerManager().ClearTimer(M_LaserIterationTimerHandle);

	M_CurrentPulseSerial++;
	M_IterationsDone = 0;
	M_IterationsToDo = FMath::Max(1, M_LaserWeaponSettings.BeamIterations);

	const float IterationTime = GetIterationTime();
	M_PulseEndTime = World->GetTimeSeconds() + M_LaserWeaponSettings.BeamTime;

	// First iteration immediately at t=0
	FireTraceIteration(M_CurrentPulseSerial);

	// If only one iteration is needed, we’re done
	if (M_IterationsToDo <= 1)
	{
		return;
	}

	// Schedule the remaining (N-1) iterations at fixed intervals
	TWeakObjectPtr<UWeaponStateLaser> WeakThis(this);
	FTimerDelegate IterDel;
	IterDel.BindLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->DoBeamIteration();
	});

	World->GetTimerManager().SetTimer(
		M_LaserIterationTimerHandle,
		IterDel,
		IterationTime,
		/*bLoop=*/true
	);
}

void UWeaponStateLaser::DoBeamIteration()
{
	if (!World)
	{
		return;
	}

	// Stop after we’ve executed all scheduled iterations
	if (M_IterationsDone >= M_IterationsToDo)
	{
		World->GetTimerManager().ClearTimer(M_LaserIterationTimerHandle);
		return;
	}

	// If pulse visually ended, we can stop scheduling too 
	if (World->GetTimeSeconds() > M_PulseEndTime)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString(
			"Laser weapon timeout before all iterations were done!"
			"\n Weapon: " + UEnum::GetValueAsString(WeaponData.WeaponName)));
		World->GetTimerManager().ClearTimer(M_LaserIterationTimerHandle);
		return;
	}

	FireTraceIteration(M_CurrentPulseSerial);
}

void UWeaponStateLaser::FireTraceIteration(const int32 PulseSerial)
{
	if (!IsValid(World) || !IsValid(MeshComponent) || !WeaponOwner.GetInterface())
	{
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	const FVector LaunchLocation = LaunchAndForward.Key;
	const FVector Direction = WeaponOwner->GetFireDirection(WeaponIndex);

	FVector TraceEnd = Direction * WeaponData.Range;
	TraceEnd = FRTSWeaponHelpers::GetTraceEndWithAccuracy(LaunchLocation, Direction, WeaponData.Range,
	                                                      WeaponData.Accuracy, bIsAircraftWeapon);

	const float ProjectileLaunchTime = World->GetTimeSeconds();

	FCollisionQueryParams TraceParams(FName(TEXT("LaserFireTrace")), true, nullptr);
	TraceParams.bTraceComplex = false;
	TraceParams.bReturnPhysicalMaterial = true;

	// Ignore self (owner of the weapon mesh)
	if (AActor* OwnerActor = MeshComponent->GetOwner())
	{
		TraceParams.AddIgnoredActor(OwnerActor);
	}


	TWeakObjectPtr<UWeaponStateLaser> WeakThis(this);
	const FVector TraceEndCopy = TraceEnd;
	const FVector LaunchCopy = LaunchLocation;

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindLambda(
		[WeakThis, ProjectileLaunchTime, LaunchCopy, TraceEndCopy, PulseSerial]
	(const FTraceHandle& /*TraceHandle*/, FTraceDatum& TraceDatum)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnLaserAsyncTraceComplete(
				TraceDatum,
				ProjectileLaunchTime,
				LaunchCopy,
				TraceEndCopy,
				PulseSerial
			);
		});

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);

	// Count after dispatch to keep the sequence simple
	M_IterationsDone++;
}

// ---------------- Async result handling ----------------

void UWeaponStateLaser::PlayImpactSound(const FVector& ImpactLocation) const
{
	if (!M_LaserWeaponSettings.ImpactSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		M_LaserWeaponSettings.ImpactSound,
		ImpactLocation,
		FRotator::ZeroRotator,
		1.f, 1.f, 0.f,
		M_LaserWeaponSettings.ImpactAttenuation,
		M_LaserWeaponSettings.ImpactConcurrency
	);
}

void UWeaponStateLaser::OnLaserAsyncTraceComplete(
	FTraceDatum& TraceDatum,
	const float ProjectileLaunchTime,
	const FVector& LaunchLocation,
	const FVector& TraceEnd,
	const int32 PulseSerial)
{
	// Ignore late results from older pulses (e.g., if player fired again quickly)
	if (PulseSerial != M_CurrentPulseSerial)
	{
		return;
	}

	FVector EndLocation = TraceEnd;

	if (TraceDatum.OutHits.Num() > 0)
	{
		const FHitResult TraceHit = TraceDatum.OutHits[0];
		EndLocation = TraceHit.ImpactPoint;

		AActor* HitActor = nullptr;
		const float DamageToDeal = GetDamageToDealAndVerifyHitActor(TraceHit, HitActor);
		if (HitActor && FluxDamageHitActor_DidActorDie(HitActor, DamageToDeal))
		{
			OnActorKilled(HitActor);
		}
	}

	PlayImpactSound(EndLocation);

	// --- SAFE offset application note that NaNs will break background niagara thread! ---
	if (!FMath::IsNearlyZero(M_LaserWeaponSettings.LaserImpactOffset))
	{
		const FVector ToLaunch = (LaunchLocation - EndLocation);
		const float Len = ToLaunch.Size();

		if (Len > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = ToLaunch / Len;
			EndLocation += Dir * M_LaserWeaponSettings.LaserImpactOffset;
		}
		// else: skip offset; avoids NaNs
	}

	UpdateLaserParam_Location(EndLocation);
}

float UWeaponStateLaser::GetDamageToDealAndVerifyHitActor(const FHitResult& TraceHit, AActor*& OutHitActor) const
{
	if (not RTSFunctionLibrary::RTSIsValid(TraceHit.GetActor()))
	{
		// Unit has no hp left or is simply not valid; act as if we hit no actor.
		OutHitActor = nullptr;
		return 0;
	}
	const UArmorCalculation* ArmorCalculationComp = FRTSWeaponHelpers::GetArmorAndActorOrParentFromHit(
		TraceHit, OutHitActor);

	if (IsValid(ArmorCalculationComp))
	{
		return ArmorCalculationComp->GetEffectiveDamageOnHit(
			ERTSDamageType::Laser,
			WeaponData.BaseDamage,
			TraceHit.Component,
			TraceHit.Location
		);
	}

	FRTSWeaponHelpers::Debug_Resistances(TEXT("Laser hit had no armor component; using base damage."), FColor::Red);
	return WeaponData.BaseDamage;
}

// ---------------- Niagara param helpers ----------------

void UWeaponStateLaser::UpdateLaserParam_Location(const FVector& NewEndLocation) const
{
	if (!EnsureLaserBeamSystemIsValid())
	{
		return;
	}
	if (!IsFiniteVector(NewEndLocation))
	{
		// Prevent poisoning the render thread with NaNs
		RTSFunctionLibrary::ReportError(TEXT("Laser end location is non-finite; skipping Niagara update."));
		return;
	}
	M_LaserBeamSystem->SetVectorParameter(NsLaserLocationParamName, NewEndLocation);
}

void UWeaponStateLaser::UpdateLaserParam_Color(const FLinearColor& NewColor) const
{
	if (!EnsureLaserBeamSystemIsValid())
	{
		return;
	}
	M_LaserBeamSystem->SetVariableLinearColor(NsLaserColorParamName, NewColor);
}

void UWeaponStateLaser::UpdateLaserParam_Scale(const float NewScale) const
{
	if (!EnsureLaserBeamSystemIsValid())
	{
		return;
	}
	M_LaserBeamSystem->SetFloatParameter(NsLaserScaleParamName, NewScale);
}

void UWeaponStateLaser::StartBeamPulse(const FVector& InitialEndLocation) const
{
	if (!EnsureLaserBeamSystemIsValid())
	{
		return;
	}

	// Prime parameters before first render
	UpdateLaserParam_Location(InitialEndLocation);

	// Reset + activate for a fresh pulse
	M_LaserBeamSystem->DeactivateImmediate();
	M_LaserBeamSystem->SetHiddenInGame(false, false);
	M_LaserBeamSystem->Activate(true);
}

void UWeaponStateLaser::StopBeamPulse() const
{
	if (!EnsureLaserBeamSystemIsValid())
	{
		return;
	}

	M_LaserBeamSystem->Deactivate();
	M_LaserBeamSystem->SetHiddenInGame(true, false);
}
