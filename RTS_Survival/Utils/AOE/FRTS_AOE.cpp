// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTS_AOE.h"

#include "Async/Async.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace
{
	float CalculateDamageWithFalloff(
		const FVector& Epicenter,
		const float BaseDamage,
		const float SafeRadius,
		const float SafeFalloffExponent,
		const FHitResult& Hit)
	{
		const float DistanceFromEpicenter = FVector::Dist(Epicenter, Hit.Location);
		const float NormalisedDistance = FMath::Clamp(DistanceFromEpicenter / SafeRadius, 0.f, 1.f);
		const float DamageScale = FMath::Pow(1.f - NormalisedDistance, SafeFalloffExponent);
		return BaseDamage * DamageScale;
	}

	void ApplyDamageToActor(
		AActor& HitActor,
		const float DamageToApply,
		FDamageEvent& DamageEvent,
		const TWeakObjectPtr<AActor>& WeakDamageCauser)
	{
		if (DamageToApply <= 0.f)
		{
			return;
		}

		HitActor.TakeDamage(
			DamageToApply,
			DamageEvent,
			WeakDamageCauser->GetInstigatorController(),
			WeakDamageCauser.Get()
		);
	}

	float CalculateRearArmorDamageMultiplier(
		const float RearArmor,
		const float SafeFullArmorPen,
		const float SafeMaxArmorPen,
		const float ArmorPenFallOff)
	{
		if (RearArmor <= SafeFullArmorPen)
		{
			return 1.f;
		}

		if (RearArmor >= SafeMaxArmorPen)
		{
			return 0.f;
		}

		const float ArmorRange = SafeMaxArmorPen - SafeFullArmorPen;
		if (ArmorRange <= 0.f)
		{
			return 0.f;
		}

		const float ArmorProgress = (RearArmor - SafeFullArmorPen) / ArmorRange;
		const float DamageScaler = FMath::Pow(1.f - ArmorProgress, FMath::Max(ArmorPenFallOff, 0.f));
		return FMath::Clamp(DamageScaler, 0.f, 1.f);
	}
}

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
		TArray<TWeakObjectPtr<AActor>>(),
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
	const ETriggerOverlapLogic OverlapLogic,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore)
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
		ActorsToIgnore,
		[WeakDamageCauser, Epicenter, BaseDamage, SafeRadius, SafeFalloffExponent, DamageType](
		TArray<FHitResult>&& HitResults)
		{
			FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(DamageType);
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}

				const float DamageToApply = CalculateDamageWithFalloff(
					Epicenter,
					BaseDamage,
					SafeRadius,
					SafeFalloffExponent,
					Hit);
				if (DamageToApply <= 0.f)
				{
					continue;
				}

				ApplyDamageToActor(*HitActor, DamageToApply, DamageEvent, WeakDamageCauser);
			}
		});
}

void FRTS_AOE::DealDamageVsRearArmorInRadiusAsync(
	AActor* DamageCauser,
	const FVector& Epicenter,
	const float Radius,
	const float BaseDamage,
	const float DamageFalloffExponent,
	const float FullDmgArmorPen,
	const float ArmorPenFallOff,
	const float MaxArmorPen,
	const ERTSDamageType DamageType,
	const ETriggerOverlapLogic OverlapLogic,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore)
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
	const float SafeFullArmorPen = FMath::Max(FullDmgArmorPen, 0.f);
	const float SafeMaxArmorPen = FMath::Max(MaxArmorPen, SafeFullArmorPen);
	const float SafeArmorPenFalloff = FMath::Max(ArmorPenFallOff, 0.f);
	const TWeakObjectPtr<AActor> WeakDamageCauser = DamageCauser;
	StartAsyncSphereSweep(
		DamageCauser,
		Epicenter,
		SafeRadius,
		BuildObjectQueryParams(OverlapLogic),
		ActorsToIgnore,
		[
			WeakDamageCauser,
			Epicenter,
			BaseDamage,
			SafeRadius,
			SafeFalloffExponent,
			SafeFullArmorPen,
			SafeArmorPenFalloff,
			SafeMaxArmorPen,
			DamageType
		](TArray<FHitResult>&& HitResults)
		{
			FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(DamageType);
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}

				const float DamageToApply = CalculateDamageWithFalloff(
					Epicenter,
					BaseDamage,
					SafeRadius,
					SafeFalloffExponent,
					Hit);
				if (DamageToApply <= 0.f)
				{
					continue;
				}

				UArmorCalculation* ArmorCalculation = HitActor->FindComponentByClass<UArmorCalculation>();
				if (not IsValid(ArmorCalculation))
				{
					if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
					{
						DrawDebugString(HitActor->GetWorld(), HitActor->GetActorLocation() + FVector(0, 0, 100),
						                FString::Printf(TEXT("No Armor dmg: %.1f"), DamageToApply), nullptr,
							FColor::White, 5.f);
					}
					ApplyDamageToActor(*HitActor, DamageToApply, DamageEvent, WeakDamageCauser);
					continue;
				}

				const float RearArmor = ArmorCalculation->GetRearArmor();
				const float ArmorDamageMultiplier = CalculateRearArmorDamageMultiplier(
					RearArmor,
					SafeFullArmorPen,
					SafeMaxArmorPen,
					SafeArmorPenFalloff);
				if (ArmorDamageMultiplier <= 0.f)
				{
					continue;
				}
				if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
				{
					DrawDebugString(HitActor->GetWorld(), HitActor->GetActorLocation() + FVector(0, 0, 100),
					                FString::Printf(
						                TEXT("Rear Armor: %.1f, Multiplier: %.2f"), RearArmor, ArmorDamageMultiplier),
					                nullptr, FColor::White, 5.f);
				}

				const float AdjustedDamage = DamageToApply * ArmorDamageMultiplier;
				if (AdjustedDamage <= 0.f)
				{
					continue;
				}

				ApplyDamageToActor(*HitActor, AdjustedDamage, DamageEvent, WeakDamageCauser);
			}
		});
}

void FRTS_AOE::DealDamageAndCustomArmorHandlingInRadiusAsync(
	AActor* DamageCauser,
	const FVector& Epicenter,
	const float Radius,
	const float BaseDamage,
	const float DamageFalloffExponent,
	const ERTSDamageType DamageType,
	const ETriggerOverlapLogic OverlapLogic,
	TFunction<void(UArmorCalculation*, AActor*)> OnArmorComponentHit,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore)
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
		ActorsToIgnore,
		[
			WeakDamageCauser,
			Epicenter,
			BaseDamage,
			SafeRadius,
			SafeFalloffExponent,
			DamageType,
			OnArmorComponentHit = MoveTemp(OnArmorComponentHit)
		](TArray<FHitResult>&& HitResults) mutable
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

				const float DamageToApply = CalculateDamageWithFalloff(
					Epicenter,
					BaseDamage,
					SafeRadius,
					SafeFalloffExponent,
					Hit);
				if (DamageToApply <= 0.f)
				{
					continue;
				}

				UArmorCalculation* ArmorCalculation = HitActor->FindComponentByClass<UArmorCalculation>();
				if (IsValid(ArmorCalculation) && OnArmorComponentHit)
				{
					OnArmorComponentHit(ArmorCalculation, HitActor);
					continue;
				}

				ApplyDamageToActor(*HitActor, DamageToApply, DamageEvent, WeakDamageCauser);
			}
		});
}

void FRTS_AOE::ApplyBehaviourInRadiusAsync(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius,
	const TSubclassOf<UBehaviour> BehaviourClass,
	const ETriggerOverlapLogic OverlapLogic,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore)
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
		ActorsToIgnore,
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
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);

	return ObjectQueryParams;
}

void FRTS_AOE::StartAsyncSphereSweep(
	AActor* InstigatorActor,
	const FVector& Epicenter,
	const float Radius,
	const FCollisionObjectQueryParams& ObjectQueryParams,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore,
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
	if (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
	{
		DrawDebugSphere(World, Epicenter, Radius, 32, FColor::Red, false, 5.f);
	}


	if (Radius <= 0.f)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTS_AOE_SphereSweep), false, InstigatorActor);
	QueryParams.AddIgnoredActor(InstigatorActor);
	TSet<const AActor*> IgnoredActors;
	IgnoredActors.Add(InstigatorActor);
	for (const TWeakObjectPtr<AActor>& ActorToIgnore : ActorsToIgnore)
	{
		if (not ActorToIgnore.IsValid())
		{
			continue;
		}

		AActor* Actor = ActorToIgnore.Get();
		QueryParams.AddIgnoredActor(Actor);
		IgnoredActors.Add(Actor);
	}

	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

	FTraceDelegate TraceDelegate;
	const TWeakObjectPtr<AActor> WeakInstigator = InstigatorActor;

	TraceDelegate.BindLambda(
		[WeakInstigator, OnSweepComplete = MoveTemp(OnSweepComplete), IgnoredActors](
		const FTraceHandle& /*TraceHandle*/,
		FTraceDatum& TraceDatum) mutable
		{
			TArray<FHitResult> HitResults = TraceDatum.OutHits;
			HitResults.RemoveAll([&IgnoredActors](const FHitResult& Hit)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					return true;
				}

				return IgnoredActors.Contains(HitActor);
			});
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
		&TraceDelegate,
		/*UserData*/ 0u
	);
}
