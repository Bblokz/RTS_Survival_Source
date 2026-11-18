// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftGunAudioDataAsset.h"

#include "RTS_Survival/Audio/Settings/RTSAudioDeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const UAircraftGunAudioDataAsset* UAircraftGunAudioDataAsset::Get()
{
	// Weak: every UAircraftGunAudioPlayer holds the asset, so it stays loaded while aircraft fly; reloaded after that.
	static TWeakObjectPtr<UAircraftGunAudioDataAsset> Loaded;
	static bool bHasReportedMissingData = false;
	if (Loaded.IsValid())
	{
		return Loaded.Get();
	}
	const URTSAudioDeveloperSettings* const Settings = GetDefault<URTSAudioDeveloperSettings>();
	UAircraftGunAudioDataAsset* const Data = Settings != nullptr && not Settings->AircraftGunAudioData.IsNull()
		                                         ? Settings->AircraftGunAudioData.LoadSynchronous()
		                                         : nullptr;
	if (not IsValid(Data))
	{
		if (not bHasReportedMissingData)
		{
			bHasReportedMissingData = true;
			RTSFunctionLibrary::ReportError(TEXT("UAircraftGunAudioDataAsset: no loadable Aircraft Gun Audio Data in "
				"Project Settings > Game > RTS Audio Settings; aircraft guns keep their Blueprint launch and impact sounds."));
		}
		return nullptr;
	}
	Loaded = Data;
	return Data;
}

#if WITH_EDITOR
EDataValidationResult UAircraftGunAudioDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (not MachineGun.HasFireLoop() || not MachineGun.HasImpactLoop())
	{
		Context.AddError(FText::FromString(TEXT("MachineGun needs a fire loop and a soft impact loop.")));
		Result = EDataValidationResult::Invalid;
	}
	if (not Cannon.HasFireLoop())
	{
		Context.AddError(FText::FromString(TEXT("Cannon needs a fire loop.")));
		Result = EDataValidationResult::Invalid;
	}
	if (CannonCalibreRangeMm.X > CannonCalibreRangeMm.Y)
	{
		Context.AddError(FText::FromString(TEXT("CannonCalibreRangeMm: X must not exceed Y.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
