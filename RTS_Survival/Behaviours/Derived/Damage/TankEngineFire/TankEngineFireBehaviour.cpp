#include "TankEngineFireBehaviour.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace TankEngineFireConstants
{
	constexpr float MinimumDurationSeconds = 0.01f;
}

UTankEngineFireBehaviour::UTankEngineFireBehaviour()
{
	bM_UsesTick = true;
	BehaviourLifeTime = EBehaviourLifeTime::Timed;
	BehaviourStackRule = EBehaviourStackRule::Refresh;
	M_BuffType = EBuffDebuffType::Debuff;
	M_LifeTimeDuration = DefaultMinimumDurationSeconds;
	M_DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(ERTSDamageType::Fire);
}

void UTankEngineFireBehaviour::OnAdded(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	SampleDuration();
	StartFireEffect();
	StartFireSound();
	Super::OnAdded(BehaviourOwner);
}

void UTankEngineFireBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	StopFireSound();
	StopFireEffect();
	Super::OnRemoved(BehaviourOwner);
	M_TankMaster.Reset();
	M_FireSubsystem.Reset();
}

void UTankEngineFireBehaviour::OnTick(const float DeltaTime)
{
	Super::OnTick(DeltaTime);

	if (not GetIsValidTankMaster())
	{
		return;
	}

	const float DamageSeconds = FMath::Min(FMath::Max(DeltaTime, 0.0f), M_RemainingDamageSeconds);
	M_RemainingDamageSeconds -= DamageSeconds;
	const float DamageAmount = FMath::Max(M_DamagePerSecond, 0.0f) * DamageSeconds;
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	M_TankMaster->TakeDamage(DamageAmount, M_DamageEvent, nullptr, nullptr);
}

void UTankEngineFireBehaviour::OnRefreshed(UBehaviour* RefreshingBehaviour)
{
	Super::OnRefreshed(RefreshingBehaviour);

	if (not IsValid(Cast<UTankEngineFireBehaviour>(RefreshingBehaviour)))
	{
		return;
	}

	SampleDuration();
	StopFireEffect();
	StartFireEffect();
}

void UTankEngineFireBehaviour::SampleDuration()
{
	const float MinimumDuration = FMath::Max(M_MinDurationSeconds, TankEngineFireConstants::MinimumDurationSeconds);
	const float MaximumDuration = FMath::Max(M_MaxDurationSeconds, TankEngineFireConstants::MinimumDurationSeconds);
	const float SampledDuration = FMath::FRandRange(
		FMath::Min(MinimumDuration, MaximumDuration), FMath::Max(MinimumDuration, MaximumDuration));
	SetLifetimeDuration(SampledDuration);
	M_RemainingDamageSeconds = SampledDuration;
}

void UTankEngineFireBehaviour::StartFireEffect()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	UWorld* World = M_TankMaster->GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireEffect - tank world is invalid."));
		return;
	}
	M_FireSubsystem = World->GetSubsystem<URTSFireSubsystem>();
	if (not GetIsValidFireSubsystem())
	{
		return;
	}

	USkeletalMeshComponent* HullMesh = M_TankMaster->GetTankMesh();
	if (not IsValid(HullMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireEffect - tank hull mesh is invalid."));
		return;
	}

	FName SocketName = NAME_None;
	FVector HullRelativeOffset = FVector::ZeroVector;
	if (M_AttachmentRules.Mode == ETankEngineFireAttachmentMode::HullSocket)
	{
		SocketName = M_AttachmentRules.HullSocketName;
		if (SocketName == NAME_None)
		{
			RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireEffect - hull socket name is empty."));
			return;
		}
	}
	else
	{
		const FVector PivotRelativeWorldLocation = M_TankMaster->GetActorTransform().TransformPositionNoScale(
			M_AttachmentRules.TankPivotOffset);
		HullRelativeOffset = HullMesh->GetComponentTransform().InverseTransformPosition(PivotRelativeWorldLocation);
	}

	M_FireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(M_TankMaster.Get(), HullMesh, SocketName,
		HullRelativeOffset, M_FireType, M_LifeTimeDuration, M_FireWorldScale, M_FireEffectParams);
}

void UTankEngineFireBehaviour::StopFireEffect()
{
	if (M_FireHandle == INDEX_NONE)
	{
		return;
	}

	if (GetIsValidFireSubsystem())
	{
		M_FireSubsystem->StopFireByHandle(M_FireHandle);
	}
	M_FireHandle = INDEX_NONE;
}

void UTankEngineFireBehaviour::StartFireSound()
{
	if (not GetIsValidTankMaster() || not IsValid(M_FireSound))
	{
		return;
	}

	USceneComponent* RootComponent = M_TankMaster->GetRootComponent();
	if (not IsValid(RootComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireSound - tank root component is invalid."));
		return;
	}

	UAudioComponent* FireAudioComponent = NewObject<UAudioComponent>(M_TankMaster.Get());
	if (not IsValid(FireAudioComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireSound - could not create fire audio component."));
		return;
	}

	FireAudioComponent->bAutoActivate = false;
	FireAudioComponent->bAutoDestroy = false;
	FireAudioComponent->bAllowSpatialization = true;
	USoundBase* FireSound = M_FireSound.Get();
	FireAudioComponent->SetSound(FireSound);
	FireAudioComponent->AttenuationSettings = M_SoundAttenuation;
	USoundConcurrency* SoundConcurrency = M_SoundConcurrency.Get();
	if (IsValid(SoundConcurrency))
	{
		FireAudioComponent->ConcurrencySet.Add(SoundConcurrency);
	}

	M_TankMaster->AddInstanceComponent(FireAudioComponent);
	FireAudioComponent->SetupAttachment(RootComponent);
	FireAudioComponent->RegisterComponent();
	M_FireAudioComponent = FireAudioComponent;
	if (IsValid(FireSound))
	{
		FireAudioComponent->Play();
	}
}

void UTankEngineFireBehaviour::StopFireSound()
{
	UAudioComponent* FireAudioComponent = M_FireAudioComponent.Get();
	if (not IsValid(FireAudioComponent))
	{
		M_FireAudioComponent.Reset();
		return;
	}

	FireAudioComponent->Stop();
	FireAudioComponent->DestroyComponent();
	M_FireAudioComponent.Reset();
}

bool UTankEngineFireBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TankMaster",
		"GetIsValidTankMaster", this);
	return false;
}

bool UTankEngineFireBehaviour::GetIsValidFireSubsystem() const
{
	if (M_FireSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_FireSubsystem",
		"GetIsValidFireSubsystem", this);
	return false;
}
