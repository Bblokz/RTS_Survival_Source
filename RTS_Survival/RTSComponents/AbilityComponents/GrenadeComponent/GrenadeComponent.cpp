// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GrenadeComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace GrenadeComponentConstants
{
	constexpr float ThrowTime = 2.f;
	constexpr float ThrowInterpInterval = 0.02f;
	constexpr float ArcHeight = 200.f;
	const FName GrenadeSocketName = TEXT("grenade");
}

AGrenadeActor::AGrenadeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	M_StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	M_StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(M_StaticMeshComponent);
	M_ExplosionEffectScale = FVector::OneVector;
	M_ThrowDuration = GrenadeComponentConstants::ThrowTime;
}

void AGrenadeActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
}

void AGrenadeActor::ThrowAndExplode(const FGrenadeComponentSettings& DamageParams, const FVector& StartLocation,
                                    const FVector& EndLocation, const int32 OwningPlayer, UStaticMesh* OverrideMesh)
{
	CacheEffectData(DamageParams);
	M_StartLocation = StartLocation;
	M_EndLocation = EndLocation;

	SetGrenadeMesh(OverrideMesh);

	SetActorLocation(StartLocation);
	SetActorHiddenInGame(false);
	StartThrowTimer();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_ExplosionTimerHandle,
			FTimerDelegate::CreateUObject(this, &AGrenadeActor::OnExplode, DamageParams, OwningPlayer),
			M_ThrowDuration,
			false);
	}
}

void AGrenadeActor::ResetGrenade()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ThrowTimerHandle);
		World->GetTimerManager().ClearTimer(M_ExplosionTimerHandle);
	}
	SetActorHiddenInGame(true);
}

void AGrenadeActor::SetGrenadeMesh(UStaticMesh* OverrideMesh)
{
	if (not GetIsValidStaticMeshComponent())
	{
		return;
	}

	if (OverrideMesh)
	{
		M_StaticMeshComponent->SetStaticMesh(OverrideMesh);
	}
}

bool AGrenadeActor::GetIsValidStaticMeshComponent() const
{
	if (IsValid(M_StaticMeshComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_StaticMeshComponent", __func__, this);
	return false;
}

void AGrenadeActor::CacheEffectData(const FGrenadeComponentSettings& DamageParams)
{
	M_ExplosionEffect = DamageParams.ExplosionEffect;
	M_ExplosionSound = DamageParams.ExplosionSound;
	M_ExplosionEffectScale = DamageParams.ExplosionEffectScale;
}

void AGrenadeActor::StartThrowTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_ThrowTimerHandle,
			this,
			&AGrenadeActor::OnTickThrow,
			GrenadeComponentConstants::ThrowInterpInterval,
			true);
	}
}

void AGrenadeActor::OnTickThrow()
{
	if (not GetIsValidStaticMeshComponent())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Elapsed = World->GetTimerManager().GetTimerElapsed(M_ExplosionTimerHandle);
		const float Alpha = FMath::Clamp(Elapsed / M_ThrowDuration, 0.f, 1.f);
		const FVector NewLocation = CalculateArcLocation(Alpha);
		SetActorLocation(NewLocation);
	}
}

void AGrenadeActor::OnExplode(const FGrenadeComponentSettings DamageParams, const int32 OwningPlayer)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ThrowTimerHandle);
	}

	const FVector ExplosionLocation = M_EndLocation;
	PlayExplosionFX(ExplosionLocation);
	if (DamageParams.AoeRange > 0.f && DamageParams.Damage > 0.f)
	{
		const ETriggerOverlapLogic OverlapLogic = OwningPlayer == 1
			                                          ? ETriggerOverlapLogic::OverlapEnemy
			                                          : ETriggerOverlapLogic::OverlapPlayer;

		const TArray<TWeakObjectPtr<AActor>> ActorsToIgnore;

		FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
			this,
			ExplosionLocation,
			DamageParams.AoeRange,
			DamageParams.Damage,
			DamageParams.AoeDamageExponentReduction,
			DamageParams.FullArmorPen,
			DamageParams.ArmorPenFallOff,
			DamageParams.MaxArmorPen,
			ERTSDamageType::Kinetic,
			OverlapLogic,
			ActorsToIgnore);
	}

	ResetGrenade();
}

void AGrenadeActor::PlayExplosionFX(const FVector& ExplosionLocation) const
{
	if (M_ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_ExplosionSound, ExplosionLocation);
	}
	if (M_ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			M_ExplosionEffect,
			ExplosionLocation,
			FRotator::ZeroRotator,
			M_ExplosionEffectScale);
	}
}

FVector AGrenadeActor::CalculateArcLocation(const float Alpha) const
{
	const FVector FlatLerp = FMath::Lerp(M_StartLocation, M_EndLocation, Alpha);
	const float Parabola = -4 * GrenadeComponentConstants::ArcHeight * FMath::Square(Alpha - 0.5f)
		+ GrenadeComponentConstants::ArcHeight;
	FVector Offset = FlatLerp;
	Offset.Z += Parabola;
	return Offset;
}

UGrenadeComponent::UGrenadeComponent()
	: M_AbilityState(EGrenadeAbilityState::NotActive)
	  , M_GrenadesRemaining(0)
	  , M_OwningPlayer(INDEX_NONE)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGrenadeComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheOwningPlayer();
	BeginPlay_CreateGrenadePool();
	M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
}

void UGrenadeComponent::BeginDestroy()
{
	DestroyGrenadePool();
	Super::BeginDestroy();
}

void UGrenadeComponent::Init(ASquadController* SquadController)
{
	M_SquadController = SquadController;
	if (not GetIsValidSquadController())
	{
		return;
	}

	Init_SetAbilityWhenSquadDataLoaded();
}

void UGrenadeComponent::ExecuteThrowGrenade(const FVector& TargetLocation)
{
	if (not CanUseGrenades())
	{
		return;
	}
	ResetThrowSequenceState();
	ExecuteThrowGrenade_MoveOrThrow(TargetLocation);
}

void UGrenadeComponent::TerminateThrowGrenade()
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		return;
	}

	if (M_AbilityState == EGrenadeAbilityState::MovingToThrowPosition)
	{
		if (GetIsValidSquadController())
		{
			M_SquadController->StopMovement();
		}
		ResetThrowSequenceState();
	}
	M_AbilityState = EGrenadeAbilityState::NotActive;
	SetAbilityToThrowGrenade();
}

void UGrenadeComponent::CancelThrowGrenade()
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		return;
	}

	TerminateThrowGrenade();
}

EGrenadeAbilityType UGrenadeComponent::GetGrenadeAbilityType() const
{
	return M_Settings.GrenadeAbility;
}

bool UGrenadeComponent::GetIsValidSquadController() const
{
	if (M_SquadController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController", __func__, GetOwner());
	return false;
}

void UGrenadeComponent::OnSquadUnitArrivedAtThrowLocation(ASquadUnit* SquadUnit)
{
	if (M_AbilityState != EGrenadeAbilityState::MovingToThrowPosition)
	{
		return;
	}

	if (M_ThrowSequenceState.bM_HasStarted)
	{
		return;
	}

	if (not M_ThrowSequenceState.bM_IsWaitingForArrival)
	{
		return;
	}

	if (not IsValid(SquadUnit))
	{
		return;
	}

	StartThrowSequence();
}

void UGrenadeComponent::OnSquadUnitDied(ASquadUnit* DeadUnit)
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		return;
	}

	if (not IsValid(DeadUnit))
	{
		return;
	}

	FGrenadeThrowerState* ThrowerState = FindThrowerState(DeadUnit);
	if (not ThrowerState)
	{
		return;
	}

	ResetThrowerState(*ThrowerState);
	ThrowerState->M_GrenadesRemaining = 0;
	ThrowerState->M_SquadUnit = nullptr;

	if (not HasPendingThrows())
	{
		OnThrowFinished();
	}
}

bool UGrenadeComponent::GetIsValidRTSComponent() const
{
	if (not M_RTSComponent.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_RTSComponent", __func__, GetOwner());
		return false;
	}
	return true;
}

void UGrenadeComponent::BeginPlay_CacheOwningPlayer()
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_RTSComponent = M_SquadController->GetRTSComponent();
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	M_OwningPlayer = M_RTSComponent->GetOwningPlayer();
}

void UGrenadeComponent::Init_SetAbilityWhenSquadDataLoaded()
{
	TWeakObjectPtr<UGrenadeComponent> WeakThis(this);
	auto ApplyLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->SetAbilityToThrowGrenade();
	};

	M_SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(ApplyLambda, WeakThis);
}

void UGrenadeComponent::BeginPlay_CreateGrenadePool()
{
	const int32 GrenadesNeeded = M_Settings.SquadUnitsThrowing * M_Settings.GrenadesPerSquad;
	if (GrenadesNeeded <= 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (int32 i = 0; i < GrenadesNeeded; ++i)
		{
			AGrenadeActor* NewGrenade = World->SpawnActor<AGrenadeActor>();
			if (IsValid(NewGrenade))
			{
				M_GrenadePool.Add(NewGrenade);
				continue;
			}

			RTSFunctionLibrary::ReportError("Failed to spawn grenade actor during pool creation.");
		}
	}
}

void UGrenadeComponent::ExecuteThrowGrenade_MoveOrThrow(const FVector& TargetLocation)
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	CacheThrowSequenceLocations(TargetLocation);
	const float DistanceToTarget = FVector::Dist(M_SquadController->GetActorLocation(), TargetLocation);
	if (DistanceToTarget > M_Settings.ThrowRange)
	{
		M_AbilityState = EGrenadeAbilityState::MovingToThrowPosition;
		M_ThrowSequenceState.bM_IsWaitingForArrival = true;
		SetAbilityToCancel();
		M_SquadController->RequestSquadMoveForAbility(M_ThrowSequenceState.ThrowLocation, EAbilityID::IdThrowGrenade);
		return;
	}

	StartThrowSequence();
}

FVector UGrenadeComponent::CalculateThrowMoveLocation(const FVector& TargetLocation) const
{
	if (not GetIsValidSquadController())
	{
		return TargetLocation;
	}

	const FVector SquadLocation = M_SquadController->GetActorLocation();
	const FVector DirectionFromTarget = (SquadLocation - TargetLocation).GetSafeNormal();
	if (DirectionFromTarget.IsNearlyZero())
	{
		return SquadLocation;
	}

	return TargetLocation + (DirectionFromTarget * M_Settings.ThrowRange);
}

void UGrenadeComponent::CacheThrowSequenceLocations(const FVector& TargetLocation)
{
	M_ThrowSequenceState.TargetLocation = TargetLocation;
	M_ThrowSequenceState.ThrowLocation = CalculateThrowMoveLocation(TargetLocation);
	M_ThrowSequenceState.bM_IsWaitingForArrival = false;
	M_ThrowSequenceState.bM_HasStarted = false;
}

void UGrenadeComponent::StartThrowSequence()
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(TEXT("Throwing"), TEXT("Throwing"));
		return;
	}

	M_ThrowSequenceState.bM_HasStarted = true;
	M_ThrowSequenceState.bM_IsWaitingForArrival = false;
	M_AbilityState = EGrenadeAbilityState::Throwing;
	SetAbilityToResupplying();
	StartCooldown();
	M_GrenadesRemaining = FMath::Max(0, M_GrenadesRemaining - 1);

	if (M_OwningPlayer == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Owning player not cached before grenade throw. at function: StartThrowSequence"));
	}

	BuildThrowerStates();
	if (M_ThrowerStates.Num() <= 0)
	{
		OnThrowFinished();
		return;
	}

	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		StartThrowForThrower(ThrowerState);
	}
}

void UGrenadeComponent::BuildThrowerStates()
{
	M_ThrowerStates.Reset();

	if (not GetIsValidSquadController())
	{
		return;
	}

	if (M_Settings.SquadUnitsThrowing <= 0 || M_Settings.GrenadesPerSquad <= 0)
	{
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_SquadController->GetSquadUnitsChecked();
	int32 UnitsAdded = 0;

	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		FGrenadeThrowerState ThrowerState;
		ThrowerState.M_SquadUnit = SquadUnit;
		ThrowerState.M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
		M_ThrowerStates.Add(ThrowerState);

		UnitsAdded++;
		if (UnitsAdded >= M_Settings.SquadUnitsThrowing)
		{
			break;
		}
	}
}

void UGrenadeComponent::StartThrowForThrower(FGrenadeThrowerState& ThrowerState)
{
	if (ThrowerState.M_GrenadesRemaining <= 0)
	{
		return;
	}

	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		ThrowerState.M_GrenadesRemaining = 0;
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit();
	if (IsValid(AnimInstance))
	{
		AnimInstance->PlayGrenadeThrowMontage(M_Settings.GrenadeThrowMontageDuration);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Invalid anim instance for grenade throw on unit %s."), *SquadUnit->GetName()));
	}

	AGrenadeActor* Grenade = AcquireGrenade();
	if (not IsValid(Grenade))
	{
		Grenade = SpawnFallbackGrenade();
	}

	if (not IsValid(Grenade))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("No grenade available to throw after attempting fallback spawn."));
		ThrowerState.M_GrenadesRemaining = 0;
		return;
	}

	AttachGrenadeToThrower(ThrowerState, Grenade);
	if (not ThrowerState.M_AttachedGrenade.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to attach grenade to squad unit before throwing."));
		ThrowerState.M_GrenadesRemaining = 0;
		return;
	}

	StartThrowTimer(ThrowerState);
}

void UGrenadeComponent::StartThrowTimer(FGrenadeThrowerState& ThrowerState)
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	ClearThrowTimer(ThrowerState);
	const TWeakObjectPtr<ASquadUnit> WeakSquadUnit = ThrowerState.M_SquadUnit;
	World->GetTimerManager().SetTimer(
		ThrowerState.M_ThrowTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGrenadeComponent::OnThrowMontageFinished, WeakSquadUnit),
		M_Settings.GrenadeThrowMontageDuration,
		false);
}

void UGrenadeComponent::OnThrowMontageFinished(TWeakObjectPtr<ASquadUnit> SquadUnit)
{
	FGrenadeThrowerState* ThrowerState = FindThrowerState(SquadUnit);
	if (not ThrowerState)
	{
		return;
	}

	ClearThrowTimer(*ThrowerState);

	if (not ThrowerState->M_AttachedGrenade.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Grenade missing when throw montage finished."));
		ThrowerState->M_GrenadesRemaining = 0;
		if (not HasPendingThrows())
		{
			OnThrowFinished();
		}
		return;
	}

	if (not ThrowerState->M_SquadUnit.IsValid())
	{
		ResetThrowerState(*ThrowerState);
		if (not HasPendingThrows())
		{
			OnThrowFinished();
		}
		return;
	}

	const FVector StartLocation = GetThrowStartLocation(*ThrowerState);
	AGrenadeActor* Grenade = ThrowerState->M_AttachedGrenade.Get();
	DetachGrenadeFromThrower(*ThrowerState);
	Grenade->ThrowAndExplode(M_Settings, StartLocation, M_ThrowSequenceState.TargetLocation,
	                         M_OwningPlayer, M_Settings.GrenadeMesh);
	ThrowerState->M_AttachedGrenade = nullptr;
	ThrowerState->M_AttachedGrenadeScale = FVector::OneVector;
	ThrowerState->M_GrenadesRemaining = FMath::Max(0, ThrowerState->M_GrenadesRemaining - 1);

	if (ThrowerState->M_GrenadesRemaining > 0)
	{
		StartThrowForThrower(*ThrowerState);
		return;
	}

	if (not HasPendingThrows())
	{
		OnThrowFinished();
	}
}

void UGrenadeComponent::ClearThrowTimer(FGrenadeThrowerState& ThrowerState)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowerState.M_ThrowTimerHandle);
	}
}

void UGrenadeComponent::ResetThrowerState(FGrenadeThrowerState& ThrowerState)
{
	ClearThrowTimer(ThrowerState);

	if (ThrowerState.M_AttachedGrenade.IsValid())
	{
		DetachGrenadeFromThrower(ThrowerState);
		ThrowerState.M_AttachedGrenade->ResetGrenade();
	}

	ThrowerState.M_AttachedGrenade = nullptr;
	ThrowerState.M_AttachedGrenadeScale = FVector::OneVector;
}

void UGrenadeComponent::AttachGrenadeToThrower(FGrenadeThrowerState& ThrowerState, AGrenadeActor* Grenade)
{
	if (not IsValid(Grenade))
	{
		return;
	}

	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return;
	}

	ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	USkeletalMeshComponent* MeshComponent = SquadUnit->GetMesh();
	if (not IsValid(MeshComponent))
	{
		return;
	}

	if (not MeshComponent->DoesSocketExist(GrenadeComponentConstants::GrenadeSocketName))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Grenade socket missing on squad unit %s."), *SquadUnit->GetName()));
		return;
	}

	ThrowerState.M_AttachedGrenade = Grenade;
	ThrowerState.M_AttachedGrenadeScale = Grenade->GetActorScale3D();
	Grenade->SetGrenadeMesh(M_Settings.GrenadeMesh);
	Grenade->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		GrenadeComponentConstants::GrenadeSocketName);
	Grenade->SetActorScale3D(ThrowerState.M_AttachedGrenadeScale);
	Grenade->SetActorHiddenInGame(false);
}

void UGrenadeComponent::DetachGrenadeFromThrower(FGrenadeThrowerState& ThrowerState)
{
	if (not ThrowerState.M_AttachedGrenade.IsValid())
	{
		return;
	}

	AGrenadeActor* Grenade = ThrowerState.M_AttachedGrenade.Get();
	Grenade->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Grenade->SetActorScale3D(ThrowerState.M_AttachedGrenadeScale);
}

FVector UGrenadeComponent::GetThrowStartLocation(const FGrenadeThrowerState& ThrowerState) const
{
	if (not ThrowerState.M_SquadUnit.IsValid())
	{
		return FVector::ZeroVector;
	}

	const ASquadUnit* SquadUnit = ThrowerState.M_SquadUnit.Get();
	USkeletalMeshComponent* MeshComponent = SquadUnit->GetMesh();
	if (not IsValid(MeshComponent))
	{
		return SquadUnit->GetActorLocation();
	}

	if (not MeshComponent->DoesSocketExist(GrenadeComponentConstants::GrenadeSocketName))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Grenade socket missing on squad unit %s."), *SquadUnit->GetName()));
		return SquadUnit->GetActorLocation();
	}

	return MeshComponent->GetSocketLocation(GrenadeComponentConstants::GrenadeSocketName);
}

UGrenadeComponent::FGrenadeThrowerState* UGrenadeComponent::FindThrowerState(
	const TWeakObjectPtr<ASquadUnit>& SquadUnit)
{
	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		if (ThrowerState.M_SquadUnit == SquadUnit)
		{
			return &ThrowerState;
		}
	}
	return nullptr;
}

bool UGrenadeComponent::HasPendingThrows() const
{
	for (const FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		if (ThrowerState.M_SquadUnit.IsValid() && ThrowerState.M_GrenadesRemaining > 0)
		{
			return true;
		}
	}
	return false;
}

void UGrenadeComponent::ResetThrowSequenceState()
{
	for (FGrenadeThrowerState& ThrowerState : M_ThrowerStates)
	{
		ResetThrowerState(ThrowerState);
	}

	M_ThrowerStates.Reset();
	M_ThrowSequenceState = FGrenadeThrowSequenceState();
}

void UGrenadeComponent::OnThrowFinished()
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(UEnum::GetValueAsString(M_AbilityState), TEXT("ResupplyingGrenades"));
	}

	ResetThrowSequenceState();
	M_AbilityState = EGrenadeAbilityState::ResupplyingGrenades;
	if (GetIsValidSquadController())
	{
		M_SquadController->DoneExecutingCommand(EAbilityID::IdThrowGrenade);
	}
}

void UGrenadeComponent::StartCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_CooldownTimerHandle,
			this,
			&UGrenadeComponent::ResetCooldown,
			M_Settings.Cooldown,
			false);
	}
}

void UGrenadeComponent::ResetCooldown()
{
	if (M_AbilityState == EGrenadeAbilityState::MovingToThrowPosition)
	{
		ReportIllegalStateTransition(TEXT("MovingToThrowPosition"), TEXT("NotActive"));
	}
	if (M_AbilityState == EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(TEXT("Throwing"), TEXT("NotActive"));
	}

	M_GrenadesRemaining = M_Settings.GrenadesPerSquad;
	M_AbilityState = EGrenadeAbilityState::NotActive;
	SetAbilityToThrowGrenade();
}

bool UGrenadeComponent::CanUseGrenades() const
{
	if (M_AbilityState == EGrenadeAbilityState::Throwing || M_AbilityState == EGrenadeAbilityState::ResupplyingGrenades)
	{
		return false;
	}
	return M_GrenadesRemaining > 0;
}

AGrenadeActor* UGrenadeComponent::AcquireGrenade()
{
	for (TObjectPtr<AGrenadeActor>& Grenade : M_GrenadePool)
	{
		if (IsValid(Grenade) && Grenade->IsHidden())
		{
			return Grenade.Get();
		}
	}

	RTSFunctionLibrary::ReportError(
		"Grenade pool exhausted or invalid when attempting to acquire a grenade at function AquireGrenade");
	return nullptr;
}

AGrenadeActor* UGrenadeComponent::SpawnFallbackGrenade()
{
	if (UWorld* World = GetWorld())
	{
		AGrenadeActor* FallbackGrenade = World->SpawnActor<AGrenadeActor>();
		if (IsValid(FallbackGrenade))
		{
			M_GrenadePool.Add(FallbackGrenade);
			return FallbackGrenade;
		}
	}

	RTSFunctionLibrary::ReportError("Failed to spawn fallback grenade actor. SpawnFallBackGrenade");
	return nullptr;
}

void UGrenadeComponent::DestroyGrenadePool()
{
	for (TObjectPtr<AGrenadeActor>& Grenade : M_GrenadePool)
	{
		if (IsValid(Grenade))
		{
			Grenade->Destroy();
		}
	}
	M_GrenadePool.Empty();
}

FUnitAbilityEntry UGrenadeComponent::CreateGrenadeAbilityEntry(const EAbilityID AbilityId) const
{
	FUnitAbilityEntry AbilityEntry;
	AbilityEntry.AbilityId = AbilityId;
	AbilityEntry.CustomType = static_cast<int32>(M_Settings.GrenadeAbility);
	if (AbilityId == EAbilityID::IdThrowGrenade)
	{
		AbilityEntry.CooldownDuration = M_Settings.Cooldown;
	}
	return AbilityEntry;
}

void UGrenadeComponent::SetAbilityToResupplying()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdGrenadesResupplying);
	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::SetAbilityToThrowGrenade()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdThrowGrenade);

	if (M_SquadController->HasAbility(EAbilityID::IdGrenadesResupplying))
	{
		M_SquadController->SwapAbility(EAbilityID::IdGrenadesResupplying, GrenadeAbilityEntry);
		return;
	}

	if (not M_SquadController->HasAbility(EAbilityID::IdThrowGrenade))
	{
		M_SquadController->AddAbility(GrenadeAbilityEntry, M_Settings.PreferredIndex);
	}
	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::SetAbilityToCancel()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	const FUnitAbilityEntry GrenadeAbilityEntry = CreateGrenadeAbilityEntry(EAbilityID::IdCancelThrowGrenade);
	M_SquadController->SwapAbility(EAbilityID::IdThrowGrenade, GrenadeAbilityEntry);
}

void UGrenadeComponent::ReportIllegalStateTransition(const FString& FromState, const FString& ToState) const
{
	const FString Message = FString::Printf(TEXT("Illegal grenade state transition: %s to %s"), *FromState, *ToState);
	RTSFunctionLibrary::ReportError(Message);
}
