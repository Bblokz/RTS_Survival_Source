// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DeckCardCatalog.h"

#include "Engine/DataTable.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDeckCardCatalog, Log, All);

namespace DeckCardCatalogPrivate
{
	// Blueprint struct members are stored as <Name>_<index>_<guid>; the authored name lookup is editor only, so the
	// stored name prefix is matched (the same approach as UUnitVariantPresentationLibrary).
	const TCHAR* const ImageBrushColumn = TEXT("ImageBrush");
	const TCHAR* const TitleColumn = TEXT("Title");
	const TCHAR* const DescriptionColumn = TEXT("Description");
	const TCHAR* const CardTypeColumn = TEXT("CardType");
	const TCHAR* const CardRarityColumn = TEXT("CardRarity");
	const TCHAR* const TechnologyColumn = TEXT("Technology");
	const TCHAR* const TrainingOptionColumn = TEXT("TrainingOption");

	/** The columns of the card table, resolved once per load. A missing column reads as its default value. */
	struct FCardTableColumns
	{
		const FProperty* ImageBrush = nullptr;
		const FProperty* Title = nullptr;
		const FProperty* Description = nullptr;
		const FProperty* CardType = nullptr;
		const FProperty* CardRarity = nullptr;
		const FProperty* Technology = nullptr;
		const FProperty* TrainingOption = nullptr;
	};

	const FProperty* FindColumn(const UScriptStruct* RowStruct, const TCHAR* ColumnName)
	{
		const FString ColumnPrefix = FString(ColumnName) + TEXT("_");
		for (TFieldIterator<FProperty> PropertyIt(RowStruct); PropertyIt; ++PropertyIt)
		{
			const FString PropertyName = PropertyIt->GetName();
			if (PropertyName == ColumnName || PropertyName.StartsWith(ColumnPrefix))
			{
				return *PropertyIt;
			}
		}
		UE_LOG(LogRTSDeckCardCatalog, Warning, TEXT("Card table row struct %s has no column %s"),
		       *RowStruct->GetName(), ColumnName);
		return nullptr;
	}

	FCardTableColumns FindColumns(const UScriptStruct* RowStruct)
	{
		FCardTableColumns Columns;
		Columns.ImageBrush = FindColumn(RowStruct, ImageBrushColumn);
		Columns.Title = FindColumn(RowStruct, TitleColumn);
		Columns.Description = FindColumn(RowStruct, DescriptionColumn);
		Columns.CardType = FindColumn(RowStruct, CardTypeColumn);
		Columns.CardRarity = FindColumn(RowStruct, CardRarityColumn);
		Columns.Technology = FindColumn(RowStruct, TechnologyColumn);
		Columns.TrainingOption = FindColumn(RowStruct, TrainingOptionColumn);
		return Columns;
	}

	FText ReadText(const FProperty* Column, const uint8* RowData)
	{
		const FTextProperty* TextProperty = CastField<FTextProperty>(Column);
		return TextProperty ? TextProperty->GetPropertyValue_InContainer(RowData) : FText::GetEmpty();
	}

	/** Blueprint structs store a C++ enum class as an enum property and a TEnumAsByte as a byte property. */
	int64 ReadEnumValue(const FProperty* Column, const uint8* RowData)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Column))
		{
			const void* ValueAddress = EnumProperty->ContainerPtrToValuePtr<void>(RowData);
			return EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValueAddress);
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Column))
		{
			return ByteProperty->GetPropertyValue_InContainer(RowData);
		}
		return 0;
	}

	FTrainingOption ReadTrainingOption(const FProperty* Column, const uint8* RowData)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Column);
		if (not StructProperty || StructProperty->Struct != FTrainingOption::StaticStruct())
		{
			return FTrainingOption();
		}
		return *StructProperty->ContainerPtrToValuePtr<FTrainingOption>(RowData);
	}

	/** The picture column is either a brush by value or a brush asset; both give the same brush. */
	FSlateBrush ReadImageBrush(const FProperty* Column, const uint8* RowData)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Column))
		{
			if (StructProperty->Struct == FSlateBrush::StaticStruct())
			{
				return *StructProperty->ContainerPtrToValuePtr<FSlateBrush>(RowData);
			}
		}
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Column))
		{
			if (const USlateBrushAsset* BrushAsset = Cast<USlateBrushAsset>(
				ObjectProperty->GetObjectPropertyValue_InContainer(RowData)))
			{
				return BrushAsset->Brush;
			}
		}
		return FSlateBrush();
	}

	FDeckCardDefinition ReadRow(const ERTSCard Card, const FCardTableColumns& Columns, const uint8* RowData)
	{
		FDeckCardDefinition Definition;
		Definition.Card = Card;
		Definition.CardType = static_cast<ECardType>(ReadEnumValue(Columns.CardType, RowData));
		Definition.Rarity = static_cast<ERTSCardRarity>(ReadEnumValue(Columns.CardRarity, RowData));
		Definition.Technology = static_cast<ETechnology>(ReadEnumValue(Columns.Technology, RowData));
		Definition.Title = ReadText(Columns.Title, RowData);
		Definition.Description = ReadText(Columns.Description, RowData);
		Definition.ImageBrush = ReadImageBrush(Columns.ImageBrush, RowData);
		Definition.TrainingOption = ReadTrainingOption(Columns.TrainingOption, RowData);
		Definition.VariantUnit = UPlayerUnitVariantManager::GetSpawnerKey(Definition.TrainingOption);
		return Definition;
	}
}

int32 UDeckCardCatalog::LoadFromTable(const UDataTable* CardTable)
{
	M_Definitions.Reset();
	M_CardTable = CardTable;
	if (not IsValid(CardTable) || not CardTable->GetRowStruct())
	{
		RTSFunctionLibrary::ReportError(TEXT("UDeckCardCatalog::LoadFromTable: the card data table is missing or has "
			"no row struct, so the deck builder has no cards to show."));
		return 0;
	}
	const UEnum* CardEnum = StaticEnum<ERTSCard>();
	const DeckCardCatalogPrivate::FCardTableColumns Columns = DeckCardCatalogPrivate::FindColumns(
		CardTable->GetRowStruct());
	for (const TPair<FName, uint8*>& Row : CardTable->GetRowMap())
	{
		const int64 CardValue = CardEnum->GetValueByNameString(Row.Key.ToString());
		if (CardValue == INDEX_NONE || not Row.Value)
		{
			UE_LOG(LogRTSDeckCardCatalog, Warning, TEXT("Card table row %s is not an ERTSCard value; skipped"),
			       *Row.Key.ToString());
			continue;
		}
		const ERTSCard Card = static_cast<ERTSCard>(CardValue);
		const FDeckCardDefinition Definition = DeckCardCatalogPrivate::ReadRow(Card, Columns, Row.Value);
		UE_LOG(LogRTSDeckCardCatalog, Log, TEXT("DECKCATALOG card=%s type=%s rarity=%d unit=%s tech=%s picture=%s"),
		       *Row.Key.ToString(), *Global_GetCardTypeAsString(Definition.CardType),
		       static_cast<int32>(Definition.Rarity), *Definition.TrainingOption.GetTrainingName(),
		       *Global_GetTechAsString(Definition.Technology),
		       Definition.ImageBrush.GetResourceObject() ? TEXT("yes") : TEXT("no"));
		M_Definitions.Add(Card, Definition);
	}
	return M_Definitions.Num();
}

const FDeckCardDefinition* UDeckCardCatalog::FindDefinition(const ERTSCard Card) const
{
	return M_Definitions.Find(Card);
}

const FDeckCardDefinition* UDeckCardCatalog::FindDefinitionOfUnit(const FTrainingOption& VariantUnit) const
{
	if (VariantUnit.IsNone())
	{
		return nullptr;
	}
	for (const TPair<ERTSCard, FDeckCardDefinition>& Entry : M_Definitions)
	{
		EDeckSection Section = EDeckSection::Technologies;
		const bool bIsTrainingCard = DeckBuilder::GetSectionOfCardType(Entry.Value.CardType, Section)
			&& Section != EDeckSection::Technologies;
		if (bIsTrainingCard && Entry.Value.VariantUnit == VariantUnit)
		{
			return &Entry.Value;
		}
	}
	return nullptr;
}
