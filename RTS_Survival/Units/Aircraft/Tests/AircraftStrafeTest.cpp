// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftStrafeValidate: the player's Ho 229 strafing global ability (UAircraftTestSubsystem), and a 30 s
// measurement of every MG round (nose angle, ground hits, impact vs tracer timing; CSV in Saved/AircraftStrafeQA).
// Add -AircraftStrafeLegacyGuns to measure the old multi-trace guns for comparison.

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeShotRecorder.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/WeaponStateAircraftStrafeGun.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftStrafeTest, Log, All);

namespace AircraftStrafeTestPrivate
{
	const TCHAR* const DefaultStrafeAircraftClassMarker = TEXT("Ger_Aircraft_Horten229_Airborne");
	constexpr int32 ExpectedAircraftCount = 2;
	constexpr int32 StrafingPlayer = 1;
	// The aircraft come in from the edge of the map before the first pass.
	constexpr float StrafeTimeoutSeconds = 150.f;
	// Rounds are measured this long after the first one: the whole strafe of both aircraft, several runs.
	constexpr float StrafeQAWindowSeconds = 30.f;

	// Strafe gun acceptance (the legacy guns are only measured, for the comparison).
	constexpr int32 MinMeasuredShots = 100;
	constexpr float MinGroundHitShare = 0.9f;
	constexpr float MaxOffNoseShare = 0.02f;
	// Impacts may land at most one arrival-check interval plus a frame off the tracer's arrival.
	constexpr float MaxMeanImpactLeadSeconds = 0.05f;

	// -AircraftStrafeAbility=<EGlobalAbility name> runs another strafe template (the Ho 229 has no MGs in play; the
	// Bf 110's GA_2xBf110Strafing does); -AircraftStrafeClassMarker=<part of the aircraft class name> finds its aircraft.
	EGlobalAbility GetStrafeAbilityUnderTest()
	{
		FString AbilityName;
		if (not FParse::Value(FCommandLine::Get(), TEXT("AircraftStrafeAbility="), AbilityName))
		{
			return EGlobalAbility::GA_2xHortentrafing;
		}
		const int64 AbilityValue = StaticEnum<EGlobalAbility>()->GetValueByNameString(AbilityName);
		return AbilityValue == INDEX_NONE
			       ? EGlobalAbility::GA_2xHortentrafing
			       : static_cast<EGlobalAbility>(AbilityValue);
	}

	FString GetStrafeAircraftClassMarker()
	{
		FString ClassMarker;
		return FParse::Value(FCommandLine::Get(), TEXT("AircraftStrafeClassMarker="), ClassMarker)
			       ? ClassMarker
			       : FString(DefaultStrafeAircraftClassMarker);
	}
}

void UAircraftTestSubsystem::StartAircraftStrafe()
{
	using namespace AircraftStrafeTestPrivate;
	EnterPhase(EAircraftTestPhase::AircraftStrafeRunning);
	// A map started directly has no commander in the game instance, so the player's manager loads no abilities:
	// check the commander's loadout from the settings and build the ability from its template as the manager does.
	const EGlobalAbility StrafeAbility = GetStrafeAbilityUnderTest();
	if (StrafeAbility == EGlobalAbility::GA_2xHortentrafing)
	{
		const UCommanderSettings* CommanderSettings = UCommanderSettings::Get();
		const bool bInLoadout = IsValid(CommanderSettings)
			&& CommanderSettings->GetCommanderSettingsForType(ERTSCommander::LuftwaffeLiaison).GlobalAbilities.Contains(
				EGlobalAbility::GA_2xHortentrafing);
		Check(bInLoadout, TEXT("strafe_in_luftwaffe_liaison_loadout"));
	}

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	UGlobalAbility* Ability = IsValid(AbilitySet)
		                          ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
			                          StrafeAbility)
		                          : nullptr;
	Check(IsValid(Ability), TEXT("strafe_ability_template"),
	      StaticEnum<EGlobalAbility>()->GetNameStringByValue(static_cast<int64>(StrafeAbility)));
	Check(IsValid(Cast<UGA_AircraftStrafing>(Ability)), TEXT("strafe_ability_is_strafing"),
	      IsValid(Ability) ? Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Ability) || not IsValid(Manager))
	{
		Check(IsValid(Manager), TEXT("strafe_player_ability_manager"));
		Finish();
		return;
	}
	FAircraftStrafeShotRecorder::Enable(UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested()
		                                    ? TEXT("strafe_legacy_guns")
		                                    : TEXT("strafe_gun"));
	Ability->InitGlobalAbility(StrafingPlayer, Manager, Controller);
	// Straight to the ability: its tech requirement and cooldown are the menu's business, not this test's.
	Ability->ExecuteAbilityAtLocation(M_Anchor);
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_EXECUTED at=%s"),
	       *M_Anchor.ToCompactString());
}

void UAircraftTestSubsystem::TickAircraftStrafe()
{
	using namespace AircraftStrafeTestPrivate;
	const bool bAnyFired = TrackStrafeAircraft();
	const float Now = GetNow();
	const float Waited = Now - M_PhaseStartSeconds;
	const bool bAllArrived = M_StrafeMostAircraftSeen >= ExpectedAircraftCount;
	if (M_StrafeFirstShotSeconds < 0.f && FAircraftStrafeShotRecorder::GetShotCount() > 0)
	{
		M_StrafeFirstShotSeconds = Now;
	}
	const bool bQAWindowDone = M_StrafeFirstShotSeconds >= 0.f
		&& Now - M_StrafeFirstShotSeconds >= StrafeQAWindowSeconds;
	if ((not bQAWindowDone || not bAllArrived) && Waited < StrafeTimeoutSeconds)
	{
		return;
	}
	Check(bAllArrived, TEXT("strafe_two_aircraft_spawned"), FString::Printf(TEXT("seen=%d"), M_StrafeMostAircraftSeen));
	Check(bAnyFired, TEXT("strafe_aircraft_fired"), FString::Printf(TEXT("after=%.1fs"), Waited));
	FinishAircraftStrafeQA();
	Finish();
}

void UAircraftTestSubsystem::FinishAircraftStrafeQA()
{
	using namespace AircraftStrafeTestPrivate;
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_QA %s"),
	       *FAircraftStrafeShotRecorder::DescribeMetrics());
	FAircraftStrafeShotRecorder::WriteCsv();
	FAircraftStrafeShotRecorder::Disable();
	if (UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested())
	{
		// Legacy guns are the "before" of the comparison: measured, not judged.
		return;
	}
	Check(FAircraftStrafeShotRecorder::GetShotCount() >= MinMeasuredShots, TEXT("strafe_gun_enough_rounds"),
	      FString::Printf(TEXT("shots=%d"), FAircraftStrafeShotRecorder::GetShotCount()));
	Check(FAircraftStrafeShotRecorder::GetGroundHitShare() >= MinGroundHitShare, TEXT("strafe_gun_rounds_hit_ground"),
	      FString::Printf(TEXT("share=%.2f"), FAircraftStrafeShotRecorder::GetGroundHitShare()));
	Check(FAircraftStrafeShotRecorder::GetShareOfShotsOutsideNoseCone() <= MaxOffNoseShare,
	      TEXT("strafe_gun_fires_along_nose"),
	      FString::Printf(TEXT("off_nose_share=%.3f"), FAircraftStrafeShotRecorder::GetShareOfShotsOutsideNoseCone()));
	Check(FAircraftStrafeShotRecorder::GetMeanAbsImpactLeadSeconds() <= MaxMeanImpactLeadSeconds,
	      TEXT("strafe_gun_impacts_with_tracers"),
	      FString::Printf(TEXT("lead_abs_mean=%.3fs"), FAircraftStrafeShotRecorder::GetMeanAbsImpactLeadSeconds()));
}

bool UAircraftTestSubsystem::TrackStrafeAircraft()
{
	using namespace AircraftStrafeTestPrivate;
	int32 AircraftCount = 0;
	const FString ClassMarker = GetStrafeAircraftClassMarker();
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AAircraftMaster* Aircraft = *It;
		if (not IsValid(Aircraft) || not Aircraft->GetClass()->GetName().Contains(ClassMarker))
		{
			continue;
		}
		++AircraftCount;
		const TArray<UWeaponState*> Weapons = Aircraft->GetAllAircraftWeapons();
		if (Weapons.Num() == 0)
		{
			continue;
		}
		const int32 Magazine = Weapons[0]->GetCurrentMagCapacity();
		const int32 FirstMagazine = M_StrafeAircraftFirstMagazines.FindOrAdd(Aircraft, Magazine);
		// Latched: a reload refills the magazine.
		bM_StrafeAnyFired |= Magazine < FirstMagazine;
	}
	M_StrafeMostAircraftSeen = FMath::Max(M_StrafeMostAircraftSeen, AircraftCount);
	return bM_StrafeAnyFired;
}

#endif
