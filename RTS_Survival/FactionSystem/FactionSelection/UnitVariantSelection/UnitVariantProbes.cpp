// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantProbes.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace UnitVariantProbesPrivate
{
	// The player the hangar previews belong to; weapon and health data are looked up per owning player.
	constexpr uint8 ProbeOwningPlayer = 1;
	// Far below the map, so nothing sees or hears a probe while it settles.
	const FVector ProbeLocation(0.f, 0.f, -50000.f);
	constexpr float ProbeSpacing = 5000.f;
	// Past the longest BeginPlay delay of a variant (Pz III M Spaced Armor sets its health after 1.25 s) and the
	// tank speed setup (about 1 s).
	constexpr float ProbeSettleSeconds = 2.f;
	const FName ProbeTag(TEXT("UnitVariantProbe"));
	constexpr float CentimetresPerMetre = 100.f;
}

void UUnitVariantProbes::ProbeClasses(UWorld& World, const TArray<UClass*>& UnitClasses,
                                      TFunction<void()> OnProbesRead)
{
	using namespace UnitVariantProbesPrivate;
	CancelProbes();
	M_World = &World;
	M_OnProbesRead = MoveTemp(OnProbesRead);
	for (UClass* const UnitClass : UnitClasses)
	{
		if (not IsValid(UnitClass) || M_Results.Contains(UnitClass))
		{
			continue;
		}
		if (AActor* const Probe = SpawnProbe(World, UnitClass))
		{
			M_LiveProbes.Add(Probe);
		}
	}
	if (M_LiveProbes.IsEmpty())
	{
		ReadAndDestroyProbes();
		return;
	}
	World.GetTimerManager().SetTimer(M_SettleTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ReadAndDestroyProbes();
	}), ProbeSettleSeconds, false);
}

AActor* UUnitVariantProbes::SpawnProbe(UWorld& World, UClass* UnitClass)
{
	using namespace UnitVariantProbesPrivate;
	static int32 ProbeCount = 0;
	const FVector Location = ProbeLocation + FVector(ProbeSpacing * (ProbeCount++ % 8), 0.f, 0.f);
	AActor* const Probe = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(World, UnitClass, FTransform(Location),
	                                                                ProbeOwningPlayer);
	if (not IsValid(Probe))
	{
		return nullptr;
	}
	Probe->Tags.Add(ProbeTag);
	Probe->SetActorHiddenInGame(true);
	Probe->SetActorEnableCollision(false);
	// Frozen in place: a vehicle would otherwise fall for the whole settle time.
	if (UPrimitiveComponent* const RootPrimitive = Cast<UPrimitiveComponent>(Probe->GetRootComponent()))
	{
		RootPrimitive->SetSimulatePhysics(false);
	}
	return Probe;
}

void UUnitVariantProbes::ReadAndDestroyProbes()
{
	for (const TWeakObjectPtr<AActor>& Probe : M_LiveProbes)
	{
		if (not Probe.IsValid())
		{
			continue;
		}
		M_Results.Add(Probe->GetClass(), ReadProbe(*Probe));
		Probe->Destroy();
	}
	M_LiveProbes.Reset();
	TFunction<void()> OnProbesRead = MoveTemp(M_OnProbesRead);
	M_OnProbesRead = nullptr;
	if (OnProbesRead)
	{
		OnProbesRead();
	}
}

FUnitVariantProbeResult UUnitVariantProbes::ReadProbe(const AActor& Probe)
{
	FUnitVariantProbeResult Result;
	if (const UHealthComponent* const Health = Probe.FindComponentByClass<UHealthComponent>())
	{
		Result.MaxHealth = Health->GetMaxHealth();
	}
	if (const ATankMaster* const Tank = Cast<ATankMaster>(&Probe))
	{
		Result.MaxSpeedMultiplier = Tank->VariantMobilityMultipliers.MaxSpeedMultiplier;
	}
	for (const UWeaponState* const Weapon : GetProbeWeapons(Probe))
	{
		if (IsValid(Weapon))
		{
			AddWeapon(Result.Weapons, *Weapon);
		}
	}
	return Result;
}

TArray<UWeaponState*> UUnitVariantProbes::GetProbeWeapons(const AActor& Probe)
{
	if (const ATankMaster* const Tank = Cast<ATankMaster>(&Probe))
	{
		return FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
	}
	if (const ABuildingExpansion* const BuildingExpansion = Cast<ABuildingExpansion>(&Probe))
	{
		return FRTSWeaponHelpers::GetWeaponsMountedOnBxp(BuildingExpansion);
	}
	return {};
}

void UUnitVariantProbes::AddWeapon(TArray<FUnitVariantWeaponStats>& Weapons, const UWeaponState& WeaponState)
{
	using namespace UnitVariantProbesPrivate;
	const FWeaponData& Data = WeaponState.GetRawWeaponData();
	const FName RowName(Global_GetWeaponEnumAsString(Data.WeaponName));
	if (FUnitVariantWeaponStats* const SameWeapon = Weapons.FindByPredicate(
		[RowName](const FUnitVariantWeaponStats& Existing) { return Existing.WeaponRowName == RowName; }))
	{
		++SameWeapon->Count;
		return;
	}
	FUnitVariantWeaponStats& Stats = Weapons.AddDefaulted_GetRef();
	Stats.WeaponRowName = RowName;
	Stats.DisplayName = FText::FromString(Global_GetWeaponDisplayName(Data.WeaponName));
	Stats.Count = 1;
	Stats.CalibreMm = Data.WeaponCalibre;
	Stats.Damage = Data.BaseDamage;
	Stats.ArmorPen = Data.ArmorPen;
	Stats.ReloadSeconds = Data.ReloadSpeed;
	Stats.RangeMeters = Data.Range / CentimetresPerMetre;
	Stats.MagCapacity = Data.MagCapacity;
}

const FUnitVariantProbeResult* UUnitVariantProbes::FindResult(const UClass* UnitClass) const
{
	// The map keys hold non-const class pointers; the lookup does not change the class.
	return M_Results.Find(const_cast<UClass*>(UnitClass));
}

void UUnitVariantProbes::CancelProbes()
{
	if (UWorld* const World = M_World.Get())
	{
		World->GetTimerManager().ClearTimer(M_SettleTimer);
	}
	for (const TWeakObjectPtr<AActor>& Probe : M_LiveProbes)
	{
		if (Probe.IsValid())
		{
			Probe->Destroy();
		}
	}
	M_LiveProbes.Reset();
	M_OnProbesRead = nullptr;
}
