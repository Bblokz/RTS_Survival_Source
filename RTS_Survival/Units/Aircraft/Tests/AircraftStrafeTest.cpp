// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftStrafeValidate: the player's Ho 229 strafing global ability (UAircraftTestSubsystem).

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

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftStrafeTest, Log, All);

namespace AircraftStrafeTestPrivate
{
	const TCHAR* const StrafeAircraftClassMarker = TEXT("Ger_Aircraft_Horten229_Airborne");
	constexpr int32 ExpectedAircraftCount = 2;
	constexpr int32 StrafingPlayer = 1;
	// The aircraft come in from the edge of the map before the first pass.
	constexpr float StrafeTimeoutSeconds = 120.f;
}

void UAircraftTestSubsystem::StartAircraftStrafe()
{
	using namespace AircraftStrafeTestPrivate;
	EnterPhase(EAircraftTestPhase::AircraftStrafeRunning);
	// A map started directly has no commander in the game instance, so the player's manager loads no abilities:
	// check the commander's loadout from the settings and build the ability from its template as the manager does.
	const UCommanderSettings* CommanderSettings = UCommanderSettings::Get();
	const bool bInLoadout = IsValid(CommanderSettings)
		&& CommanderSettings->GetCommanderSettingsForType(ERTSCommander::BalancedCommander).GlobalAbilities.Contains(
			EGlobalAbility::GA_2xHortentrafing);
	Check(bInLoadout, TEXT("strafe_in_balanced_commander_loadout"));

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	UGlobalAbility* Ability = IsValid(AbilitySet)
		                          ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
			                          EGlobalAbility::GA_2xHortentrafing)
		                          : nullptr;
	Check(IsValid(Ability), TEXT("strafe_ability_template"), TEXT("GA_2xHortentrafing"));
	Check(IsValid(Cast<UGA_AircraftStrafing>(Ability)), TEXT("strafe_ability_is_strafing"),
	      IsValid(Ability) ? Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Ability) || not IsValid(Manager))
	{
		Check(IsValid(Manager), TEXT("strafe_player_ability_manager"));
		Finish();
		return;
	}
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
	const float Waited = GetNow() - M_PhaseStartSeconds;
	const bool bAllArrived = M_StrafeMostAircraftSeen >= ExpectedAircraftCount;
	if ((not bAnyFired || not bAllArrived) && Waited < StrafeTimeoutSeconds)
	{
		return;
	}
	Check(bAllArrived, TEXT("strafe_two_ho229_spawned"), FString::Printf(TEXT("seen=%d"), M_StrafeMostAircraftSeen));
	Check(bAnyFired, TEXT("strafe_ho229_fired"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Finish();
}

bool UAircraftTestSubsystem::TrackStrafeAircraft()
{
	using namespace AircraftStrafeTestPrivate;
	int32 AircraftCount = 0;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AAircraftMaster* Aircraft = *It;
		if (not IsValid(Aircraft) || not Aircraft->GetClass()->GetName().Contains(StrafeAircraftClassMarker))
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
