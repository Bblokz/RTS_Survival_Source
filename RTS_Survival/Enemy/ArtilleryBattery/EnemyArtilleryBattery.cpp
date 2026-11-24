// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyArtilleryBattery.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace EnemyArtilleryBatteryConstants
{
	// Placed pits register their turrets during their own BeginPlay; the battery looks at them after this delay.
	constexpr float StartDelaySeconds = 2.f;
	constexpr float UpdateIntervalSeconds = 1.f;
	// The first barrage of a new supply waits a moment so the raised ranges have reached every turret.
	constexpr float FirstBarrageDelaySeconds = 1.f;
	// Aim points stay a little inside the turret's range, so the turret never reports the point out of range.
	constexpr float TurretRangeSafetyFraction = 0.95f;
	// Candidates tried per turret before it is left to its own targeting.
	constexpr int32 MaxAimPickAttempts = 6;

	inline const FColor BatteryColor(240, 140, 30);
	constexpr float SuppliedRangeThickness = 6.f;
	constexpr float ArrivalRadiusThickness = 4.f;
	constexpr float AutoCollectThickness = 2.f;
	constexpr float PitLinkThickness = 3.f;
	constexpr float PitMarkerRadius = 150.f;
	constexpr float DetailBrightness = 0.6f;
}

AEnemyArtilleryBattery::AEnemyArtilleryBattery()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::AddSuppliedSeconds(const float Seconds)
{
	if (Seconds <= 0.f || bM_IsDestroyed)
	{
		return;
	}
	const float SecondsLeftBefore = GetSuppliedSecondsLeft();
	const float SecondsLeftAfter = FMath::Min(SecondsLeftBefore + Seconds, Supply.MaxSuppliedSeconds);
	const float SecondsAdded = FMath::Max(SecondsLeftAfter - SecondsLeftBefore, 0.f);
	M_SuppliedUntilSeconds = GetWorldSeconds() + SecondsLeftAfter;
	if (not bM_IsSupplied)
	{
		StartSupply();
	}
	OnSuppliedTimeAdded.Broadcast(this, SecondsAdded, SecondsLeftAfter);
}

bool AEnemyArtilleryBattery::NotifyAmmoTruckArrived(AActor* Truck)
{
	if (bM_IsDestroyed)
	{
		return false;
	}
	if (IsValid(Truck))
	{
		if (GetIsTruckCredited(Truck))
		{
			return false;
		}
		M_CreditedTrucks.RemoveAll([](const TWeakObjectPtr<AActor>& Credited) { return not Credited.IsValid(); });
		M_CreditedTrucks.Add(Truck);
	}
	AddSuppliedSeconds(Supply.SecondsPerAmmoTruck);
	return true;
}

void AEnemyArtilleryBattery::AddPitActor(AActor* PitActor)
{
	if (bM_IsDestroyed)
	{
		return;
	}
	TryAddPit(PitActor);
	if (bM_IsSupplied)
	{
		RefreshRangeBoosts();
	}
}

void AEnemyArtilleryBattery::EndSupplyNow()
{
	if (not bM_IsSupplied)
	{
		return;
	}
	EndSupply();
}

float AEnemyArtilleryBattery::GetSuppliedSecondsLeft() const
{
	if (not bM_IsSupplied)
	{
		return 0.f;
	}
	return FMath::Max(static_cast<float>(M_SuppliedUntilSeconds - GetWorldSeconds()), 0.f);
}

int32 AEnemyArtilleryBattery::GetRemainingPitCount() const
{
	int32 RemainingPits = 0;
	for (const TWeakObjectPtr<AActor>& Pit : M_Pits)
	{
		if (not GetIsPitLost(Pit.Get()))
		{
			RemainingPits++;
		}
	}
	return RemainingPits;
}

AEnemyArtilleryBattery* AEnemyArtilleryBattery::FindEnemyArtilleryBattery(const UObject* WorldContextObject,
                                                                         const FName BatteryIdToFind)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World) || BatteryIdToFind.IsNone())
	{
		RTSFunctionLibrary::ReportError("FindEnemyArtilleryBattery needs a valid world context and a battery ID.");
		return nullptr;
	}
	for (TActorIterator<AEnemyArtilleryBattery> It(World); It; ++It)
	{
		if (It->BatteryId == BatteryIdToFind)
		{
			return *It;
		}
	}
	RTSFunctionLibrary::ReportError("FindEnemyArtilleryBattery found no battery with ID: " +
		BatteryIdToFind.ToString());
	return nullptr;
}

FColor AEnemyArtilleryBattery::GetPlacementColor() const
{
	return EnemyArtilleryBatteryConstants::BatteryColor;
}

FString AEnemyArtilleryBattery::GetConfigurationProblem() const
{
	if (BatteryId.IsNone())
	{
		return TEXT("No BatteryId: missions cannot find it");
	}
	if (Pits.PitActors.IsEmpty() && not Pits.bAutoCollectPits)
	{
		return TEXT("No pits: add PitActors or turn on bAutoCollectPits");
	}
	return FString();
}

// ------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_ScheduleStart();
}

void AEnemyArtilleryBattery::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBatteryTimers();
	// Only a battery removed during play hands the pits back; on world teardown the pits go away with it.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		RemoveAllRangeBoosts();
		ReturnAllBarrageTurretsToOwnTargeting();
	}
	M_RangeBoosts.Reset();
	M_BarrageTurrets.Reset();
	bM_IsSupplied = false;
	Super::EndPlay(EndPlayReason);
}

void AEnemyArtilleryBattery::BeginPlay_ScheduleStart()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyArtilleryBattery> WeakThis(this);
	World->GetTimerManager().SetTimer(M_StartTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->StartBattery();
		}
	}), EnemyArtilleryBatteryConstants::StartDelaySeconds, false);
}

void AEnemyArtilleryBattery::StartBattery()
{
	UWorld* World = GetWorld();
	if (bM_HasStarted || not IsValid(World))
	{
		return;
	}
	bM_HasStarted = true;
	CollectPits();
	if (M_Pits.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Artillery battery " + GetName() + " (" + BatteryId.ToString() +
			") found no pits; it can be supplied but will never fire or be destroyed.");
	}
	TWeakObjectPtr<AEnemyArtilleryBattery> WeakThis(this);
	World->GetTimerManager().SetTimer(M_UpdateTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateBattery();
		}
	}), EnemyArtilleryBatteryConstants::UpdateIntervalSeconds, true);
	if (bM_IsSupplied)
	{
		// Supplied before the pits were known: give them the range now.
		RefreshRangeBoosts();
	}
}

void AEnemyArtilleryBattery::UpdateBattery()
{
	if (bM_IsDestroyed)
	{
		return;
	}
	if (CheckBatteryDestroyed())
	{
		HandleBatteryDestroyed();
		return;
	}
	if (TruckDetection.bAutoDetectAmmoTrucks)
	{
		DetectArrivingAmmoTrucks();
	}
	if (not bM_IsSupplied)
	{
		return;
	}
	if (GetWorldSeconds() >= M_SuppliedUntilSeconds)
	{
		EndSupply();
		return;
	}
	RefreshRangeBoosts();
	if (GetWorldSeconds() >= M_NextBarrageSeconds)
	{
		M_NextBarrageSeconds = GetWorldSeconds() + Barrage.BarrageIntervalSeconds;
		FireBarrage();
	}
}

void AEnemyArtilleryBattery::StopBatteryTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_StartTimerHandle);
	World->GetTimerManager().ClearTimer(M_UpdateTimerHandle);
}

double AEnemyArtilleryBattery::GetWorldSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

// ------------------------------------------------------------------
// Pits
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::CollectPits()
{
	for (AActor* PitActor : Pits.PitActors)
	{
		TryAddPit(PitActor);
	}
	if (Pits.bAutoCollectPits)
	{
		CollectPits_AutoCollect();
	}
}

void AEnemyArtilleryBattery::CollectPits_AutoCollect()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FVector Centre = GetActorLocation();
	const float RadiusSquared = FMath::Square(Pits.AutoCollectRadiusCm);
	for (TActorIterator<ABuildingExpansion> It(World); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Centre) <= RadiusSquared)
		{
			TryAddPit(*It);
		}
	}
	for (TActorIterator<ATeamWeapon> It(World); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Centre) <= RadiusSquared)
		{
			TryAddPit(*It);
		}
	}
}

void AEnemyArtilleryBattery::TryAddPit(AActor* PitActor)
{
	if (not RTSFunctionLibrary::RTSIsValid(PitActor) || not GetIsPitOfBatteryPlayer(PitActor))
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Existing : M_Pits)
	{
		if (Existing.Get() == PitActor)
		{
			return;
		}
	}
	M_Pits.Add(PitActor);
}

bool AEnemyArtilleryBattery::GetIsPitOfBatteryPlayer(AActor* PitActor) const
{
	if (not IsValid(PitActor))
	{
		return false;
	}
	// The RTS component follows captures (a team weapon matches it to its crew); a bare turret knows its own owner.
	int32 PitOwner = FRTSPlayers::GetOwningPlayerOfActor(PitActor);
	ACPPTurretsMaster* TurretPit = Cast<ACPPTurretsMaster>(PitActor);
	if (PitOwner == INDEX_NONE && IsValid(TurretPit))
	{
		PitOwner = TurretPit->GetOwningPlayer();
	}
	return FRTSPlayers::AreAllied(FRTSPlayers::NormalizePlayer(PitOwner), GetEnemyPlayer());
}

bool AEnemyArtilleryBattery::GetIsPitLost(AActor* PitActor) const
{
	return not RTSFunctionLibrary::RTSIsValid(PitActor) || not GetIsPitOfBatteryPlayer(PitActor);
}

bool AEnemyArtilleryBattery::GetIsPitActive(AActor* PitActor) const
{
	if (GetIsPitLost(PitActor))
	{
		return false;
	}
	const ATeamWeapon* TeamWeapon = Cast<ATeamWeapon>(PitActor);
	return not IsValid(TeamWeapon) || not TeamWeapon->GetIsAbandoned();
}

void AEnemyArtilleryBattery::GatherTurretsOfPit(AActor* PitActor, TArray<ACPPTurretsMaster*>& OutTurrets) const
{
	if (not IsValid(PitActor))
	{
		return;
	}
	if (const ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(PitActor))
	{
		OutTurrets.Append(BuildingExpansion->GetTurrets());
		return;
	}
	if (ACPPTurretsMaster* TurretPit = Cast<ACPPTurretsMaster>(PitActor))
	{
		OutTurrets.Add(TurretPit);
		return;
	}
	TArray<AActor*> AttachedActors;
	PitActor->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (ACPPTurretsMaster* AttachedTurret = Cast<ACPPTurretsMaster>(AttachedActor))
		{
			OutTurrets.Add(AttachedTurret);
		}
	}
}

bool AEnemyArtilleryBattery::GetIsTurretBarrageCapable(const ACPPTurretsMaster* Turret) const
{
	if (not IsValid(Turret) || Turret->GetWeaponCount() == 0)
	{
		return false;
	}
	if (Pits.MinimumCalibreMm <= 0.f)
	{
		return true;
	}
	const UWeaponState* LargestWeapon = Turret->GetLargestCalibreWeapon();
	return IsValid(LargestWeapon) && LargestWeapon->GetRawWeaponData().WeaponCalibre >= Pits.MinimumCalibreMm;
}

void AEnemyArtilleryBattery::GatherActiveTurrets(TArray<FEnemyArtilleryBatteryActiveTurret>& OutTurrets) const
{
	for (const TWeakObjectPtr<AActor>& Pit : M_Pits)
	{
		AActor* PitActor = Pit.Get();
		if (GetIsPitActive(PitActor))
		{
			AddBarrageCapableTurretsOfPit(PitActor, OutTurrets);
		}
	}
}

void AEnemyArtilleryBattery::AddBarrageCapableTurretsOfPit(AActor* PitActor,
                                                           TArray<FEnemyArtilleryBatteryActiveTurret>& OutTurrets) const
{
	TArray<ACPPTurretsMaster*> TurretsOfPit;
	GatherTurretsOfPit(PitActor, TurretsOfPit);
	for (ACPPTurretsMaster* Turret : TurretsOfPit)
	{
		if (GetIsTurretBarrageCapable(Turret))
		{
			OutTurrets.Add({Turret, PitActor});
		}
	}
}

bool AEnemyArtilleryBattery::CheckBatteryDestroyed()
{
	if (M_Pits.IsEmpty())
	{
		return false;
	}
	return GetRemainingPitCount() == 0;
}

void AEnemyArtilleryBattery::HandleBatteryDestroyed()
{
	if (bM_IsDestroyed)
	{
		return;
	}
	bM_IsDestroyed = true;
	if (bM_IsSupplied)
	{
		EndSupply();
	}
	RemoveAllRangeBoosts();
	M_BarrageTurrets.Reset();
	StopBatteryTimers();
	OnBatteryDestroyed.Broadcast(this);
}

// ------------------------------------------------------------------
// Supply
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::StartSupply()
{
	bM_IsSupplied = true;
	M_NextBarrageSeconds = GetWorldSeconds() + EnemyArtilleryBatteryConstants::FirstBarrageDelaySeconds;
	if (bM_HasStarted)
	{
		// Pick up pits placed or crewed since the start.
		CollectPits();
		RefreshRangeBoosts();
	}
	OnSupplied.Broadcast(this);
}

void AEnemyArtilleryBattery::EndSupply()
{
	bM_IsSupplied = false;
	M_SuppliedUntilSeconds = 0.0;
	RemoveAllRangeBoosts();
	ReturnAllBarrageTurretsToOwnTargeting();
	OnSupplyEnded.Broadcast(this);
}

// ------------------------------------------------------------------
// Range
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::RefreshRangeBoosts()
{
	TArray<FEnemyArtilleryBatteryActiveTurret> ActiveTurrets;
	GatherActiveTurrets(ActiveTurrets);
	RemoveRangeBoostsOfInactiveTurrets(ActiveTurrets);
	for (const FEnemyArtilleryBatteryActiveTurret& ActiveTurret : ActiveTurrets)
	{
		ApplyRangeBoostToTurret(ActiveTurret.Turret, ActiveTurret.Pit);
	}
}

void AEnemyArtilleryBattery::ApplyRangeBoostToTurret(ACPPTurretsMaster* Turret, AActor* PitActor)
{
	if (not IsValid(Turret))
	{
		return;
	}
	// Copy: Upgrade makes the turret rebuild its range from its weapons.
	const TArray<UWeaponState*> Weapons = Turret->GetWeapons();
	for (UWeaponState* Weapon : Weapons)
	{
		if (not IsValid(Weapon) || GetHasRangeBoost(Weapon))
		{
			continue;
		}
		const float AddedRangeCm = Supply.SuppliedRangeCm - Weapon->GetRange();
		if (AddedRangeCm <= 0.f)
		{
			continue;
		}
		FBehaviourWeaponAttributes RangeAttributes;
		RangeAttributes.Range = AddedRangeCm;
		Weapon->Upgrade(RangeAttributes, true);

		FEnemyArtilleryBatteryRangeBoost& Boost = M_RangeBoosts.AddDefaulted_GetRef();
		Boost.Weapon = Weapon;
		Boost.Turret = Turret;
		Boost.Pit = PitActor;
		Boost.AddedRangeCm = AddedRangeCm;
	}
}

bool AEnemyArtilleryBattery::GetHasRangeBoost(const UWeaponState* Weapon) const
{
	for (const FEnemyArtilleryBatteryRangeBoost& Boost : M_RangeBoosts)
	{
		if (Boost.Weapon.Get() == Weapon)
		{
			return true;
		}
	}
	return false;
}

void AEnemyArtilleryBattery::RemoveRangeBoostsOfInactiveTurrets(
	const TArray<FEnemyArtilleryBatteryActiveTurret>& ActiveTurrets)
{
	for (int32 BoostIndex = M_RangeBoosts.Num() - 1; BoostIndex >= 0; --BoostIndex)
	{
		const ACPPTurretsMaster* BoostedTurret = M_RangeBoosts[BoostIndex].Turret.Get();
		const bool bIsActive = BoostedTurret != nullptr && ActiveTurrets.ContainsByPredicate(
			[BoostedTurret](const FEnemyArtilleryBatteryActiveTurret& ActiveTurret)
			{
				return ActiveTurret.Turret == BoostedTurret;
			});
		if (not bIsActive)
		{
			RemoveRangeBoostAt(BoostIndex);
		}
	}
}

void AEnemyArtilleryBattery::RemoveRangeBoostAt(const int32 BoostIndex)
{
	if (not M_RangeBoosts.IsValidIndex(BoostIndex))
	{
		return;
	}
	// Copy before removing: the entry is gone after RemoveAtSwap.
	const FEnemyArtilleryBatteryRangeBoost Boost = M_RangeBoosts[BoostIndex];
	M_RangeBoosts.RemoveAtSwap(BoostIndex);
	UWeaponState* Weapon = Boost.Weapon.Get();
	if (not IsValid(Weapon))
	{
		return;
	}
	FBehaviourWeaponAttributes RangeAttributes;
	RangeAttributes.Range = Boost.AddedRangeCm;
	Weapon->Upgrade(RangeAttributes, false);
}

void AEnemyArtilleryBattery::RemoveAllRangeBoosts()
{
	for (int32 BoostIndex = M_RangeBoosts.Num() - 1; BoostIndex >= 0; --BoostIndex)
	{
		RemoveRangeBoostAt(BoostIndex);
	}
	M_RangeBoosts.Reset();
}

// ------------------------------------------------------------------
// Barrage
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::FireBarrage()
{
	FEnemyArtilleryBatteryTargets Targets;
	GatherBarrageTargets(Targets);
	if (Targets.GetIsEmpty())
	{
		ReturnAllBarrageTurretsToOwnTargeting();
		return;
	}
	TArray<FEnemyArtilleryBatteryActiveTurret> ActiveTurrets;
	GatherActiveTurrets(ActiveTurrets);
	// Barrage turrets of pits that died, were captured or lost their crew are dropped without an order.
	M_BarrageTurrets.RemoveAll([&ActiveTurrets](const FEnemyArtilleryBatteryBarrageTurret& BarrageTurret)
	{
		const ACPPTurretsMaster* Turret = BarrageTurret.Turret.Get();
		return Turret == nullptr || not ActiveTurrets.ContainsByPredicate(
			[Turret](const FEnemyArtilleryBatteryActiveTurret& ActiveTurret)
			{
				return ActiveTurret.Turret == Turret;
			});
	});

	int32 PickIndex = FMath::RandRange(0, FMath::Max(Targets.BaseLocations.Num() + Targets.UnitLocations.Num(), 1));
	for (const FEnemyArtilleryBatteryActiveTurret& ActiveTurret : ActiveTurrets)
	{
		ACPPTurretsMaster* Turret = ActiveTurret.Turret;
		const bool bIsDefendingItself = GetHasTargetNear(Turret->GetActorLocation(), Targets,
		                                                 Barrage.SelfDefenceRadiusCm);
		FVector AimPoint = FVector::ZeroVector;
		if (bIsDefendingItself || not TryPickAimPoint(*Turret, Targets, PickIndex, AimPoint))
		{
			ReturnTurretToOwnTargeting(Turret);
			continue;
		}
		SetTurretOnBarrage(Turret, ActiveTurret.Pit, AimPoint);
	}
}

void AEnemyArtilleryBattery::GatherBarrageTargets(FEnemyArtilleryBatteryTargets& OutTargets) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}
	const int32 BatteryPlayer = GetEnemyPlayer();
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (FRTSPlayers::AreHostile(BatteryPlayer, Player))
		{
			GatherBarrageTargetsOfPlayer(*GameUnitManager, static_cast<uint8>(Player), OutTargets);
		}
	}
}

void AEnemyArtilleryBattery::GatherBarrageTargetsOfPlayer(const UGameUnitManager& GameUnitManager,
                                                          const uint8 Player,
                                                          FEnemyArtilleryBatteryTargets& OutTargets) const
{
	for (ABuildingExpansion* BuildingExpansion : GameUnitManager.GetPlayerBxps(Player))
	{
		TryAddBarrageTarget(BuildingExpansion, Player, true, OutTargets);
	}
	for (ATankMaster* Tank : GameUnitManager.GetTanksOfPlayerView(Player))
	{
		const ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(Tank);
		const bool bIsBaseBuilding = IsValid(NomadicVehicle) && not NomadicVehicle->GetIsInTruckMode();
		TryAddBarrageTarget(Tank, Player, bIsBaseBuilding, OutTargets);
	}
	for (ASquadUnit* SquadUnit : GameUnitManager.GetSquadUnitsOfPlayerView(Player))
	{
		TryAddBarrageTarget(SquadUnit, Player, false, OutTargets);
	}
}

void AEnemyArtilleryBattery::TryAddBarrageTarget(AActor* Target, const int32 TargetOwner, const bool bIsBaseTarget,
                                                 FEnemyArtilleryBatteryTargets& OutTargets) const
{
	if (not RTSFunctionLibrary::RTSIsValid(Target))
	{
		return;
	}
	const FVector TargetLocation = Target->GetActorLocation();
	if (FVector::DistSquared2D(TargetLocation, GetActorLocation()) > FMath::Square(Supply.SuppliedRangeCm))
	{
		return;
	}
	if (Barrage.bOnlyVisibleTargets &&
		not RTSFunctionLibrary::RTSIsVisibleTargetOfOwner(Target, TargetOwner, GetEnemyPlayer()))
	{
		return;
	}
	if (bIsBaseTarget)
	{
		OutTargets.BaseLocations.Add(TargetLocation);
		return;
	}
	OutTargets.UnitLocations.Add(TargetLocation);
}

bool AEnemyArtilleryBattery::GetHasTargetNear(const FVector& Location, const FEnemyArtilleryBatteryTargets& Targets,
                                              const float Radius) const
{
	if (Radius <= 0.f)
	{
		return false;
	}
	const float RadiusSquared = FMath::Square(Radius);
	auto IsNear = [&Location, RadiusSquared](const FVector& TargetLocation)
	{
		return FVector::DistSquared2D(Location, TargetLocation) <= RadiusSquared;
	};
	return Targets.BaseLocations.ContainsByPredicate(IsNear) || Targets.UnitLocations.ContainsByPredicate(IsNear);
}

bool AEnemyArtilleryBattery::TryPickAimPoint(ACPPTurretsMaster& Turret, const FEnemyArtilleryBatteryTargets& Targets,
                                             int32& InOutPickIndex, FVector& OutAimPoint) const
{
	const bool bHasBase = not Targets.BaseLocations.IsEmpty();
	const bool bHasUnits = not Targets.UnitLocations.IsEmpty();
	const bool bPreferBase = bHasBase && (not bHasUnits || FMath::FRand() < Barrage.BaseTargetPreference);
	const TArray<FVector>& FirstChoice = bPreferBase ? Targets.BaseLocations : Targets.UnitLocations;
	const TArray<FVector>& SecondChoice = bPreferBase ? Targets.UnitLocations : Targets.BaseLocations;

	return TryPickAimPointFrom(Turret, FirstChoice, InOutPickIndex, OutAimPoint)
		|| TryPickAimPointFrom(Turret, SecondChoice, InOutPickIndex, OutAimPoint);
}

bool AEnemyArtilleryBattery::TryPickAimPointFrom(ACPPTurretsMaster& Turret, const TArray<FVector>& Candidates,
                                                 int32& InOutPickIndex, FVector& OutAimPoint) const
{
	const int32 CandidateCount = Candidates.Num();
	const int32 Attempts = FMath::Min(CandidateCount, EnemyArtilleryBatteryConstants::MaxAimPickAttempts);
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const FVector AimPoint = GetScatteredAimPoint(Candidates[(InOutPickIndex + Attempt) % CandidateCount]);
		if (GetCanTurretReach(Turret, AimPoint))
		{
			InOutPickIndex += Attempt + 1;
			OutAimPoint = AimPoint;
			return true;
		}
	}
	return false;
}

bool AEnemyArtilleryBattery::GetCanTurretReach(ACPPTurretsMaster& Turret, const FVector& AimPoint) const
{
	const float ReachCm = Turret.GetMaxWeaponRange() * EnemyArtilleryBatteryConstants::TurretRangeSafetyFraction;
	if (FVector::DistSquared(Turret.GetActorLocation(), AimPoint) > FMath::Square(ReachCm))
	{
		return false;
	}
	return Turret.GetCanTraverseToLocation(AimPoint);
}

FVector AEnemyArtilleryBattery::GetScatteredAimPoint(const FVector& TargetLocation) const
{
	if (Barrage.AimSpreadRadiusCm <= 0.f)
	{
		return TargetLocation;
	}
	// Square root keeps the scatter even over the disc instead of bunching at the centre.
	const float ScatterDistance = FMath::Sqrt(FMath::FRand()) * Barrage.AimSpreadRadiusCm;
	const float ScatterYaw = FMath::FRandRange(0.f, 360.f);
	return TargetLocation + FRotator(0.f, ScatterYaw, 0.f).Vector() * ScatterDistance;
}

void AEnemyArtilleryBattery::SetTurretOnBarrage(ACPPTurretsMaster* Turret, AActor* PitActor, const FVector& AimPoint)
{
	if (not IsValid(Turret))
	{
		return;
	}
	const bool bIsTracked = M_BarrageTurrets.ContainsByPredicate(
		[Turret](const FEnemyArtilleryBatteryBarrageTurret& BarrageTurret)
		{
			return BarrageTurret.Turret.Get() == Turret;
		});
	if (not bIsTracked)
	{
		FEnemyArtilleryBatteryBarrageTurret& BarrageTurret = M_BarrageTurrets.AddDefaulted_GetRef();
		BarrageTurret.Turret = Turret;
		BarrageTurret.Pit = PitActor;
	}
	Turret->SetEngageGroundLocation(AimPoint);
}

void AEnemyArtilleryBattery::ReturnTurretToOwnTargeting(ACPPTurretsMaster* Turret)
{
	const int32 RemovedCount = M_BarrageTurrets.RemoveAll(
		[Turret](const FEnemyArtilleryBatteryBarrageTurret& BarrageTurret)
		{
			return BarrageTurret.Turret.Get() == Turret;
		});
	// Only turrets the battery took over are handed back; others keep whatever they were doing.
	if (RemovedCount > 0 && IsValid(Turret))
	{
		Turret->SetAutoEngageTargets(false);
	}
}

void AEnemyArtilleryBattery::ReturnAllBarrageTurretsToOwnTargeting()
{
	// Copy: the list is cleared before the turrets get their orders.
	const TArray<FEnemyArtilleryBatteryBarrageTurret> BarrageTurrets = M_BarrageTurrets;
	M_BarrageTurrets.Reset();
	for (const FEnemyArtilleryBatteryBarrageTurret& BarrageTurret : BarrageTurrets)
	{
		ACPPTurretsMaster* Turret = BarrageTurret.Turret.Get();
		// A dead, captured or abandoned pit must not have its guns switched back on.
		if (IsValid(Turret) && GetIsPitActive(BarrageTurret.Pit.Get()))
		{
			Turret->SetAutoEngageTargets(false);
		}
	}
}

// ------------------------------------------------------------------
// Ammo trucks
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::DetectArrivingAmmoTrucks()
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}
	const FVector Centre = GetActorLocation();
	const float RadiusSquared = FMath::Square(TruckDetection.ArrivalRadiusCm);
	// Collected first: crediting a truck broadcasts events, which may spawn or remove units mid-walk.
	TArray<ATankMaster*> ArrivedTrucks;
	for (ATankMaster* Tank : GameUnitManager->GetTanksOfPlayerView(static_cast<uint8>(GetEnemyPlayer())))
	{
		if (not IsValid(Tank) || FVector::DistSquared2D(Tank->GetActorLocation(), Centre) > RadiusSquared)
		{
			continue;
		}
		if (not GetIsTruckCredited(Tank) && GetIsArrivingAmmoTruck(Tank))
		{
			ArrivedTrucks.Add(Tank);
		}
	}
	for (ATankMaster* Truck : ArrivedTrucks)
	{
		NotifyAmmoTruckArrived(Truck);
	}
}

bool AEnemyArtilleryBattery::GetIsArrivingAmmoTruck(ATankMaster* Tank) const
{
	if (not RTSFunctionLibrary::RTSIsValid(Tank))
	{
		return false;
	}
	const AConvoy* Convoy = AConvoy::FindConvoyOfActiveMember(Tank);
	if (not IsValid(Convoy))
	{
		return false;
	}
	if (TruckDetection.bRequireAmmoCargo)
	{
		return GetIsAmmoCargoTruck(*Convoy, Tank);
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy->GetMemberSnapshots(Snapshots);
	return Snapshots.ContainsByPredicate([Tank](const FConvoyMemberSnapshot& Snapshot)
	{
		return Snapshot.Actor.Get() == Tank && Snapshot.Role == EConvoyUnitRole::Truck;
	});
}

bool AEnemyArtilleryBattery::GetIsAmmoCargoTruck(const AConvoy& Convoy, const ATankMaster* Tank) const
{
	// Per-truck cargo (a spec's override in a mixed column); only truck members of the convoy answer.
	EConvoyCargoType TruckCargoType = EConvoyCargoType::Fuel;
	if (Convoy.GetTruckCargoType(Tank, TruckCargoType))
	{
		return TruckCargoType == EConvoyCargoType::Ammo;
	}
	// Not a known truck member: fall back to the convoy's cargo, but only for an actual truck.
	const UConvoyDefinition* Definition = Convoy.GetDefinition();
	if (not IsValid(Definition) || Definition->Cargo.CargoType != EConvoyCargoType::Ammo)
	{
		return false;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy.GetMemberSnapshots(Snapshots);
	return Snapshots.ContainsByPredicate([Tank](const FConvoyMemberSnapshot& Snapshot)
	{
		return Snapshot.Actor.Get() == Tank && Snapshot.Role == EConvoyUnitRole::Truck;
	});
}

bool AEnemyArtilleryBattery::GetIsTruckCredited(const AActor* Truck) const
{
	return M_CreditedTrucks.ContainsByPredicate([Truck](const TWeakObjectPtr<AActor>& Credited)
	{
		return Credited.Get() == Truck;
	});
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

void AEnemyArtilleryBattery::BuildEditorShapes(FEnemyPlacementShapes& OutSummary,
                                               FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyArtilleryBatteryConstants;
	const FColor Color = GetPlacementColor();
	const FColor DetailColor = EnemyPlacementColors::GetDimmed(Color, DetailBrightness);
	const FVector Centre = GetActorLocation();
	// The supplied reach is the battery's threat: it should reach the player's base, the arrival ring stays small.
	OutSummary.AddCircle(Centre, Supply.SuppliedRangeCm, Color, SuppliedRangeThickness);
	if (TruckDetection.bAutoDetectAmmoTrucks)
	{
		OutSummary.AddCircle(Centre, TruckDetection.ArrivalRadiusCm, Color, ArrivalRadiusThickness);
	}
	if (Pits.bAutoCollectPits)
	{
		OutDetail.AddCircle(Centre, Pits.AutoCollectRadiusCm, DetailColor, AutoCollectThickness);
	}
	for (const AActor* PitActor : Pits.PitActors)
	{
		if (not IsValid(PitActor))
		{
			continue;
		}
		const FVector PitLocation = PitActor->GetActorLocation();
		OutDetail.Lines.Emplace(Centre, PitLocation, DetailColor, PitLinkThickness);
		OutDetail.Spheres.Emplace(PitMarkerRadius, PitLocation, FLinearColor(Color),
		                          FDebugRenderSceneProxy::EDrawType::WireMesh);
	}
}
