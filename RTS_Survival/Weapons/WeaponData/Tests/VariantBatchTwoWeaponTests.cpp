// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the unit variant batch 2 C++: the new weapon entries, the Tiger II rocket data, the new behaviour ability and
// icon enum values, the splitter shell-multiplier fix, the E25's own tank data and the cargo garrison behaviour hook.
// Editor-only and dev-test-only: UBT defines WITH_DEV_AUTOMATION_TESTS=0 for Shipping and Test and WITH_EDITOR=0 for
// every game target, so this translation unit is empty in the shipping binary.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyAbilityUsageProfiles.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/UnitData/VehicleData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace VariantBatchTwoWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float Tolerance = 0.01f;
	const TCHAR* const MissingDisplayName = TEXT("NoWpTranslation");

	const TArray<EWeaponName>& GetNewWeapons()
	{
		static const TArray<EWeaponName> NewWeapons = {
			EWeaponName::StuH_200MM_DirectSiege,
			EWeaponName::Flak43_37MM_Ostwind,
			EWeaponName::Rocket_320mm_Incendiary,
			EWeaponName::RadixiteHail_Rocket_76MM,
			EWeaponName::E25_Hunter_60MM_APCR,
			EWeaponName::E25_Assault_90MM_HE,
			EWeaponName::KwK40_L48_75MM_HEAT_TigerTwin,
			EWeaponName::TigerRail_100MM_Railgun,
			EWeaponName::Ger_2xTwin35MM_TigerII,
			EWeaponName::Maus_AA_88MM_Flak
		};
		return NewWeapons;
	}

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}

	/** Split settings with a fixed speed roll so the projectile speed is deterministic. */
	FSplitterArcWeaponSplitSettings MakeSplitSettings(const float DamageMlt, const float PenMlt, const float AoeMlt)
	{
		FSplitterArcWeaponSplitSettings Settings;
		Settings.SplitDamageMultiplier = DamageMlt;
		Settings.SplitArmorPenMultiplier = PenMlt;
		Settings.SplitAoeMultiplier = AoeMlt;
		Settings.SplitCalibreMultiplier = 1.f;
		Settings.SplitProjectileSpeedMultiplierMin = 1.f;
		Settings.SplitProjectileSpeedMultiplierMax = 1.f;
		return Settings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoWeaponRegistrationTest,
	"RTS.Weapons.VariantBatch.B2.Registration",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoWeaponRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchTwoWeaponTestsPrivate;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}

	for (const EWeaponName WeaponName : GetNewWeapons())
	{
		const FString EnumString = Global_GetWeaponEnumAsString(WeaponName);
		const FWeaponData* PlayerData = GameState->GetWeaponDataOfPlayer(PlayerOne, WeaponName);
		const FWeaponData* EnemyData = GameState->GetWeaponDataOfPlayer(EnemyPlayer, WeaponName);
		if (not TestNotNull(*FString::Printf(TEXT("%s player data"), *EnumString), PlayerData)
			|| not TestNotNull(*FString::Printf(TEXT("%s enemy data"), *EnumString), EnemyData))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s data names its own weapon"), *EnumString), PlayerData->WeaponName,
		          WeaponName);
		TestNotEqual(*FString::Printf(TEXT("%s has a display name"), *EnumString),
		             Global_GetWeaponDisplayName(WeaponName), FString(MissingDisplayName));
		TestTrue(*FString::Printf(TEXT("%s default shell is in its shell list"), *EnumString),
		         PlayerData->ShellTypes.Contains(PlayerData->ShellType));
		TestTrue(*FString::Printf(TEXT("%s has a range"), *EnumString), PlayerData->Range > 0.f);
		TestTrue(*FString::Printf(TEXT("%s has damage"), *EnumString), PlayerData->BaseDamage > 0.f);
		TestTrue(*FString::Printf(TEXT("%s has a magazine"), *EnumString), PlayerData->MagCapacity > 0);
		TestTrue(*FString::Printf(TEXT("%s has a reload"), *EnumString), PlayerData->ReloadSpeed > 0.f);
		TestEqual(*FString::Printf(TEXT("%s enemy copy reloads the same"), *EnumString), EnemyData->ReloadSpeed,
		          PlayerData->ReloadSpeed, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoWeaponKeyStatsTest,
	"RTS.Weapons.VariantBatch.B2.KeyStats",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoWeaponKeyStatsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchTwoWeaponTestsPrivate;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	auto Get = [GameState](const EWeaponName WeaponName) -> const FWeaponData*
	{
		return GameState->GetWeaponDataOfPlayer(PlayerOne, WeaponName);
	};
	const FWeaponData* SiegeGun = Get(EWeaponName::StuH_200MM_DirectSiege);
	const FWeaponData* StuH43 = Get(EWeaponName::Stu_H_43_L_12_150MM);
	const FWeaponData* Ostwind = Get(EWeaponName::Flak43_37MM_Ostwind);
	const FWeaponData* Incendiary = Get(EWeaponName::Rocket_320mm_Incendiary);
	const FWeaponData* Hail = Get(EWeaponName::RadixiteHail_Rocket_76MM);
	const FWeaponData* Rocket50 = Get(EWeaponName::Rocket_50mm);
	const FWeaponData* Hunter = Get(EWeaponName::E25_Hunter_60MM_APCR);
	const FWeaponData* Assault = Get(EWeaponName::E25_Assault_90MM_HE);
	const FWeaponData* TwinGun = Get(EWeaponName::KwK40_L48_75MM_HEAT_TigerTwin);
	const FWeaponData* KwK40 = Get(EWeaponName::KwK40_L48_75MM);
	const FWeaponData* RailBunker = Get(EWeaponName::TigerRail_100MM_Railgun);
	const FWeaponData* RailGun80 = Get(EWeaponName::RailGun80MM);
	const FWeaponData* QuadThirtyFive = Get(EWeaponName::Ger_2xTwin35MM_TigerII);
	const FWeaponData* MausAa = Get(EWeaponName::Maus_AA_88MM_Flak);
	if (not TestNotNull(TEXT("Siege gun"), SiegeGun) || not TestNotNull(TEXT("StuH 43"), StuH43)
		|| not TestNotNull(TEXT("Ostwind"), Ostwind) || not TestNotNull(TEXT("Incendiary"), Incendiary)
		|| not TestNotNull(TEXT("Radixite hail"), Hail) || not TestNotNull(TEXT("Rocket 50mm"), Rocket50)
		|| not TestNotNull(TEXT("E25 Hunter"), Hunter) || not TestNotNull(TEXT("E25 Assault"), Assault)
		|| not TestNotNull(TEXT("Tiger twin gun"), TwinGun) || not TestNotNull(TEXT("KwK 40"), KwK40)
		|| not TestNotNull(TEXT("Rail bunker"), RailBunker) || not TestNotNull(TEXT("Railgun 80mm"), RailGun80)
		|| not TestNotNull(TEXT("2x twin 35mm"), QuadThirtyFive) || not TestNotNull(TEXT("Maus AA"), MausAa))
	{
		return false;
	}

	// Brummbar Direct Siege.
	TestEqual(TEXT("Siege gun calibre"), SiegeGun->WeaponCalibre, 200.f, Tolerance);
	TestTrue(TEXT("Siege gun reloads slower than the StuH 43"), SiegeGun->ReloadSpeed > StuH43->ReloadSpeed);
	TestEqual(TEXT("Siege gun fired HE pen"), SiegeGun->ArmorPen * HE_ArmorPenMlt, 80.f, Tolerance);
	TestEqual(TEXT("Siege gun default shell is HE"), SiegeGun->ShellType, EWeaponShellType::Shell_HE);

	// Ostwind: the clip drill is authored for these values.
	TestEqual(TEXT("Ostwind clip"), Ostwind->MagCapacity, 8);
	TestEqual(TEXT("Ostwind reload"), Ostwind->ReloadSpeed, 5.f, Tolerance);
	TestEqual(TEXT("Ostwind cooldown"), Ostwind->BaseCooldown, 0.12f, Tolerance);
	TestEqual(TEXT("Ostwind calibre"), Ostwind->WeaponCalibre, 37.f, Tolerance);

	// Incendiary Salvo.
	TestEqual(TEXT("Incendiary salvo size"), Incendiary->MagCapacity, 3);
	TestEqual(TEXT("Incendiary interval"), Incendiary->BaseCooldown, 0.8f, Tolerance);
	TestEqual(TEXT("Incendiary reload"), Incendiary->ReloadSpeed, 45.f, Tolerance);
	TestEqual(TEXT("Incendiary deals fire damage"), Incendiary->DamageType, ERTSDamageType::Fire);
	TestEqual(TEXT("Incendiary fires fire shells"), Incendiary->ShellType, EWeaponShellType::Shell_Fire);

	// Radixite Hail: each rocket (HE applied when fired) hits softer than one Rocket_50mm (HEAT keeps its damage).
	TestTrue(TEXT("Hail rocket fired damage below Rocket_50mm"),
	         Hail->BaseDamage * HE_DamageMlt < Rocket50->BaseDamage);
	TestEqual(TEXT("Hail fires HE"), Hail->ShellType, EWeaponShellType::Shell_HE);

	// E25 Hunter / Assault.
	const TArray<EWeaponShellType> ApcrOnly = {EWeaponShellType::Shell_APCR};
	const TArray<EWeaponShellType> HeOnly = {EWeaponShellType::Shell_HE};
	TestEqual(TEXT("Hunter fires APCR only"), Hunter->ShellTypes, ApcrOnly);
	TestTrue(TEXT("Hunter calibre passes the ambush behaviour filter (>= 40)"), Hunter->WeaponCalibre >= 40.f);
	TestEqual(TEXT("Hunter damage has the x2.6 bonus"), Hunter->BaseDamage, 187.2f, Tolerance);
	TestEqual(TEXT("Assault fires HE only"), Assault->ShellTypes, HeOnly);
	TestEqual(TEXT("Assault fired HE pen"), Assault->ArmorPen * HE_ArmorPenMlt, 25.f, Tolerance);

	// Tiger H1 Twin Gun.
	const TArray<EWeaponShellType> HeatThenHe = {EWeaponShellType::Shell_HEAT, EWeaponShellType::Shell_HE};
	TestEqual(TEXT("Twin gun loads HEAT by default"), TwinGun->ShellType, EWeaponShellType::Shell_HEAT);
	TestEqual(TEXT("Twin gun can switch to HE"), TwinGun->ShellTypes, HeatThenHe);
	TestEqual(TEXT("Twin gun reload per gun"), TwinGun->ReloadSpeed, 11.f, Tolerance);
	TestEqual(TEXT("Twin gun pen as the KwK 40"), TwinGun->ArmorPen, KwK40->ArmorPen, Tolerance);

	// Tiger Rail Bunker.
	TestEqual(TEXT("Rail bunker fires railgun shells"), RailBunker->ShellType, EWeaponShellType::Shell_Railgun);
	TestEqual(TEXT("Rail bunker calibre"), RailBunker->WeaponCalibre, 100.f, Tolerance);
	TestTrue(TEXT("Rail bunker out-pens the 80mm"), RailBunker->ArmorPen > RailGun80->ArmorPen);
	TestTrue(TEXT("Rail bunker hits harder than the 80mm"), RailBunker->BaseDamage > RailGun80->BaseDamage);
	TestTrue(TEXT("Rail bunker outranges the 80mm"), RailBunker->Range > RailGun80->Range);
	TestEqual(TEXT("Rail bunker reload"), RailBunker->ReloadSpeed, 14.f, Tolerance);

	// Tiger II twin 35mm (per projectile) and the Maus AA.
	TestEqual(TEXT("2x twin 35mm calibre"), QuadThirtyFive->WeaponCalibre, 35.f, Tolerance);
	TestEqual(TEXT("2x twin 35mm magazine"), QuadThirtyFive->MagCapacity, 8);
	TestTrue(TEXT("2x twin 35mm cooldown covers the recoil take"), QuadThirtyFive->BaseCooldown >= 0.4f - Tolerance);
	TestEqual(TEXT("Maus AA magazine"), MausAa->MagCapacity, 3);
	TestEqual(TEXT("Maus AA calibre"), MausAa->WeaponCalibre, 88.f, Tolerance);
	TestTrue(TEXT("Maus AA can fire HE"), MausAa->ShellTypes.Contains(EWeaponShellType::Shell_HE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoTigerIIRocketsTest,
	"RTS.Weapons.VariantBatch.B2.TigerIIRockets",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoTigerIIRocketsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchTwoWeaponTestsPrivate;
	constexpr float SalvoReloadSeconds = 75.f;
	constexpr float RocketIntervalSeconds = 0.45f;

	const ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FAttachedRocketsData PlayerRockets = GameState->GetAttachedRocketDataOfPlayer(
		PlayerOne, ERocketAbility::TigerIIRockets);
	const FAttachedRocketsData EnemyRockets = GameState->GetAttachedRocketDataOfPlayer(
		EnemyPlayer, ERocketAbility::TigerIIRockets);
	TestEqual(TEXT("Reload between salvos"), PlayerRockets.ReloadSpeed, SalvoReloadSeconds, Tolerance);
	TestEqual(TEXT("Interval between rockets"), PlayerRockets.Cooldown, RocketIntervalSeconds, Tolerance);
	TestEqual(TEXT("Enemy copy reloads the same"), EnemyRockets.ReloadSpeed, SalvoReloadSeconds, Tolerance);
	TestTrue(TEXT("Rockets do damage"), PlayerRockets.Damage > 0.f);

	const FUnitAbilityEntry RocketAbility =
		FAbilityHelpers::GetRocketAbilityForRocketType(ERocketAbility::TigerIIRockets);
	const FUnitAbilityEntry HetzerAbility =
		FAbilityHelpers::GetRocketAbilityForRocketType(ERocketAbility::HetzerRockets);
	TestEqual(TEXT("Fire rockets ability"), RocketAbility.AbilityId, EAbilityID::IdFireRockets);
	TestEqual(TEXT("Uses the large (Hetzer) rocket card"), RocketAbility.CustomType, HetzerAbility.CustomType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoSplitterTest,
	"RTS.Weapons.VariantBatch.B2.SplitterShellMultipliers",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoSplitterTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchTwoWeaponTestsPrivate;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ArmorPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_DamageMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ShrapnelRangeMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::HE_ProjectileSpeedMlt;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FWeaponData* Sig33 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::sIG_33_150MM);
	const FWeaponData* Rw61 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::RW61_Mortar_380MM);
	const FWeaponData* KwK40 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::KwK40_L48_75MM);
	const FWeaponData* Incendiary = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Rocket_320mm_Incendiary);
	if (not TestNotNull(TEXT("sIG 33"), Sig33) || not TestNotNull(TEXT("RW61"), Rw61)
		|| not TestNotNull(TEXT("KwK 40"), KwK40) || not TestNotNull(TEXT("Incendiary"), Incendiary))
	{
		return false;
	}

	// Bison IF settings (3 splits, damage 0.33, pen 1, aoe 1): each split now fires the shell's 61mm, not 610mm.
	const FWeaponData BisonSplit = UWeaponStateSplitterArchProjectile::BuildSplitProjectileDataFromSettings(
		*Sig33, MakeSplitSettings(0.33f, 1.f, 1.f));
	TestEqual(TEXT("Bison IF split pen = the shell's fired HE pen (61)"), BisonSplit.ArmorPen, 61.f, Tolerance);
	TestEqual(TEXT("Bison IF split max range pen = pen under HE"), BisonSplit.ArmorPenMaxRange, 61.f, Tolerance);
	TestEqual(TEXT("Bison IF split damage gets the HE damage multiplier"), BisonSplit.BaseDamage,
	          Sig33->BaseDamage * HE_DamageMlt * 0.33f, Tolerance);
	TestEqual(TEXT("Bison IF split speed gets the HE speed multiplier"), BisonSplit.ProjectileMovementSpeed,
	          Sig33->ProjectileMovementSpeed * HE_ProjectileSpeedMlt, 1.f);

	// Sturmtiger Scatterer (user decision): half the shell's pen per split, 1/6 of the blast area (radius x 0.408).
	constexpr float ScattererPenMlt = 0.5f;
	constexpr float ScattererAoeMlt = 0.408f;
	const FWeaponData ScatterSplit = UWeaponStateSplitterArchProjectile::BuildSplitProjectileDataFromSettings(
		*Rw61, MakeSplitSettings(1.f / 6.f, ScattererPenMlt, ScattererAoeMlt));
	TestEqual(TEXT("Scatterer split pen = half of the 320mm shell"), ScatterSplit.ArmorPen, 160.f, Tolerance);
	TestEqual(TEXT("Scatterer split shrapnel radius = shell radius x 0.408"), ScatterSplit.ShrapnelRange,
	          Rw61->ShrapnelRange * HE_ShrapnelRangeMlt * ScattererAoeMlt, Tolerance);

	// AP-family shells are not shell-adjusted (same rule as the main shell).
	FWeaponData ApSource = *KwK40;
	ApSource.ShellType = EWeaponShellType::Shell_APHE;
	const FWeaponData ApSplit = UWeaponStateSplitterArchProjectile::BuildSplitProjectileDataFromSettings(
		ApSource, MakeSplitSettings(1.f, 1.f, 1.f));
	TestEqual(TEXT("APHE split keeps the stored pen"), ApSplit.ArmorPen, KwK40->ArmorPen, Tolerance);
	TestEqual(TEXT("APHE split keeps the stored damage"), ApSplit.BaseDamage, KwK40->BaseDamage, Tolerance);

	// Fire shells are not shell-adjusted either (GLOBAL_GetWeaponDataForShellType has no fire case).
	const FWeaponData FireSplit = UWeaponStateSplitterArchProjectile::BuildSplitProjectileDataFromSettings(
		*Incendiary, MakeSplitSettings(1.f, 1.f, 1.f));
	TestEqual(TEXT("Fire split keeps the stored damage"), FireSplit.BaseDamage, Incendiary->BaseDamage, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoE25TankDataTest,
	"RTS.Weapons.VariantBatch.B2.E25TankData",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoE25TankDataTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchTwoWeaponTestsPrivate;

	const ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FTankData E25 = GameState->GetTankDataOfPlayer(PlayerOne, ETankSubtype::Tank_E25);
	const FTankData JagdPanzerIV70 = GameState->GetTankDataOfPlayer(PlayerOne, ETankSubtype::Tank_PzIV_70);
	TestTrue(TEXT("E25 has health"), E25.MaxHealth > 0.f);
	TestTrue(TEXT("E25 has less health than the JP IV/70"), E25.MaxHealth < JagdPanzerIV70.MaxHealth);
	TestTrue(TEXT("E25 is faster than the JP IV/70"), E25.VehicleMaxSpeedKmh > JagdPanzerIV70.VehicleMaxSpeedKmh);
	TestTrue(TEXT("E25 turns faster than the JP IV/70"), E25.VehicleRotationSpeed > JagdPanzerIV70.VehicleRotationSpeed);
	TestEqual(TEXT("E25 keeps the tank destroyer ability bar"), E25.Abilities.Num(), JagdPanzerIV70.Abilities.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoGarrisonHookTest,
	"RTS.Weapons.VariantBatch.B2.CargoGarrisonBehaviour",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoGarrisonHookTest::RunTest(const FString& Parameters)
{
	// The hook is opt-in: a plain cargo component has no garrison behaviour and has applied it to nobody.
	const UCargo* const CargoDefaults = GetDefault<UCargo>();
	if (not TestNotNull(TEXT("Cargo defaults"), CargoDefaults))
	{
		return false;
	}
	TestTrue(TEXT("No garrison behaviour by default"), CargoDefaults->GetGarrisonBehaviour().Get() == nullptr);
	TestFalse(TEXT("No squad has the garrison behaviour"),
	          CargoDefaults->GetIsGarrisonBehaviourAppliedToSquad(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchTwoEnumKeysTest,
	"RTS.Weapons.VariantBatch.B2.EnumKeys",
	VariantBatchTwoWeaponTestsPrivate::TestFlags)

bool FVariantBatchTwoEnumKeysTest::RunTest(const FString& Parameters)
{
	// Card keys and saved Blueprint values: a reorder must fail here.
	TestEqual(TEXT("StuH_200MM_DirectSiege"), static_cast<int32>(EWeaponName::StuH_200MM_DirectSiege), 190);
	TestEqual(TEXT("Flak43_37MM_Ostwind"), static_cast<int32>(EWeaponName::Flak43_37MM_Ostwind), 191);
	TestEqual(TEXT("Rocket_320mm_Incendiary"), static_cast<int32>(EWeaponName::Rocket_320mm_Incendiary), 192);
	TestEqual(TEXT("RadixiteHail_Rocket_76MM"), static_cast<int32>(EWeaponName::RadixiteHail_Rocket_76MM), 193);
	TestEqual(TEXT("E25_Hunter_60MM_APCR"), static_cast<int32>(EWeaponName::E25_Hunter_60MM_APCR), 194);
	TestEqual(TEXT("E25_Assault_90MM_HE"), static_cast<int32>(EWeaponName::E25_Assault_90MM_HE), 195);
	TestEqual(TEXT("KwK40_L48_75MM_HEAT_TigerTwin"), static_cast<int32>(EWeaponName::KwK40_L48_75MM_HEAT_TigerTwin),
	          196);
	TestEqual(TEXT("TigerRail_100MM_Railgun"), static_cast<int32>(EWeaponName::TigerRail_100MM_Railgun), 197);
	TestEqual(TEXT("Ger_2xTwin35MM_TigerII"), static_cast<int32>(EWeaponName::Ger_2xTwin35MM_TigerII), 198);
	TestEqual(TEXT("Maus_AA_88MM_Flak"), static_cast<int32>(EWeaponName::Maus_AA_88MM_Flak), 199);

	TestEqual(TEXT("TigerIIRockets key"), static_cast<int32>(ERocketAbility::TigerIIRockets), 4);
	TestEqual(TEXT("HastyReload key"), static_cast<int32>(EBehaviourAbilityType::HastyReload), 25);
	TestEqual(TEXT("E25HunterAmbush key"), static_cast<int32>(EBehaviourAbilityType::E25HunterAmbush), 26);
	TestEqual(TEXT("TwinAutocannonOverload key"), static_cast<int32>(EBehaviourAbilityType::TwinAutocannonOverload),
	          27);
	// The Ostwind's long belt reuses the existing ability and icon.
	TestEqual(TEXT("LongAmmoBelt key (existing)"), static_cast<int32>(EBehaviourAbilityType::LongAmmoBelt), 8);
	TestEqual(TEXT("HastyReload icon"), static_cast<int32>(EBehaviourIcon::HastyReload), 89);
	TestEqual(TEXT("TwinAutocannonOverload icon"), static_cast<int32>(EBehaviourIcon::TwinAutocannonOverload), 90);

	TestTrue(TEXT("Hasty Reload is used against any target"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::HastyReload)
	         == EEnemyBehaviourUsage::CombatAnyTarget);
	TestTrue(TEXT("E25 Hunter Ambush is used against armour"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::E25HunterAmbush)
	         == EEnemyBehaviourUsage::CombatVsArmor);
	TestTrue(TEXT("Twin autocannon overload is used against any target"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::TwinAutocannonOverload)
	         == EEnemyBehaviourUsage::CombatAnyTarget);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
