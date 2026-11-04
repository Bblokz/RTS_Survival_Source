#pragma once

#include "CoreMinimal.h"

#include "BattlefieldCorpsesLevel.generated.h"

/**
 * @brief How many crew corpses stay on the battlefield at once; the oldest makes way for a new one.
 * The count is handed to URTSCorpseSubsystem through the console variable rts.Corpses.
 */
UENUM(BlueprintType)
enum class ERTSBattlefieldCorpsesLevel : uint8
{
	Off UMETA(DisplayName="Off"),
	Few UMETA(DisplayName="50"),
	Some UMETA(DisplayName="100"),
	Many UMETA(DisplayName="200"),
	Maximum UMETA(DisplayName="400")
};

inline int32 global_GetBattlefieldCorpsesCount(const ERTSBattlefieldCorpsesLevel Level)
{
	constexpr int32 FewCorpses = 50;
	constexpr int32 SomeCorpses = 100;
	constexpr int32 ManyCorpses = 200;
	constexpr int32 MaximumCorpses = 400;
	switch (Level)
	{
	case ERTSBattlefieldCorpsesLevel::Off:
		return 0;
	case ERTSBattlefieldCorpsesLevel::Few:
		return FewCorpses;
	case ERTSBattlefieldCorpsesLevel::Some:
		return SomeCorpses;
	case ERTSBattlefieldCorpsesLevel::Many:
		return ManyCorpses;
	case ERTSBattlefieldCorpsesLevel::Maximum:
		return MaximumCorpses;
	}
	return SomeCorpses;
}

inline FText global_GetBattlefieldCorpsesText(const ERTSBattlefieldCorpsesLevel Level)
{
	if (Level == ERTSBattlefieldCorpsesLevel::Off)
	{
		return FText::FromString(TEXT("Off"));
	}
	return FText::AsNumber(global_GetBattlefieldCorpsesCount(Level));
}
