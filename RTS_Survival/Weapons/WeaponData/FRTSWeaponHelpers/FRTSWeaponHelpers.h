#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

class UBombComponent;
class AAircraftMaster;
class ATankMaster;
enum class ERTSDeathType : uint8;
class ASmallArmsProjectileManager;
class UWeaponState;
class UArmorCalculation;

namespace FRTSWeaponHelpers
{
	UArmorCalculation* GetArmorAndActorOrParentFromHit(const FHitResult& HitResult, AActor*& OutHitActor);

	TArray<UWeaponState*> GetWeaponsMountedOnTank(const ATankMaster* Tank);

	TArray<UWeaponState*> GetWeaponsMountedOnAircraft(const AAircraftMaster* Aircraft,
	                                                         UBombComponent*& OutBombCompPtr);

	AActor* GetHitActorAdjustedForChildActorComponents(AActor* OriginalHitActor);

	void SetupProjectileManagerForWeapon(UWeaponState* Weapon, ASmallArmsProjectileManager* ProjectileManager);

	// Returns only a non null if RTS ISValid; checks for parent actor as well.
	FORCEINLINE AActor* GetRTSValidHitActor(const FHitResult& HitResult)
	{
		AActor* OutHitActor = HitResult.GetActor();
		const bool bIsValid = RTSFunctionLibrary::RTSIsValid(OutHitActor);
		if (not bIsValid)
		{
			return nullptr;
		}
		if (OutHitActor->GetParentActor() && RTSFunctionLibrary::RTSIsValid(OutHitActor))
		{
			OutHitActor = HitResult.GetActor()->GetParentActor();
		}
		return OutHitActor;
	}

	ERTSDeathType TranslateDamageIntoDeathType(const ERTSDamageType DamageType);

	bool GetAdjustedRangeIfFlameThrowerPresent(const TArray<UWeaponState*>& Weapons, float& OutAdjustedRange);

	FVector GetTraceEndWithAccuracy(
		const FVector& LaunchLocation,
		const FVector& Direction,
		float RangeCm,
		int32 Accuracy01to100,
		bool bIsAircraftWeapon);

	FRotator ApplyAccuracyDeviationForProjectile(const FRotator& BaseRotation, const int32 Accuracy);

	FVector ApplyAccuracyDeviationForArchWeapon(const FVector& TargetLocation, const int32 Accuracy);

	float GetDamageWithFlux(const float BaseDamage, const int32 DamageFluxPercentage);

	void Debug_ResistancesAtLocation(const FVector& Location, const UObject* WorldContextObject,
	                                 const FString& Text, const FColor Color = FColor::White);
	void Debug_Resistances(const FString& Text, const FColor Color = FColor::White);

	FORCEINLINE TSubclassOf<UDamageType> GetDamageTypeClass(const ERTSDamageType Type)
	{
		switch (Type)
		{
		default:
		case ERTSDamageType::None:
			{
				RTSFunctionLibrary::ReportError("No damage type set on weapon, defaulting to kinetic.");
				return UKineticDamageType::StaticClass();
			}
		case ERTSDamageType::Kinetic: return UKineticDamageType::StaticClass();
		case ERTSDamageType::Fire: return UFireDamageType::StaticClass();
		case ERTSDamageType::Laser: return ULaserDamageType::StaticClass();
		case ERTSDamageType::Radiation: return URadiationDamageType::StaticClass();
		}
	}

	FORCEINLINE FDamageEvent MakeBasicDamageEvent(const ERTSDamageType Type)
	{
		FDamageEvent E;
		E.DamageTypeClass = GetDamageTypeClass(Type);
		return E;
	}

	FORCEINLINE FPointDamageEvent MakePointDamageEvent(
		const ERTSDamageType Type,
		const float Damage,
		const FHitResult& Hit,
		const FVector& ShotDir = FVector::ZeroVector)
	{
		FPointDamageEvent E;
		E.DamageTypeClass = GetDamageTypeClass(Type);
		E.Damage = Damage;
		E.HitInfo = Hit;
		E.ShotDirection = ShotDir;
		return E;
	}

	FORCEINLINE ERTSDamageType GetRTSDamageType(const FDamageEvent& DamageEvent)
	{
		// Default to kinetic if missing or not our custom type
		if (!DamageEvent.DamageTypeClass ||
			!DamageEvent.DamageTypeClass->IsChildOf(URTSDamageType::StaticClass()))
		{
			return ERTSDamageType::Kinetic;
		}

		const URTSDamageType* Dflt = Cast<URTSDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
		if (!Dflt || Dflt->RTSDamageType == ERTSDamageType::None)
		{
			return ERTSDamageType::Kinetic;
		}
		return Dflt->RTSDamageType;
	}
}
