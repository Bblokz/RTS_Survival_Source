// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ConvoyTypes.h"

DEFINE_LOG_CATEGORY(LogConvoy);

namespace
{
	template <typename TEnum>
	FString ConvoyNames_GetShortEnumName(const TEnum Value)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		if (not IsValid(Enum))
		{
			return TEXT("Unknown");
		}
		return Enum->GetNameStringByValue(static_cast<int64>(Value));
	}
}

namespace ConvoyNames
{
	FString GetStateName(const EConvoyState State)
	{
		return ConvoyNames_GetShortEnumName(State);
	}

	FString GetEventName(const EConvoyEventType Type)
	{
		return ConvoyNames_GetShortEnumName(Type);
	}

	FString GetRoleName(const EConvoyUnitRole Role)
	{
		return ConvoyNames_GetShortEnumName(Role);
	}

	FString GetStationName(const EConvoyStation Station)
	{
		return ConvoyNames_GetShortEnumName(Station);
	}

	FString GetMemberStateName(const EConvoyMemberState State)
	{
		return ConvoyNames_GetShortEnumName(State);
	}

	FString GetOutcomeName(const EConvoyOutcome Outcome)
	{
		return ConvoyNames_GetShortEnumName(Outcome);
	}

	FString GetCargoName(const EConvoyCargoType CargoType)
	{
		return ConvoyNames_GetShortEnumName(CargoType);
	}
}
