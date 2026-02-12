// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrainingOptionLibrary.h"

#include "RTS_Survival/CardSystem/ERTSCard/CardUnitStructure.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

FString UTrainingOptionLibrary::GetTrainingOptionName(EAllUnitType UnitType, uint8 SubtypeValue)
{
	FString SubtypeName = GetEnumValueName(UnitType, SubtypeValue);

	// Construct the training option name
	FString TrainingOptionName = "Train_" + SubtypeName;

	return TrainingOptionName;
}

FString UTrainingOptionLibrary::GetTrainingOptionDisplayName(const EAllUnitType UnitType, const uint8 SubtypeValue)
{
	if (UnitType == EAllUnitType::UNType_Tank)
	{
		const ETankSubtype TankSubtype = static_cast<ETankSubtype>(SubtypeValue);
		if (TankSubtype == ETankSubtype::Tank_Sdkfz251_Mortar || TankSubtype == ETankSubtype::Tank_Sdkfz251_Transport)
		{
			return "sdkfz-251";
		}
	}

	FString SubtypeName = GetEnumValueName(UnitType, SubtypeValue);

	auto RemovePrefix = [](FString& InOutName, const FString& Prefix)
	{
		const int32 Index = InOutName.Find(Prefix);
		if (Index != INDEX_NONE)
		{
			InOutName.RemoveAt(Index, Prefix.Len());
		}
	};

	// --- Remove known boilerplate prefixes ---
	RemovePrefix(SubtypeName, TEXT("Ger_"));
	RemovePrefix(SubtypeName, TEXT("Ger"));
	RemovePrefix(SubtypeName, TEXT("Rus_"));
	RemovePrefix(SubtypeName, TEXT("Rus"));
	RemovePrefix(SubtypeName, TEXT("Tank_"));
	RemovePrefix(SubtypeName, TEXT("Squad_"));
	RemovePrefix(SubtypeName, TEXT("Nomadic_"));
	RemovePrefix(SubtypeName, TEXT("Aircraft_"));

	// --- Insert spaces between words and number groups ---
	FString Formatted;
	Formatted.Reserve(SubtypeName.Len() * 2); // minor perf optimization

	for (int32 i = 0; i < SubtypeName.Len(); ++i)
	{
		const TCHAR CurrentChar = SubtypeName[i];

		if (i > 0)
		{
			const TCHAR PrevChar = SubtypeName[i - 1];
			const bool bAddSpace =
				(FChar::IsUpper(CurrentChar) && !FChar::IsUpper(PrevChar)) ||    // Between lowercase→uppercase
				(FChar::IsDigit(CurrentChar) && !FChar::IsDigit(PrevChar)) ||    // Between letters→digits
				(!FChar::IsDigit(CurrentChar) && FChar::IsDigit(PrevChar));      // Between digits→letters

			if (bAddSpace)
			{
				Formatted.AppendChar(TEXT(' '));
			}
		}

		Formatted.AppendChar(CurrentChar);
	}

	return Formatted.TrimStartAndEnd();
}

FString UTrainingOptionLibrary::GetEnumValueName(const EAllUnitType UnitType, const uint8 SubtypeValue)
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_Tank:
		{
			const UEnum* SubtypeEnum = StaticEnum<ETankSubtype>();
			return SubtypeEnum->GetNameStringByValue(static_cast<int64>(SubtypeValue));
		}
	case EAllUnitType::UNType_Nomadic:
		{
			const UEnum* SubtypeEnum = StaticEnum<ENomadicSubtype>();
			return SubtypeEnum->GetNameStringByValue(static_cast<int64>(SubtypeValue));
		}
	case EAllUnitType::UNType_Squad:
		{
			const UEnum* SubtypeEnum = StaticEnum<ESquadSubtype>();
			return SubtypeEnum->GetNameStringByValue(static_cast<int64>(SubtypeValue));
		}
		case EAllUnitType::UNType_Aircraft:
		{
			const UEnum* SubtypeEnum = StaticEnum<EAircraftSubtype>();
			return SubtypeEnum->GetNameStringByValue(static_cast<int64>(SubtypeValue));
		}
	default:
		return TEXT("UnknownSubtype");
	}
}

FTrainingOption UTrainingOptionLibrary::GetTrainingOptionFromCardSelector(FCardUnitTypeSelector CardSelector)
{
	const bool bHasSquadSubtype = CardSelector.SquadSubtype != ESquadSubtype::Squad_None;
	const bool bHasNomadicSubtype = CardSelector.NomadicSubtype != ENomadicSubtype::Nomadic_None;
	const bool bHasTankSubtype = CardSelector.TankSubtype != ETankSubtype::Tank_None;
	const bool bHasAircraftSubtype = CardSelector.AircraftSubtype != EAircraftSubtype::Aircarft_None;

	int32 NumberOfSetSubtypes = 0;
	if (bHasSquadSubtype)
	{
		++NumberOfSetSubtypes;
	}
	if (bHasNomadicSubtype)
	{
		++NumberOfSetSubtypes;
	}
	if (bHasTankSubtype)
	{
		++NumberOfSetSubtypes;
	}
	if (bHasAircraftSubtype)
	{
		++NumberOfSetSubtypes;
	}

	if (NumberOfSetSubtypes == 0)
	{
		RTSFunctionLibrary::ReportError(
			"UTrainingOptionLibrary::GetTrainingOptionFromCardSelector - CardSelector has no unit subtype set.");
		return FTrainingOption();
	}

	if (NumberOfSetSubtypes > 1)
	{
		RTSFunctionLibrary::ReportError(
			"UTrainingOptionLibrary::GetTrainingOptionFromCardSelector - CardSelector has multiple unit subtypes set.");
		return FTrainingOption();
	}

	if (bHasSquadSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(CardSelector.SquadSubtype));
	}
	if (bHasNomadicSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(CardSelector.NomadicSubtype));
	}
	if (bHasTankSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(CardSelector.TankSubtype));
	}
	if (bHasAircraftSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Aircraft, static_cast<uint8>(CardSelector.AircraftSubtype));
	}

	// Should be unreachable.
	return FTrainingOption();
}
