#pragma once

#include "CoreMinimal.h"

#include "HealthBarVisibilityStrategy.generated.h"

UENUM(BlueprintType)
enum class ERTSPlayerHealthBarVisibilityStrategy : uint8
{
	NotInitialized,
	UnitDefaults,
	AwaysVisible,
	VisibleWhenDamagedOnly,
	VisibleOnSelectionAndDamaged,
	VisibleOnSelectionOnly,
	NeverVisible
};

UENUM(BlueprintType)
enum class ERTSEnemyHealthBarVisibilityStrategy : uint8
{
	NotInitialized,
	UnitDefaults,
	AwaysVisible,
	VisibleWhenDamagedOnly
};

inline FText global_GetPlayerVisibilityStrategyText(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	switch (Strategy)
	{
	case ERTSPlayerHealthBarVisibilityStrategy::NotInitialized:
		return FText::FromString(TEXT("Not Initialized"));
	case ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults:
		return FText::FromString(TEXT("Unit Defaults"));
	case ERTSPlayerHealthBarVisibilityStrategy::AwaysVisible:
		return FText::FromString(TEXT("Aways Visible"));
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleWhenDamagedOnly:
		return FText::FromString(TEXT("Visible When Damaged Only"));
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionAndDamaged:
		return FText::FromString(TEXT("Visible On Selection And Damaged"));
	case ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionOnly:
		return FText::FromString(TEXT("Visible On Selection Only"));
	case ERTSPlayerHealthBarVisibilityStrategy::NeverVisible:
		return FText::FromString(TEXT("Never Visible"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}

inline FText global_GetEnemyVisibilityStrategyText(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	switch (Strategy)
	{
	case ERTSEnemyHealthBarVisibilityStrategy::NotInitialized:
		return FText::FromString(TEXT("Not Initialized"));
	case ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults:
		return FText::FromString(TEXT("Unit Defaults"));
	case ERTSEnemyHealthBarVisibilityStrategy::AwaysVisible:
		return FText::FromString(TEXT("Aways Visible"));
	case ERTSEnemyHealthBarVisibilityStrategy::VisibleWhenDamagedOnly:
		return FText::FromString(TEXT("Visible When Damaged Only"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}
