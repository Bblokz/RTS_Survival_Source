// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyPlacementTypes.h"

#include "NavigationSystem.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace EnemyPlacementConstants
{
	constexpr float RingSpreadSpacing = 450.f;
	constexpr int32 RingSpreadSlotsPerRing = 8;
	constexpr float NavProjectionHorizontalExtent = 400.f;
	constexpr float NavProjectionHalfHeight = 2000.f;
}

FTrainingOption FEnemyPlacementUnit::ToTrainingOption() const
{
	if (Kind == EEnemyPlacementUnitKind::Squad)
	{
		return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
	}
	return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(VehicleSubtype));
}

bool FEnemyPlacementUnit::GetIsSet() const
{
	if (Kind == EEnemyPlacementUnitKind::Squad)
	{
		return SquadSubtype != ESquadSubtype::Squad_None;
	}
	return VehicleSubtype != ETankSubtype::Tank_None;
}

FEnemyPlacementUnit FEnemyPlacementUnit::MakeVehicle(const ETankSubtype InVehicleSubtype)
{
	FEnemyPlacementUnit Unit;
	Unit.Kind = EEnemyPlacementUnitKind::Vehicle;
	Unit.VehicleSubtype = InVehicleSubtype;
	return Unit;
}

FEnemyPlacementUnit FEnemyPlacementUnit::MakeSquad(const ESquadSubtype InSquadSubtype)
{
	FEnemyPlacementUnit Unit;
	Unit.Kind = EEnemyPlacementUnitKind::Squad;
	Unit.SquadSubtype = InSquadSubtype;
	return Unit;
}

FColor EnemyPlacementColors::GetDimmed(const FColor& Color, const float Brightness)
{
	const float Clamped = FMath::Clamp(Brightness, 0.f, 1.f);
	return FColor(static_cast<uint8>(Color.R * Clamped), static_cast<uint8>(Color.G * Clamped),
	              static_cast<uint8>(Color.B * Clamped), Color.A);
}

bool EnemyPlacement::TryGetGameDifficulty(const UObject* WorldContextObject, ERTSGameDifficulty& OutDifficulty)
{
	const AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(WorldContextObject);
	if (IsValid(MissionManager) && MissionManager->GetIsGameDifficultyInitialized())
	{
		OutDifficulty = MissionManager->GetCurrentGameDifficulty().DifficultyLevel;
		return true;
	}
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(WorldContextObject);
	if (IsValid(GameInstance) && GameInstance->GetSelectedGameDifficulty().bIsInitialized)
	{
		OutDifficulty = GameInstance->GetSelectedGameDifficulty().DifficultyLevel;
		return true;
	}
	return false;
}

bool EnemyPlacement::GetIsDifficultyIncluded(const ERTSGameDifficulty Current, const ERTSGameDifficulty FromDifficulty)
{
	// ERTSGameDifficulty is declared from easiest to hardest.
	return static_cast<uint8>(Current) >= static_cast<uint8>(FromDifficulty);
}

FVector EnemyPlacement::GetRingSpreadOffset(const int32 UseIndex)
{
	using namespace EnemyPlacementConstants;
	if (UseIndex <= 0)
	{
		return FVector::ZeroVector;
	}
	const int32 Slot = UseIndex - 1;
	const int32 Ring = 1 + Slot / RingSpreadSlotsPerRing;
	const float SlotAngleDegrees = 360.f / RingSpreadSlotsPerRing;
	// Every other ring is turned half a slot so its units stand between those of the ring inside it.
	const float AngleDegrees = (Slot % RingSpreadSlotsPerRing) * SlotAngleDegrees + (Ring % 2) * SlotAngleDegrees * 0.5f;
	return FRotator(0.f, AngleDegrees, 0.f).Vector() * (Ring * RingSpreadSpacing);
}

FVector EnemyPlacement::ProjectToNavigationIfPossible(UWorld* World, const FVector& Location)
{
	using namespace EnemyPlacementConstants;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (not IsValid(NavigationSystem))
	{
		return Location;
	}
	FNavLocation ProjectedLocation;
	const FVector Extent(NavProjectionHorizontalExtent, NavProjectionHorizontalExtent, NavProjectionHalfHeight);
	if (not NavigationSystem->ProjectPointToNavigation(Location, ProjectedLocation, Extent))
	{
		return Location;
	}
	return ProjectedLocation.Location;
}

TArray<FTrainingOption> EnemyPlacement::ToTrainingOptions(const TArray<FEnemyPlacementUnit>& Units)
{
	TArray<FTrainingOption> Options;
	Options.Reserve(Units.Num());
	for (const FEnemyPlacementUnit& Unit : Units)
	{
		if (Unit.GetIsSet())
		{
			Options.Add(Unit.ToTrainingOption());
		}
	}
	return Options;
}

bool EnemyPlacement::GetAreUnitsSet(const TArray<FEnemyPlacementUnit>& Units)
{
	if (Units.IsEmpty())
	{
		return false;
	}
	for (const FEnemyPlacementUnit& Unit : Units)
	{
		if (not Unit.GetIsSet())
		{
			return false;
		}
	}
	return true;
}
