// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"

void AWorldTankDivision::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_StartingTankCount = M_TankSubtypes.Num();
	RefreshStrengthFromTankCount();
}

void AWorldTankDivision::ApplyWorldDivisionDamage(const int32 DamageEntryCount)
{
	if (DamageEntryCount <= 0 || M_TankSubtypes.Num() == 0)
	{
		return;
	}

	const int32 RemovedTankCount = FMath::Min(DamageEntryCount, M_TankSubtypes.Num());
	M_TankSubtypes.RemoveAt(M_TankSubtypes.Num() - RemovedTankCount, RemovedTankCount);
	RefreshStrengthFromTankCount();
	BroadcastDivisionStrengthChanged();
}

void AWorldTankDivision::SetTankSubtypes(const TArray<ETankSubtype>& TankSubtypes)
{
	M_TankSubtypes = TankSubtypes;
	M_StartingTankCount = M_TankSubtypes.Num();
	RefreshStrengthFromTankCount();
}

void AWorldTankDivision::FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const
{
	OutDivisionSaveData.TankSubtypes = M_TankSubtypes;
}

void AWorldTankDivision::RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
	M_TankSubtypes = DivisionSaveData.TankSubtypes;
	M_StartingTankCount = EstimateStartingEntryCountFromCurrentStrength(M_TankSubtypes.Num());
}

void AWorldTankDivision::RefreshStrengthFromTankCount()
{
	RecalculateCurrentStrengthFromEntries(M_TankSubtypes.Num(), M_StartingTankCount);
}
