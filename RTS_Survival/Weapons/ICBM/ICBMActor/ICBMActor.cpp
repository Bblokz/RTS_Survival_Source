#include "ICBMActor.h"

#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Physics/FRTS_PhysicsHelper.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/ICBM/ICBMLaunchComponent/ICBMLaunchComponent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

AICBMActor::AICBMActor()
{
	PrimaryActorTick.bCanEverTick = true;

	M_MissileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
	M_MissileMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = M_MissileMeshComponent;

	M_ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	M_ProjectileMovementComp->UpdatedComponent = M_MissileMeshComponent;
	M_ProjectileMovementComp->bAutoActivate = false;
	M_ProjectileMovementComp->bRotationFollowsVelocity = true;

	M_AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	M_AudioComponent->SetupAttachment(RootComponent);

	M_NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	M_NiagaraComponent->SetupAttachment(RootComponent);
}

void AICBMActor::BeginPlay()
{
	Super::BeginPlay();
}

void AICBMActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateVerticalMove(DeltaSeconds);

	switch (M_FlightStage)
	{
	case EICBMFlightStage::Launch:
		UpdateLaunchStage(DeltaSeconds);
		return;
	case EICBMFlightStage::Arc:
		UpdateArcStage(DeltaSeconds);
		return;
	case EICBMFlightStage::Terminal:
		UpdateTerminalStage();
		return;
	default:
		return;
	}
}

void AICBMActor::InitICBMActor(UICBMLaunchComponent* OwningLaunchComponent, const int32 SocketIndex, const int32 OwningPlayer)
{
	if (not IsValid(OwningLaunchComponent))
	{
		RTSFunctionLibrary::ReportError("InitICBMActor failed due to invalid owning launch component.");
		return;
	}
	M_OwningLaunchComponent = OwningLaunchComponent;
	M_LaunchSettings = OwningLaunchComponent->GetLaunchSettings();
	M_OwningPlayer = OwningPlayer;
	M_SocketIndex = SocketIndex;
	bM_IsLaunchReady = false;
	M_FlightStage = EICBMFlightStage::None;

	M_MissileMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (GetIsValidProjectileMovementComp())
	{
		M_ProjectileMovementComp->Deactivate();
		M_ProjectileMovementComp->Velocity = FVector::ZeroVector;
	}
}

void AICBMActor::StartVerticalMoveToLaunchReady(const FVector& TargetLocation, const float MoveDuration)
{
	M_VerticalMoveStart = GetActorLocation();
	M_VerticalMoveTarget = TargetLocation;
	M_VerticalMoveDuration = FMath::Max(0.01f, MoveDuration);
	M_VerticalMoveElapsed = 0.0f;
	bM_IsLaunchReady = false;
}

void AICBMActor::FireToLocation(const FVector& TargetLocation, const int32 OwningPlayer)
{
	M_TargetLocation = TargetLocation;
	M_OwningPlayer = OwningPlayer;
	bM_IsLaunchReady = false;
	EnterPrepStage();
}

void AICBMActor::EnterPrepStage()
{
	M_FlightStage = EICBMFlightStage::Prep;
	if (IsValid(M_LaunchSettings.FireStages.PrepStage.PrepFireSound))
	{
		M_AudioComponent->SetSound(M_LaunchSettings.FireStages.PrepStage.PrepFireSound);
		M_AudioComponent->Play();
	}
	if (IsValid(M_LaunchSettings.FireStages.PrepStage.PrepFireVfx))
	{
		M_NiagaraComponent->SetAsset(M_LaunchSettings.FireStages.PrepStage.PrepFireVfx);
		M_NiagaraComponent->Activate(true);
	}

	float PrepDuration = M_LaunchSettings.FireStages.PrepStage.PrepFallbackTime;
	if (IsValid(M_LaunchSettings.FireStages.PrepStage.PrepFireSound))
	{
		PrepDuration = FMath::Max(0.01f, M_LaunchSettings.FireStages.PrepStage.PrepFireSound->GetDuration());
	}

	GetWorldTimerManager().SetTimer(M_PrepStageTimer, this, &AICBMActor::EnterLaunchStage, PrepDuration, false);
}

void AICBMActor::EnterLaunchStage()
{
	M_FlightStage = EICBMFlightStage::Launch;
	M_LaunchStartZ = GetActorLocation().Z;
	M_CurrentLaunchSpeed = 0.0f;

	if (IsValid(M_LaunchSettings.FireStages.LaunchStage.LaunchSound))
	{
		M_AudioComponent->SetSound(M_LaunchSettings.FireStages.LaunchStage.LaunchSound);
		M_AudioComponent->Play();
	}
	if (IsValid(M_LaunchSettings.FireStages.LaunchStage.LaunchVfx))
	{
		M_NiagaraComponent->SetAsset(M_LaunchSettings.FireStages.LaunchStage.LaunchVfx);
		M_NiagaraComponent->Activate(true);
	}

	if (GetIsValidProjectileMovementComp())
	{
		M_ProjectileMovementComp->Activate(true);
		M_ProjectileMovementComp->Velocity = FVector::UpVector * 10.0f;
	}
}

void AICBMActor::EnterArcStage()
{
	M_FlightStage = EICBMFlightStage::Arc;
	M_ArcElapsed = 0.0f;
	M_ArcStartLocation = GetActorLocation();
	const FVector DirectionToTarget = (M_TargetLocation - M_ArcStartLocation).GetSafeNormal();
	const FVector CurvatureOffset = FVector::UpVector * M_LaunchSettings.FireStages.ArcStage.ArcLength *
		M_LaunchSettings.FireStages.ArcStage.ArcCurvature;
	M_ArcControlLocation = M_ArcStartLocation + DirectionToTarget * M_LaunchSettings.FireStages.ArcStage.ArcLength * 0.5f +
		CurvatureOffset;
	const float SafeArcLength = FMath::Max(100.0f, M_LaunchSettings.FireStages.ArcStage.ArcLength);
	const float SafeArcSpeed = FMath::Max(100.0f, M_CurrentLaunchSpeed);
	M_ArcDuration = SafeArcLength / SafeArcSpeed;
}

void AICBMActor::EnterTerminalStage()
{
	M_FlightStage = EICBMFlightStage::Terminal;
	if (GetIsValidProjectileMovementComp())
	{
		const FVector DirectionToTarget = (M_TargetLocation - GetActorLocation()).GetSafeNormal();
		M_ProjectileMovementComp->Velocity = DirectionToTarget * FMath::Max(M_CurrentLaunchSpeed, 400.0f);
	}
	StartTerminalTraceTimer();
}

void AICBMActor::UpdateVerticalMove(const float DeltaSeconds)
{
	if (bM_IsLaunchReady)
	{
		return;
	}
	if (M_VerticalMoveDuration <= 0.0f)
	{
		return;
	}

	M_VerticalMoveElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(M_VerticalMoveElapsed / M_VerticalMoveDuration, 0.0f, 1.0f);
	SetActorLocation(FMath::Lerp(M_VerticalMoveStart, M_VerticalMoveTarget, Alpha));
	if (Alpha < 1.0f)
	{
		return;
	}

	bM_IsLaunchReady = true;
	M_VerticalMoveDuration = 0.0f;
	if (GetIsValidOwningLaunchComponent())
	{
		M_OwningLaunchComponent->OnICBMReachedLaunchReady(this);
	}
}

void AICBMActor::UpdateLaunchStage(const float DeltaSeconds)
{
	const FICBMStageLaunchSettings& LaunchSettings = M_LaunchSettings.FireStages.LaunchStage;
	M_CurrentLaunchSpeed = FMath::Min(
		LaunchSettings.LaunchMaxSpeed,
		M_CurrentLaunchSpeed + LaunchSettings.LaunchAcceleration * DeltaSeconds);

	if (GetIsValidProjectileMovementComp())
	{
		M_ProjectileMovementComp->Velocity = FVector::UpVector * M_CurrentLaunchSpeed;
	}

	if (GetActorLocation().Z - M_LaunchStartZ >= LaunchSettings.LaunchHeight)
	{
		EnterArcStage();
	}
}

void AICBMActor::UpdateArcStage(const float DeltaSeconds)
{
	M_ArcElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(M_ArcElapsed / M_ArcDuration, 0.0f, 1.0f);
	const FVector FirstLerp = FMath::Lerp(M_ArcStartLocation, M_ArcControlLocation, Alpha);
	const FVector SecondLerp = FMath::Lerp(M_ArcControlLocation, M_TargetLocation, Alpha);
	const FVector ArcLocation = FMath::Lerp(FirstLerp, SecondLerp, Alpha);
	SetActorLocation(ArcLocation);

	const FVector NextDirection = (M_TargetLocation - ArcLocation).GetSafeNormal();
	if (GetIsValidProjectileMovementComp())
	{
		M_ProjectileMovementComp->Velocity = NextDirection * FMath::Max(M_CurrentLaunchSpeed, 400.0f);
	}
	SetActorRotation(NextDirection.Rotation());

	if (Alpha >= 1.0f)
	{
		EnterTerminalStage();
	}
}

void AICBMActor::UpdateTerminalStage()
{
	if (GetIsValidProjectileMovementComp())
	{
		SetActorRotation(M_ProjectileMovementComp->Velocity.Rotation());
	}
}

void AICBMActor::StartTerminalTraceTimer()
{
	if (not IsValid(GetWorld()))
	{
		return;
	}

	TWeakObjectPtr<AICBMActor> WeakThis(this);
	GetWorldTimerManager().SetTimer(
		M_TerminalTraceTimer,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->PerformAsyncLineTrace();
		}),
		0.05f,
		true);
}

void AICBMActor::PerformAsyncLineTrace()
{
	if (M_FlightStage != EICBMFlightStage::Terminal)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (not GetIsValidMissileMeshComponent())
	{
		return;
	}

	const FVector TraceStart = M_MissileMeshComponent->DoesSocketExist(M_LaunchSettings.TraceSocketName)
		? M_MissileMeshComponent->GetSocketLocation(M_LaunchSettings.TraceSocketName)
		: GetActorLocation();
	const FVector Direction = GetActorForwardVector();
	const FVector TraceEnd = TraceStart + Direction * 350.0f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ICBMTrace), false, this);
	QueryParams.bReturnPhysicalMaterial = true;
	FTraceDelegate TraceDelegate;
	TraceDelegate.BindUObject(this, &AICBMActor::OnAsyncTraceComplete);

	const ECollisionChannel TraceChannel = M_OwningPlayer == 1 ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER;
	World->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		TraceStart,
		TraceEnd,
		TraceChannel,
		QueryParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate);
}

void AICBMActor::OnAsyncTraceComplete(const FTraceHandle& /*TraceHandle*/, FTraceDatum& TraceDatum)
{
	if (TraceDatum.OutHits.IsEmpty())
	{
		return;
	}
	HandleImpact(TraceDatum.OutHits[0]);
}

void AICBMActor::HandleImpact(const FHitResult& HitResult)
{
	AActor* HitActor = FRTSWeaponHelpers::GetHitActorAdjustedForChildActorComponents(HitResult.GetActor());
	const ERTSSurfaceType SurfaceType = FRTS_PhysicsHelper::GetRTSSurfaceType(HitResult.PhysMaterial);
	SpawnImpactEffects(HitResult.ImpactPoint, SurfaceType);
	ApplyDirectDamage(HitResult, HitActor);
	ApplyAOEDamage(HitResult.ImpactPoint, HitActor);

	GetWorldTimerManager().ClearTimer(M_TerminalTraceTimer);
	Destroy();
}

void AICBMActor::SpawnImpactEffects(const FVector& ImpactLocation, const ERTSSurfaceType SurfaceType) const
{
	const FRTSSurfaceImpactData* SurfaceData = M_LaunchSettings.ImpactSettings.ImpactBySurface.Find(SurfaceType);
	if (not SurfaceData)
	{
		RTSFunctionLibrary::ReportError("ICBM impact settings missing surface mapping at impact.");
		return;
	}
	if (IsValid(SurfaceData->ImpactEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			SurfaceData->ImpactEffect,
			ImpactLocation,
			FRotator::ZeroRotator,
			M_LaunchSettings.ImpactSettings.ImpactVfxScale);
	}
	if (IsValid(SurfaceData->ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			SurfaceData->ImpactSound,
			ImpactLocation,
			FRotator::ZeroRotator,
			1.0f,
			1.0f,
			0.0f,
			M_LaunchSettings.ImpactSettings.ImpactAttenuation,
			M_LaunchSettings.ImpactSettings.ImpactConcurrency);
	}
}

void AICBMActor::ApplyDirectDamage(const FHitResult& HitResult, AActor* HitActor) const
{
	if (not IsValid(HitActor))
	{
		return;
	}

	AActor* AdjustedHitActor = HitActor;
	UArmorCalculation* ArmorCalculationComp = FRTSWeaponHelpers::GetArmorAndActorOrParentFromHit(HitResult, AdjustedHitActor);
	float DamageToApply = M_LaunchSettings.WeaponData.BaseDamage;
	if (IsValid(ArmorCalculationComp))
	{
		float RawArmor = 0.0f;
		float AdjustedPen = M_LaunchSettings.WeaponData.ArmorPen;
		EArmorPlate PlateHit = EArmorPlate::Plate_Front;
		const FVector FlightDirection = GetActorForwardVector();
		const float EffectiveArmor = ArmorCalculationComp->GetEffectiveArmorOnHit(
			HitResult.Component,
			HitResult.ImpactPoint,
			FlightDirection,
			HitResult.ImpactNormal,
			RawArmor,
			AdjustedPen,
			PlateHit);
		if (EffectiveArmor >= AdjustedPen)
		{
			DamageToApply *= 0.35f;
		}
	}

	FPointDamageEvent DamageEvent = FRTSWeaponHelpers::MakePointDamageEvent(
		M_LaunchSettings.WeaponData.DamageType,
		DamageToApply,
		HitResult,
		GetActorForwardVector());
	AdjustedHitActor->TakeDamage(DamageToApply, DamageEvent, nullptr, this);
}

void AICBMActor::ApplyAOEDamage(const FVector& ImpactLocation, AActor* PrimaryHitActor) const
{
	if (M_LaunchSettings.WeaponData.ShrapnelRange <= 0.0f)
	{
		return;
	}
	TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;
	if (IsValid(PrimaryHitActor))
	{
		ActorsToIgnore.Add(PrimaryHitActor);
	}
	const float MaxArmorDamaged = M_LaunchSettings.WeaponData.ShrapnelPen * 1.5f;
	const float DamageFallOff = FRTSWeaponHelpers::GetAoEFalloffExponentFromShrapnelParticles(
		M_LaunchSettings.WeaponData.ShrapnelParticles,
		3.0f,
		0.5f);
	const ETriggerOverlapLogic OverlapLogic = M_OwningPlayer == 1
		? ETriggerOverlapLogic::OverlapEnemy
		: ETriggerOverlapLogic::OverlapPlayer;

	FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
		this,
		ImpactLocation,
		M_LaunchSettings.WeaponData.ShrapnelRange,
		M_LaunchSettings.WeaponData.ShrapnelDamage,
		DamageFallOff,
		M_LaunchSettings.WeaponData.ShrapnelPen,
		1,
		MaxArmorDamaged,
		M_LaunchSettings.WeaponData.DamageType,
		OverlapLogic,
		ActorsToIgnore);
}

bool AICBMActor::GetIsValidProjectileMovementComp() const
{
	if (IsValid(M_ProjectileMovementComp))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ProjectileMovementComp", "GetIsValidProjectileMovementComp", this);
	return false;
}

bool AICBMActor::GetIsValidOwningLaunchComponent() const
{
	if (M_OwningLaunchComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_OwningLaunchComponent", "GetIsValidOwningLaunchComponent", this);
	return false;
}

bool AICBMActor::GetIsValidMissileMeshComponent() const
{
	if (IsValid(M_MissileMeshComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_MissileMeshComponent", "GetIsValidMissileMeshComponent", this);
	return false;
}
