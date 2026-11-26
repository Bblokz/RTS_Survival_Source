// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TechWheelEffectProbeDumpSubsystem.h"

#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantStatComparison.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelEffectProbes.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTechWheelProbeDump, Log, All);

namespace TechWheelEffectProbeDumpConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("TechWheelEffectProbeDump");
	// Time for the world map to finish loading before the first vehicle is spawned.
	constexpr double StartSeconds = 8.0;

	FString DescribeWeaponChange(const FTechWheelWeaponChange& Change)
	{
		using namespace UnitVariantStatComparison;
		return FString::Printf(
			TEXT("%s: damage %.0f->%.0f (pips %d->%d), pen %.0f->%.0f (pips %d->%d), reload %.2f->%.2f (pips %d->%d), "
				"mag %d->%d (pips %d->%d), range %.0f->%.0f"),
			*Change.Before.WeaponRowName.ToString(),
			Change.Before.Damage, Change.After.Damage,
			GetDamageLevel(Change.Before.Damage), GetDamageLevel(Change.After.Damage),
			Change.Before.ArmorPen, Change.After.ArmorPen,
			GetPenetrationLevel(Change.Before.ArmorPen), GetPenetrationLevel(Change.After.ArmorPen),
			Change.Before.ReloadSeconds, Change.After.ReloadSeconds,
			GetReloadLevel(Change.Before.ReloadSeconds), GetReloadLevel(Change.After.ReloadSeconds),
			Change.Before.MagCapacity, Change.After.MagCapacity,
			GetMagazineLevel(Change.Before.MagCapacity), GetMagazineLevel(Change.After.MagCapacity),
			Change.Before.RangeMeters, Change.After.RangeMeters);
	}
}

bool UTechWheelEffectProbeDumpSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TechWheelEffectProbeDumpConstants::CommandLineSwitch);
#endif
}

void UTechWheelEffectProbeDumpSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_StartSeconds = FPlatformTime::Seconds();
	M_Probes = NewObject<UTechWheelEffectProbes>(this);
	UE_LOG(LogRTSTechWheelProbeDump, Display, TEXT("TECHPROBE started"));
}

void UTechWheelEffectProbeDumpSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bM_IsDone || bM_IsWaitingForProbe || not IsValid(M_Probes))
	{
		return;
	}

	if (FPlatformTime::Seconds() - M_StartSeconds < TechWheelEffectProbeDumpConstants::StartSeconds)
	{
		return;
	}

	ProbeNextTechnology();
}

void UTechWheelEffectProbeDumpSubsystem::ProbeNextTechnology()
{
	const UEnum* const TechnologyEnum = StaticEnum<ETechnology>();
	M_TechnologyIndex++;
	// The last entry of a UENUM is its hidden _MAX.
	if (M_TechnologyIndex >= TechnologyEnum->NumEnums() - 1)
	{
		Finish();
		return;
	}

	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		Finish();
		return;
	}

	bM_IsWaitingForProbe = true;
	const ETechnology Technology = static_cast<ETechnology>(TechnologyEnum->GetValueByIndex(M_TechnologyIndex));
	TWeakObjectPtr<UTechWheelEffectProbeDumpSubsystem> WeakThis(this);
	M_Probes->ProbeTechnology(*World, Technology, [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->LogProbedTechnology();
		WeakThis->bM_IsWaitingForProbe = false;
	});
}

void UTechWheelEffectProbeDumpSubsystem::LogProbedTechnology() const
{
	const UEnum* const TechnologyEnum = StaticEnum<ETechnology>();
	const ETechnology Technology = static_cast<ETechnology>(TechnologyEnum->GetValueByIndex(M_TechnologyIndex));
	const FString TechnologyName = Global_GetTechAsString(Technology);
	UClass* const EffectClass = TechWheelEffects::LoadEffectClass(Technology);
	if (not IsValid(EffectClass))
	{
		UE_LOG(LogRTSTechWheelProbeDump, Display, TEXT("TECHPROBE %s: no effect class"), *TechnologyName);
		return;
	}

	const UTechnologyEffect* const Effect = EffectClass->GetDefaultObject<UTechnologyEffect>();
	TArray<FString> UnitNames;
	for (const FTrainingOption& Unit : TechWheelEffects::GetAffectedUnits(*Effect))
	{
		UnitNames.Add(Unit.GetTrainingName());
	}

	const FTechWheelEffectResult* const Result = M_Probes->FindResult(Technology);
	UE_LOG(LogRTSTechWheelProbeDump, Display,
	       TEXT("TECHPROBE %s: class %s (its own technology %s), units [%s], probed %d, weapons read %d, changed %d"),
	       *TechnologyName, *EffectClass->GetName(), *Global_GetTechAsString(Effect->GetTechnology()),
	       *FString::Join(UnitNames, TEXT(", ")), Result != nullptr ? Result->ProbedUnitCount : 0,
	       Result != nullptr ? Result->WeaponsRead : 0, Result != nullptr ? Result->WeaponChanges.Num() : 0);
	if (Result == nullptr)
	{
		return;
	}

	for (const FTechWheelWeaponChange& Change : Result->WeaponChanges)
	{
		UE_LOG(LogRTSTechWheelProbeDump, Display, TEXT("TECHPROBE   %s"),
		       *TechWheelEffectProbeDumpConstants::DescribeWeaponChange(Change));
	}
}

void UTechWheelEffectProbeDumpSubsystem::Finish()
{
	bM_IsDone = true;
	UE_LOG(LogRTSTechWheelProbeDump, Display, TEXT("TECHPROBE done"));
	FPlatformMisc::RequestExit(false, TEXT("TechWheelEffectProbeDump"));
}

TStatId UTechWheelEffectProbeDumpSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTechWheelEffectProbeDumpSubsystem, STATGROUP_Tickables);
}
