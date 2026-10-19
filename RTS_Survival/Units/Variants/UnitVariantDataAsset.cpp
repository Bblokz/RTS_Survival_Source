// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UUnitVariantDataAsset::GetImplementedVariantCount(const FTrainingOption& Unit) const
{
	const FUnitVariantEntry* Entry = FindEntry(Unit);
	if (not Entry)
	{
		return 1;
	}
	return FMath::Clamp(Entry->Variants.Num(), 1, UnitVariants::MaxVariantsPerUnit);
}

bool UUnitVariantDataAsset::GetIsVariantImplemented(const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	return static_cast<int32>(Variant) < GetImplementedVariantCount(Unit);
}

const FUnitVariantDefinition* UUnitVariantDataAsset::FindVariantDefinition(
	const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	const FUnitVariantEntry* Entry = FindEntry(Unit);
	if (not Entry || not GetIsVariantImplemented(Unit, Variant))
	{
		return nullptr;
	}
	const int32 VariantIndex = static_cast<int32>(Variant);
	if (not Entry->Variants.IsValidIndex(VariantIndex))
	{
		return nullptr;
	}
	return &Entry->Variants[VariantIndex];
}

TArray<FTrainingOption> UUnitVariantDataAsset::GetUnitsWithChoice() const
{
	TArray<FTrainingOption> UnitsWithChoice;
	for (const TPair<ETankSubtype, FUnitVariantEntry>& TankPair : M_TankVariants)
	{
		const FTrainingOption Unit(EAllUnitType::UNType_Tank, static_cast<uint8>(TankPair.Key));
		if (GetImplementedVariantCount(Unit) > 1)
		{
			UnitsWithChoice.Add(Unit);
		}
	}
	for (const TPair<ESquadSubtype, FUnitVariantEntry>& SquadPair : M_SquadVariants)
	{
		const FTrainingOption Unit(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadPair.Key));
		if (GetImplementedVariantCount(Unit) > 1)
		{
			UnitsWithChoice.Add(Unit);
		}
	}
	for (const TPair<EBuildingExpansionType, FUnitVariantEntry>& BxpPair : M_BuildingExpansionVariants)
	{
		const FTrainingOption Unit(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(BxpPair.Key));
		if (GetImplementedVariantCount(Unit) > 1)
		{
			UnitsWithChoice.Add(Unit);
		}
	}
	return UnitsWithChoice;
}

const FUnitVariantEntry* UUnitVariantDataAsset::FindEntry(const FTrainingOption& Unit) const
{
	switch (Unit.UnitType)
	{
	case EAllUnitType::UNType_Tank:
		return M_TankVariants.Find(static_cast<ETankSubtype>(Unit.SubtypeValue));
	case EAllUnitType::UNType_Squad:
		return M_SquadVariants.Find(static_cast<ESquadSubtype>(Unit.SubtypeValue));
	case EAllUnitType::UNType_BuildingExpansion:
		return M_BuildingExpansionVariants.Find(static_cast<EBuildingExpansionType>(Unit.SubtypeValue));
	default:
		return nullptr;
	}
}

#if WITH_EDITOR
EDataValidationResult UUnitVariantDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bAllEntriesValid = true;

	for (const TPair<ETankSubtype, FUnitVariantEntry>& TankPair : M_TankVariants)
	{
		bAllEntriesValid &= ValidateEntry(StaticEnum<ETankSubtype>(), static_cast<int64>(TankPair.Key),
		                                  TankPair.Key == ETankSubtype::Tank_None, TankPair.Value, Context);
	}
	for (const TPair<ESquadSubtype, FUnitVariantEntry>& SquadPair : M_SquadVariants)
	{
		bAllEntriesValid &= ValidateEntry(StaticEnum<ESquadSubtype>(), static_cast<int64>(SquadPair.Key),
		                                  SquadPair.Key == ESquadSubtype::Squad_None, SquadPair.Value, Context);
	}
	for (const TPair<EBuildingExpansionType, FUnitVariantEntry>& BxpPair : M_BuildingExpansionVariants)
	{
		bAllEntriesValid &= ValidateEntry(StaticEnum<EBuildingExpansionType>(), static_cast<int64>(BxpPair.Key),
		                                  BxpPair.Key == EBuildingExpansionType::BXT_Invalid, BxpPair.Value,
		                                  Context);
	}

	if (not bAllEntriesValid)
	{
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}

bool UUnitVariantDataAsset::ValidateEntry(const UEnum* KeyEnum, const int64 KeyValue, const bool bIsInvalidKey,
                                          const FUnitVariantEntry& Entry, FDataValidationContext& Context)
{
	const FString KeyName = KeyEnum->GetNameStringByValue(KeyValue);
	bool bIsValid = true;

	if (bIsInvalidKey)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s is not a unit; remove this entry."), *KeyName)));
		bIsValid = false;
	}
	if (GetIsRussianKey(KeyEnum, KeyValue))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s is a Russian unit; only units the player owns can have variants."), *KeyName)));
		bIsValid = false;
	}

	const int32 VariantCount = Entry.Variants.Num();
	if (VariantCount == 0 || VariantCount > UnitVariants::MaxVariantsPerUnit)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s has %d variants; a unit has 1 to %d (index 0 is the base unit)."),
			*KeyName, VariantCount, UnitVariants::MaxVariantsPerUnit)));
		bIsValid = false;
	}

	for (int32 VariantIndex = 1; VariantIndex < VariantCount; ++VariantIndex)
	{
		if (not Entry.Variants[VariantIndex].DisplayName.IsEmpty())
		{
			continue;
		}
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("%s variant %d has no display name; the variant UI needs one for every non-base variant."),
			*KeyName, VariantIndex)));
		bIsValid = false;
	}
	return bIsValid;
}

bool UUnitVariantDataAsset::GetIsRussianKey(const UEnum* KeyEnum, const int64 KeyValue)
{
	// Squads and BXPs carry Rus in the enumerator name; tanks only in the display name (Tank_T34_85 is "Rus Medium").
	const FString KeyName = KeyEnum->GetNameStringByValue(KeyValue);
	const FString KeyDisplayName = KeyEnum->GetDisplayNameTextByValue(KeyValue).ToString();
	return KeyName.Contains(TEXT("Rus"), ESearchCase::CaseSensitive)
		|| KeyDisplayName.StartsWith(TEXT("Rus"), ESearchCase::CaseSensitive);
}
#endif
