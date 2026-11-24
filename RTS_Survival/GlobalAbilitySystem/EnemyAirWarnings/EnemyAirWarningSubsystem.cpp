// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirWarningSubsystem.h"

#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundBase.h"

UEnemyAirWarningSubsystem* UEnemyAirWarningSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UEnemyAirWarningSubsystem>() : nullptr;
}

EEnemyAirWarningResult UEnemyAirWarningSubsystem::OnEnemyAbilityExecuted(const EGlobalAbility Ability)
{
	const UEnemyAirWarningSettings* Settings = GetDefault<UEnemyAirWarningSettings>();
	const EEnemyAirWarning Warning = Settings->GetWarningForAbility(Ability);
	const UWorld* World = GetWorld();
	if (Warning == EEnemyAirWarning::None or not IsValid(World))
	{
		return EEnemyAirWarningResult::NotAnAirAbility;
	}
	const double NowSeconds = World->GetTimeSeconds();
	const EEnemyAirWarningResult Blocked = GetBlockingCooldown(Warning, NowSeconds);
	if (Blocked != EEnemyAirWarningResult::Played)
	{
		return Blocked;
	}
	return PlayWarningLine(Warning, NowSeconds);
}

EEnemyAirWarningResult UEnemyAirWarningSubsystem::GetBlockingCooldown(
	const EEnemyAirWarning Warning, const double NowSeconds) const
{
	const UEnemyAirWarningSettings* Settings = GetDefault<UEnemyAirWarningSettings>();
	if (const double* LastPlayed = M_LastPlayedSecondsPerWarning.Find(Warning))
	{
		if (NowSeconds - *LastPlayed < Settings->SameWarningCooldownSeconds)
		{
			return EEnemyAirWarningResult::SameWarningCooldown;
		}
	}
	const bool bAnyWarningPlayed = M_LastAnyWarningSeconds >= 0.0;
	if (bAnyWarningPlayed and NowSeconds - M_LastAnyWarningSeconds < Settings->MinSecondsBetweenWarnings)
	{
		return EEnemyAirWarningResult::TooSoonAfterOtherWarning;
	}
	return EEnemyAirWarningResult::Played;
}

EEnemyAirWarningResult UEnemyAirWarningSubsystem::PlayWarningLine(
	const EEnemyAirWarning Warning, const double NowSeconds)
{
	const FEnemyAirWarningLines& Lines = GetDefault<UEnemyAirWarningSettings>()->GetLines(Warning);
	const int32 LineIndex = M_WarningDecks.FindOrAdd(Warning).DrawNextLineIndex(Lines.VoiceLines.Num());
	if (not Lines.VoiceLines.IsValidIndex(LineIndex))
	{
		// No lines set (yet): this warning stays silent.
		return EEnemyAirWarningResult::NoLines;
	}
	USoundBase* VoiceLine = Lines.VoiceLines[LineIndex].LoadSynchronous();
	if (not IsValid(VoiceLine))
	{
		return EEnemyAirWarningResult::NoLines;
	}
	UPlayerPortraitManager* PortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	if (not IsValid(PortraitManager))
	{
		return EEnemyAirWarningResult::NoPortrait;
	}
	// Always through the portrait (with subtitles), never as a bare announcer line: these warnings are rare.
	PortraitManager->PlayPortrait(Lines.PortraitType, VoiceLine);
	M_LastPlayedSecondsPerWarning.Add(Warning, NowSeconds);
	M_LastAnyWarningSeconds = NowSeconds;
	M_LastPlayedVoiceLine = VoiceLine;
	return EEnemyAirWarningResult::Played;
}
