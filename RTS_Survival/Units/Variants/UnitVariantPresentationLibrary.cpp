// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantPresentationLibrary.h"

#include "Engine/DataTable.h"
#include "Internationalization/Regex.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutDataAsset.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "Slate/SlateBrushAsset.h"

namespace UnitVariantPresentationLibraryPrivate
{
	// DT_TrainingOptions rows are a Blueprint struct, whose members are stored as Title_13_<guid>; the authored name
	// lookup is editor only, so the stored name prefix is matched.
	const TCHAR* const TitleColumnName = TEXT("Title");
	const TCHAR* const TitleColumnPrefix = TEXT("Title_");
	const TCHAR* const RichTextTagPattern = TEXT("<[^>]*>");
	// Leading words of the subtype display names ("Ger Light Pz 38t Railgun") that the UI does not show.
	const TCHAR* const DisplayNamePrefixes[] = {
		TEXT("Ger "), TEXT("Rus "), TEXT("Light "), TEXT("Medium "), TEXT("Heavy "), TEXT("ArmCar ")
	};

	const uint8* FindTrainingTableRow(const UDataTable* TrainingOptionsTable, const FTrainingOption& Unit)
	{
		if (not IsValid(TrainingOptionsTable) || not TrainingOptionsTable->GetRowStruct())
		{
			return nullptr;
		}
		const FName RowName(*UTrainingOptionLibrary::GetTrainingOptionName(Unit.UnitType, Unit.SubtypeValue));
		return TrainingOptionsTable->FindRowUnchecked(RowName);
	}
}

USlateBrushAsset* UUnitVariantPresentationLibrary::GetTrainingImageBrush(
	const FUnitVariantTrainingPresentation& Presentation, USlateBrushAsset* TableImageBrush)
{
	if (not Presentation.bOverrideImageBrush || not IsValid(Presentation.ImageBrush))
	{
		return TableImageBrush;
	}
	return Presentation.ImageBrush;
}

FText UUnitVariantPresentationLibrary::GetTrainingTitle(const FUnitVariantTrainingPresentation& Presentation,
                                                        const FText& TableTitle)
{
	return Presentation.bOverrideTitle ? Presentation.Title : TableTitle;
}

FText UUnitVariantPresentationLibrary::GetTrainingDescription(const FUnitVariantTrainingPresentation& Presentation,
                                                              const FText& TableDescription)
{
	return Presentation.bOverrideDescription ? Presentation.Description : TableDescription;
}

FText UUnitVariantPresentationLibrary::GetTrainingTooltip(const FUnitVariantTrainingPresentation& Presentation,
                                                          const FText& TableTooltip)
{
	return Presentation.bOverrideTooltip ? Presentation.Tooltip : TableTooltip;
}

const UDataTable* UUnitVariantPresentationLibrary::LoadTrainingOptionsTable()
{
	const UTrainingLayoutDataAsset* TrainingLayout = UTrainingLayoutSettings::Get()->LoadTrainingLayout();
	if (not IsValid(TrainingLayout))
	{
		return nullptr;
	}
	return TrainingLayout->TrainingOptionIconTable.LoadSynchronous();
}

USlateBrushAsset* UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(const UDataTable* TrainingOptionsTable,
                                                                              const FTrainingOption& Unit)
{
	const uint8* RowData = UnitVariantPresentationLibraryPrivate::FindTrainingTableRow(TrainingOptionsTable, Unit);
	if (not RowData)
	{
		return nullptr;
	}
	for (TFieldIterator<FObjectPropertyBase> PropertyIt(TrainingOptionsTable->GetRowStruct()); PropertyIt; ++PropertyIt)
	{
		if (USlateBrushAsset* BrushAsset = Cast<USlateBrushAsset>(PropertyIt->GetObjectPropertyValue_InContainer(RowData)))
		{
			return BrushAsset;
		}
	}
	return nullptr;
}

FText UUnitVariantPresentationLibrary::FindTrainingTableTitle(const UDataTable* TrainingOptionsTable,
                                                              const FTrainingOption& Unit)
{
	const uint8* RowData = UnitVariantPresentationLibraryPrivate::FindTrainingTableRow(TrainingOptionsTable, Unit);
	if (not RowData)
	{
		return FText::GetEmpty();
	}
	for (TFieldIterator<FTextProperty> PropertyIt(TrainingOptionsTable->GetRowStruct()); PropertyIt; ++PropertyIt)
	{
		const FString PropertyName = PropertyIt->GetName();
		const bool bIsTitleColumn = PropertyName == UnitVariantPresentationLibraryPrivate::TitleColumnName
			|| PropertyName.StartsWith(UnitVariantPresentationLibraryPrivate::TitleColumnPrefix);
		if (not bIsTitleColumn)
		{
			continue;
		}
		return StripRichTextMarkup(PropertyIt->GetPropertyValue_InContainer(RowData));
	}
	return FText::GetEmpty();
}

FText UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(const FTrainingOption& Unit)
{
	FText SubtypeDisplayName;
	switch (Unit.UnitType)
	{
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Harvester:
		SubtypeDisplayName = StaticEnum<ETankSubtype>()->GetDisplayNameTextByValue(Unit.SubtypeValue);
		break;
	case EAllUnitType::UNType_Squad:
		SubtypeDisplayName = StaticEnum<ESquadSubtype>()->GetDisplayNameTextByValue(Unit.SubtypeValue);
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		SubtypeDisplayName = StaticEnum<EBuildingExpansionType>()->GetDisplayNameTextByValue(Unit.SubtypeValue);
		break;
	default:
		return FText::FromString(Unit.GetDisplayName());
	}
	FString DisplayName = SubtypeDisplayName.ToString();
	for (const TCHAR* const Prefix : UnitVariantPresentationLibraryPrivate::DisplayNamePrefixes)
	{
		DisplayName.RemoveFromStart(Prefix);
	}
	return FText::FromString(DisplayName);
}

FText UUnitVariantPresentationLibrary::StripRichTextMarkup(const FText& RichText)
{
	FString PlainText = RichText.ToString();
	const FRegexPattern TagPattern(UnitVariantPresentationLibraryPrivate::RichTextTagPattern);
	FRegexMatcher TagMatcher(TagPattern, PlainText);
	TArray<FString> Tags;
	while (TagMatcher.FindNext())
	{
		Tags.Add(TagMatcher.GetCaptureGroup(0));
	}
	for (const FString& Tag : Tags)
	{
		PlainText.ReplaceInline(*Tag, TEXT(""));
	}
	return FText::FromString(PlainText.TrimStartAndEnd());
}
