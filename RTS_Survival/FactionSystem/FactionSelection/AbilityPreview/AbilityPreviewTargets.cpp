// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AbilityPreviewTargets.h"

#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftBombing/GA_AircraftBombing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FireStrikes/GA_IncendiaryAirStrike.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAbilityPreviewTargets, Log, All);

namespace AbilityPreviewTargetsPrivate
{
	// Vehicles and squads spawn this high over the traced ground and settle onto it, as the menu battles do.
	constexpr float SpawnLiftCm = 100.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	// Squads spawn their soldiers over a few frames; the soldiers are matched to the squad's owner until then and
	// kept out of the fog for the whole preview.
	constexpr float KeepPresentableInterval = 0.2f;
	// Targets stand across the attack line, turned a little each, so the line does not read as a parade.
	constexpr float FacingAcrossLineDegrees = 90.f;
	constexpr float FacingVariationDegrees = 22.f;

	const TCHAR* const MosinSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	const TCHAR* const SvtSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_SVT.Rus_Squad_SVT_C");
	const TCHAR* const T70 = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T70/Rus_Vehicle_T-70.Rus_Vehicle_T-70_C");
	const TCHAR* const BA11 = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/BA-11/Rus_ArmoredCar_BA11."
		"Rus_ArmoredCar_BA11_C");
	const TCHAR* const T26 = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C");
	const TCHAR* const T3476 = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476."
		"Rus_Vehicle_T3476_C");
	const TCHAR* const T3485 = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3485."
		"Rus_Vehicle_T3485_C");

	FAbilityPreviewTargetSet MakeSet(std::initializer_list<const TCHAR*> ClassPaths)
	{
		FAbilityPreviewTargetSet Set;
		for (const TCHAR* const ClassPath : ClassPaths)
		{
			Set.UnitClasses.Add(TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath)));
		}
		return Set;
	}
}

UAbilityPreviewTargetsComponent::UAbilityPreviewTargetsComponent()
{
	using namespace AbilityPreviewTargetsPrivate;
	PrimaryComponentTick.bCanEverTick = false;

	M_TargetSets.Add(EAbilityPreviewTargetKind::Infantry,
	                 MakeSet({MosinSquad, SvtSquad, MosinSquad, SvtSquad, MosinSquad}));
	M_TargetSets.Add(EAbilityPreviewTargetKind::LightVehicles, MakeSet({T70, BA11, T26}));
	M_TargetSets.Add(EAbilityPreviewTargetKind::MediumVehicles, MakeSet({T3476, T3485, T3476}));

	// Machine gun fighters.
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_2xBf110Strafing, EAbilityPreviewTargetKind::Infantry);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_Bf109Strafing, EAbilityPreviewTargetKind::Infantry);
	// Cannon aircraft.
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_2xHortentrafing, EAbilityPreviewTargetKind::LightVehicles);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_GerJu87GTankBusters, EAbilityPreviewTargetKind::LightVehicles);
	// Bombers: the Ju 87 and Me 410 strafe and drop bombs. The incendiary Ju 87 is not among them: its canisters are
	// "deadly to infantry, harmless to tanks", so it gets infantry.
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_GerIncendiaryJu87, EAbilityPreviewTargetKind::Infantry);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_Ju87Strafing, EAbilityPreviewTargetKind::MediumVehicles);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_3xJu87Strafing, EAbilityPreviewTargetKind::MediumVehicles);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_Me410Airstrike, EAbilityPreviewTargetKind::MediumVehicles);
	M_AbilityTargetKinds.Add(EGlobalAbility::GA_HeavyCarpet_Ju390B_250Gr, EAbilityPreviewTargetKind::MediumVehicles);
}

void UAbilityPreviewTargetsComponent::BeginPlay()
{
	Super::BeginPlay();
	PreloadTargetClasses();
}

void UAbilityPreviewTargetsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTargets();
	if (M_PreloadHandle.IsValid())
	{
		M_PreloadHandle->CancelHandle();
		M_PreloadHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UAbilityPreviewTargetsComponent::PreloadTargetClasses()
{
	TArray<FSoftObjectPath> ClassPaths;
	for (const TPair<EAbilityPreviewTargetKind, FAbilityPreviewTargetSet>& KindAndSet : M_TargetSets)
	{
		for (const TSoftClassPtr<AActor>& UnitClass : KindAndSet.Value.UnitClasses)
		{
			if (not UnitClass.IsNull())
			{
				ClassPaths.AddUnique(UnitClass.ToSoftObjectPath());
			}
		}
	}
	if (ClassPaths.IsEmpty())
	{
		return;
	}
	M_PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ClassPaths, FStreamableDelegate());
}

EAbilityPreviewTargetKind UAbilityPreviewTargetsComponent::GetTargetKindForAbility(
	const EGlobalAbility Ability, const UGlobalAbility& AbilityCopy) const
{
	if (const EAbilityPreviewTargetKind* const ListedKind = M_AbilityTargetKinds.Find(Ability))
	{
		return *ListedKind;
	}
	if (AbilityCopy.IsA<UGA_AircraftBombing>())
	{
		return EAbilityPreviewTargetKind::MediumVehicles;
	}
	if (AbilityCopy.IsA<UGA_AircraftStrafing>() || AbilityCopy.IsA<UGA_IncendiaryAirStrike>())
	{
		return EAbilityPreviewTargetKind::Infantry;
	}
	return EAbilityPreviewTargetKind::None;
}

int32 UAbilityPreviewTargetsComponent::SpawnTargets(const EAbilityPreviewTargetKind Kind, const FVector& LineStart,
                                                    const FVector& LineEnd)
{
	using namespace AbilityPreviewTargetsPrivate;
	ClearTargets();
	const FAbilityPreviewTargetSet* const TargetSet = M_TargetSets.Find(Kind);
	if (Kind == EAbilityPreviewTargetKind::None || TargetSet == nullptr || TargetSet->UnitClasses.IsEmpty())
	{
		return 0;
	}
	FVector LineDirection = LineEnd - LineStart;
	LineDirection.Z = 0.f;
	const float LineLength = LineDirection.Size();
	LineDirection = LineDirection.GetSafeNormal();
	if (LineDirection.IsNearlyZero())
	{
		LineDirection = FVector::ForwardVector;
	}
	const FVector LineSide(-LineDirection.Y, LineDirection.X, 0.f);
	const float UsableLength = LineLength * (1.f - 2.f * M_LineEndMargin);
	const int32 UnitCount = TargetSet->UnitClasses.Num();
	const float LineYaw = LineDirection.Rotation().Yaw;

	M_SpawnedKind = Kind;
	for (int32 UnitIndex = 0; UnitIndex < UnitCount; ++UnitIndex)
	{
		UClass* const UnitClass = TargetSet->UnitClasses[UnitIndex].LoadSynchronous();
		if (not IsValid(UnitClass))
		{
			UE_LOG(LogRTSAbilityPreviewTargets, Warning, TEXT("RTS_ABILITY_PREVIEW_TARGETS class missing %s"),
			       *TargetSet->UnitClasses[UnitIndex].ToString());
			continue;
		}
		const float AlongLine = LineLength * M_LineEndMargin + UsableLength * (UnitIndex + 0.5f) / UnitCount;
		const float SideSign = UnitIndex % 2 == 0 ? 1.f : -1.f;
		const FVector Location = LineStart + LineDirection * AlongLine + LineSide * (SideSign * M_SideOffset);
		const float Yaw = LineYaw + FacingAcrossLineDegrees + SideSign * FacingVariationDegrees;
		if (AActor* const Target = SpawnTarget(UnitClass, GetGroundPoint(Location), Yaw))
		{
			M_Targets.Add(Target);
		}
	}

	if (UWorld* const World = GetWorld(); IsValid(World) && not M_Targets.IsEmpty())
	{
		World->GetTimerManager().SetTimer(M_KeepPresentableTimerHandle, this,
		                                  &UAbilityPreviewTargetsComponent::KeepTargetsPresentable,
		                                  KeepPresentableInterval, true, 0.f);
	}
	UE_LOG(LogRTSAbilityPreviewTargets, Display, TEXT("RTS_ABILITY_PREVIEW_TARGETS kind=%s spawned=%d line=%s->%s"),
	       *UEnum::GetValueAsString(Kind), M_Targets.Num(), *LineStart.ToCompactString(),
	       *LineEnd.ToCompactString());
	return M_Targets.Num();
}

AActor* UAbilityPreviewTargetsComponent::SpawnTarget(UClass* UnitClass, const FVector& GroundLocation,
                                                     const float Yaw)
{
	using namespace AbilityPreviewTargetsPrivate;
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	const FTransform SpawnTransform(FRotator(0.f, Yaw, 0.f), GroundLocation + FVector::UpVector * SpawnLiftCm);
	AActor* const Target = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, UnitClass, SpawnTransform,
	                                                                 M_TargetOwningPlayer);
	if (not IsValid(Target))
	{
		UE_LOG(LogRTSAbilityPreviewTargets, Warning, TEXT("RTS_ABILITY_PREVIEW_TARGETS spawn failed %s"),
		       *UnitClass->GetName());
		return nullptr;
	}
	PrepareTarget(*Target);
	// Idle squads walk to the nearest cover, here the rocks by the pond, out of the attack.
	if (ASquadController* const Squad = Cast<ASquadController>(Target))
	{
		Squad->SetAutomaticCoverBlocked(true);
	}
	return Target;
}

FVector UAbilityPreviewTargetsComponent::GetGroundPoint(const FVector& Location) const
{
	using namespace AbilityPreviewTargetsPrivate;
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return Location;
	}
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AbilityPreviewTargetGround), false, GetOwner());
	FHitResult Hit;
	const FVector TraceOffset = FVector::UpVector * GroundTraceHalfHeight;
	if (World->LineTraceSingleByObjectType(Hit, Location + TraceOffset, Location - TraceOffset, ObjectQuery,
	                                       QueryParams))
	{
		return Hit.ImpactPoint;
	}
	return Location;
}

void UAbilityPreviewTargetsComponent::ClearTargets()
{
	if (UWorld* const World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_KeepPresentableTimerHandle);
	}
	// Soldiers first: a squad that is destroyed first may still be tearing its soldiers down.
	for (const TWeakObjectPtr<AActor>& Soldier : M_TargetSoldiers)
	{
		if (Soldier.IsValid())
		{
			Soldier->Destroy();
		}
	}
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		if (Target.IsValid())
		{
			Target->Destroy();
		}
	}
	M_TargetSoldiers.Reset();
	M_Targets.Reset();
	M_SpawnedKind = EAbilityPreviewTargetKind::None;
}

void UAbilityPreviewTargetsComponent::KeepTargetsPresentable()
{
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	CollectSquadSoldiers();
	for (const TArray<TWeakObjectPtr<AActor>>* const Units : {&M_Targets, &M_TargetSoldiers})
	{
		for (const TWeakObjectPtr<AActor>& Unit : *Units)
		{
			if (not Unit.IsValid())
			{
				continue;
			}
			// In enemy vision: the German aircraft may pick the targets; never hidden: the camera shows them.
			Unit->Tags.AddUnique(TagUnitInEnemyVision);
			if (Unit->IsHidden())
			{
				Unit->SetActorHiddenInGame(false);
			}
			TArray<UWidgetComponent*> Widgets;
			Unit->GetComponents<UWidgetComponent>(Widgets, true);
			for (UWidgetComponent* const Widget : Widgets)
			{
				if (IsValid(Widget) && not Widget->bHiddenInGame)
				{
					Widget->SetHiddenInGame(true);
				}
			}
		}
	}
}

void UAbilityPreviewTargetsComponent::CollectSquadSoldiers()
{
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		ASquadController* const Squad = Cast<ASquadController>(Target.Get());
		const URTSComponent* const SquadRTS = IsValid(Squad) ? Squad->FindComponentByClass<URTSComponent>() : nullptr;
		if (SquadRTS == nullptr)
		{
			continue;
		}
		for (ASquadUnit* const Soldier : Squad->GetSquadUnitsChecked())
		{
			if (not IsValid(Soldier) || M_TargetSoldiers.Contains(Soldier))
			{
				continue;
			}
			M_TargetSoldiers.Add(Soldier);
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, SquadRTS->GetOwningPlayer());
			PrepareTarget(*Soldier);
		}
	}
}

void UAbilityPreviewTargetsComponent::PrepareTarget(AActor& Target)
{
	// Out of the fog system: the hangar has no fog of war for player 2, and the fog would hide the targets.
	if (UFowComp* const Fow = Target.FindComponentByClass<UFowComp>())
	{
		Fow->StopFowParticipation();
	}
	if (const UHealthComponent* const Health = Target.FindComponentByClass<UHealthComponent>())
	{
		Health->MakeHealthBarInvisible();
	}
}

FAbilityPreviewTargetHealth UAbilityPreviewTargetsComponent::GetTargetHealth() const
{
	FAbilityPreviewTargetHealth TargetHealth;
	auto CountUnit = [&TargetHealth](const TWeakObjectPtr<AActor>& Unit)
	{
		++TargetHealth.Units;
		if (GetIsUnitDestroyed(Unit.Get()))
		{
			++TargetHealth.Destroyed;
			++TargetHealth.Damaged;
			return;
		}
		if (GetIsUnitDamaged(Unit.Get()))
		{
			++TargetHealth.Damaged;
		}
	};
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		// Squads are counted by their soldiers.
		if (Target.IsValid() && Target->IsA<ASquadController>())
		{
			continue;
		}
		CountUnit(Target);
	}
	for (const TWeakObjectPtr<AActor>& Soldier : M_TargetSoldiers)
	{
		CountUnit(Soldier);
	}
	return TargetHealth;
}

bool UAbilityPreviewTargetsComponent::GetIsUnitDestroyed(const AActor* Unit)
{
	if (not IsValid(Unit))
	{
		return true;
	}
	const UHealthComponent* const Health = Unit->FindComponentByClass<UHealthComponent>();
	return IsValid(Health) && Health->GetCurrentHealth() <= 0.f;
}

bool UAbilityPreviewTargetsComponent::GetIsUnitDamaged(const AActor* Unit)
{
	const UHealthComponent* const Health = IsValid(Unit) ? Unit->FindComponentByClass<UHealthComponent>() : nullptr;
	return IsValid(Health) && Health->GetCurrentHealth() < Health->GetMaxHealth();
}
