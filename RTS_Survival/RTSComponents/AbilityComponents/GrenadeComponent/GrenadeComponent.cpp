// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GrenadeComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

namespace GrenadeComponentConstants
{
	constexpr float ThrowTime = 2.f;
	constexpr float ThrowInterpInterval = 0.02f;
	constexpr float ArcHeight = 200.f;
	constexpr float InnerRadiusScale = 0.5f;
	constexpr float DamageFalloffExponent = 1.f;
}

AGrenadeActor::AGrenadeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	M_StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	M_StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(M_StaticMeshComponent);
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

	if (OverrideMesh)
	{
		M_StaticMeshComponent->SetStaticMesh(OverrideMesh);
	}

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

void AGrenadeActor::CacheEffectData(const FGrenadeComponentSettings& DamageParams)
{
	M_ExplosionEffect = DamageParams.ExplosionEffect;
	M_ExplosionSound = DamageParams.ExplosionSound;
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
	if (not IsValid(M_StaticMeshComponent))
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
	const float InnerRadius = DamageParams.AoeRange * GrenadeComponentConstants::InnerRadiusScale;
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		DamageParams.Damage,
		DamageParams.DamageAtOuterRadius,
		ExplosionLocation,
		InnerRadius,
		DamageParams.AoeRange,
		GrenadeComponentConstants::DamageFalloffExponent,
		nullptr,
		TArray<AActor*>(),
		this,
		nullptr,
		ECC_Visibility);

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
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, M_ExplosionEffect, ExplosionLocation);
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

bool UGrenadeComponent::GetIsValidSquadController() const
{
	if (M_SquadController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController", __func__, GetOwner());
	return false;
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

	const float DistanceToTarget = FVector::Dist(M_SquadController->GetActorLocation(), TargetLocation);
	if (DistanceToTarget > M_Settings.ThrowRange)
	{
		M_AbilityState = EGrenadeAbilityState::MovingToThrowPosition;
		SetAbilityToCancel();
		M_SquadController->RequestSquadMoveForAbility(TargetLocation, EAbilityID::IdThrowGrenade);
		return;
	}

	StartThrowSequence(TargetLocation);
}

void UGrenadeComponent::StartThrowSequence(const FVector& TargetLocation)
{
	if (M_AbilityState == EGrenadeAbilityState::MovingToThrowPosition)
	{
		ReportIllegalStateTransition(TEXT("MovingToThrowPosition"), TEXT("Throwing"));
	}

	M_AbilityState = EGrenadeAbilityState::Throwing;
	SetAbilityToResupplying();
	StartCooldown();
	M_GrenadesRemaining = FMath::Max(0, M_GrenadesRemaining - 1);

	if (M_OwningPlayer == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Owning player not cached before grenade throw. at function: StartThrowSequence"));
	}

	for (int32 i = 0; i < M_Settings.SquadUnitsThrowing; ++i)
	{
		AGrenadeActor* Grenade = AcquireGrenade();
		if (not IsValid(Grenade))
		{
			Grenade = SpawnFallbackGrenade();
		}

		if (not IsValid(Grenade))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("No grenade available to throw after attempting fallback spawn."));
			continue;
		}

		const FVector StartLocation = GetOwner()->GetActorLocation();
		Grenade->ThrowAndExplode(M_Settings, StartLocation, TargetLocation, M_OwningPlayer, M_Settings.GrenadeMesh);
	}

	OnThrowFinished();
}

void UGrenadeComponent::OnThrowFinished()
{
	if (M_AbilityState != EGrenadeAbilityState::Throwing)
	{
		ReportIllegalStateTransition(UEnum::GetValueAsString(M_AbilityState), TEXT("ResupplyingGrenades"));
	}

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

void UGrenadeComponent::SetAbilityToResupplying()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, EAbilityID::IdGrenadesResupplying);
}

void UGrenadeComponent::SetAbilityToThrowGrenade()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	if (not M_SquadController->HasAbility(EAbilityID::IdThrowGrenade))
	{
		M_SquadController->AddAbility(EAbilityID::IdThrowGrenade);
	}
	M_SquadController->SwapAbility(EAbilityID::IdGrenadesResupplying, EAbilityID::IdThrowGrenade);
	M_SquadController->SwapAbility(EAbilityID::IdCancelThrowGrenade, EAbilityID::IdThrowGrenade);
}

void UGrenadeComponent::SetAbilityToCancel()
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	M_SquadController->SwapAbility(EAbilityID::IdThrowGrenade, EAbilityID::IdCancelThrowGrenade);
}

void UGrenadeComponent::ReportIllegalStateTransition(const FString& FromState, const FString& ToState) const
{
	const FString Message = FString::Printf(TEXT("Illegal grenade state transition: %s to %s"), *FromState, *ToState);
	RTSFunctionLibrary::ReportError(Message);
}
