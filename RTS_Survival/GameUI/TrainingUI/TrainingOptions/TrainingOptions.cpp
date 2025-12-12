#include "TrainingOptions.h"

#include "TrainingOptionLibrary/TrainingOptionLibrary.h"

FString FTrainingOption::GetTrainingName() const
{
	return UTrainingOptionLibrary::GetTrainingOptionName(UnitType, SubtypeValue);
}

FString FTrainingOption::GetDisplayName() const
{
	return UTrainingOptionLibrary::GetTrainingOptionDisplayName(UnitType, SubtypeValue);
}

bool FTrainingOption::IsNone() const
{
	return UnitType == EAllUnitType::UNType_None || SubtypeValue == 0;
}
