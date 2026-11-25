// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSVehicleCrushSettings.h"

#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace RTSVehicleCrushDefaults
{
	// Written by Source/Tools/VehicleCrush/ue_import_crush_assets.py.
	const TCHAR* const ScreamFolder = TEXT("/Game/RTS_Survival/Sounds/VoiceLines/RunOverScreams/");
	constexpr int32 MaleScreamCount = 8;
	constexpr int32 FemaleScreamCount = 4;

	const TCHAR* const PlayerAttenuationPath =
		TEXT("/Game/RTS_Survival/Sounds/Settings/VoiceLInes/Att_VoiceLines_PlayerInfantry.Att_VoiceLines_PlayerInfantry");
	const TCHAR* const EnemyAttenuationPath =
		TEXT("/Game/RTS_Survival/Sounds/Settings/VoiceLInes/Att_VoiceLines_EnemyInfantry.Att_VoiceLines_EnemyInfantry");

	const TCHAR* const BurstPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/Realistic_Starter_VFX_Pack/Particles/Blood/P_Blood_Splat_3d_Converted.P_Blood_Splat_3d_Converted");
	const TCHAR* const GroundSprayPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/Realistic_Starter_VFX_Pack/Particles/Blood/P_Blood_Splat_2d_Converted.P_Blood_Splat_2d_Converted");
	const TCHAR* const DecalPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/VehicleCrush/M_CrushBloodDecal.M_CrushBloodDecal");

	FSoftObjectPath MakeScreamPath(const TCHAR* const Kind, const int32 Index)
	{
		const FString Name = FString::Printf(TEXT("RunOverScream_%s_%02d"), Kind, Index);
		return FSoftObjectPath(FString(ScreamFolder) + Name + TEXT(".") + Name);
	}
}

URTSVehicleCrushSettings::URTSVehicleCrushSettings()
{
	using namespace RTSVehicleCrushDefaults;
	CategoryName = TEXT("Game");

	NotCrushableSubtypes = {
		ESquadSubtype::Squad_Ger_RedSpine,
		ESquadSubtype::Squad_Ger_FelsKrieger,
		ESquadSubtype::Squad_Ger_KR_12_Dachs,
	};
	FemaleScreamSubtypes = {
		ESquadSubtype::Squad_Ger_Gebirgsjagerin,
		ESquadSubtype::Squad_Rus_GhostsOfStalingrad,
	};

	for (int32 Index = 1; Index <= MaleScreamCount; ++Index)
	{
		MaleScreams.Add(TSoftObjectPtr<USoundBase>(MakeScreamPath(TEXT("Male"), Index)));
	}
	for (int32 Index = 1; Index <= FemaleScreamCount; ++Index)
	{
		FemaleScreams.Add(TSoftObjectPtr<USoundBase>(MakeScreamPath(TEXT("Female"), Index)));
	}
	PlayerScreamAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(PlayerAttenuationPath));
	EnemyScreamAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(EnemyAttenuationPath));

	CrushBloodBurst = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(BurstPath));
	CrushBloodGroundSpray = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(GroundSprayPath));
	CrushBloodDecal = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DecalPath));
}

bool URTSVehicleCrushSettings::GetIsCrushableSubtype(const ESquadSubtype Subtype) const
{
	return not NotCrushableSubtypes.Contains(Subtype);
}

USoundBase* URTSVehicleCrushSettings::PickScream(const ESquadSubtype Subtype) const
{
	const bool bIsFemale = FemaleScreamSubtypes.Contains(Subtype);
	const TArray<TSoftObjectPtr<USoundBase>>& Pool = bIsFemale && not FemaleScreams.IsEmpty()
		                                                 ? FemaleScreams
		                                                 : MaleScreams;
	if (Pool.IsEmpty())
	{
		return nullptr;
	}
	// A missing take is skipped rather than leaving the soldier silent.
	const int32 StartIndex = FMath::RandRange(0, Pool.Num() - 1);
	for (int32 Offset = 0; Offset < Pool.Num(); ++Offset)
	{
		if (USoundBase* const Scream = Pool[(StartIndex + Offset) % Pool.Num()].LoadSynchronous())
		{
			return Scream;
		}
	}
	return nullptr;
}
