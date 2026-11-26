// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionTankFormation.h"

AWorldTankDivision::AWorldTankDivision()
{
	// The name must not match a Blueprint component variable of the division Blueprints.
	M_TankFormation = CreateDefaultSubobject<UWorldDivisionTankFormation>(TEXT("WorldDivisionTankFormation"));
}

void AWorldTankDivision::BeginPlay()
{
	Super::BeginPlay();

	if (GetIsValidTankFormation())
	{
		M_TankFormation->RebuildFromRoster(M_TankSubtypes);
	}
}

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
	// Before the broadcast: the manager may remove this division while it handles the strength change.
	if (GetIsValidTankFormation())
	{
		M_TankFormation->HandleRosterShrunk(M_TankSubtypes.Num());
	}
	BroadcastDivisionStrengthChanged();
}

void AWorldTankDivision::SetTankSubtypes(const TArray<ETankSubtype>& TankSubtypes)
{
	M_TankSubtypes = TankSubtypes;
	M_StartingTankCount = M_TankSubtypes.Num();
	RefreshStrengthFromTankCount();
	if (GetIsValidTankFormation())
	{
		M_TankFormation->RebuildFromRoster(M_TankSubtypes);
	}
}

void AWorldTankDivision::FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const
{
	OutDivisionSaveData.TankSubtypes = M_TankSubtypes;
}

void AWorldTankDivision::RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
	M_TankSubtypes = DivisionSaveData.TankSubtypes;
	M_StartingTankCount = EstimateStartingEntryCountFromCurrentStrength(M_TankSubtypes.Num());
	if (GetIsValidTankFormation())
	{
		M_TankFormation->RebuildFromRoster(M_TankSubtypes);
	}
}

void AWorldTankDivision::RefreshStrengthFromTankCount()
{
	RecalculateCurrentStrengthFromEntries(M_TankSubtypes.Num(), M_StartingTankCount);
}

bool AWorldTankDivision::GetIsValidTankFormation() const
{
	if (IsValid(M_TankFormation))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_TankFormation"),
		TEXT("AWorldTankDivision::GetIsValidTankFormation"),
		this);
	return false;
}
