#include "UWeaponStateMultiHitLaser.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/RTSFunctionLibrary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "Sound/SoundBase.h"

void UWeaponStateMultiHitLaser::InitMultiHitLaserWeapon(
	int32 NewOwningPlayer,
	const int32 NewWeaponIndex,
	const EWeaponName NewWeaponName,
	TScriptInterface<IWeaponOwner> NewWeaponOwner,
	UMeshComponent* NewMeshComponent,
	const FName NewFireSocketName,
	UWorld* NewWorld,
	const FMultiHitLaserWeaponSettings& MultiHitLaserWeaponSettings)
{
	M_MultiHitLaserWeaponSettings = MultiHitLaserWeaponSettings;
	if (M_MultiHitLaserWeaponSettings.MaxHits < 1)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser MaxHits must be at least 1.");
		M_MultiHitLaserWeaponSettings.MaxHits = 1;
	}

	UWeaponStateLaser::InitLaserWeapon(
		NewOwningPlayer,
		NewWeaponIndex,
		NewWeaponName,
		NewWeaponOwner,
		NewMeshComponent,
		NewFireSocketName,
		NewWorld,
		M_MultiHitLaserWeaponSettings.LaserWeaponSettings);

	(void)GetIsValidLaserStartUpSound();

	WeaponData.BaseCooldown += GetStartUpSoundDuration();
}

void UWeaponStateMultiHitLaser::FireWeaponSystem()
{
	if (bM_IsStartupSoundActive)
	{
		return;
	}
	RTSFunctionLibrary::ReportError("Multi-hit laser fired before startup sound completed.");
}

void UWeaponStateMultiHitLaser::CreateLaunchVfx(
	const FVector& LaunchLocation,
	const FVector& /*ForwardVector*/,
	const bool /*bCreateShellCase*/)
{
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser has no valid world to play startup sound.");
		return;
	}

	if (bM_IsStartupSoundActive)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser startup sound was triggered while already active.");
		return;
	}

	PlayStartupSound(LaunchLocation);
	ScheduleLaserAfterStartupSound();
}

void UWeaponStateMultiHitLaser::BeginDestroy()
{
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_LaserStartUpTimerHandle);
	}
	bM_IsStartupSoundActive = false;

	Super::BeginDestroy();
}

void UWeaponStateMultiHitLaser::OnStopFire()
{
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_LaserStartUpTimerHandle);
	}
	bM_IsStartupSoundActive = false;

	Super::OnStopFire();
}

void UWeaponStateMultiHitLaser::FireTraceIteration(const int32 PulseSerial)
{
	if (not World || not GetIsValidMeshComponent() || not WeaponOwner.GetInterface())
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

	TWeakObjectPtr<UWeaponStateMultiHitLaser> WeakThis(this);
	const FVector TraceEndCopy = TraceEnd;
	const FVector LaunchCopy = LaunchLocation;

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindLambda(
		[WeakThis, ProjectileLaunchTime, LaunchCopy, TraceEndCopy, PulseSerial]
	(const FTraceHandle& /*TraceHandle*/, FTraceDatum& TraceDatum)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnMultiHitLaserAsyncTraceComplete(
				TraceDatum,
				ProjectileLaunchTime,
				LaunchCopy,
				TraceEndCopy,
				PulseSerial
			);
		});

	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Multi,
		LaunchLocation,
		TraceEnd,
		TraceChannel,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);

	M_IterationsDone++;
}

void UWeaponStateMultiHitLaser::StartLaserAfterStartupSound()
{
	bM_IsStartupSoundActive = false;
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser tried to fire without a valid world.");
		return;
	}

	const TPair<FVector, FVector> LaunchAndForward = GetLaunchAndForwardVector();
	Super::CreateLaunchVfx(LaunchAndForward.Key, LaunchAndForward.Value, false);
	Super::FireWeaponSystem();
}

void UWeaponStateMultiHitLaser::ScheduleLaserAfterStartupSound()
{
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser has no valid world to schedule startup delay.");
		return;
	}

	const float StartUpDuration = GetStartUpSoundDuration();
	bM_IsStartupSoundActive = true;

	TWeakObjectPtr<UWeaponStateMultiHitLaser> WeakThis(this);
	const FTimerDelegate Del = FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->StartLaserAfterStartupSound();
	});

	World->GetTimerManager().ClearTimer(M_LaserStartUpTimerHandle);
	World->GetTimerManager().SetTimer(M_LaserStartUpTimerHandle, Del, StartUpDuration, false);
}

void UWeaponStateMultiHitLaser::PlayStartupSound(const FVector& LaunchLocation) const
{
	if (not GetIsValidLaserStartUpSound())
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World,
		M_MultiHitLaserWeaponSettings.LaserStartUpSound,
		LaunchLocation,
		FRotator::ZeroRotator,
		1.f,
		1.f,
		0.f,
		M_MultiHitLaserWeaponSettings.LaserStartUpAttenuation,
		M_MultiHitLaserWeaponSettings.LaserStartUpConcurrency
	);
}

float UWeaponStateMultiHitLaser::GetStartUpSoundDuration() const
{
	if (not GetIsValidLaserStartUpSound())
	{
		return 0.f;
	}

	const float Duration = M_MultiHitLaserWeaponSettings.LaserStartUpSound->GetDuration();
	if (Duration < 0.f)
	{
		RTSFunctionLibrary::ReportError("Multi-hit laser startup sound duration is invalid.");
		return 0.f;
	}
	return Duration;
}

bool UWeaponStateMultiHitLaser::GetIsValidLaserStartUpSound() const
{
	if (IsValid(M_MultiHitLaserWeaponSettings.LaserStartUpSound))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Multi-hit laser startup sound is not set.");
	return false;
}

bool UWeaponStateMultiHitLaser::GetIsValidMeshComponent() const
{
	if (IsValid(MeshComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Multi-hit laser mesh component is not valid.");
	return false;
}

void UWeaponStateMultiHitLaser::OnMultiHitLaserAsyncTraceComplete(
	FTraceDatum& TraceDatum,
	const float /*ProjectileLaunchTime*/,
	const FVector& LaunchLocation,
	const FVector& TraceEnd,
	const int32 PulseSerial)
{
	if (PulseSerial != M_CurrentPulseSerial)
	{
		return;
	}

	FVector EndLocation = TraceEnd;

	if (TraceDatum.OutHits.Num() > 0)
	{
		const int32 MaxHitCount = FMath::Max(1, M_MultiHitLaserWeaponSettings.MaxHits);
		int32 HitsProcessed = 0;
		TSet<AActor*> HitActors;
		for (const FHitResult& TraceHit : TraceDatum.OutHits)
		{
			AActor* HitActor = nullptr;
			const float DamageToDeal = GetDamageToDealAndVerifyHitActor(TraceHit, HitActor);
			if (HitActor == nullptr || HitActors.Contains(HitActor))
			{
				continue;
			}

			HitActors.Add(HitActor);
			EndLocation = TraceHit.ImpactPoint;
			if (FluxDamageHitActor_DidActorDie(HitActor, DamageToDeal))
			{
				OnActorKilled(HitActor);
			}

			HitsProcessed++;
			if (HitsProcessed >= MaxHitCount)
			{
				break;
			}
		}
	}

	PlayImpactSound(EndLocation);

	if (not FMath::IsNearlyZero(M_MultiHitLaserWeaponSettings.LaserWeaponSettings.LaserImpactOffset))
	{
		const FVector ToLaunch = (LaunchLocation - EndLocation);
		const float Len = ToLaunch.Size();

		if (Len > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = ToLaunch / Len;
			EndLocation += Dir * M_MultiHitLaserWeaponSettings.LaserWeaponSettings.LaserImpactOffset;
		}
	}

	UpdateLaserParam_Location(EndLocation);
}
