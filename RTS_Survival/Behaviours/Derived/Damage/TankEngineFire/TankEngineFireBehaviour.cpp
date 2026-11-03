#include "TankEngineFireBehaviour.h"

#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace TankEngineFireConstants
{
	constexpr float MinimumDurationSeconds = 0.01f;
	// Ground traces for the fuel patches start this far above the leak and end this far below it.
	constexpr float PatchGroundTraceUp = 150.0f;
	constexpr float PatchGroundTraceDown = 600.0f;
	// Patches sit a little to the side of the drip point so a trail does not read as a dotted line.
	constexpr float PatchScatterCm = 35.0f;
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

bool UTankEngineFireBehaviour::GetFireWorldLocation(const ATankMaster& Tank, FVector& OutWorldLocation) const
{
	if (M_AttachmentRules.Mode == ETankEngineFireAttachmentMode::HullSocket)
	{
		const UMeshComponent* SocketMesh = FindMeshWithSocket(Tank, M_AttachmentRules.HullSocketName);
		if (not IsValid(SocketMesh))
		{
			return false;
		}
		OutWorldLocation = SocketMesh->GetSocketLocation(M_AttachmentRules.HullSocketName);
		return true;
	}
	OutWorldLocation = Tank.GetActorTransform().TransformPositionNoScale(M_AttachmentRules.TankPivotOffset);
	return true;
}

void UTankEngineFireBehaviour::OnAdded(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	if (GetIsValidTankMaster())
	{
		M_TankMaster->SetIsEngineBurning(true);
	}
	SampleDuration();
	StartFireEffect();
	StartFuelLeak();
	StartFireSound();
	Super::OnAdded(BehaviourOwner);
}

void UTankEngineFireBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	StopFireSound();
	StopFireEffect();
	StopFuelLeak();
	// No validator: during tank destruction the tank may already be gone, which is not an error here.
	if (M_TankMaster.IsValid())
	{
		M_TankMaster->SetIsEngineBurning(false);
	}
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
	TickFuelLeak(DeltaTime);

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
	// The leak flame is pooled with the fire's lifetime; restart it so it burns as long as the refreshed fire.
	StopFuelLeak();
	StartFuelLeak();
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

	UMeshComponent* AttachmentMesh = M_TankMaster->GetTankMesh();
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
		AttachmentMesh = FindTankMeshWithSocket(SocketName);
		if (not IsValid(AttachmentMesh))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("UTankEngineFireBehaviour::StartFireEffect - no tank mesh owns socket '%s'."),
				*SocketName.ToString()));
			return;
		}
	}
	else
	{
		if (not IsValid(AttachmentMesh))
		{
			RTSFunctionLibrary::ReportError(TEXT("UTankEngineFireBehaviour::StartFireEffect - tank hull mesh is invalid."));
			return;
		}
		const FVector PivotRelativeWorldLocation = M_TankMaster->GetActorTransform().TransformPositionNoScale(
			M_AttachmentRules.TankPivotOffset);
		HullRelativeOffset = AttachmentMesh->GetComponentTransform().InverseTransformPosition(PivotRelativeWorldLocation);
	}

	M_FireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(M_TankMaster.Get(), AttachmentMesh, SocketName,
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

void UTankEngineFireBehaviour::StartFuelLeak()
{
	M_FuelLeak = FTankEngineFireFuelLeakState();
	if (not M_FuelLeakSettings.bLeakFuel || not GetIsValidTankMaster() || not GetIsValidFireSubsystem())
	{
		return;
	}
	UMeshComponent* HullMesh = M_TankMaster->GetTankMesh();
	if (not IsValid(HullMesh))
	{
		return;
	}
	const FVector LeakWorldLocation = GetFuelLeakWorldLocation();
	if (M_FuelLeakSettings.LeakFireType != ERTSFireType::None)
	{
		const FVector LeakOffset = HullMesh->GetComponentTransform().InverseTransformPosition(LeakWorldLocation);
		M_FuelLeak.LeakFireHandle = M_FireSubsystem->SpawnFireAttachedToComponent(
			M_TankMaster.Get(), HullMesh, NAME_None, LeakOffset, M_FuelLeakSettings.LeakFireType, M_LifeTimeDuration,
			M_FuelLeakSettings.LeakFireScale, M_FuelLeakSettings.LeakFireEffectParams);
	}
	DropBurningFuelPatch(LeakWorldLocation);
}

void UTankEngineFireBehaviour::StopFuelLeak()
{
	// Patches already on the ground keep burning out on their own lifetime; only the flame on the hull stops.
	if (M_FuelLeak.LeakFireHandle != INDEX_NONE && GetIsValidFireSubsystem())
	{
		M_FireSubsystem->StopFireByHandle(M_FuelLeak.LeakFireHandle);
	}
	M_FuelLeak = FTankEngineFireFuelLeakState();
}

void UTankEngineFireBehaviour::TickFuelLeak(const float DeltaTime)
{
	if (not M_FuelLeakSettings.bLeakFuel || not M_FuelLeak.bHasDroppedPatch)
	{
		return;
	}
	M_FuelLeak.SecondsSinceLastPatch += DeltaTime;
	const FVector LeakWorldLocation = GetFuelLeakWorldLocation();
	const bool bHasDrivenFarEnough = FVector::DistSquared2D(LeakWorldLocation, M_FuelLeak.LastPatchLocation)
		>= FMath::Square(M_FuelLeakSettings.PatchSpacingCm);
	const bool bShouldRenewStandingPatch =
		M_FuelLeak.SecondsSinceLastPatch >= M_FuelLeakSettings.StandingPatchIntervalSeconds;
	if (bHasDrivenFarEnough || bShouldRenewStandingPatch)
	{
		DropBurningFuelPatch(LeakWorldLocation);
	}
}

FVector UTankEngineFireBehaviour::GetFuelLeakWorldLocation() const
{
	FVector EngineWorldLocation = M_TankMaster->GetActorLocation();
	(void)GetFireWorldLocation(*M_TankMaster, EngineWorldLocation);
	return FVector(EngineWorldLocation.X, EngineWorldLocation.Y,
	               M_TankMaster->GetActorLocation().Z + M_FuelLeakSettings.LeakHeightAboveGround);
}

void UTankEngineFireBehaviour::DropBurningFuelPatch(const FVector& LeakWorldLocation)
{
	M_FuelLeak.bHasDroppedPatch = true;
	M_FuelLeak.LastPatchLocation = LeakWorldLocation;
	M_FuelLeak.SecondsSinceLastPatch = 0.0f;
	FVector GroundLocation;
	const FVector ScatteredLeak = LeakWorldLocation
		+ FVector(FMath::RandPointInCircle(TankEngineFireConstants::PatchScatterCm), 0.0f);
	if (M_FuelLeakSettings.PatchFireType == ERTSFireType::None || not GetIsValidFireSubsystem()
		|| not TryGetGroundBelow(ScatteredLeak, GroundLocation))
	{
		return;
	}

	// Keep the cap first so a fresh patch never pushes out a patch that has not been counted yet.
	while (M_FuelLeak.PatchFireHandles.Num() >= FMath::Max(M_FuelLeakSettings.MaxPatchesPerTank, 1))
	{
		// Stopping a patch that already burned out is harmless: fire handles are never reused.
		M_FireSubsystem->StopFireByHandle(M_FuelLeak.PatchFireHandles[0]);
		M_FuelLeak.PatchFireHandles.RemoveAt(0);
	}
	const int32 PatchHandle = M_FireSubsystem->SpawnFireAtLocation(
		M_FuelLeakSettings.PatchFireType, M_FuelLeakSettings.PatchLifeTime, GroundLocation,
		M_FuelLeakSettings.PatchFireScale, FRTSFireEffectParams());
	if (PatchHandle != INDEX_NONE)
	{
		M_FuelLeak.PatchFireHandles.Add(PatchHandle);
	}
	if (M_FuelLeakSettings.bLeaveScorchMarks)
	{
		URTSBlueprintFunctionLibrary::RTSSpawnDecal(this, ERTS_DecalType::Scorch, GroundLocation,
		                                            M_FuelLeakSettings.ScorchMarkSize,
		                                            M_FuelLeakSettings.ScorchMarkLifeTime, FVector2D::ZeroVector);
	}
}

bool UTankEngineFireBehaviour::TryGetGroundBelow(const FVector& WorldLocation, FVector& OutGroundLocation) const
{
	const UWorld* World = M_TankMaster->GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector TraceStart = WorldLocation + FVector(0.0f, 0.0f, TankEngineFireConstants::PatchGroundTraceUp);
	const FVector TraceEnd = WorldLocation - FVector(0.0f, 0.0f, TankEngineFireConstants::PatchGroundTraceDown);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TankFuelLeakPatch), false, M_TankMaster.Get());
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return false;
	}
	OutGroundLocation = GroundHit.ImpactPoint;
	return true;
}

UMeshComponent* UTankEngineFireBehaviour::FindTankMeshWithSocket(const FName SocketName) const
{
	if (not GetIsValidTankMaster())
	{
		return nullptr;
	}
	return FindMeshWithSocket(*M_TankMaster, SocketName);
}

UMeshComponent* UTankEngineFireBehaviour::FindMeshWithSocket(const AActor& Tank, const FName SocketName)
{
	if (SocketName == NAME_None)
	{
		return nullptr;
	}

	TInlineComponentArray<UMeshComponent*> TankMeshes(&Tank);
	for (UMeshComponent* TankMesh : TankMeshes)
	{
		if (IsValid(TankMesh) && TankMesh->DoesSocketExist(SocketName))
		{
			return TankMesh;
		}
	}

	return nullptr;
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
