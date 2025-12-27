// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTS_AOE.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Physics/AsyncTrace.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

FRTS_AOE::FRTS_AOE() = default;

FRTS_AOE::~FRTS_AOE() = default;

void FRTS_AOE::ApplyToActorsAsync(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius,
	const ETriggerOverlapLogic OverlapLogic,
	TFunction<void(AActor&)> OnActorFound)
{
	if (not OnActorFound)
	{
		return;
	}

	StartAsyncSphereSweep(
		InstigatorActor,
		Epicenter,
		Radius,
		BuildObjectQueryParams(OverlapLogic),
		[OnActorFound = MoveTemp(OnActorFound)](TArray<FHitResult>&& HitResults)
		{
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}

				OnActorFound(*HitActor);
			}
		});
}

void FRTS_AOE::DealDamageInRadiusAsync(
	AActor* DamageCauser,
	const FVector& Epicenter,
	const float Radius,
	const float BaseDamage,
	const float DamageFalloffExponent,
	const ERTSDamageType DamageType,
	const ETriggerOverlapLogic OverlapLogic)
{
	if (not IsValid(DamageCauser))
	{
		return;
	}

	const float SafeRadius = FMath::Max(Radius, 0.f);
	if (SafeRadius <= 0.f)
	{
		return;
	}

	const float SafeFalloffExponent = FMath::Max(DamageFalloffExponent, 0.f);
	const TWeakObjectPtr<AActor> WeakDamageCauser = DamageCauser;
	StartAsyncSphereSweep(
		DamageCauser,
		Epicenter,
		SafeRadius,
		BuildObjectQueryParams(OverlapLogic),
		[WeakDamageCauser, Epicenter, BaseDamage, SafeRadius, SafeFalloffExponent, DamageType](
			TArray<FHitResult>&& HitResults)
		{
			if (not WeakDamageCauser.IsValid())
			{
				return;
			}

			FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(DamageType);
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}

				const float DistanceFromEpicenter = FVector::Dist(Epicenter, Hit.Location);
				const float NormalisedDistance = FMath::Clamp(DistanceFromEpicenter / SafeRadius, 0.f, 1.f);
				const float DamageScale = FMath::Pow(1.f - NormalisedDistance, SafeFalloffExponent);
				const float DamageToApply = BaseDamage * DamageScale;
				if (DamageToApply <= 0.f)
				{
					continue;
				}

				HitActor->TakeDamage(
					DamageToApply,
					DamageEvent,
					WeakDamageCauser->GetInstigatorController(),
					WeakDamageCauser.Get()
				);
			}
		});
}

void FRTS_AOE::ApplyBehaviourInRadiusAsync(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius,
	const TSubclassOf<UBehaviour> BehaviourClass,
	const ETriggerOverlapLogic OverlapLogic)
{
	if (not BehaviourClass)
	{
		return;
	}

	StartAsyncSphereSweep(
		InstigatorActor,
		Epicenter,
		Radius,
		BuildObjectQueryParams(OverlapLogic),
		[BehaviourClass](TArray<FHitResult>&& HitResults)
		{
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}

				UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
				if (not IsValid(BehaviourComponent))
				{
					continue;
				}

				BehaviourComponent->AddBehaviour(BehaviourClass);
			}
		});
}

FCollisionObjectQueryParams FRTS_AOE::BuildObjectQueryParams(const ETriggerOverlapLogic OverlapLogic)
{
	FCollisionObjectQueryParams ObjectQueryParams;
	if (OverlapLogic == ETriggerOverlapLogic::OverlapEnemy || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	}

	if (OverlapLogic == ETriggerOverlapLogic::OverlapPlayer || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	}

	return ObjectQueryParams;
}

void FRTS_AOE::StartAsyncSphereSweep(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius,
	const FCollisionObjectQueryParams& ObjectQueryParams,
	TFunction<void(TArray<FHitResult>&&)> OnSweepComplete)
{
	if (not OnSweepComplete)
	{
		return;
	}

	if (not IsValid(InstigatorActor))
	{
		return;
	}

	UWorld* World = InstigatorActor->GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	if (Radius <= 0.f)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTS_AOE_SphereSweep), false, InstigatorActor);
	QueryParams.AddIgnoredActor(InstigatorActor);

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

	FTraceDelegate TraceDelegate;
	const TWeakObjectPtr<AActor> WeakInstigator = InstigatorActor;

	TraceDelegate.BindLambda(
		[WeakInstigator, OnSweepComplete = MoveTemp(OnSweepComplete)](
			const FTraceHandle& /*TraceHandle*/,
			FTraceDatum& TraceDatum) mutable
		{
			TArray<FHitResult> HitResults = TraceDatum.OutHits;
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakInstigator, OnSweepComplete = MoveTemp(OnSweepComplete), HitResults = MoveTemp(HitResults)]()
				mutable
				{
					if (not WeakInstigator.IsValid())
					{
						return;
					}

					OnSweepComplete(MoveTemp(HitResults));
				});
		});

	World->AsyncSweepByObjectType(
		EAsyncTraceType::Multi,
		Epicenter,
		Epicenter,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}
