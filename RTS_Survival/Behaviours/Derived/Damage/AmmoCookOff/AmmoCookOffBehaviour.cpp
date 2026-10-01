#include "AmmoCookOffBehaviour.h"

#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace AmmoCookOffConstants
{
	constexpr float PermanentFireLifetimeSeconds = 0.0f;
}

UAmmoCookOffBehaviour::UAmmoCookOffBehaviour()
{
	bM_UsesTick = true;
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
	M_BuffType = EBuffDebuffType::Debuff;
	M_DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(ERTSDamageType::Fire);
}

void UAmmoCookOffBehaviour::OnAdded(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	StartCookOffEffect();
	StartCookOffSound();
	Super::OnAdded(BehaviourOwner);
}

void UAmmoCookOffBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	StopCookOffSound();
	StopCookOffEffect();
	Super::OnRemoved(BehaviourOwner);
	M_TankMaster.Reset();
	M_FireSubsystem.Reset();
}

void UAmmoCookOffBehaviour::OnTick(const float DeltaTime)
{
	Super::OnTick(DeltaTime);

	if (not GetIsValidTankMaster())
	{
		return;
	}

	const float DamageAmount = FMath::Max(M_DamagePerSecond, 0.0f) * FMath::Max(DeltaTime, 0.0f);
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	M_TankMaster->TakeDamage(DamageAmount, M_DamageEvent, nullptr, nullptr);
}

void UAmmoCookOffBehaviour::StartCookOffEffect()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	UWorld* World = M_TankMaster->GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("UAmmoCookOffBehaviour::StartCookOffEffect - tank world is invalid."));
		return;
	}

	M_FireSubsystem = World->GetSubsystem<URTSFireSubsystem>();
	if (not GetIsValidFireSubsystem())
	{
		return;
	}

	AActor* AttachActor = nullptr;
	USceneComponent* AttachComponent = nullptr;
	FName SocketName = NAME_None;
	FVector RelativeOffset = FVector::ZeroVector;
	if (not TryGetAttachment(AttachActor, AttachComponent, SocketName, RelativeOffset))
	{
		return;
	}

	M_FireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(
		AttachActor,
		AttachComponent,
		SocketName,
		RelativeOffset,
		M_FireType,
		AmmoCookOffConstants::PermanentFireLifetimeSeconds,
		M_FireWorldScale,
		M_FireEffectParams);
}

void UAmmoCookOffBehaviour::StopCookOffEffect()
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

void UAmmoCookOffBehaviour::StartCookOffSound()
{
	if (M_CookOffSound == nullptr || not GetIsValidTankMaster() || not GetIsValidCookOffSound())
	{
		return;
	}

	AActor* AttachActor = nullptr;
	USceneComponent* AttachComponent = nullptr;
	FName SocketName = NAME_None;
	FVector RelativeOffset = FVector::ZeroVector;
	if (not TryGetAttachment(AttachActor, AttachComponent, SocketName, RelativeOffset))
	{
		return;
	}

	UAudioComponent* CookOffAudioComponent = NewObject<UAudioComponent>(AttachActor);
	if (not IsValid(CookOffAudioComponent))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UAmmoCookOffBehaviour::StartCookOffSound - could not create cook-off audio component."));
		return;
	}

	CookOffAudioComponent->bAutoActivate = false;
	CookOffAudioComponent->bAutoDestroy = false;
	CookOffAudioComponent->bAllowSpatialization = true;
	CookOffAudioComponent->SetSound(M_CookOffSound.Get());
	CookOffAudioComponent->AttenuationSettings = M_SoundAttenuation;
	if (USoundConcurrency* SoundConcurrency = M_SoundConcurrency.Get())
	{
		CookOffAudioComponent->ConcurrencySet.Add(SoundConcurrency);
	}

	AttachActor->AddInstanceComponent(CookOffAudioComponent);
	CookOffAudioComponent->SetupAttachment(AttachComponent, SocketName);
	CookOffAudioComponent->SetRelativeLocation(RelativeOffset);
	CookOffAudioComponent->RegisterComponent();
	M_CookOffAudioComponent = CookOffAudioComponent;
	CookOffAudioComponent->Play();
}

void UAmmoCookOffBehaviour::StopCookOffSound()
{
	if (M_CookOffSound == nullptr)
	{
		return;
	}
	if (not GetIsValidCookOffAudioComponent())
	{
		M_CookOffAudioComponent.Reset();
		return;
	}

	M_CookOffAudioComponent->Stop();
	M_CookOffAudioComponent->DestroyComponent();
	M_CookOffAudioComponent.Reset();
}

ACPPTurretsMaster* UAmmoCookOffBehaviour::GetFirstMountedTurret() const
{
	if (not GetIsValidTankMaster())
	{
		return nullptr;
	}

	const TArray<ACPPTurretsMaster*> MountedTurrets = M_TankMaster->GetTurrets();
	for (ACPPTurretsMaster* MountedTurret : MountedTurrets)
	{
		if (IsValid(MountedTurret))
		{
			return MountedTurret;
		}
	}

	RTSFunctionLibrary::ReportError(
		TEXT("UAmmoCookOffBehaviour::GetFirstMountedTurret - tank has no valid mounted turret."));
	return nullptr;
}

bool UAmmoCookOffBehaviour::TryGetAttachment(AActor*& OutAttachActor,
	USceneComponent*& OutAttachComponent,
	FName& OutSocketName,
	FVector& OutRelativeOffset) const
{
	OutAttachActor = nullptr;
	OutAttachComponent = nullptr;
	OutSocketName = NAME_None;
	OutRelativeOffset = FVector::ZeroVector;

	if (not GetIsValidTankMaster())
	{
		return false;
	}

	if (M_AttachmentRules.Mode == EAmmoCookOffAttachmentMode::TurretSocket)
	{
		ACPPTurretsMaster* FirstMountedTurret = GetFirstMountedTurret();
		if (not IsValid(FirstMountedTurret))
		{
			return false;
		}

		UMeshComponent* TurretMesh = FirstMountedTurret->GetModuleBindingMesh();
		if (not IsValid(TurretMesh))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("UAmmoCookOffBehaviour::TryGetAttachment - first mounted turret mesh is invalid."));
			return false;
		}
		if (M_AttachmentRules.TurretSocketName == NAME_None)
		{
			RTSFunctionLibrary::ReportError(
				TEXT("UAmmoCookOffBehaviour::TryGetAttachment - turret socket name is empty."));
			return false;
		}

		OutAttachActor = FirstMountedTurret;
		OutAttachComponent = TurretMesh;
		OutSocketName = M_AttachmentRules.TurretSocketName;
		return true;
	}

	USkeletalMeshComponent* HullMesh = M_TankMaster->GetTankMesh();
	if (not IsValid(HullMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("UAmmoCookOffBehaviour::TryGetAttachment - tank hull mesh is invalid."));
		return false;
	}

	const FVector PivotRelativeWorldLocation = M_TankMaster->GetActorTransform().TransformPositionNoScale(
		M_AttachmentRules.TankPivotOffset);
	OutAttachActor = M_TankMaster.Get();
	OutAttachComponent = HullMesh;
	OutRelativeOffset = HullMesh->GetComponentTransform().InverseTransformPosition(PivotRelativeWorldLocation);
	return true;
}

bool UAmmoCookOffBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_TankMaster", "GetIsValidTankMaster", this);
	return false;
}

bool UAmmoCookOffBehaviour::GetIsValidFireSubsystem() const
{
	if (M_FireSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_FireSubsystem", "GetIsValidFireSubsystem", this);
	return false;
}

bool UAmmoCookOffBehaviour::GetIsValidCookOffSound() const
{
	if (IsValid(M_CookOffSound))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_CookOffSound", "GetIsValidCookOffSound", this);
	return false;
}

bool UAmmoCookOffBehaviour::GetIsValidCookOffAudioComponent() const
{
	if (M_CookOffAudioComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_CookOffAudioComponent", "GetIsValidCookOffAudioComponent", this);
	return false;
}
