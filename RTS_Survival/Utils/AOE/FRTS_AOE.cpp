// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FRTS_AOE.h"

#include "Async/Async.h"
#include "Containers/Set.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleDamageEvent.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldComponent.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldOwner/ShieldOwner.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace
{
	float CalculateDamageAtDistance(
		const float BaseDamage,
		const float SafeRadius,
		const float SafeFalloffExponent,
		const float DistanceFromEpicenter)
	{
		const float NormalisedDistance = FMath::Clamp(DistanceFromEpicenter / SafeRadius, 0.f, 1.f);
		const float DamageScale = FMath::Pow(1.f - NormalisedDistance, SafeFalloffExponent);
		return BaseDamage * DamageScale;
	}

	float CalculateDamageWithFalloff(
		const FVector& Epicenter,
		const float BaseDamage,
		const float SafeRadius,
		const float SafeFalloffExponent,
		const FVector& HitLocation)
	{
		return CalculateDamageAtDistance(BaseDamage, SafeRadius, SafeFalloffExponent,
		                                 FVector::Dist(Epicenter, HitLocation));
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

	// Game-thread serial giving every opted-in explosion its own vehicle module roll identity.
	uint32 GModuleSplashExplosionSerial = 0;

	/**
	 * @brief Applies AOE damage through a module damage event so external vehicle modules resolve once,
	 * after the hull damage, from the attenuated damage this victim actually received.
	 */
	void ApplyDamageWithModuleSplash(
		AActor& HitActor,
		UArmorCalculation& ArmorCalculation,
		const float AttenuatedDamage,
		const FVector& Epicenter,
		const uint32 ExplosionId,
		const FDamageEvent& BasicDamageEvent,
		const TWeakObjectPtr<AActor>& WeakDamageCauser,
		const EWeaponShellType DamageShellType)
	{
		FVehicleModuleDamageEvent ModuleDamageEvent = FVehicleModuleDamageEvent::MakeExplosionEvent(
			&ArmorCalculation, EVehicleModuleDelivery::Splash, Epicenter, AttenuatedDamage, ExplosionId,
			BasicDamageEvent.DamageTypeClass, DamageShellType);
		ApplyDamageToActor(HitActor, AttenuatedDamage, ModuleDamageEvent, WeakDamageCauser);
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

	bool GetWasDamageAbsorbedByShield(
		AActor& HitActor,
		const float DamageToApply,
		const EShieldDamageSource ShieldDamageSource,
		const FVector& ImpactLocation)
	{
		IShieldOwner* ShieldOwner = Cast<IShieldOwner>(&HitActor);
		if (ShieldOwner == nullptr)
		{
			return false;
		}

		UShieldComponent* ShieldComponent = ShieldOwner->GetShield();
		if (not IsValid(ShieldComponent))
		{
			return false;
		}

		FShieldDamageRequest DamageRequest;
		DamageRequest.BaseDamage = DamageToApply;
		DamageRequest.DamageSource = ShieldDamageSource;
		DamageRequest.ImpactLocation = ImpactLocation;
		return ShieldComponent->ApplyShieldDamage(DamageRequest) == EShieldDamageResult::Absorbed;
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
	const EShieldDamageSource ShieldDamageSource,
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
		[WeakDamageCauser, Epicenter, BaseDamage, SafeRadius, SafeFalloffExponent, DamageType, ShieldDamageSource](
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
				const FVector HitLocation = HitActor->GetActorLocation();

				const float DamageToApply = CalculateDamageWithFalloff(
					Epicenter,
					BaseDamage,
					SafeRadius,
					SafeFalloffExponent,
					HitLocation);
				if (DamageToApply <= 0.f)
				{
					continue;
				}
				if (GetWasDamageAbsorbedByShield(
					*HitActor,
					DamageToApply,
					ShieldDamageSource,
					HitLocation))
				{
					continue;
				}

				ApplyDamageToActor(*HitActor, DamageToApply, DamageEvent, WeakDamageCauser);
			}
		});
}

namespace
{
	// Everything one rear-armor explosion needs per victim; captured by value into the async sweep callback.
	struct FRearArmorAoeParams
	{
		TWeakObjectPtr<AActor> WeakDamageCauser;
		FVector Epicenter = FVector::ZeroVector;
		float BaseDamage = 0.f;
		float SafeRadius = 0.f;
		float SafeFalloffExponent = 0.f;
		float SafeFullArmorPen = 0.f;
		float SafeArmorPenFalloff = 0.f;
		float SafeMaxArmorPen = 0.f;
		ERTSDamageType DamageType = ERTSDamageType::Kinetic;
		EShieldDamageSource ShieldDamageSource = EShieldDamageSource::AreaOfEffect;
		bool bDamagesVehicleModules = false;
		uint32 ModuleExplosionId = 0;
		EWeaponShellType ModuleDamageShellType = static_cast<EWeaponShellType>(0);
	};

	void ApplyRearArmorAoeToActor(
		AActor& HitActor,
		const float DistanceFromEpicenter,
		const FRearArmorAoeParams& Params,
		FDamageEvent& DamageEvent)
	{
		const FVector EnemyLocation = HitActor.GetActorLocation();
		const float DamageToApply = CalculateDamageAtDistance(
			Params.BaseDamage, Params.SafeRadius, Params.SafeFalloffExponent, DistanceFromEpicenter);
		if (DamageToApply <= 0.f)
		{
			return;
		}
		if (GetWasDamageAbsorbedByShield(HitActor, DamageToApply, Params.ShieldDamageSource, EnemyLocation))
		{
			return;
		}

		UArmorCalculation* ArmorCalculation = HitActor.FindComponentByClass<UArmorCalculation>();
		if (not IsValid(ArmorCalculation))
		{
			if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
			{
				DrawDebugString(HitActor.GetWorld(), EnemyLocation + FVector(0, 0, 100),
				                FString::Printf(TEXT("No Armor found, dmg: %.1f"), DamageToApply), nullptr,
				                FColor::White, 5.f);
			}
			ApplyDamageToActor(HitActor, DamageToApply, DamageEvent, Params.WeakDamageCauser);
			return;
		}

		const float RearArmor = ArmorCalculation->GetRearArmor();
		const float ArmorDamageMultiplier = CalculateRearArmorDamageMultiplier(
			RearArmor,
			Params.SafeFullArmorPen,
			Params.SafeMaxArmorPen,
			Params.SafeArmorPenFalloff);
		const float AdjustedDamage = DamageToApply * ArmorDamageMultiplier;

		if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
		{
			DrawDebugString(
				HitActor.GetWorld(),
				EnemyLocation + FVector(0.f, 0.f, 100.f),
				FString::Printf(
					TEXT("Rear Armor: %.1f | dmg x%.2f | %.1f -> %.1f"),
					RearArmor,
					ArmorDamageMultiplier,
					DamageToApply,
					AdjustedDamage),
				nullptr,
				FColor::White,
				5.f);
		}

		if (AdjustedDamage <= 0.f)
		{
			return;
		}

		// Module splash is opt-in per weapon adapter; the default Ignore keeps ICBMs and bombs hull-only.
		if (Params.bDamagesVehicleModules)
		{
			ApplyDamageWithModuleSplash(HitActor, *ArmorCalculation, AdjustedDamage, Params.Epicenter,
			                            Params.ModuleExplosionId, DamageEvent, Params.WeakDamageCauser,
			                            Params.ModuleDamageShellType);
			return;
		}
		ApplyDamageToActor(HitActor, AdjustedDamage, DamageEvent, Params.WeakDamageCauser);
	}

	/**
	 * @brief Merges the sweep's hits to one entry per actor at the smallest collision distance.
	 * A vehicle with several colliding components is otherwise damaged once per component.
	 */
	TMap<TWeakObjectPtr<AActor>, float> GetClosestCollisionDistancePerActor(
		const TArray<FHitResult>& HitResults,
		const FVector& Epicenter)
	{
		TMap<TWeakObjectPtr<AActor>, float> DistancePerActor;
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (not IsValid(HitActor))
			{
				continue;
			}
			EAoeDistanceSource DistanceSource = EAoeDistanceSource::ActorOrigin;
			const float Distance = FRTS_AOE::GetFalloffDistance(*HitActor, Hit.GetComponent(), Epicenter, DistanceSource);
			float* KnownDistance = DistancePerActor.Find(HitActor);
			if (KnownDistance == nullptr)
			{
				DistancePerActor.Add(HitActor, Distance);
				continue;
			}
			*KnownDistance = FMath::Min(*KnownDistance, Distance);
		}
		return DistancePerActor;
	}
}

float FRTS_AOE::GetFalloffDistance(
	const AActor& HitActor,
	const UPrimitiveComponent* HitComponent,
	const FVector& Epicenter,
	EAoeDistanceSource& OutSource)
{
	if (not IsValid(HitComponent))
	{
		OutSource = EAoeDistanceSource::ActorOrigin;
		return FVector::Dist(Epicenter, HitActor.GetActorLocation());
	}

	FVector ClosestPoint;
	const float CollisionDistance = HitComponent->GetClosestPointOnCollision(Epicenter, ClosestPoint);
	if (CollisionDistance >= 0.f)
	{
		OutSource = EAoeDistanceSource::Collision;
		return CollisionDistance;
	}

	// Negative means no simple collision to query; the bounds box keeps the hull distance instead of the origin.
	OutSource = EAoeDistanceSource::BoundsBox;
	return FMath::Sqrt(HitComponent->Bounds.GetBox().ComputeSquaredDistanceToPoint(Epicenter));
}

float FRTS_AOE::GetRearArmorDamageMultiplier(
	const float RearArmor,
	const float FullDmgArmorPen,
	const float ArmorPenFallOff,
	const float MaxArmorPen)
{
	const float SafeFullArmorPen = FMath::Max(FullDmgArmorPen, 0.f);
	return CalculateRearArmorDamageMultiplier(
		RearArmor,
		SafeFullArmorPen,
		FMath::Max(MaxArmorPen, SafeFullArmorPen),
		FMath::Max(ArmorPenFallOff, 0.f));
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
	const EShieldDamageSource ShieldDamageSource,
	const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore,
	const EVehicleModuleSplashPolicy ModuleSplashPolicy,
	const EWeaponShellType ModuleDamageShellType,
	const EAoeFalloffDistance FalloffDistance)
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

	FRearArmorAoeParams Params;
	Params.WeakDamageCauser = DamageCauser;
	Params.Epicenter = Epicenter;
	Params.BaseDamage = BaseDamage;
	Params.SafeRadius = SafeRadius;
	Params.SafeFalloffExponent = FMath::Max(DamageFalloffExponent, 0.f);
	Params.SafeFullArmorPen = FMath::Max(FullDmgArmorPen, 0.f);
	Params.SafeMaxArmorPen = FMath::Max(MaxArmorPen, Params.SafeFullArmorPen);
	Params.SafeArmorPenFalloff = FMath::Max(ArmorPenFallOff, 0.f);
	Params.DamageType = DamageType;
	Params.ShieldDamageSource = ShieldDamageSource;
	Params.bDamagesVehicleModules = ModuleSplashPolicy == EVehicleModuleSplashPolicy::DamageExternalModules;
	Params.ModuleExplosionId = Params.bDamagesVehicleModules ? ++GModuleSplashExplosionSerial : 0;
	Params.ModuleDamageShellType = ModuleDamageShellType;
	StartAsyncSphereSweep(
		DamageCauser,
		Epicenter,
		SafeRadius,
		BuildObjectQueryParams(OverlapLogic),
		ActorsToIgnore,
		[Params, FalloffDistance](TArray<FHitResult>&& HitResults)
		{
			FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(Params.DamageType);
			if (FalloffDistance == EAoeFalloffDistance::ClosestCollision)
			{
				for (const TPair<TWeakObjectPtr<AActor>, float>& ActorDistance :
				     GetClosestCollisionDistancePerActor(HitResults, Params.Epicenter))
				{
					// An earlier victim's damage can destroy another victim of the same explosion.
					if (AActor* HitActor = ActorDistance.Key.Get())
					{
						ApplyRearArmorAoeToActor(*HitActor, ActorDistance.Value, Params, DamageEvent);
					}
				}
				return;
			}
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					continue;
				}
				const float DistanceToOrigin = FVector::Dist(Params.Epicenter, HitActor->GetActorLocation());
				ApplyRearArmorAoeToActor(*HitActor, DistanceToOrigin, Params, DamageEvent);
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
	const EShieldDamageSource ShieldDamageSource,
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
			ShieldDamageSource,
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
				const FVector HitLocation = HitActor->GetActorLocation();

				const float DamageToApply = CalculateDamageWithFalloff(
					Epicenter,
					BaseDamage,
					SafeRadius,
					SafeFalloffExponent,
					HitLocation);
				if (DamageToApply <= 0.f)
				{
					continue;
				}
				if (GetWasDamageAbsorbedByShield(
					*HitActor,
					DamageToApply,
					ShieldDamageSource,
					HitLocation))
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
	if (not OnSweepComplete || Radius < 85)
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
	if constexpr (DeveloperSettings::Debugging::GAOELibrary_Compile_DebugSymbols)
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
