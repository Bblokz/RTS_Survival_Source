// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the unit variant batch C++: the new weapon entries, the Pz 38(t) rocket data, the new behaviour ability and
// icon enum values. Editor-only and dev-test-only: UBT defines WITH_DEV_AUTOMATION_TESTS=0 for Shipping and Test and
// WITH_EDITOR=0 for every game target, so this translation unit is empty in the shipping binary.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyAbilityUsageProfiles.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace VariantBatchWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float Tolerance = 0.001f;
	const TCHAR* const MissingDisplayName = TEXT("NoWpTranslation");

	const TArray<EWeaponName>& GetNewWeapons()
	{
		static const TArray<EWeaponName> NewWeapons = {
			EWeaponName::KwK_25MM_APCR_Pz38t,
			EWeaponName::GerRailSniper20MM,
			EWeaponName::KwK37_75MM_HE_Sdkfz250,
			EWeaponName::MG151_15MM_Twin,
			EWeaponName::PzI_Bison_L60_50MM,
			EWeaponName::Ger_Twin40MM_Marder,
			EWeaponName::PzIVG_DT_50MM_Artillery,
			EWeaponName::Mortar_100MM_JPzIVSiege,
			EWeaponName::Pak42_L70_75MM_APCR
		};
		return NewWeapons;
	}

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchWeaponRegistrationTest,
	"RTS.Weapons.VariantBatch.Registration",
	VariantBatchWeaponTestsPrivate::TestFlags)

bool FVariantBatchWeaponRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchWeaponTestsPrivate;

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
		TestEqual(*FString::Printf(TEXT("%s enemy copy reloads the same"), *EnumString), EnemyData->ReloadSpeed,
		          PlayerData->ReloadSpeed, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchWeaponKeyStatsTest,
	"RTS.Weapons.VariantBatch.KeyStats",
	VariantBatchWeaponTestsPrivate::TestFlags)

bool FVariantBatchWeaponKeyStatsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchWeaponTestsPrivate;
	// Reload times that animations and crew drills are authored for.
	constexpr float TwinMg151ReloadSeconds = 8.f;
	constexpr float TwinFortyReloadSeconds = 4.5f;
	constexpr int32 ApcrAutocannonClip = 3;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FWeaponData* TwinMg151 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::MG151_15MM_Twin);
	const FWeaponData* TwinForty = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Ger_Twin40MM_Marder);
	const FWeaponData* ApcrAutocannon = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::KwK_25MM_APCR_Pz38t);
	const FWeaponData* HowitzerHe = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::KwK37_75MM_HE_Sdkfz250);
	const FWeaponData* SiegeMortar = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Mortar_100MM_JPzIVSiege);
	const FWeaponData* RailSniper = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::GerRailSniper20MM);
	const FWeaponData* Bison = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::PzI_Bison_L60_50MM);
	const FWeaponData* KwK39 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::KwK39_1_50MM);
	if (not TestNotNull(TEXT("Twin MG 151/15"), TwinMg151) || not TestNotNull(TEXT("Twin 40mm"), TwinForty)
		|| not TestNotNull(TEXT("25mm APCR"), ApcrAutocannon) || not TestNotNull(TEXT("Howitzer HE"), HowitzerHe)
		|| not TestNotNull(TEXT("Siege mortar"), SiegeMortar) || not TestNotNull(TEXT("Rail sniper"), RailSniper)
		|| not TestNotNull(TEXT("Bison 50mm"), Bison) || not TestNotNull(TEXT("KwK 39/1"), KwK39))
	{
		return false;
	}

	TestEqual(TEXT("Twin MG 151/15 reload matches the crew drill"), TwinMg151->ReloadSpeed, TwinMg151ReloadSeconds,
	          Tolerance);
	TestEqual(TEXT("Twin 40mm reload keeps the loader drill under HEAT Overload"), TwinForty->ReloadSpeed,
	          TwinFortyReloadSeconds, Tolerance);
	TestEqual(TEXT("25mm APCR clip"), ApcrAutocannon->MagCapacity, ApcrAutocannonClip);

	const TArray<EWeaponShellType> ApcrOnly = {EWeaponShellType::Shell_APCR};
	const TArray<EWeaponShellType> HeOnly = {EWeaponShellType::Shell_HE};
	TestEqual(TEXT("25mm fires APCR only"), ApcrAutocannon->ShellTypes, ApcrOnly);
	TestEqual(TEXT("Rail sniper fires APCR only"), RailSniper->ShellTypes, ApcrOnly);
	// APCR is applied when firing: these are the values the variant menu shows.
	using DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_ArmorPenMlt;
	using DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_DamageMlt;
	TestEqual(TEXT("Rail sniper fires 100 damage"), RailSniper->BaseDamage * APCR_DamageMlt, 100.f, Tolerance);
	TestEqual(TEXT("Rail sniper fires 225 pen"), RailSniper->ArmorPen * APCR_ArmorPenMlt, 225.f, Tolerance);
	TestEqual(TEXT("Howitzer fires HE only"), HowitzerHe->ShellTypes, HeOnly);
	TestEqual(TEXT("Siege mortar fires HE only"), SiegeMortar->ShellTypes, HeOnly);

	TestTrue(TEXT("Bison reloads faster than the KwK 39/1"), Bison->ReloadSpeed < KwK39->ReloadSpeed);
	TestTrue(TEXT("Bison outranges the KwK 39/1"), Bison->Range > KwK39->Range);
	TestTrue(TEXT("Bison out-pens the KwK 39/1"), Bison->ArmorPen > KwK39->ArmorPen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchPak42ApcrTest,
	"RTS.Weapons.VariantBatch.Pak42Apcr",
	VariantBatchWeaponTestsPrivate::TestFlags)

bool FVariantBatchPak42ApcrTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchWeaponTestsPrivate;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FWeaponData* Pak42 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Pak42_L70_75MM);
	const FWeaponData* Pak42Apcr = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Pak42_L70_75MM_APCR);
	if (not TestNotNull(TEXT("Pak 42 data"), Pak42) || not TestNotNull(TEXT("Pak 42 APCR data"), Pak42Apcr))
	{
		return false;
	}

	const TArray<EWeaponShellType> ApcrOnly = {EWeaponShellType::Shell_APCR};
	TestEqual(TEXT("Default shell is APCR"), Pak42Apcr->ShellType, EWeaponShellType::Shell_APCR);
	TestEqual(TEXT("APCR is the only shell"), Pak42Apcr->ShellTypes, ApcrOnly);
	TestTrue(TEXT("The base Pak 42 keeps its shell choice"), Pak42->ShellTypes.Num() > 1);

	TestEqual(TEXT("Calibre as the Pak 42"), Pak42Apcr->WeaponCalibre, Pak42->WeaponCalibre, Tolerance);
	TestEqual(TEXT("Base damage as the Pak 42"), Pak42Apcr->BaseDamage, Pak42->BaseDamage, Tolerance);
	TestEqual(TEXT("Armor pen as the Pak 42"), Pak42Apcr->ArmorPen, Pak42->ArmorPen, Tolerance);
	TestEqual(TEXT("Armor pen at max range as the Pak 42"), Pak42Apcr->ArmorPenMaxRange, Pak42->ArmorPenMaxRange,
	          Tolerance);
	TestEqual(TEXT("Range as the Pak 42"), Pak42Apcr->Range, Pak42->Range, Tolerance);
	TestEqual(TEXT("Reload as the Pak 42"), Pak42Apcr->ReloadSpeed, Pak42->ReloadSpeed, Tolerance);
	TestEqual(TEXT("Accuracy as the Pak 42"), Pak42Apcr->Accuracy, Pak42->Accuracy);
	TestEqual(TEXT("Projectile speed as the Pak 42"), Pak42Apcr->ProjectileMovementSpeed,
	          Pak42->ProjectileMovementSpeed, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchPz38tRocketsTest,
	"RTS.Weapons.VariantBatch.Pz38tRockets",
	VariantBatchWeaponTestsPrivate::TestFlags)

bool FVariantBatchPz38tRocketsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchWeaponTestsPrivate;
	constexpr float SalvoReloadSeconds = 120.f;
	constexpr float RocketIntervalSeconds = 0.33f;

	const ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FAttachedRocketsData PlayerRockets = GameState->GetAttachedRocketDataOfPlayer(
		PlayerOne, ERocketAbility::Pz38tRockets);
	const FAttachedRocketsData EnemyRockets = GameState->GetAttachedRocketDataOfPlayer(
		EnemyPlayer, ERocketAbility::Pz38tRockets);
	TestEqual(TEXT("Reload between salvos"), PlayerRockets.ReloadSpeed, SalvoReloadSeconds, Tolerance);
	TestEqual(TEXT("Interval between rockets"), PlayerRockets.Cooldown, RocketIntervalSeconds, Tolerance);
	TestEqual(TEXT("Enemy copy reloads the same"), EnemyRockets.ReloadSpeed, SalvoReloadSeconds, Tolerance);

	const FUnitAbilityEntry RocketAbility = FAbilityHelpers::GetRocketAbilityForRocketType(ERocketAbility::Pz38tRockets);
	TestEqual(TEXT("Fire rockets ability"), RocketAbility.AbilityId, EAbilityID::IdFireRockets);
	TestEqual(TEXT("Uses the small rockets card"), RocketAbility.CustomType,
	          static_cast<int32>(EAttachedRocketAbilityType::SmallRockets));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchEnumKeysTest,
	"RTS.Weapons.VariantBatch.EnumKeys",
	VariantBatchWeaponTestsPrivate::TestFlags)

bool FVariantBatchEnumKeysTest::RunTest(const FString& Parameters)
{
	// Card keys and saved Blueprint values: a reorder must fail here.
	TestEqual(TEXT("Pz38tRockets key"), static_cast<int32>(ERocketAbility::Pz38tRockets), 3);
	TestEqual(TEXT("PzJagerAmbush key"), static_cast<int32>(EBehaviourAbilityType::PzJagerAmbush), 22);
	TestEqual(TEXT("PzJagerBarrage key"), static_cast<int32>(EBehaviourAbilityType::PzJagerBarrage), 23);
	TestEqual(TEXT("HEATOverload key"), static_cast<int32>(EBehaviourAbilityType::HEATOverload), 24);
	TestEqual(TEXT("Pz38RailOvercharge icon"), static_cast<int32>(EBehaviourIcon::Pz38RailOvercharge), 83);
	TestEqual(TEXT("HEAT_Rounds icon"), static_cast<int32>(EBehaviourIcon::HEAT_Rounds), 88);
	TestEqual(TEXT("First new weapon"), static_cast<int32>(EWeaponName::KwK_25MM_APCR_Pz38t), 181);
	TestEqual(TEXT("Last new weapon"), static_cast<int32>(EWeaponName::Pak42_L70_75MM_APCR), 189);

	TestTrue(TEXT("Ambush is used against armour"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::PzJagerAmbush)
	         == EEnemyBehaviourUsage::CombatVsArmor);
	TestTrue(TEXT("HEAT Overload is used against armour"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::HEATOverload)
	         == EEnemyBehaviourUsage::CombatVsArmor);
	TestTrue(TEXT("Barrage is used while stationary"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::PzJagerBarrage)
	         == EEnemyBehaviourUsage::CombatStationary);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
