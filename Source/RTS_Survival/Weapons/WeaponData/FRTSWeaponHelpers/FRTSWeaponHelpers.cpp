#include "FRTSWeaponHelpers.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"

UArmorCalculation* FRTSWeaponHelpers::GetArmorAndActorOrParentFromHit(const FHitResult& HitResult, AActor*& OutHitActor)
{
	UArmorCalculation* ArmorCalculationComp = HitResult.GetActor()->FindComponentByClass<UArmorCalculation>();
	OutHitActor = HitResult.GetActor();
	if (not ArmorCalculationComp && HitResult.GetActor() && HitResult.GetActor()->GetParentActor())
	{
		ArmorCalculationComp = HitResult.GetActor()->GetParentActor()->FindComponentByClass<UArmorCalculation>();
		// In this case we hit an armor component on a child actor.
		OutHitActor = HitResult.GetActor()->GetParentActor();
	}
	return ArmorCalculationComp;
}

AActor* FRTSWeaponHelpers::GetHitActorAdjustedForChildActorComponents(AActor* OriginalHitActor)
{
	if (not IsValid(OriginalHitActor))
	{
		return nullptr;
	}
	if (IsValid(OriginalHitActor->GetParentActor()))
	{
		return OriginalHitActor->GetParentActor();
	}
	return OriginalHitActor;
}

void FRTSWeaponHelpers::SetupProjectileManagerForWeapon(UWeaponState* Weapon,
                                                        ASmallArmsProjectileManager* ProjectileManager)
{
	if (not IsValid(Weapon))
	{
		return;
	}
	if (not IsValid(ProjectileManager))
	{
		const FString WeaponName = IsValid(Weapon) ? Weapon->GetName() : TEXT("InvalidWeapon");
		RTSFunctionLibrary::ReportError("Failed to setup projectile manager for weapon: " + WeaponName
			+ "\n as provided manager is not valid!"
			"\n See FRTSWeaponHelpers::SetupProjectileManagerForWeapon");
		return;
	}
	UWeaponStateTrace* TraceWeapon = Cast<UWeaponStateTrace>(Weapon);

	if (IsValid(TraceWeapon))
	{
		TraceWeapon->SetupProjectileManager(ProjectileManager);
		return;
	}
	UWeaponStateProjectile* ProjectileWeapon = Cast<UWeaponStateProjectile>(Weapon);
	if (IsValid(ProjectileWeapon))
	{
		ProjectileWeapon->SetupProjectileManager(ProjectileManager);
		return;
	}
	UWeaponStateMultiTrace* MultiTraceWeapon = Cast<UWeaponStateMultiTrace>(Weapon);
	if (IsValid(MultiTraceWeapon))
	{
		MultiTraceWeapon->SetupProjectileManager(ProjectileManager);
		return;
	}
	UWeaponStateMultiProjectile* MultiProjectileWeapon = Cast<UWeaponStateMultiProjectile>(Weapon);
	if (IsValid(MultiProjectileWeapon))
	{
		MultiProjectileWeapon->SetupProjectileManager(ProjectileManager);
		return;
	}
}

ERTSDeathType FRTSWeaponHelpers::TranslateDamageIntoDeathType(const ERTSDamageType DamageType)
{
	switch (DamageType)
	{
	// Falls through.
	case ERTSDamageType::None:
	case ERTSDamageType::Kinetic:
		return ERTSDeathType::Kinetic;
	// Falls through.
	case ERTSDamageType::Fire:
	case ERTSDamageType::Laser:
		return ERTSDeathType::FireOrLaser;
	case ERTSDamageType::Radiation:
		return ERTSDeathType::Radiation;
	}
	return ERTSDeathType::Kinetic;
}

bool FRTSWeaponHelpers::GetAdjustedRangeIfFlameThrowerPresent(const TArray<UWeaponState*>& Weapons,
                                                              float& OutAdjustedRange)
{
	for (auto EachWeapon : Weapons)
	{
		if (not IsValid(EachWeapon))
		{
			continue;
		}
		if (not EachWeapon->IsA(UWeaponStateFlameThrower::StaticClass()))
		{
			continue;
		}
		UWeaponStateFlameThrower* FlameThrower = Cast<UWeaponStateFlameThrower>(EachWeapon);
		if (not FlameThrower)
		{
			continue;
		}
		OutAdjustedRange = FlameThrower->GetRange();
		return true;
	}
	return false;
}

static FORCEINLINE FVector RandomPerpOffset(const FVector& DirNorm, float Radius)
{
	FVector U, V;
	DirNorm.FindBestAxisVectors(U, V);

	const float angle = FMath::FRand() * 2.f * PI;
	const float r = FMath::Sqrt(FMath::FRand()) * Radius; // uniform in disk
	return U * (r * FMath::Cos(angle)) + V * (r * FMath::Sin(angle));
}

FVector FRTSWeaponHelpers::GetTraceEndWithAccuracy(
	const FVector& LaunchLocation,
	const FVector& Direction, // may be non-normalized
	float RangeCm,
	int32 Accuracy01to100,
	bool bIsAircraftWeapon)
{
	FVector DirNorm = Direction.GetSafeNormal();
	if (DirNorm.IsNearlyZero())
	{
		DirNorm = FVector::ForwardVector;
	}

	FVector End = LaunchLocation + DirNorm * RangeCm;

	const float inaccuracy = FMath::Clamp(1.f - (Accuracy01to100 / 100.f), 0.f, 1.f);
	const float MaxOffset =
		bIsAircraftWeapon
			? DeveloperSettings::GameBalance::Weapons::TraceBulletAircraftAccuracy
			: DeveloperSettings::GameBalance::Weapons::TraceBulletAccuracy;

	const float radius = MaxOffset * inaccuracy;
	if (radius <= KINDA_SMALL_NUMBER)
	{
		return End;
	}

	return End + RandomPerpOffset(DirNorm, radius);
}


FRotator FRTSWeaponHelpers::ApplyAccuracyDeviationForProjectile(const FRotator& BaseRotation, const int32 Accuracy)
{
	if (Accuracy == 100)
	{
		// No deviation if accuracy is perfect.
		return BaseRotation;
	}

	const float DeviationAngle = ((100.0f - Accuracy)) / 10.f *
		DeveloperSettings::GameBalance::Weapons::ProjectileAccuracyMlt;

	// Random deviation within the possible range
	const float RandomPitch = FMath::RandRange(-DeviationAngle, DeviationAngle);
	const float RandomYaw = FMath::RandRange(-DeviationAngle, DeviationAngle);

	return BaseRotation + FRotator(RandomPitch, RandomYaw, 0.0f);
}

FVector FRTSWeaponHelpers::ApplyAccuracyDeviationForArchWeapon(const FVector& TargetLocation, const int32 Accuracy)
{
	if (Accuracy == 100)
	{
		// No deviation if accuracy is perfect.
		return TargetLocation;
	}

	const float Deviation = ((100.0f - Accuracy)) * DeveloperSettings::GameBalance::Weapons::ArchProjectileAccuracyMlt;

	// Random deviation within the possible range
	const float RandomX = FMath::RandRange(-Deviation, Deviation);
	const float RandomY = FMath::RandRange(-Deviation, Deviation);

	FVector AdjustedLocation = TargetLocation;
	AdjustedLocation.X += RandomX;
	AdjustedLocation.Y += RandomY;

	return AdjustedLocation;
}

float FRTSWeaponHelpers::GetDamageWithFlux(const float BaseDamage, const int32 DamageFluxPercentage)
{
	return FMath::RandRange(BaseDamage * (1.0f - DamageFluxPercentage / 100.0f),
	                        BaseDamage * (1.0f + DamageFluxPercentage / 100.0f));
}

void FRTSWeaponHelpers::Debug_ResistancesAtLocation(const FVector& Location, const UObject* WorldContextObject,
                                                    const FString& Text, const FColor Color)
{
	if (DeveloperSettings::Debugging::GArmorCalculation_Resistances_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Location, WorldContextObject, Text, Color);
	}
}

void FRTSWeaponHelpers::Debug_Resistances(const FString& Text, const FColor Color)
{
	if (DeveloperSettings::Debugging::GArmorCalculation_Resistances_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Text, Color);
	}
}
