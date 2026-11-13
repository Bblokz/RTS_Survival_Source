// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BulletFlyByDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

namespace BulletFlyByDataValidation
{
	// Longer sounds stop reading as a passing bullet and pile up in a heavy fight.
	constexpr float LongFlyBySoundSeconds = 1.5f;
}

EDataValidationResult UBulletFlyByDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (FlyBySounds.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("FlyBySounds is empty; no bullet fly-by will play.")));
		Result = EDataValidationResult::Invalid;
	}
	for (int32 Index = 0; Index < FlyBySounds.Num(); ++Index)
	{
		const USoundBase* const Sound = FlyBySounds[Index];
		if (not IsValid(Sound))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("FlyBySounds[%d] is empty."), Index)));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (Sound->GetDuration() > BulletFlyByDataValidation::LongFlyBySoundSeconds)
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("%s lasts %.1f s; fly-bys longer than %.1f s smear together in a heavy fight."),
				*Sound->GetName(), Sound->GetDuration(), BulletFlyByDataValidation::LongFlyBySoundSeconds)));
		}
	}
	if (not IsValid(SoundClass))
	{
		Context.AddWarning(FText::FromString(
			TEXT("No SoundClass: the fly-bys ignore the SFX volume slider (the fly-by waves have no class).")));
	}
	if (FullVolumeCameraDistance > MaxCameraDistance)
	{
		Context.AddError(FText::FromString(TEXT("FullVolumeCameraDistance must not exceed MaxCameraDistance.")));
		Result = EDataValidationResult::Invalid;
	}
	if (PitchRange.X <= 0.f || PitchRange.Y < PitchRange.X)
	{
		Context.AddError(FText::FromString(TEXT("PitchRange needs 0 < X <= Y.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
