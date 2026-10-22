// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerUnitVariantManager.h"

#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPlayerUnitVariantManager::UPlayerUnitVariantManager()
	: M_VariantDataAsset(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerUnitVariantManager::InitFromProfile(const TArray<FUnitVariantSelection>& SavedSelections)
{
	CacheVariantDataAsset();
	M_SelectedVariants.Empty();
	for (const FUnitVariantSelection& Selection : SavedSelections)
	{
		(void)StoreSelectionClamped(Selection);
	}
#if !UE_BUILD_SHIPPING
	ApplyDebugForcedVariants();
#endif
}

void UPlayerUnitVariantManager::ApplySelectionsToSpawner(ARTSAsyncSpawner* AsyncSpawner)
{
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Cannot apply the unit variant selections as the async spawner is not valid."
			"\nAt function: UPlayerUnitVariantManager::ApplySelectionsToSpawner");
		return;
	}
	M_AsyncSpawner = AsyncSpawner;
	if (not GetIsValidVariantDataAsset())
	{
		return;
	}
	// The spawner reports each failure; dropping it keeps the selection equal to what actually spawns.
	const TArray<FTrainingOption> FailedUnits = AsyncSpawner->ApplyPlayerUnitVariants(
		*M_VariantDataAsset, GetSelectionsForSave());
	for (const FTrainingOption& FailedUnit : FailedUnits)
	{
		M_SelectedVariants.Remove(FailedUnit);
	}
}

EUnitVariant UPlayerUnitVariantManager::GetSelectedVariant(const FTrainingOption& Unit) const
{
	// Runs on every training menu refresh: Find, never operator[], and no reporting here.
	if (const EUnitVariant* SelectedVariant = M_SelectedVariants.Find(GetSpawnerKey(Unit)))
	{
		return *SelectedVariant;
	}
	return EUnitVariant::Base;
}

bool UPlayerUnitVariantManager::SetSelectedVariant(const FTrainingOption& Unit, const EUnitVariant Variant)
{
	FUnitVariantSelection Selection;
	Selection.Unit = GetSpawnerKey(Unit);
	Selection.Variant = Variant;
	if (not GetIsUnitTypeSupported(Selection.Unit.UnitType))
	{
		return false;
	}
	CacheVariantDataAsset();
	const bool bWasStored = StoreSelectionClamped(Selection);
	if (not GetIsValidAsyncSpawnerNoReport() || not GetIsValidVariantDataAsset())
	{
		return bWasStored;
	}
	// A clamp falls back to Base; the spawner follows the stored choice so selection and spawned class agree.
	const EUnitVariant StoredVariant = GetSelectedVariant(Selection.Unit);
	if (M_AsyncSpawner->SetPlayerUnitVariant(*M_VariantDataAsset, Selection.Unit, StoredVariant))
	{
		return bWasStored;
	}
	M_SelectedVariants.Remove(Selection.Unit);
	return false;
}

FUnitVariantTrainingPresentation UPlayerUnitVariantManager::GetTrainingPresentation(const FTrainingOption& Unit) const
{
	const FTrainingOption SpawnerKey = GetSpawnerKey(Unit);
	const EUnitVariant SelectedVariant = GetSelectedVariant(SpawnerKey);
	if (SelectedVariant == EUnitVariant::Base)
	{
		return FUnitVariantTrainingPresentation();
	}
	const UUnitVariantDataAsset* VariantDataAsset = GetVariantDataAsset();
	if (not IsValid(VariantDataAsset))
	{
		return FUnitVariantTrainingPresentation();
	}
	return VariantDataAsset->GetTrainingPresentation(SpawnerKey, SelectedVariant);
}

TArray<FUnitVariantSelection> UPlayerUnitVariantManager::GetSelectionsForSave() const
{
	TArray<FUnitVariantSelection> Selections;
	Selections.Reserve(M_SelectedVariants.Num());
	for (const TPair<FTrainingOption, EUnitVariant>& SelectedPair : M_SelectedVariants)
	{
		FUnitVariantSelection Selection;
		Selection.Unit = SelectedPair.Key;
		Selection.Variant = SelectedPair.Value;
		Selections.Add(Selection);
	}
	return Selections;
}

const UUnitVariantDataAsset* UPlayerUnitVariantManager::GetVariantDataAsset() const
{
	if (IsValid(M_VariantDataAsset))
	{
		return M_VariantDataAsset;
	}
	return UUnitVariantSettings::Get()->LoadVariantDataAsset();
}

bool UPlayerUnitVariantManager::GetIsUnitTypeSupported(const EAllUnitType UnitType)
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Squad:
	case EAllUnitType::UNType_BuildingExpansion:
		return true;
	default:
		// Nomadic trucks, aircraft, standalone turrets and garrison buildings stay on Base; harvesters are keyed as
		// tanks through GetSpawnerKey. See Docs/Units/UnitVariants.md section 13 to extend.
		return false;
	}
}

FTrainingOption UPlayerUnitVariantManager::GetSpawnerKey(const FTrainingOption& Unit)
{
	// The spawner's setup arrays key every tank subtype, the Pz I harvester included, under UNType_Tank.
	if (Unit.UnitType == EAllUnitType::UNType_Harvester)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, Unit.SubtypeValue);
	}
	return Unit;
}

void UPlayerUnitVariantManager::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_LoadVariantDataAsset();
}

void UPlayerUnitVariantManager::BeginPlay_LoadVariantDataAsset()
{
	CacheVariantDataAsset();
}

void UPlayerUnitVariantManager::ApplyDebugForcedVariants()
{
	if (not GetIsValidVariantDataAsset())
	{
		return;
	}
	for (const FUnitVariantSelection& ForcedSelection : M_VariantDataAsset->GetDebugForcedVariants())
	{
		if (not StoreSelectionClamped(ForcedSelection))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unit variants: ignored forced variant %s for %s."),
			       *Global_GetUnitVariantAsString(ForcedSelection.Variant), *ForcedSelection.Unit.GetTrainingName());
			continue;
		}
		UE_LOG(LogTemp, Warning, TEXT("Unit variants: FORCED %s -> %s from the Debug list of the unit variant data "
			       "asset."), *ForcedSelection.Unit.GetTrainingName(),
		       *Global_GetUnitVariantAsString(ForcedSelection.Variant));
	}
}

bool UPlayerUnitVariantManager::StoreSelectionClamped(const FUnitVariantSelection& Selection)
{
	const FTrainingOption SpawnerKey = GetSpawnerKey(Selection.Unit);
	if (not GetIsUnitTypeSupported(SpawnerKey.UnitType))
	{
		return false;
	}
	if (Selection.Variant == EUnitVariant::Base)
	{
		M_SelectedVariants.Remove(SpawnerKey);
		return true;
	}
	if (not GetIsValidVariantDataAsset())
	{
		M_SelectedVariants.Remove(SpawnerKey);
		return false;
	}
	if (not M_VariantDataAsset->GetIsVariantImplemented(SpawnerKey, Selection.Variant))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Unit variant %s is not implemented for %s in the unit variant data asset; the unit uses Base."
				"\nAt function: UPlayerUnitVariantManager::StoreSelectionClamped"),
			*Global_GetUnitVariantAsString(Selection.Variant), *SpawnerKey.GetTrainingName()));
		M_SelectedVariants.Remove(SpawnerKey);
		return false;
	}
	M_SelectedVariants.Add(SpawnerKey, Selection.Variant);
	return true;
}

void UPlayerUnitVariantManager::CacheVariantDataAsset()
{
	if (IsValid(M_VariantDataAsset))
	{
		return;
	}
	M_VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
}

bool UPlayerUnitVariantManager::GetIsValidVariantDataAsset() const
{
	if (IsValid(M_VariantDataAsset))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_VariantDataAsset",
	                                                             "GetIsValidVariantDataAsset", this);
	return false;
}

bool UPlayerUnitVariantManager::GetIsValidAsyncSpawnerNoReport() const
{
	return M_AsyncSpawner.IsValid();
}
