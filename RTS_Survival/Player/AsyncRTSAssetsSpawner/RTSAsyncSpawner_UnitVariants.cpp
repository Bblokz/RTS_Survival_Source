// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSAsyncSpawner.h"

#include "Engine/StaticMesh.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

TArray<FTrainingOption> ARTSAsyncSpawner::ApplyPlayerUnitVariants(const UUnitVariantDataAsset& VariantDataAsset,
                                                                  const TArray<FUnitVariantSelection>& Selections)
{
	TArray<FTrainingOption> FailedUnits;
	for (const FUnitVariantSelection& Selection : Selections)
	{
		if (Selection.Variant == EUnitVariant::Base)
		{
			continue;
		}
		if (not ApplyUnitVariant(VariantDataAsset, Selection.Unit, Selection.Variant))
		{
			FailedUnits.Add(Selection.Unit);
		}
	}
	return FailedUnits;
}

bool ARTSAsyncSpawner::SetPlayerUnitVariant(const UUnitVariantDataAsset& VariantDataAsset,
                                            const FTrainingOption& Unit, const EUnitVariant Variant)
{
	return ApplyUnitVariant(VariantDataAsset, Unit, Variant);
}

TSoftClassPtr<AActor> ARTSAsyncSpawner::GetTrainingOptionClass(const FTrainingOption& TrainingOption) const
{
	if (const TSoftClassPtr<AActor>* const MappedClass = M_TrainingOptionMap.Find(TrainingOption))
	{
		return *MappedClass;
	}
	if (TrainingOption.UnitType != EAllUnitType::UNType_BuildingExpansion)
	{
		return TSoftClassPtr<AActor>();
	}
	const EBuildingExpansionType BxpType = static_cast<EBuildingExpansionType>(TrainingOption.SubtypeValue);
	const TSoftClassPtr<ABuildingExpansion>* const MappedBxpClass = BuildingExpansionMap.Find(BxpType);
	if (not MappedBxpClass)
	{
		return TSoftClassPtr<AActor>();
	}
	return TSoftClassPtr<AActor>(MappedBxpClass->ToSoftObjectPath());
}

TSoftClassPtr<AActor> ARTSAsyncSpawner::GetBaseTrainingOptionClass(const FTrainingOption& TrainingOption) const
{
	if (const TSoftClassPtr<AActor>* const BaseClass = M_PreVariantTrainingOptionClasses.Find(TrainingOption))
	{
		return *BaseClass;
	}
	if (TrainingOption.UnitType == EAllUnitType::UNType_BuildingExpansion)
	{
		const EBuildingExpansionType BxpType = static_cast<EBuildingExpansionType>(TrainingOption.SubtypeValue);
		if (const TSoftClassPtr<ABuildingExpansion>* const BaseBxpClass = M_PreVariantBuildingExpansionClasses.Find(BxpType))
		{
			return TSoftClassPtr<AActor>(BaseBxpClass->ToSoftObjectPath());
		}
	}
	return GetTrainingOptionClass(TrainingOption);
}

bool ARTSAsyncSpawner::ApplyUnitVariant(const UUnitVariantDataAsset& VariantDataAsset, const FTrainingOption& Unit,
                                        const EUnitVariant Variant)
{
	switch (Unit.UnitType)
	{
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Squad:
		return ApplyTrainingOptionVariant(VariantDataAsset, Unit, Variant);
	case EAllUnitType::UNType_BuildingExpansion:
		return ApplyBuildingExpansionVariant(VariantDataAsset,
		                                     static_cast<EBuildingExpansionType>(Unit.SubtypeValue), Variant);
	default:
		RTSFunctionLibrary::ReportError("Unit variants are not supported for unit " + Unit.GetTrainingName() +
			"\nAt function: ARTSAsyncSpawner::ApplyUnitVariant");
		return false;
	}
}

bool ARTSAsyncSpawner::ApplyTrainingOptionVariant(const UUnitVariantDataAsset& VariantDataAsset,
                                                  const FTrainingOption& Unit, const EUnitVariant Variant)
{
	// Not ApplyTrainingOptionOverride: that one is silent on a missing key, a variant without a base is a data error.
	TSoftClassPtr<AActor>* const ExistingUnitClass = M_TrainingOptionMap.Find(Unit);
	if (not ExistingUnitClass)
	{
		RTSFunctionLibrary::ReportError("Unit variant " + Global_GetUnitVariantAsString(Variant) +
			" has no base spawn mapping for " + Unit.GetTrainingName() + "; add the unit to the spawner setup."
			"\nAt function: ARTSAsyncSpawner::ApplyTrainingOptionVariant");
		return false;
	}
	if (not M_PreVariantTrainingOptionClasses.Contains(Unit))
	{
		M_PreVariantTrainingOptionClasses.Add(Unit, *ExistingUnitClass);
	}
	if (Variant == EUnitVariant::Base)
	{
		*ExistingUnitClass = M_PreVariantTrainingOptionClasses.FindChecked(Unit);
		return true;
	}

	const TSoftClassPtr<AActor> VariantClass = VariantDataAsset.FindVariantClass(Unit, Variant);
	if (VariantClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("Unit variant " + Global_GetUnitVariantAsString(Variant) + " of " +
			Unit.GetTrainingName() + " has no unit class in the unit variant data asset; the base class stays."
			"\nAt function: ARTSAsyncSpawner::ApplyTrainingOptionVariant");
		return false;
	}
	if (M_DirectorOverriddenOptions.Contains(Unit))
	{
		RTSFunctionLibrary::ReportError("Unit variant " + Global_GetUnitVariantAsString(Variant) +
			" replaces an enemy director override for " + Unit.GetTrainingName() + "; check the director asset."
			"\nAt function: ARTSAsyncSpawner::ApplyTrainingOptionVariant");
	}
	*ExistingUnitClass = VariantClass;
	return true;
}

bool ARTSAsyncSpawner::ApplyBuildingExpansionVariant(const UUnitVariantDataAsset& VariantDataAsset,
                                                     const EBuildingExpansionType BxpType,
                                                     const EUnitVariant Variant)
{
	const FTrainingOption BxpUnit(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(BxpType));
	TSoftClassPtr<ABuildingExpansion>* const ExistingBxpClass = BuildingExpansionMap.Find(BxpType);
	if (not ExistingBxpClass)
	{
		RTSFunctionLibrary::ReportError("Unit variant " + Global_GetUnitVariantAsString(Variant) +
			" has no base spawn mapping for " + BxpUnit.GetTrainingName() + "; add it to BuildingExpansionMap."
			"\nAt function: ARTSAsyncSpawner::ApplyBuildingExpansionVariant");
		return false;
	}
	if (not M_PreVariantBuildingExpansionClasses.Contains(BxpType))
	{
		M_PreVariantBuildingExpansionClasses.Add(BxpType, *ExistingBxpClass);
		// Only an existing preview entry is remembered, so restoring also restores "no entry".
		if (UStaticMesh* const* const BasePreviewMesh = BxpPreviewMeshMap.Find(BxpType))
		{
			M_PreVariantBxpPreviewMeshes.Add(BxpType, *BasePreviewMesh);
		}
	}
	if (Variant == EUnitVariant::Base)
	{
		*ExistingBxpClass = M_PreVariantBuildingExpansionClasses.FindChecked(BxpType);
		RestoreBasePreviewMesh(BxpType);
		return true;
	}

	const TSoftClassPtr<AActor> VariantClass = VariantDataAsset.FindVariantClass(BxpUnit, Variant);
	if (VariantClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("Unit variant " + Global_GetUnitVariantAsString(Variant) + " of " +
			BxpUnit.GetTrainingName() + " has no unit class in the unit variant data asset; the base class stays."
			"\nAt function: ARTSAsyncSpawner::ApplyBuildingExpansionVariant");
		return false;
	}
	// The asset holds AActor soft classes; the map is typed. IsDataValid guarantees the class is an expansion.
	*ExistingBxpClass = TSoftClassPtr<ABuildingExpansion>(VariantClass.ToSoftObjectPath());
	ApplyBxpVariantPreviewMesh(VariantDataAsset, BxpType, Variant);
	return true;
}

void ARTSAsyncSpawner::ApplyBxpVariantPreviewMesh(const UUnitVariantDataAsset& VariantDataAsset,
                                                  const EBuildingExpansionType BxpType,
                                                  const EUnitVariant Variant)
{
	const TSoftObjectPtr<UStaticMesh> VariantPreviewMesh = VariantDataAsset.FindBxpVariantPreviewMesh(BxpType, Variant);
	if (VariantPreviewMesh.IsNull())
	{
		// A previous variant may have replaced the mesh; this one keeps the base preview.
		RestoreBasePreviewMesh(BxpType);
		return;
	}
	// SyncGetBuildingExpansionPreviewMesh reads hard pointers at placement time, so load now.
	UStaticMesh* const LoadedPreviewMesh = VariantPreviewMesh.LoadSynchronous();
	if (not IsValid(LoadedPreviewMesh))
	{
		RTSFunctionLibrary::ReportError("Could not load the variant preview mesh " + VariantPreviewMesh.ToString() +
			"; the base preview mesh stays."
			"\nAt function: ARTSAsyncSpawner::ApplyBxpVariantPreviewMesh");
		RestoreBasePreviewMesh(BxpType);
		return;
	}
	BxpPreviewMeshMap.Add(BxpType, LoadedPreviewMesh);
}

void ARTSAsyncSpawner::RestoreBasePreviewMesh(const EBuildingExpansionType BxpType)
{
	// Only called after ApplyBuildingExpansionVariant remembered the base, so a missing entry means the base had none.
	const TObjectPtr<UStaticMesh>* const BasePreviewMesh = M_PreVariantBxpPreviewMeshes.Find(BxpType);
	if (not BasePreviewMesh)
	{
		BxpPreviewMeshMap.Remove(BxpType);
		return;
	}
	BxpPreviewMeshMap.Add(BxpType, *BasePreviewMesh);
}
