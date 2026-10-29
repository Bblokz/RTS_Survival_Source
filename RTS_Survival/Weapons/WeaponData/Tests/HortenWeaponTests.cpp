// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the Horten Ho 229 C++: the MK108_30MM_Horten weapon entry (enum key, names, stored values relative to the
// other aircraft guns) and the Aircraft_Horten229 unit data.
// Guarded by WITH_DEV_AUTOMATION_TESTS only (no WITH_EDITOR) so the Development game target compiles it too.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponNameConversion.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace HortenWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr float Tolerance = 0.01f;

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHortenMk108WeaponDataTest,
	"RTS.Weapons.Aircraft.HortenMk108WeaponData",
	HortenWeaponTestsPrivate::TestFlags)

bool FHortenMk108WeaponDataTest::RunTest(const FString& Parameters)
{
	using namespace HortenWeaponTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FWeaponData* Mk108 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::MK108_30MM_Horten);
	const FWeaponData* Mg151 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::MG_151);
	if (not TestNotNull(TEXT("MK 108 weapon data"), Mk108) || not TestNotNull(TEXT("MG 151 weapon data"), Mg151))
	{
		return false;
	}

	// Saved Blueprint values are names, but the enum is append-only: the key must not move.
	TestEqual(TEXT("MK 108 enum key"), static_cast<int32>(EWeaponName::MK108_30MM_Horten), 228);
	TestEqual(TEXT("Display name"), Global_GetWeaponDisplayName(EWeaponName::MK108_30MM_Horten),
	          FString(TEXT("MK 108")));
	TestEqual(TEXT("Enum string"), Global_GetWeaponEnumAsString(EWeaponName::MK108_30MM_Horten),
	          FString(TEXT("MK108_30MM_Horten")));
	TestEqual(TEXT("Blueprint FName resolves"),
	          static_cast<int32>(Global_GetWeaponNameFromFName(FName(TEXT("MK108_30MM_Horten")))),
	          static_cast<int32>(EWeaponName::MK108_30MM_Horten));

	TestEqual(TEXT("Calibre"), Mk108->WeaponCalibre, 30.f, Tolerance);
	TestEqual(TEXT("Aircraft gun range"), Mk108->Range,
	          static_cast<float>(DeveloperSettings::GameBalance::Ranges::AircraftSmallArmsRange), Tolerance);
	TestTrue(TEXT("Hits harder than the MG 151"), Mk108->BaseDamage > Mg151->BaseDamage);
	TestTrue(TEXT("Fires slower than the MG 151"), Mk108->BaseCooldown > Mg151->BaseCooldown);
	TestTrue(TEXT("Smaller magazine than the MG 151"), Mk108->MagCapacity < Mg151->MagCapacity);
	TestTrue(TEXT("Slower shell than the MG 151"), Mk108->ProjectileMovementSpeed < Mg151->ProjectileMovementSpeed);
	TestTrue(TEXT("AP loaded by default"), Mk108->ShellType == EWeaponShellType::Shell_AP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHortenAircraftDataTest,
	"RTS.Units.Aircraft.HortenAircraftData",
	HortenWeaponTestsPrivate::TestFlags)

bool FHortenAircraftDataTest::RunTest(const FString& Parameters)
{
	using namespace HortenWeaponTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FAircraftData HortenData = GameState->GetAircraftDataOfPlayer(EAircraftSubtype::Aircraft_Horten229, PlayerOne);
	TestTrue(TEXT("Has health"), HortenData.MaxHealth > 0.f);
	TestTrue(TEXT("Has abilities"), HortenData.Abilities.Num() > 0);
	return true;
}

#endif
