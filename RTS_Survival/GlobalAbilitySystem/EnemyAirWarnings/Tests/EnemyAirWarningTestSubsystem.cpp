// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirWarningTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/GlobalAbilitySystem/EnemyAirWarnings/EnemyAirWarningSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyAirWarningTest, Log, All);

namespace EnemyAirWarningTestPrivate
{
	const TCHAR* const Switch = TEXT("EnemyAirWarningsValidate");
	constexpr double StartDelaySeconds = 6.0;
	constexpr double GapMarginSeconds = 1.0;
	// Long enough for the longest warning line (~8 s) to play out under the portrait.
	constexpr double PortraitPlaySeconds = 10.0;
	const TCHAR* const ExpectedSoundClass = TEXT("RTS_SClass_CinematicAndTransmissions");

	constexpr EEnemyAirWarning AllWarnings[] = {
		EEnemyAirWarning::ScoutAircraft,
		EEnemyAirWarning::AttackAircraft,
		EEnemyAirWarning::ParatrooperPlane,
		EEnemyAirWarning::Bombers
	};

	FString WarningName(const EEnemyAirWarning Warning)
	{
		return StaticEnum<EEnemyAirWarning>()->GetNameStringByValue(static_cast<int64>(Warning));
	}

	EGlobalAbility FirstAbilityOf(const EEnemyAirWarning Warning)
	{
		const TArray<EGlobalAbility>& Abilities = GetDefault<UEnemyAirWarningSettings>()->GetLines(Warning).Abilities;
		return Abilities.IsEmpty() ? EGlobalAbility::GA_None : Abilities[0];
	}

	FString ResultName(const EEnemyAirWarningResult Result)
	{
		switch (Result)
		{
		case EEnemyAirWarningResult::Played: return TEXT("Played");
		case EEnemyAirWarningResult::NotAnAirAbility: return TEXT("NotAnAirAbility");
		case EEnemyAirWarningResult::NoLines: return TEXT("NoLines");
		case EEnemyAirWarningResult::SameWarningCooldown: return TEXT("SameWarningCooldown");
		case EEnemyAirWarningResult::TooSoonAfterOtherWarning: return TEXT("TooSoonAfterOtherWarning");
		case EEnemyAirWarningResult::NoPortrait: return TEXT("NoPortrait");
		default: return TEXT("?");
		}
	}
}

bool UEnemyAirWarningTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), EnemyAirWarningTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UEnemyAirWarningTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyAirWarningTestSubsystem, STATGROUP_Tickables);
}

void UEnemyAirWarningTestSubsystem::Tick(const float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	using namespace EnemyAirWarningTestPrivate;
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}
	if (M_PhaseStartSeconds < 0.0)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		return;
	}
	const double GapSeconds = GetDefault<UEnemyAirWarningSettings>()->MinSecondsBetweenWarnings + GapMarginSeconds;
	switch (M_Phase)
	{
	case EPhase::WaitingForStart:
		if (GetPhaseSeconds() >= StartDelaySeconds)
		{
			StartTest();
		}
		break;
	case EPhase::WaitingForGap:
		if (GetPhaseSeconds() >= GapSeconds)
		{
			CheckWarningAfterGap();
		}
		break;
	case EPhase::WaitingForPortrait:
		if (GetPhaseSeconds() >= PortraitPlaySeconds)
		{
			Evaluate();
		}
		break;
	case EPhase::Finished:
		break;
	}
#endif
}

void UEnemyAirWarningTestSubsystem::SetPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetWorld()->GetTimeSeconds();
}

double UEnemyAirWarningTestSubsystem::GetPhaseSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds;
}

void UEnemyAirWarningTestSubsystem::StartTest()
{
	CheckSettings();
	CheckAbilityMapping();
	CheckFirstWarning();
	CheckCooldowns();
	SetPhase(EPhase::WaitingForGap);
}

void UEnemyAirWarningTestSubsystem::CheckSettings()
{
	using namespace EnemyAirWarningTestPrivate;
	const UEnemyAirWarningSettings* Settings = GetDefault<UEnemyAirWarningSettings>();
	int32 TotalLines = 0;
	for (const EEnemyAirWarning Warning : AllWarnings)
	{
		const FEnemyAirWarningLines& Lines = Settings->GetLines(Warning);
		int32 LoadedWaves = 0;
		int32 RightSoundClass = 0;
		for (const TSoftObjectPtr<USoundBase>& Line : Lines.VoiceLines)
		{
			const USoundWave* Wave = Cast<USoundWave>(Line.LoadSynchronous());
			LoadedWaves += IsValid(Wave) ? 1 : 0;
			const bool bRightClass = IsValid(Wave) and IsValid(Wave->SoundClassObject)
				and Wave->SoundClassObject->GetName() == ExpectedSoundClass;
			RightSoundClass += bRightClass ? 1 : 0;
		}
		TotalLines += Lines.VoiceLines.Num();
		Check(not Lines.Abilities.IsEmpty() and not Lines.VoiceLines.IsEmpty()
		      and LoadedWaves == Lines.VoiceLines.Num() and RightSoundClass == LoadedWaves
		      and Lines.PortraitType == ERTSPortraitTypes::GermanAnnouncer,
		      TEXT("settings_") + WarningName(Warning),
		      FString::Printf(TEXT("abilities=%d lines=%d waves=%d soundclass_ok=%d"), Lines.Abilities.Num(),
		                      Lines.VoiceLines.Num(), LoadedWaves, RightSoundClass));
	}
	constexpr int32 PickedLineCount = 15;
	Check(TotalLines == PickedLineCount, TEXT("settings_total_lines"), FString::Printf(TEXT("%d"), TotalLines));
}

void UEnemyAirWarningTestSubsystem::CheckAbilityMapping()
{
	const UEnemyAirWarningSettings* Settings = GetDefault<UEnemyAirWarningSettings>();
	const TPair<EGlobalAbility, EEnemyAirWarning> Expected[] = {
		{EGlobalAbility::GA_SovietReconFlight, EEnemyAirWarning::ScoutAircraft},
		{EGlobalAbility::GA_2xPE2Strafing, EEnemyAirWarning::AttackAircraft},
		{EGlobalAbility::GA_4xPE2Strafing, EEnemyAirWarning::AttackAircraft},
		{EGlobalAbility::GA_SovietParatroopers, EEnemyAirWarning::ParatrooperPlane},
		{EGlobalAbility::GA_HeavyCarpet_PE8_500Gr, EEnemyAirWarning::Bombers},
		{EGlobalAbility::GA_HeavyCarpet_PE8_1000Gr, EEnemyAirWarning::Bombers},
		{EGlobalAbility::GA_HeavyCarpet_PE8_3000Gr, EEnemyAirWarning::Bombers},
		{EGlobalAbility::GA_SovietSmallBarrage, EEnemyAirWarning::None},
		{EGlobalAbility::GA_SovietT34Drop, EEnemyAirWarning::None}
	};
	int32 Wrong = 0;
	FString Detail;
	for (const TPair<EGlobalAbility, EEnemyAirWarning>& Pair : Expected)
	{
		const EEnemyAirWarning Got = Settings->GetWarningForAbility(Pair.Key);
		Wrong += Got == Pair.Value ? 0 : 1;
		Detail += FString::Printf(TEXT("%s->%s "), *StaticEnum<EGlobalAbility>()->GetNameStringByValue(
			                          static_cast<int64>(Pair.Key)), *EnemyAirWarningTestPrivate::WarningName(Got));
	}
	Check(Wrong == 0, TEXT("ability_mapping"), Detail);
}

void UEnemyAirWarningTestSubsystem::CheckFirstWarning()
{
	using namespace EnemyAirWarningTestPrivate;
	UEnemyAirWarningSubsystem* AirWarnings = UEnemyAirWarningSubsystem::Get(this);
	if (not IsValid(AirWarnings))
	{
		Check(false, TEXT("subsystem_exists"), TEXT("no UEnemyAirWarningSubsystem"));
		return;
	}
	EEnemyAirWarning FiredWarning = EEnemyAirWarning::None;
	if (TryFireLoadedEnemyAirAbility(FiredWarning))
	{
		M_FirstWarning = FiredWarning;
		Check(GetIsLastPlayedLineOf(FiredWarning), TEXT("enemy_manager_fire_plays_warning"),
		      WarningName(FiredWarning) + TEXT(" line ") + GetNameSafe(AirWarnings->GetLastPlayedVoiceLine()));
		return;
	}
	// The map's enemy has no air ability loaded (or none is ready): drive the subsystem directly.
	M_FirstWarning = EEnemyAirWarning::ScoutAircraft;
	const EEnemyAirWarningResult Result = AirWarnings->OnEnemyAbilityExecuted(FirstAbilityOf(M_FirstWarning));
	Check(Result == EEnemyAirWarningResult::Played and GetIsLastPlayedLineOf(M_FirstWarning),
	      TEXT("direct_first_warning_plays"),
	      ResultName(Result) + TEXT(" line ") + GetNameSafe(AirWarnings->GetLastPlayedVoiceLine()));
}

bool UEnemyAirWarningTestSubsystem::TryFireLoadedEnemyAirAbility(EEnemyAirWarning& OutWarning)
{
	using namespace EnemyAirWarningTestPrivate;
	const AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	UGlobalAbilitiesManager* Manager = IsValid(EnemyController)
		                                   ? EnemyController->GetEnemyGlobalAbilitiesManager()
		                                   : nullptr;
	if (not IsValid(Manager))
	{
		M_Lines.Add(TEXT("NOTE no enemy abilities manager on this map: the manager hook is not exercised"));
		return false;
	}
	const UEnemyAirWarningSettings* Settings = GetDefault<UEnemyAirWarningSettings>();
	const FVector Target = EnemyController->GetActorLocation();
	for (const EEnemyAirWarning Warning : AllWarnings)
	{
		for (const EGlobalAbility AbilityType : Settings->GetLines(Warning).Abilities)
		{
			UGlobalAbility* Ability = Manager->FindLoadedAbilityByType(AbilityType);
			if (IsValid(Ability) and Manager->TryExecuteAbilityAtLocation(Ability, Target))
			{
				M_Lines.Add(TEXT("NOTE fired enemy ability ") + StaticEnum<EGlobalAbility>()->GetNameStringByValue(
					static_cast<int64>(AbilityType)));
				OutWarning = Warning;
				return true;
			}
		}
	}
	M_Lines.Add(TEXT("NOTE the enemy manager has no air ability loaded and ready: the manager hook is not exercised"));
	return false;
}

void UEnemyAirWarningTestSubsystem::CheckCooldowns()
{
	using namespace EnemyAirWarningTestPrivate;
	UEnemyAirWarningSubsystem* AirWarnings = UEnemyAirWarningSubsystem::Get(this);
	if (not IsValid(AirWarnings) or M_FirstWarning == EEnemyAirWarning::None)
	{
		return;
	}
	const EEnemyAirWarningResult Same = AirWarnings->OnEnemyAbilityExecuted(FirstAbilityOf(M_FirstWarning));
	Check(Same == EEnemyAirWarningResult::SameWarningCooldown, TEXT("same_warning_cooldown"), ResultName(Same));
	const EEnemyAirWarning Other = M_FirstWarning == EEnemyAirWarning::Bombers
		                               ? EEnemyAirWarning::ParatrooperPlane
		                               : EEnemyAirWarning::Bombers;
	const EEnemyAirWarningResult TooSoon = AirWarnings->OnEnemyAbilityExecuted(FirstAbilityOf(Other));
	Check(TooSoon == EEnemyAirWarningResult::TooSoonAfterOtherWarning, TEXT("between_warnings_gap"),
	      ResultName(TooSoon));
	const EEnemyAirWarningResult NotAir = AirWarnings->OnEnemyAbilityExecuted(EGlobalAbility::GA_SovietSmallBarrage);
	Check(NotAir == EEnemyAirWarningResult::NotAnAirAbility, TEXT("barrage_is_silent"), ResultName(NotAir));
}

void UEnemyAirWarningTestSubsystem::CheckWarningAfterGap()
{
	using namespace EnemyAirWarningTestPrivate;
	UEnemyAirWarningSubsystem* AirWarnings = UEnemyAirWarningSubsystem::Get(this);
	if (IsValid(AirWarnings))
	{
		const EEnemyAirWarning Other = M_FirstWarning == EEnemyAirWarning::Bombers
			                               ? EEnemyAirWarning::ParatrooperPlane
			                               : EEnemyAirWarning::Bombers;
		const EEnemyAirWarningResult Result = AirWarnings->OnEnemyAbilityExecuted(FirstAbilityOf(Other));
		Check(Result == EEnemyAirWarningResult::Played and GetIsLastPlayedLineOf(Other),
		      TEXT("other_warning_after_gap_plays"),
		      WarningName(Other) + TEXT(" ") + ResultName(Result) + TEXT(" line ")
		      + GetNameSafe(AirWarnings->GetLastPlayedVoiceLine()));
	}
	SetPhase(EPhase::WaitingForPortrait);
}

bool UEnemyAirWarningTestSubsystem::GetIsLastPlayedLineOf(const EEnemyAirWarning Warning) const
{
	const UEnemyAirWarningSubsystem* AirWarnings = UEnemyAirWarningSubsystem::Get(this);
	const USoundBase* Played = IsValid(AirWarnings) ? AirWarnings->GetLastPlayedVoiceLine() : nullptr;
	if (not IsValid(Played))
	{
		return false;
	}
	const FEnemyAirWarningLines& Lines = GetDefault<UEnemyAirWarningSettings>()->GetLines(Warning);
	return Lines.VoiceLines.ContainsByPredicate([Played](const TSoftObjectPtr<USoundBase>& Line)
	{
		return Line.Get() == Played;
	});
}

void UEnemyAirWarningTestSubsystem::Evaluate()
{
	if (M_Phase == EPhase::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSEnemyAirWarningTest, Display, TEXT("RTS_AIR_WARNING_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("EnemyAirWarningTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	SetPhase(EPhase::Finished);
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEnemyAirWarningTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSEnemyAirWarningTest, Display, TEXT("RTS_AIR_WARNING_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}
