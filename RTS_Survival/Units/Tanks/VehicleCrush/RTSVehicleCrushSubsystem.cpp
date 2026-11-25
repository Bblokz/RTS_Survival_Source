// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSVehicleCrushSubsystem.h"

#include "Engine/World.h"
#include "RTSVehicleCrushSettings.h"
#include "TimerManager.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSVehicleCrush, Log, All);

namespace RTSVehicleCrushStatics
{
	constexpr float PruneIntervalSeconds = 1.f;
	// The look-ahead corridor also reaches a little behind the hull centre: a soldier beside the rear is not safe yet.
	constexpr float BehindCentreFraction = 0.5f;
}

bool URTSVehicleCrushSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) && World->IsGameWorld();
}

void URTSVehicleCrushSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	if (not IsValid(Settings) || not Settings->bEnableVehicleCrush)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(
		M_CheckTimer, this, &URTSVehicleCrushSubsystem::RunCrushCheck, Settings->CheckIntervalSeconds, true);
}

void URTSVehicleCrushSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CheckTimer);
	}
	M_FootprintByVehicle.Empty();
	M_EncounterBySoldier.Empty();
	M_ScreamWindowByVehicle.Empty();
	Super::Deinitialize();
}

void URTSVehicleCrushSubsystem::RunCrushCheck()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RTSVehicleCrush_RunCrushCheck);
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	UWorld* World = GetWorld();
	// Silent: a world without a game state (main menu backdrops before it spawns) has no units to crush.
	UGameUnitManager* UnitManager = IsValid(World) ? FRTS_Statics::GetGameUnitManager(World) : nullptr;
	if (not IsValid(Settings) || not Settings->bEnableVehicleCrush || not IsValid(UnitManager))
	{
		return;
	}
	const float NowSeconds = World->GetTimeSeconds();

	// Killing a soldier removes him from the unit manager's arrays, so the walk only collects; deaths come after.
	TArray<TPair<TWeakObjectPtr<ASquadUnit>, FRTSVehicleCrushHull>> Crushed;
	TArray<TPair<TWeakObjectPtr<ASquadUnit>, FRTSVehicleCrushHull>> InPath;

	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		const uint8 HostileMask = FRTSPlayers::GetHostileMask(Player);
		if (HostileMask == 0)
		{
			continue;
		}
		for (ATankMaster* const Vehicle : UnitManager->GetTanksOfPlayerView(static_cast<uint8>(Player)))
		{
			FRTSVehicleCrushHull Hull;
			if (not TryBuildHull(Vehicle, *Settings, Hull))
			{
				continue;
			}
			Hull.OwningPlayer = Player;
			const float BroadphaseRadius = Hull.Footprint.GetCornerRadius() + Hull.LookAheadCm +
				Settings->DodgeSideMarginCm;
			for (int32 HostilePlayer = RTSPlayers::HumanPlayer; HostilePlayer <= RTSPlayers::MaxPlayers;
			     ++HostilePlayer)
			{
				if (not FRTSPlayers::IsPlayerInMask(HostilePlayer, HostileMask))
				{
					continue;
				}
				for (ASquadUnit* const Soldier : UnitManager->GetSquadUnitsOfPlayerView(
					     static_cast<uint8>(HostilePlayer)))
				{
					if (not IsValid(Soldier) ||
						FVector2D::DistSquared(FVector2D(Soldier->GetActorLocation()), Hull.Center) >
						FMath::Square(BroadphaseRadius))
					{
						continue;
					}
					bool bIsUnderHull = false;
					bool bIsInPath = false;
					ClassifySoldier(Hull, *Soldier, *Settings, bIsUnderHull, bIsInPath);
					if (bIsUnderHull)
					{
						Crushed.Emplace(Soldier, Hull);
					}
					else if (bIsInPath)
					{
						InPath.Emplace(Soldier, Hull);
					}
				}
			}
		}
	}

	for (const TPair<TWeakObjectPtr<ASquadUnit>, FRTSVehicleCrushHull>& Entry : Crushed)
	{
		// Two hulls can cover one soldier; the first kills him and the second finds him dead.
		if (Entry.Key.IsValid() && Entry.Key->IsUnitAlive() && IsValid(Entry.Value.Vehicle))
		{
			CrushSoldier(Entry.Key.Get(), Entry.Value, NowSeconds);
		}
	}
	for (const TPair<TWeakObjectPtr<ASquadUnit>, FRTSVehicleCrushHull>& Entry : InPath)
	{
		if (Entry.Key.IsValid() && Entry.Key->IsUnitAlive() && IsValid(Entry.Value.Vehicle))
		{
			UpdateDodge(Entry.Key.Get(), Entry.Value, *Settings, NowSeconds);
		}
	}
	if (NowSeconds >= M_NextPruneSeconds)
	{
		M_NextPruneSeconds = NowSeconds + RTSVehicleCrushStatics::PruneIntervalSeconds;
		PruneStaleEntries(*Settings, NowSeconds);
	}
}

bool URTSVehicleCrushSubsystem::TryBuildHull(
	ATankMaster* const Vehicle,
	const URTSVehicleCrushSettings& Settings,
	FRTSVehicleCrushHull& OutHull)
{
	if (not IsValid(Vehicle) || not Vehicle->IsUnitAlive())
	{
		return false;
	}
	const FVector Velocity = Vehicle->GetVelocity();
	const FVector2D Velocity2D(Velocity);
	const float Speed = Velocity2D.Size();
	if (Speed < Settings.MinCrushSpeed)
	{
		return false;
	}
	FRTSVehicleFootprint& Footprint = M_FootprintByVehicle.FindOrAdd(Vehicle);
	if (not Footprint.GetIsValid())
	{
		// The chassis has no bodies until it is set up; the next check measures again.
		Footprint = RTSVehicleFootprint::BuildFromChassisMesh(Vehicle->GetTankMesh());
		if (not Footprint.GetIsValid())
		{
			return false;
		}
	}
	const FVector Location = Vehicle->GetActorLocation();
	OutHull.Vehicle = Vehicle;
	OutHull.Forward = FVector2D(Vehicle->GetActorForwardVector()).GetSafeNormal();
	if (OutHull.Forward.IsNearlyZero())
	{
		return false;
	}
	OutHull.Footprint = Footprint;
	OutHull.Center = Footprint.GetWorldCenter(FVector2D(Location), OutHull.Forward);
	// The way it really goes: a reversing or sliding tank crushes what is behind or beside it.
	OutHull.DriveDirection = Velocity2D / Speed;
	OutHull.VehicleZ = Location.Z;
	OutHull.LookAheadCm = FMath::Clamp(
		Speed * Settings.DodgeLookAheadSeconds,
		Settings.MinDodgeLookAheadCm,
		Settings.MaxDodgeLookAheadCm);
	return true;
}

void URTSVehicleCrushSubsystem::ClassifySoldier(
	const FRTSVehicleCrushHull& Hull,
	const ASquadUnit& Soldier,
	const URTSVehicleCrushSettings& Settings,
	bool& OutIsUnderHull,
	bool& OutIsInPath) const
{
	OutIsUnderHull = false;
	OutIsInPath = false;
	const FVector SoldierLocation = Soldier.GetActorLocation();
	const float HeightAboveVehicle = SoldierLocation.Z - Soldier.GetSimpleCollisionHalfHeight() - Hull.VehicleZ;
	if (HeightAboveVehicle > Settings.MaxHeightAboveVehicleCm ||
		HeightAboveVehicle < -Settings.MaxDepthBelowVehicleCm)
	{
		return;
	}
	if (not Soldier.GetCanBeRunOver())
	{
		return;
	}
	const FVector2D Offset = FVector2D(SoldierLocation) - Hull.Center;
	const FVector2D Right(-Hull.Forward.Y, Hull.Forward.X);
	const float Along = FVector2D::DotProduct(Offset, Hull.Forward);
	const float Across = FVector2D::DotProduct(Offset, Right);
	if (FMath::Abs(Along) <= Hull.Footprint.HalfLength - Settings.HullInsetCm &&
		FMath::Abs(Across) <= Hull.Footprint.HalfWidth - Settings.HullInsetCm)
	{
		OutIsUnderHull = true;
		return;
	}

	const FVector2D DriveRight(-Hull.DriveDirection.Y, Hull.DriveDirection.X);
	const float ReachAhead = Hull.Footprint.GetReachAlong(Hull.Forward, Hull.DriveDirection);
	const float ReachSide = Hull.Footprint.GetReachAlong(Hull.Forward, DriveRight);
	const float AlongDrive = FVector2D::DotProduct(Offset, Hull.DriveDirection);
	const float AcrossDrive = FVector2D::DotProduct(Offset, DriveRight);
	OutIsInPath = AlongDrive >= -ReachAhead * RTSVehicleCrushStatics::BehindCentreFraction &&
		AlongDrive <= ReachAhead + Hull.LookAheadCm &&
		FMath::Abs(AcrossDrive) <= ReachSide + Settings.DodgeSideMarginCm;
}

void URTSVehicleCrushSubsystem::CrushSoldier(ASquadUnit* const Soldier, const FRTSVehicleCrushHull& Hull,
                                             const float NowSeconds)
{
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	const bool bMayScream = IsValid(Settings) && TryConsumeScream(Hull.Vehicle, *Settings, NowSeconds);
	const FVector DriveDirection(Hull.DriveDirection, 0.f);
	M_CrushOwnerPairs.Emplace(Hull.OwningPlayer, Soldier->GetOwningPlayer());
	Soldier->OnRunOverByVehicle(Hull.Vehicle, DriveDirection, bMayScream);
	M_EncounterBySoldier.Remove(Soldier);
	++M_CrushCount;
	UE_LOG(LogRTSVehicleCrush, Verbose, TEXT("%s ran over %s."), *Hull.Vehicle->GetName(), *Soldier->GetName());
}

void URTSVehicleCrushSubsystem::UpdateDodge(
	ASquadUnit* const Soldier,
	const FRTSVehicleCrushHull& Hull,
	const URTSVehicleCrushSettings& Settings,
	const float NowSeconds)
{
	FRTSVehicleCrushEncounter* Encounter = M_EncounterBySoldier.Find(Soldier);
	const bool bIsNewEncounter = Encounter == nullptr || Encounter->Vehicle.Get() != Hull.Vehicle ||
		NowSeconds - Encounter->LastSeenSeconds > Settings.EncounterForgetSeconds;
	if (bIsNewEncounter)
	{
		// One roll per soldier and vehicle: rolling every check would make every soldier jump clear in the end.
		Encounter = &M_EncounterBySoldier.Add(Soldier);
		Encounter->Vehicle = Hull.Vehicle;
		Encounter->bWillDodge = FMath::FRand() < Settings.DodgeChance;
		Encounter->bPicksWrongSide = FMath::FRand() < Settings.WrongSideChance;
		Encounter->DodgeAtSeconds = NowSeconds + FMath::FRandRange(
			Settings.DodgeReactionSeconds.X, Settings.DodgeReactionSeconds.Y);
		Encounter->ClearanceCm = FMath::FRandRange(Settings.DodgeClearanceCm.X, Settings.DodgeClearanceCm.Y);
		Encounter->bHasDodged = false;
	}
	Encounter->LastSeenSeconds = NowSeconds;
	if (not Encounter->bWillDodge || Encounter->bHasDodged || NowSeconds < Encounter->DodgeAtSeconds)
	{
		return;
	}
	// A soldier busy walking somewhere keeps walking; he tries again on the next check if he stops in the path.
	if (Soldier->TryDodgeVehicle(GetDodgeLocation(*Soldier, Hull, *Encounter)))
	{
		Encounter->bHasDodged = true;
		++M_DodgeCount;
	}
}

FVector URTSVehicleCrushSubsystem::GetDodgeLocation(
	const ASquadUnit& Soldier,
	const FRTSVehicleCrushHull& Hull,
	const FRTSVehicleCrushEncounter& Encounter) const
{
	const FVector SoldierLocation = Soldier.GetActorLocation();
	const FVector2D Offset = FVector2D(SoldierLocation) - Hull.Center;
	const FVector2D DriveRight(-Hull.DriveDirection.Y, Hull.DriveDirection.X);
	const float AlongDrive = FVector2D::DotProduct(Offset, Hull.DriveDirection);
	const float AcrossDrive = FVector2D::DotProduct(Offset, DriveRight);
	// Out of the corridor on the side he is already on; a panicked few run across the vehicle's path instead.
	float Side = AcrossDrive >= 0.f ? 1.f : -1.f;
	if (Encounter.bPicksWrongSide)
	{
		Side = -Side;
	}
	const float ReachSide = Hull.Footprint.GetReachAlong(Hull.Forward, DriveRight);
	const FVector2D Target = Hull.Center + Hull.DriveDirection * AlongDrive +
		DriveRight * (Side * (ReachSide + Encounter.ClearanceCm));
	return FVector(Target, SoldierLocation.Z);
}

bool URTSVehicleCrushSubsystem::TryConsumeScream(ATankMaster* const Vehicle,
                                                 const URTSVehicleCrushSettings& Settings,
                                                 const float NowSeconds)
{
	TPair<float, int32>& Window = M_ScreamWindowByVehicle.FindOrAdd(Vehicle, TPair<float, int32>(NowSeconds, 0));
	if (NowSeconds - Window.Key >= 1.f)
	{
		Window = TPair<float, int32>(NowSeconds, 0);
	}
	if (Window.Value >= Settings.MaxScreamsPerVehiclePerSecond)
	{
		return false;
	}
	++Window.Value;
	return true;
}

void URTSVehicleCrushSubsystem::PruneStaleEntries(const URTSVehicleCrushSettings& Settings, const float NowSeconds)
{
	for (auto It = M_EncounterBySoldier.CreateIterator(); It; ++It)
	{
		if (not It.Key().IsValid() || NowSeconds - It.Value().LastSeenSeconds > Settings.EncounterForgetSeconds)
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = M_FootprintByVehicle.CreateIterator(); It; ++It)
	{
		if (not It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = M_ScreamWindowByVehicle.CreateIterator(); It; ++It)
	{
		if (not It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
