// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlayerUnitVariantManager.h"

#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
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

EUnitVariant UPlayerUnitVariantManager::GetSelectedVariant(const FTrainingOption& Unit) const
{
	// Runs for every spawned unit: Find, never operator[], and no reporting here.
	if (const EUnitVariant* SelectedVariant = M_SelectedVariants.Find(Unit))
	{
		return *SelectedVariant;
	}
	return EUnitVariant::Base;
}

EUnitVariant UPlayerUnitVariantManager::ResolveVariantForUnit(const URTSComponent* RTSComponent) const
{
	if (not IsValid(RTSComponent))
	{
		return EUnitVariant::Base;
	}
	if (RTSComponent->GetOwningPlayer() != UPlayerTechManager::PlayerTechOwnerIndex)
	{
		return EUnitVariant::Base;
	}
	const EAllUnitType UnitType = RTSComponent->GetUnitType();
	if (not GetIsUnitTypeSupported(UnitType))
	{
		return EUnitVariant::Base;
	}
	return GetSelectedVariant(FTrainingOption(UnitType, RTSComponent->GetUnitSubType()));
}

bool UPlayerUnitVariantManager::SetSelectedVariant(const FTrainingOption& Unit, const EUnitVariant Variant)
{
	CacheVariantDataAsset();
	FUnitVariantSelection Selection;
	Selection.Unit = Unit;
	Selection.Variant = Variant;
	return StoreSelectionClamped(Selection);
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
		// Harvesters (Pz I harvester is an ATankMaster), nomadic trucks, aircraft, standalone turrets and garrison
		// buildings stay on Base; see Docs/Units/UnitVariants.md section 10 to extend.
		return false;
	}
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
	for (const FUnitVariantSelection& ForcedSelection : UUnitVariantSettings::Get()->DebugForcedVariants)
	{
		if (not GetIsUnitTypeSupported(ForcedSelection.Unit.UnitType))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unit variants: ignored forced variant for unsupported unit %s."),
			       *ForcedSelection.Unit.GetTrainingName());
			continue;
		}
		if (ForcedSelection.Variant == EUnitVariant::Base)
		{
			M_SelectedVariants.Remove(ForcedSelection.Unit);
		}
		else
		{
			M_SelectedVariants.Add(ForcedSelection.Unit, ForcedSelection.Variant);
		}
		UE_LOG(LogTemp, Warning,
		       TEXT("Unit variants: FORCED %s -> %s from Project Settings > Unit Variants > Debug."),
		       *ForcedSelection.Unit.GetTrainingName(), *Global_GetUnitVariantAsString(ForcedSelection.Variant));
	}
}

bool UPlayerUnitVariantManager::StoreSelectionClamped(const FUnitVariantSelection& Selection)
{
	if (not GetIsUnitTypeSupported(Selection.Unit.UnitType))
	{
		return false;
	}
	if (Selection.Variant == EUnitVariant::Base)
	{
		M_SelectedVariants.Remove(Selection.Unit);
		return true;
	}
	if (not GetIsValidVariantDataAsset())
	{
		M_SelectedVariants.Remove(Selection.Unit);
		return false;
	}
	if (not M_VariantDataAsset->GetIsVariantImplemented(Selection.Unit, Selection.Variant))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Unit variant %s is not implemented for %s in the unit variant data asset; the unit uses Base."
				"\nAt function: UPlayerUnitVariantManager::StoreSelectionClamped"),
			*Global_GetUnitVariantAsString(Selection.Variant), *Selection.Unit.GetTrainingName()));
		M_SelectedVariants.Remove(Selection.Unit);
		return false;
	}
	M_SelectedVariants.Add(Selection.Unit, Selection.Variant);
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
