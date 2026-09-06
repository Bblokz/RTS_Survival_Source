// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSAsyncSpawner.h"

#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirector.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirectorUnitOverrideSettings.h"
#include "RTS_Survival/Enemy/EnemyDirector/EnemyDirectorUnitOverridesDataAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides(const EEnemyDirector EnemyDirector)
{
	if (bM_HasAppliedEnemyDirectorUnitOverrides)
	{
		return;
	}
	bM_HasAppliedEnemyDirectorUnitOverrides = true;

	if (EnemyDirector == EEnemyDirector::DirectorOfShield)
	{
		return;
	}

	const UEnemyDirectorUnitOverrideSettings* const OverrideSettings =
		GetDefault<UEnemyDirectorUnitOverrideSettings>();
	if (not IsValid(OverrideSettings))
	{
		RTSFunctionLibrary::ReportError(
			"Could not get the enemy director unit override developer settings."
			"\n See ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides");
		return;
	}

	const UEnemyDirectorUnitOverridesDataAsset* const OverrideDataAsset =
		OverrideSettings->GetUnitOverridesDataAsset();
	if (not IsValid(OverrideDataAsset))
	{
		RTSFunctionLibrary::ReportError(
			"No valid enemy director unit overrides Data Asset is configured in Project Settings."
			"\n See ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides");
		return;
	}

	const FEnemyDirectorUnitOverrides* const UnitOverrides = OverrideDataAsset->FindOverrides(EnemyDirector);
	if (UnitOverrides == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"No unit overrides are available for enemy director: " + UEnum::GetValueAsString(EnemyDirector) +
			"\n See ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides");
		return;
	}

	ApplyEnemyDirectorUnitOverrides(*UnitOverrides);
}

void ARTSAsyncSpawner::ApplyEnemyDirectorUnitOverrides(const FEnemyDirectorUnitOverrides& UnitOverrides)
{
	ApplyTankUnitOverrides(UnitOverrides.M_TankOverrides);
	ApplySquadUnitOverrides(UnitOverrides.M_SquadOverrides);
	ApplyNomadicUnitOverrides(UnitOverrides.M_NomadicOverrides);
	ApplyAircraftUnitOverrides(UnitOverrides.M_AircraftOverrides);
}

void ARTSAsyncSpawner::ApplyTankUnitOverrides(const TArray<FTankTrainingOptionSetup>& UnitOverrides)
{
	for (const FTankTrainingOptionSetup& UnitOverride : UnitOverrides)
	{
		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(UnitOverride.TankSubtype));
		ApplyTrainingOptionOverride(TrainingOption, UnitOverride.UnitClass);
	}
}

void ARTSAsyncSpawner::ApplySquadUnitOverrides(const TArray<FSquadTrainingOptionSetup>& UnitOverrides)
{
	for (const FSquadTrainingOptionSetup& UnitOverride : UnitOverrides)
	{
		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(UnitOverride.SquadSubtype));
		ApplyTrainingOptionOverride(TrainingOption, UnitOverride.UnitClass);
	}
}

void ARTSAsyncSpawner::ApplyNomadicUnitOverrides(const TArray<FNomadicTrainingOptionSetup>& UnitOverrides)
{
	for (const FNomadicTrainingOptionSetup& UnitOverride : UnitOverrides)
	{
		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(UnitOverride.NomadicSubtype));
		ApplyTrainingOptionOverride(TrainingOption, UnitOverride.UnitClass);
	}
}

void ARTSAsyncSpawner::ApplyAircraftUnitOverrides(const TArray<FAircraftTrainingOptionSetup>& UnitOverrides)
{
	for (const FAircraftTrainingOptionSetup& UnitOverride : UnitOverrides)
	{
		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(UnitOverride.AircraftSubtype));
		ApplyTrainingOptionOverride(TrainingOption, UnitOverride.UnitClass);
	}
}

void ARTSAsyncSpawner::ApplyTrainingOptionOverride(
	const FTrainingOption& TrainingOption,
	const TSoftClassPtr<AActor>& UnitClass)
{
	if (TrainingOption.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"Enemy director unit overrides contain a None training option."
			"\n See ARTSAsyncSpawner::ApplyTrainingOptionOverride");
		return;
	}

	if (UnitClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"Enemy director unit override has no class for option: " + TrainingOption.GetTrainingName() +
			"\n See ARTSAsyncSpawner::ApplyTrainingOptionOverride");
		return;
	}

	TSoftClassPtr<AActor>* const ExistingUnitClass = M_TrainingOptionMap.Find(TrainingOption);
	if (ExistingUnitClass == nullptr)
	{
		return;
	}

	*ExistingUnitClass = UnitClass;
}
