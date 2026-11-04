// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the Realistic / Arcade vehicle balance: Arcade must restore the vehicle health from before the 2026-11-03
// lethality pass and switching back must return the realistic data unchanged.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/UnitData/VehicleData.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace VehicleBalanceTestsPrivate
{
	constexpr float Tolerance = 0.01f;
	// Health of these tanks before the lethality pass, when VehicleHealthMlt did not exist.
	constexpr float OldT34_85Health = 900.f;
	constexpr float OldBrumbarHealth = 1120.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleBalanceArcadeHealthTest, "RTS.Balance.VehicleBalance.ArcadeHealth",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleBalanceArcadeHealthTest::RunTest(const FString& Parameters)
{
	using namespace VehicleBalanceTestsPrivate;
	using DeveloperSettings::GameBalance::UnitHealth::VehicleHealthMlt;
	ACPPGameState* const GameState = GetMutableDefault<ACPPGameState>();
	const float RealisticT34Health = GameState->GetTankData(ETankSubtype::Tank_T34_76).MaxHealth;
	const float RealisticTigerHealth = GameState->GetTankData(ETankSubtype::Tank_Tiger).MaxHealth;
	const float RealisticT34_85Health = GameState->GetTankData(ETankSubtype::Tank_T34_85).MaxHealth;
	const float RealisticBrumbarHealth = GameState->GetTankData(ETankSubtype::Tank_Brumbar).MaxHealth;
	const float TrainHealth = GameState->GetTankData(ETankSubtype::Tank_LightTrain).MaxHealth;

	GameState->SetVehicleBalance(ERTSVehicleBalance::Arcade);
	TestEqual(TEXT("Arcade T-34/76 undoes the vehicle health multiplier"),
	          GameState->GetTankData(ETankSubtype::Tank_T34_76).MaxHealth, RealisticT34Health / VehicleHealthMlt, Tolerance);
	TestEqual(TEXT("Arcade Tiger undoes the vehicle health multiplier"),
	          GameState->GetTankData(ETankSubtype::Tank_Tiger).MaxHealth, RealisticTigerHealth / VehicleHealthMlt, Tolerance);
	TestEqual(TEXT("Arcade T-34/85 drops the realistic breakpoint trim"),
	          GameState->GetTankData(ETankSubtype::Tank_T34_85).MaxHealth, OldT34_85Health, Tolerance);
	TestEqual(TEXT("Arcade Brumbar keeps its fixed +400 unscaled"),
	          GameState->GetTankData(ETankSubtype::Tank_Brumbar).MaxHealth, OldBrumbarHealth, Tolerance);
	TestEqual(TEXT("Trains ignore the vehicle balance"),
	          GameState->GetTankData(ETankSubtype::Tank_LightTrain).MaxHealth, TrainHealth, Tolerance);

	GameState->SetVehicleBalance(ERTSVehicleBalance::Realistic);
	TestEqual(TEXT("Back to realistic restores the T-34/76"),
	          GameState->GetTankData(ETankSubtype::Tank_T34_76).MaxHealth, RealisticT34Health, Tolerance);
	TestEqual(TEXT("Back to realistic restores the T-34/85"),
	          GameState->GetTankData(ETankSubtype::Tank_T34_85).MaxHealth, RealisticT34_85Health, Tolerance);
	TestEqual(TEXT("Back to realistic restores the Brumbar"),
	          GameState->GetTankData(ETankSubtype::Tank_Brumbar).MaxHealth, RealisticBrumbarHealth, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVehicleBalanceOvermatchTest, "RTS.Balance.VehicleBalance.Overmatch",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVehicleBalanceOvermatchTest::RunTest(const FString& Parameters)
{
	// Effective front armor after the slope, as on the real tanks: T-34 45 mm at 60 degrees, Pz IV H 80 mm, Tiger 100 mm.
	constexpr float T34FrontEffectiveArmor = 90.f;
	constexpr float PzIVFrontEffectiveArmor = 80.f;
	constexpr float TigerFrontEffectiveArmor = 100.f;
	constexpr int32 PlayerOne = 1;
	ACPPGameState* const GameState = GetMutableDefault<ACPPGameState>();
	auto IsOvermatch = [this, GameState](const EWeaponName Gun, const ETankSubtype Target, const float EffectiveArmor)
	{
		const FWeaponData* const GunData = GameState->GetWeaponDataOfPlayer(PlayerOne, Gun);
		if (GunData == nullptr)
		{
			AddError(TEXT("Missing weapon data for the overmatch table"));
			return false;
		}
		return AProjectile::GetIsArmorOvermatched(GunData->ArmorPen, EffectiveArmor, GunData->BaseDamage,
		                                          GameState->GetTankData(Target).MaxHealth);
	};

	TestTrue(TEXT("Tiger 88 overmatches a T-34/76 front"),
	         IsOvermatch(EWeaponName::KwK36_88MM, ETankSubtype::Tank_T34_76, T34FrontEffectiveArmor));
	TestTrue(TEXT("Tiger 88 overmatches a T-34/85 front"),
	         IsOvermatch(EWeaponName::KwK36_88MM, ETankSubtype::Tank_T34_85, T34FrontEffectiveArmor));
	TestTrue(TEXT("Panther L70 overmatches a T-34/76 front"),
	         IsOvermatch(EWeaponName::KwK42_75MM, ETankSubtype::Tank_T34_76, T34FrontEffectiveArmor));
	TestFalse(TEXT("Pz IV long 75 does not overmatch a T-34/76 front"),
	          IsOvermatch(EWeaponName::KwK40_L48_75MM, ETankSubtype::Tank_T34_76, T34FrontEffectiveArmor));
	TestFalse(TEXT("T-34/76 does not overmatch a Pz IV H front"),
	          IsOvermatch(EWeaponName::F_34_76MM, ETankSubtype::Tank_PzIV_H, PzIVFrontEffectiveArmor));
	TestTrue(TEXT("T-34/85 overmatches a Pz IV H front"),
	         IsOvermatch(EWeaponName::ZIS_S_53_85MM, ETankSubtype::Tank_PzIV_H, PzIVFrontEffectiveArmor));
	TestFalse(TEXT("Tiger 88 does not overmatch a Tiger front"),
	          IsOvermatch(EWeaponName::KwK36_88MM, ETankSubtype::Tank_Tiger, TigerFrontEffectiveArmor));
	TestTrue(TEXT("IS-2 122 overmatches a Tiger front"),
	         IsOvermatch(EWeaponName::D_25T_122MM, ETankSubtype::Tank_Tiger, TigerFrontEffectiveArmor));
	return true;
}

#endif
