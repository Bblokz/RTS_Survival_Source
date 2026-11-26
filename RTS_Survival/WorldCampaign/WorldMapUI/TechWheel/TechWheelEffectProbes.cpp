// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TechWheelEffectProbes.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantStatComparison.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechSettings.h"

namespace TechWheelEffectProbesPrivate
{
	// Same wait as the unit variant probes: past the longest BeginPlay weapon setup of a vehicle.
	constexpr float SettleSeconds = 2.f;
	// A weapon behaviour changes the weapons on the tick after it is added; this leaves several.
	constexpr float ApplySeconds = 0.3f;
	// A technology lists a handful of vehicles; more probes than this would only repeat the same guns.
	constexpr int32 MaxProbedUnits = 6;

	template <typename TSubtype>
	void AddUnits(const TArray<TSubtype>& Subtypes, const EAllUnitType UnitType, TArray<FTrainingOption>& OutUnits)
	{
		for (const TSubtype Subtype : Subtypes)
		{
			OutUnits.Add(FTrainingOption(UnitType, static_cast<uint8>(Subtype)));
		}
	}

	bool GetIsProbedUnitType(const EAllUnitType UnitType)
	{
		return UnitType == EAllUnitType::UNType_Tank || UnitType == EAllUnitType::UNType_BuildingExpansion;
	}

	// The world map has no unit class table; the mission's spawner Blueprint holds it in its class setup.
	ARTSAsyncSpawner* FindOrSpawnClassSpawner(UWorld& World, bool& bOutSpawnedHere)
	{
		bOutSpawnedHere = false;
		if (ARTSAsyncSpawner* const Existing = Cast<ARTSAsyncSpawner>(
			UGameplayStatics::GetActorOfClass(&World, ARTSAsyncSpawner::StaticClass())))
		{
			return Existing;
		}

		UClass* const SpawnerClass = GetDefault<UWorldCampaignTechSettings>()->UnitClassSpawnerClass.LoadSynchronous();
		if (not IsValid(SpawnerClass) || not SpawnerClass->IsChildOf(ARTSAsyncSpawner::StaticClass()))
		{
			return nullptr;
		}

		bOutSpawnedHere = true;
		return World.SpawnActor<ARTSAsyncSpawner>(SpawnerClass, FTransform::Identity);
	}

	bool GetIsWeaponChanged(const FUnitVariantWeaponStats& Before, const FUnitVariantWeaponStats& After)
	{
		using namespace UnitVariantStatComparison;
		return GetTrend(After.Damage, Before.Damage, true) != EUnitVariantStatTrend::Same
			|| GetTrend(After.ArmorPen, Before.ArmorPen, true) != EUnitVariantStatTrend::Same
			|| GetTrend(After.ReloadSeconds, Before.ReloadSeconds, false) != EUnitVariantStatTrend::Same
			|| GetTrend(After.MagCapacity, Before.MagCapacity, true) != EUnitVariantStatTrend::Same;
	}
}

UClass* TechWheelEffects::LoadEffectClass(const ETechnology Technology)
{
	const TSoftClassPtr<UTechnologyEffect>* const EffectClass =
		GetDefault<UWorldCampaignTechSettings>()->WheelEffectClasses.Find(Technology);
	return EffectClass != nullptr ? EffectClass->LoadSynchronous() : nullptr;
}

TArray<FTrainingOption> TechWheelEffects::GetAffectedUnits(const UTechnologyEffect& Effect)
{
	using namespace TechWheelEffectProbesPrivate;
	TArray<FTrainingOption> Units;
	AddUnits(Effect.GetTanksToApplyTo(), EAllUnitType::UNType_Tank, Units);
	AddUnits(Effect.GetSquadsToApplyTo(), EAllUnitType::UNType_Squad, Units);
	AddUnits(Effect.GetNomadicsToApplyTo(), EAllUnitType::UNType_Nomadic, Units);
	AddUnits(Effect.GetBuildingExpansionsToApplyTo(), EAllUnitType::UNType_BuildingExpansion, Units);
	AddUnits(Effect.GetAircraftToApplyTo(), EAllUnitType::UNType_Aircraft, Units);
	return Units;
}

bool TechWheelEffects::GetCanChangeWeapons(const UTechnologyEffect& Effect)
{
	const TSubclassOf<UBehaviour> BehaviourClass = Effect.GetBehaviourClassToApply();
	return BehaviourClass != nullptr && BehaviourClass->IsChildOf(UBehaviourWeapon::StaticClass());
}

void UTechWheelEffectProbes::ProbeTechnology(UWorld& World, const ETechnology Technology, TFunction<void()> OnRead)
{
	CancelProbes();
	M_World = &World;
	M_OnRead = MoveTemp(OnRead);
	M_ProbedTechnology = Technology;
	if (M_Results.Contains(Technology))
	{
		// Known already: nothing to store, only the answer to give.
		TFunction<void()> KnownOnRead = MoveTemp(M_OnRead);
		M_OnRead = nullptr;
		if (KnownOnRead)
		{
			KnownOnRead();
		}
		return;
	}

	M_ProbedEffectClass = TechWheelEffects::LoadEffectClass(Technology);
	if (not M_ProbedEffectClass)
	{
		FinishProbing(FTechWheelEffectResult());
		return;
	}

	const UTechnologyEffect& Effect = *M_ProbedEffectClass->GetDefaultObject<UTechnologyEffect>();
	if (not TechWheelEffects::GetCanChangeWeapons(Effect))
	{
		FinishProbing(FTechWheelEffectResult());
		return;
	}

	const TArray<FSoftObjectPath> ClassPaths = FindProbeClassPaths(World, Effect);
	if (ClassPaths.IsEmpty())
	{
		FinishProbing(FTechWheelEffectResult());
		return;
	}

	M_LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ClassPaths, FStreamableDelegate::CreateWeakLambda(this, [this, ClassPaths]()
		{
			SpawnProbes(ClassPaths);
		}));
}

const FTechWheelEffectResult* UTechWheelEffectProbes::FindResult(const ETechnology Technology) const
{
	return M_Results.Find(Technology);
}

void UTechWheelEffectProbes::CancelProbes()
{
	if (M_LoadHandle.IsValid())
	{
		M_LoadHandle->CancelHandle();
		M_LoadHandle.Reset();
	}

	if (M_World.IsValid())
	{
		M_World->GetTimerManager().ClearTimer(M_StepTimer);
	}

	for (const FLiveProbe& Probe : M_LiveProbes)
	{
		if (Probe.Actor.IsValid())
		{
			Probe.Actor->Destroy();
		}
	}

	M_LiveProbes.Reset();
	M_AppliedEffect = nullptr;
	M_OnRead = nullptr;
}

TArray<FSoftObjectPath> UTechWheelEffectProbes::FindProbeClassPaths(UWorld& World, const UTechnologyEffect& Effect)
{
	using namespace TechWheelEffectProbesPrivate;
	TArray<FSoftObjectPath> ClassPaths;
	bool bSpawnedHere = false;
	ARTSAsyncSpawner* const Spawner = FindOrSpawnClassSpawner(World, bSpawnedHere);
	if (not IsValid(Spawner))
	{
		return ClassPaths;
	}

	for (const FTrainingOption& Unit : TechWheelEffects::GetAffectedUnits(Effect))
	{
		if (ClassPaths.Num() >= MaxProbedUnits)
		{
			break;
		}

		if (not GetIsProbedUnitType(Unit.UnitType))
		{
			continue;
		}

		const FSoftObjectPath ClassPath = Spawner->GetBaseTrainingOptionClass(
			UPlayerUnitVariantManager::GetSpawnerKey(Unit)).ToSoftObjectPath();
		if (ClassPath.IsValid())
		{
			ClassPaths.AddUnique(ClassPath);
		}
	}

	// A spawner made here is only a lookup table; left in the world it could be taken for the mission's own.
	if (bSpawnedHere)
	{
		Spawner->Destroy();
	}

	return ClassPaths;
}

void UTechWheelEffectProbes::SpawnProbes(const TArray<FSoftObjectPath>& ClassPaths)
{
	using namespace TechWheelEffectProbesPrivate;
	if (not M_World.IsValid())
	{
		return;
	}

	for (const FSoftObjectPath& ClassPath : ClassPaths)
	{
		UClass* const UnitClass = Cast<UClass>(ClassPath.ResolveObject());
		if (not IsValid(UnitClass))
		{
			continue;
		}

		if (AActor* const Probe = UUnitVariantProbes::SpawnHiddenProbe(*M_World, UnitClass))
		{
			FLiveProbe& LiveProbe = M_LiveProbes.AddDefaulted_GetRef();
			LiveProbe.Actor = Probe;
		}
	}

	if (M_LiveProbes.IsEmpty())
	{
		FinishProbing(FTechWheelEffectResult());
		return;
	}

	M_World->GetTimerManager().SetTimer(M_StepTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ReadBeforeAndApply();
	}), SettleSeconds, false);
}

void UTechWheelEffectProbes::ReadBeforeAndApply()
{
	using namespace TechWheelEffectProbesPrivate;
	if (not M_World.IsValid() || not M_ProbedEffectClass)
	{
		return;
	}

	M_AppliedEffect = NewObject<UTechnologyEffect>(this, M_ProbedEffectClass);
	// Blueprint-only effects leave their technology unset; the mission's tech manager sets it the same way.
	M_AppliedEffect->SetTechnology(M_ProbedTechnology);
	for (FLiveProbe& Probe : M_LiveProbes)
	{
		if (not Probe.Actor.IsValid())
		{
			continue;
		}

		Probe.Before = UUnitVariantProbes::ReadHiddenProbe(*Probe.Actor);
		if (ATankMaster* const Tank = Cast<ATankMaster>(Probe.Actor.Get()))
		{
			M_AppliedEffect->ApplyOnTank(Tank);
			continue;
		}

		if (ABuildingExpansion* const BuildingExpansion = Cast<ABuildingExpansion>(Probe.Actor.Get()))
		{
			M_AppliedEffect->ApplyOnBuildingExpansion(BuildingExpansion);
		}
	}

	M_World->GetTimerManager().SetTimer(M_StepTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ReadAfterAndDestroy();
	}), ApplySeconds, false);
}

void UTechWheelEffectProbes::ReadAfterAndDestroy()
{
	FTechWheelEffectResult Result;
	for (const FLiveProbe& Probe : M_LiveProbes)
	{
		if (not Probe.Actor.IsValid())
		{
			continue;
		}

		Result.ProbedUnitCount++;
		Result.WeaponsRead += Probe.Before.Weapons.Num();
		AddWeaponChanges(Probe.Before, UUnitVariantProbes::ReadHiddenProbe(*Probe.Actor), Result);
		Probe.Actor->Destroy();
	}

	M_LiveProbes.Reset();
	M_AppliedEffect = nullptr;
	FinishProbing(Result);
}

void UTechWheelEffectProbes::FinishProbing(const FTechWheelEffectResult& Result)
{
	M_Results.Add(M_ProbedTechnology, Result);
	TFunction<void()> OnRead = MoveTemp(M_OnRead);
	M_OnRead = nullptr;
	if (OnRead)
	{
		OnRead();
	}
}

void UTechWheelEffectProbes::AddWeaponChanges(const FUnitVariantProbeResult& Before,
                                              const FUnitVariantProbeResult& After,
                                              FTechWheelEffectResult& InOutResult)
{
	using namespace TechWheelEffectProbesPrivate;
	for (const FUnitVariantWeaponStats& WeaponBefore : Before.Weapons)
	{
		const FName RowName = WeaponBefore.WeaponRowName;
		const FUnitVariantWeaponStats* const WeaponAfter = After.Weapons.FindByPredicate(
			[RowName](const FUnitVariantWeaponStats& Weapon) { return Weapon.WeaponRowName == RowName; });
		if (WeaponAfter == nullptr || not GetIsWeaponChanged(WeaponBefore, *WeaponAfter))
		{
			continue;
		}

		const bool bIsKnownWeapon = InOutResult.WeaponChanges.ContainsByPredicate(
			[RowName](const FTechWheelWeaponChange& Change) { return Change.Before.WeaponRowName == RowName; });
		if (bIsKnownWeapon)
		{
			continue;
		}

		FTechWheelWeaponChange& Change = InOutResult.WeaponChanges.AddDefaulted_GetRef();
		Change.Before = WeaponBefore;
		Change.After = *WeaponAfter;
	}
}
