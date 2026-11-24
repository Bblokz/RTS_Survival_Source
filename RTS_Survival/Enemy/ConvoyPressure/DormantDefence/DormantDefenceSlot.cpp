// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DormantDefenceSlot.h"

#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace DormantDefenceSlotConstants
{
	// Sand brown: fortifications that are not there yet; set apart from the other placed enemy actors.
	const FColor SlotColor(205, 150, 70);
	constexpr float FacingArrowLength = 600.f;
	constexpr float GroundLiftZ = 20.f;
	constexpr float OutlineBrightness = 0.6f;
}

ADormantDefenceSlot::ADormantDefenceSlot()
{
	PrimaryActorTick.bCanEverTick = false;
}

AActor* ADormantDefenceSlot::Activate()
{
	if (bM_IsActivated)
	{
		return M_SpawnedDefence.Get();
	}
	// Marked first: a failed spawn is reported once and the slot counts as used.
	bM_IsActivated = true;
	AActor* SpawnedDefence = SpawnDefence();
	if (not IsValid(SpawnedDefence))
	{
		return nullptr;
	}
	M_SpawnedDefence = SpawnedDefence;
	OnActivated.Broadcast(this, SpawnedDefence);
	return SpawnedDefence;
}

AActor* ADormantDefenceSlot::SpawnDefence() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	if (DefenceClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("Dormant defence slot " + GetName() + " has no DefenceClass; nothing spawned.");
		return nullptr;
	}
	UClass* LoadedClass = DefenceClass.LoadSynchronous();
	if (not IsValid(LoadedClass))
	{
		RTSFunctionLibrary::ReportError("Dormant defence slot " + GetName() + " could not load "
			+ DefenceClass.ToString());
		return nullptr;
	}
	AActor* SpawnedDefence = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, LoadedClass, GetActorTransform(), static_cast<uint8>(GetEnemyPlayer()));
	if (not IsValid(SpawnedDefence))
	{
		RTSFunctionLibrary::ReportError("Dormant defence slot " + GetName() + " could not spawn "
			+ LoadedClass->GetName());
		return nullptr;
	}
	return SpawnedDefence;
}

FColor ADormantDefenceSlot::GetPlacementColor() const
{
	return DormantDefenceSlotConstants::SlotColor;
}

FString ADormantDefenceSlot::GetConfigurationProblem() const
{
	if (DefenceClass.IsNull())
	{
		return TEXT("no DefenceClass");
	}
	return FString();
}

void ADormantDefenceSlot::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	using namespace DormantDefenceSlotConstants;
	const FVector Ground = GetActorLocation() + FVector(0.f, 0.f, GroundLiftZ);
	const float Yaw = GetActorRotation().Yaw;
	const FVector HalfExtent(FootprintHalfExtent.X, FootprintHalfExtent.Y, 0.f);
	// Outline only: the defence is not there until it is switched on.
	OutSummary.AddFlatBox(Ground, HalfExtent, Yaw, EnemyPlacementColors::GetDimmed(SlotColor, OutlineBrightness), false);
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	OutDetail.ArrowLines.Emplace(Ground, Ground + Forward * FacingArrowLength, EnemyPlacementColors::Facing);
}
