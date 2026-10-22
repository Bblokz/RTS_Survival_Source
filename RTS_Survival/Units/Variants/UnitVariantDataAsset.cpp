// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantDataAsset.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#endif

namespace UnitVariantDataAssetPrivate
{
	void CopyTrainingOverrides(const FUnitVariantDefinition& Definition,
	                           FUnitVariantTrainingPresentation& OutPresentation)
	{
		if (Definition.bOverrideTrainingImageBrush && IsValid(Definition.TrainingImageBrush))
		{
			OutPresentation.bOverrideImageBrush = true;
			OutPresentation.ImageBrush = Definition.TrainingImageBrush;
		}
		if (Definition.bOverrideTrainingTitle)
		{
			OutPresentation.bOverrideTitle = true;
			OutPresentation.Title = Definition.TrainingTitle;
		}
		if (Definition.bOverrideTrainingDescription)
		{
			OutPresentation.bOverrideDescription = true;
			OutPresentation.Description = Definition.TrainingDescription;
		}
		if (Definition.bOverrideTrainingTooltip)
		{
			OutPresentation.bOverrideTooltip = true;
			OutPresentation.Tooltip = Definition.TrainingTooltip;
		}
	}

#if WITH_EDITOR
	void AddEntryError(FDataValidationContext& Context, const FString& KeyName, const int32 VariantIndex,
	                   const FString& Problem)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s variant %d: %s"), *KeyName, VariantIndex,
		                                                   *Problem)));
	}

	bool ValidateBaseDefinition(const FString& KeyName, const FUnitVariantDefinition& BaseDefinition,
	                            FDataValidationContext& Context)
	{
		bool bIsValid = true;
		if (not BaseDefinition.UnitClass.IsNull())
		{
			AddEntryError(Context, KeyName, 0, TEXT("Base must not have a unit class; the spawner's own setup holds "
				"the base class."));
			bIsValid = false;
		}
		const bool bHasTrainingOverride = BaseDefinition.bOverrideTrainingImageBrush
			|| BaseDefinition.bOverrideTrainingTitle
			|| BaseDefinition.bOverrideTrainingDescription
			|| BaseDefinition.bOverrideTrainingTooltip;
		if (bHasTrainingOverride)
		{
			AddEntryError(Context, KeyName, 0, TEXT("Base always shows the DT_TrainingOptions row; untick its "
				"training overrides."));
			bIsValid = false;
		}
		return bIsValid;
	}

	/**
	 * @param RequiredParentClass The native class every variant class of this map must derive from.
	 * @param bSupportsBxpPreviewMesh False for tanks and squads, where a preview mesh is ignored.
	 */
	bool ValidateVariantClass(const FString& KeyName, const int32 VariantIndex,
	                          const FUnitVariantDefinition& Definition, const UClass* RequiredParentClass,
	                          const bool bSupportsBxpPreviewMesh, FDataValidationContext& Context)
	{
		if (not bSupportsBxpPreviewMesh && not Definition.BxpPreviewMesh.IsNull())
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s variant %d: a preview mesh is only used by building expansions and is ignored here."),
				*KeyName, VariantIndex)));
		}
		if (Definition.UnitClass.IsNull())
		{
			AddEntryError(Context, KeyName, VariantIndex, TEXT("no unit class; an entry without a class is not "
				"implemented, shorten the array instead."));
			return false;
		}
		const UClass* VariantClass = Definition.UnitClass.LoadSynchronous();
		if (not IsValid(VariantClass))
		{
			AddEntryError(Context, KeyName, VariantIndex, FString::Printf(TEXT("could not load the unit class %s."),
				*Definition.UnitClass.ToString()));
			return false;
		}
		if (VariantClass->IsChildOf(RequiredParentClass))
		{
			return true;
		}
		AddEntryError(Context, KeyName, VariantIndex, FString::Printf(TEXT("unit class %s is not a child of %s."),
			*VariantClass->GetName(), *RequiredParentClass->GetName()));
		return false;
	}

	bool ValidateTrainingImageBrush(const FString& KeyName, const int32 VariantIndex,
	                                const FUnitVariantDefinition& Definition, FDataValidationContext& Context)
	{
		if (not Definition.bOverrideTrainingImageBrush)
		{
			return true;
		}
		if (not IsValid(Definition.TrainingImageBrush))
		{
			AddEntryError(Context, KeyName, VariantIndex, TEXT("bOverrideTrainingImageBrush is ticked without a "
				"TrainingImageBrush."));
			return false;
		}
		// The training item extracts this texture for the cooldown clock material.
		if (Cast<UTexture2D>(Definition.TrainingImageBrush->Brush.GetResourceObject()))
		{
			return true;
		}
		AddEntryError(Context, KeyName, VariantIndex, TEXT("TrainingImageBrush must use a Texture2D as its image."));
		return false;
	}

	bool ValidateTrainingTexts(const FString& KeyName, const int32 VariantIndex,
	                           const FUnitVariantDefinition& Definition, FDataValidationContext& Context)
	{
		bool bIsValid = true;
		if (Definition.bOverrideTrainingTitle && Definition.TrainingTitle.IsEmpty())
		{
			AddEntryError(Context, KeyName, VariantIndex, TEXT("bOverrideTrainingTitle is ticked with an empty "
				"TrainingTitle."));
			bIsValid = false;
		}
		if (Definition.bOverrideTrainingDescription && Definition.TrainingDescription.IsEmpty())
		{
			AddEntryError(Context, KeyName, VariantIndex, TEXT("bOverrideTrainingDescription is ticked with an "
				"empty TrainingDescription."));
			bIsValid = false;
		}
		if (Definition.bOverrideTrainingTooltip && Definition.TrainingTooltip.IsEmpty())
		{
			AddEntryError(Context, KeyName, VariantIndex, TEXT("bOverrideTrainingTooltip is ticked with an empty "
				"TrainingTooltip."));
			bIsValid = false;
		}
		return bIsValid;
	}

	/**
	 * @brief The class and training UI rules for one entry; the key and count rules live in ValidateEntry.
	 * @param RequiredParentClass The native class every variant class of this map must derive from.
	 * @param bSupportsBxpPreviewMesh False for tanks and squads.
	 * @return False when any rule is broken.
	 */
	bool ValidateDefinitions(const UEnum* KeyEnum, const int64 KeyValue, const FUnitVariantEntry& Entry,
	                         const UClass* RequiredParentClass, const bool bSupportsBxpPreviewMesh,
	                         FDataValidationContext& Context)
	{
		if (Entry.Variants.IsEmpty())
		{
			return true;
		}
		const FString KeyName = KeyEnum->GetNameStringByValue(KeyValue);
		bool bIsValid = ValidateBaseDefinition(KeyName, Entry.Variants[0], Context);
		const int32 CheckedVariantCount = FMath::Min(Entry.Variants.Num(), UnitVariants::MaxVariantsPerUnit);
		for (int32 VariantIndex = 1; VariantIndex < CheckedVariantCount; ++VariantIndex)
		{
			const FUnitVariantDefinition& Definition = Entry.Variants[VariantIndex];
			bIsValid &= ValidateVariantClass(KeyName, VariantIndex, Definition, RequiredParentClass,
			                                 bSupportsBxpPreviewMesh, Context);
			bIsValid &= ValidateTrainingImageBrush(KeyName, VariantIndex, Definition, Context);
			bIsValid &= ValidateTrainingTexts(KeyName, VariantIndex, Definition, Context);
		}
		return bIsValid;
	}

	void CollectVariantClasses(const FUnitVariantEntry& Entry, TMap<FSoftObjectPath, int32>& InOutClassUseCounts)
	{
		for (const FUnitVariantDefinition& Definition : Entry.Variants)
		{
			if (Definition.UnitClass.IsNull())
			{
				continue;
			}
			InOutClassUseCounts.FindOrAdd(Definition.UnitClass.ToSoftObjectPath())++;
		}
	}
#endif
}

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

TSoftClassPtr<AActor> UUnitVariantDataAsset::FindVariantClass(const FTrainingOption& Unit,
                                                              const EUnitVariant Variant) const
{
	if (Variant == EUnitVariant::Base)
	{
		return TSoftClassPtr<AActor>();
	}
	const FUnitVariantDefinition* Definition = FindVariantDefinition(Unit, Variant);
	if (not Definition)
	{
		return TSoftClassPtr<AActor>();
	}
	return Definition->UnitClass;
}

TSoftObjectPtr<UStaticMesh> UUnitVariantDataAsset::FindBxpVariantPreviewMesh(const EBuildingExpansionType BxpType,
                                                                             const EUnitVariant Variant) const
{
	if (Variant == EUnitVariant::Base)
	{
		return TSoftObjectPtr<UStaticMesh>();
	}
	const FTrainingOption BxpUnit(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(BxpType));
	const FUnitVariantDefinition* Definition = FindVariantDefinition(BxpUnit, Variant);
	if (not Definition)
	{
		return TSoftObjectPtr<UStaticMesh>();
	}
	return Definition->BxpPreviewMesh;
}

FUnitVariantTrainingPresentation UUnitVariantDataAsset::GetTrainingPresentation(
	const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	FUnitVariantTrainingPresentation Presentation;
	Presentation.Variant = Variant;
	if (Variant == EUnitVariant::Base)
	{
		return Presentation;
	}
	const FUnitVariantDefinition* Definition = FindVariantDefinition(Unit, Variant);
	if (not Definition)
	{
		return Presentation;
	}
	UnitVariantDataAssetPrivate::CopyTrainingOverrides(*Definition, Presentation);
	return Presentation;
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

const TArray<FUnitVariantSelection>& UUnitVariantDataAsset::GetDebugForcedVariants() const
{
#if UE_BUILD_SHIPPING
	static const TArray<FUnitVariantSelection> EmptyForcedVariants;
	return EmptyForcedVariants;
#else
	return M_DebugForcedVariants;
#endif
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
	using namespace UnitVariantDataAssetPrivate;
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bAllEntriesValid = true;

	for (const TPair<ETankSubtype, FUnitVariantEntry>& TankPair : M_TankVariants)
	{
		const int64 KeyValue = static_cast<int64>(TankPair.Key);
		bAllEntriesValid &= ValidateEntry(StaticEnum<ETankSubtype>(), KeyValue,
		                                  TankPair.Key == ETankSubtype::Tank_None, TankPair.Value, Context);
		bAllEntriesValid &= ValidateDefinitions(StaticEnum<ETankSubtype>(), KeyValue, TankPair.Value,
		                                        ATankMaster::StaticClass(), false, Context);
	}
	for (const TPair<ESquadSubtype, FUnitVariantEntry>& SquadPair : M_SquadVariants)
	{
		const int64 KeyValue = static_cast<int64>(SquadPair.Key);
		bAllEntriesValid &= ValidateEntry(StaticEnum<ESquadSubtype>(), KeyValue,
		                                  SquadPair.Key == ESquadSubtype::Squad_None, SquadPair.Value, Context);
		bAllEntriesValid &= ValidateDefinitions(StaticEnum<ESquadSubtype>(), KeyValue, SquadPair.Value,
		                                        ASquadController::StaticClass(), false, Context);
	}
	for (const TPair<EBuildingExpansionType, FUnitVariantEntry>& BxpPair : M_BuildingExpansionVariants)
	{
		const int64 KeyValue = static_cast<int64>(BxpPair.Key);
		bAllEntriesValid &= ValidateEntry(StaticEnum<EBuildingExpansionType>(), KeyValue,
		                                  BxpPair.Key == EBuildingExpansionType::BXT_Invalid, BxpPair.Value,
		                                  Context);
		bAllEntriesValid &= ValidateDefinitions(StaticEnum<EBuildingExpansionType>(), KeyValue, BxpPair.Value,
		                                        ABuildingExpansion::StaticClass(), true, Context);
	}
	bAllEntriesValid &= ValidateNoClassUsedTwice(Context);

	if (not M_DebugForcedVariants.IsEmpty())
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%d debug forced variant(s) are set; clear the Debug list before committing."),
			M_DebugForcedVariants.Num())));
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

bool UUnitVariantDataAsset::ValidateNoClassUsedTwice(FDataValidationContext& Context) const
{
	TMap<FSoftObjectPath, int32> ClassUseCounts;
	for (const TPair<ETankSubtype, FUnitVariantEntry>& TankPair : M_TankVariants)
	{
		UnitVariantDataAssetPrivate::CollectVariantClasses(TankPair.Value, ClassUseCounts);
	}
	for (const TPair<ESquadSubtype, FUnitVariantEntry>& SquadPair : M_SquadVariants)
	{
		UnitVariantDataAssetPrivate::CollectVariantClasses(SquadPair.Value, ClassUseCounts);
	}
	for (const TPair<EBuildingExpansionType, FUnitVariantEntry>& BxpPair : M_BuildingExpansionVariants)
	{
		UnitVariantDataAssetPrivate::CollectVariantClasses(BxpPair.Value, ClassUseCounts);
	}

	bool bIsValid = true;
	for (const TPair<FSoftObjectPath, int32>& ClassUseCount : ClassUseCounts)
	{
		if (ClassUseCount.Value <= 1)
		{
			continue;
		}
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("Unit class %s is used by %d variant definitions; every variant needs its own class."),
			*ClassUseCount.Key.ToString(), ClassUseCount.Value)));
		bIsValid = false;
	}
	return bIsValid;
}
#endif
