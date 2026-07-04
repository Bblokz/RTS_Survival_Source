// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldSquadDivision.h"

void AWorldSquadDivision::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	M_StartingSquadCount = M_SquadSubtypes.Num();
	RefreshStrengthFromSquadCount();
}

void AWorldSquadDivision::ApplyWorldDivisionDamage(const int32 DamageEntryCount)
{
	if (DamageEntryCount <= 0 || M_SquadSubtypes.Num() == 0)
	{
		return;
	}

	const int32 RemovedSquadCount = FMath::Min(DamageEntryCount, M_SquadSubtypes.Num());
	M_SquadSubtypes.RemoveAt(M_SquadSubtypes.Num() - RemovedSquadCount, RemovedSquadCount);
	RefreshStrengthFromSquadCount();
	BroadcastDivisionStrengthChanged();
}

void AWorldSquadDivision::SetSquadSubtypes(const TArray<ESquadSubtype>& SquadSubtypes)
{
	M_SquadSubtypes = SquadSubtypes;
	M_StartingSquadCount = M_SquadSubtypes.Num();
	RefreshStrengthFromSquadCount();
}

void AWorldSquadDivision::FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const
{
	OutDivisionSaveData.SquadSubtypes = M_SquadSubtypes;
}

void AWorldSquadDivision::RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
	M_SquadSubtypes = DivisionSaveData.SquadSubtypes;
	M_StartingSquadCount = EstimateStartingEntryCountFromCurrentStrength(M_SquadSubtypes.Num());
}

void AWorldSquadDivision::RefreshStrengthFromSquadCount()
{
	RecalculateCurrentStrengthFromEntries(M_SquadSubtypes.Num(), M_StartingSquadCount);
}
