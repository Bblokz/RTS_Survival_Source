// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MegaphoneLineSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UMegaphoneLineSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Lines.IsEmpty())
	{
		Context.AddWarning(FText::FromString(TEXT("The line set has no lines.")));
	}
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (not IsValid(Lines[LineIndex].Sound))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Line %d has no sound."), LineIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
