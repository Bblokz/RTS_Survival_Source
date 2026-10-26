// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the unit variant batch 3 C++: the new weapon entries (stored and fired values), the Sd.Kfz. 251 rocket data,
// the homing cone, the rocket launch socket order, the per-weapon radixite damage over time, the new enum values and
// the new Blueprint API. Editor-only and dev-test-only, like the batch 2 tests.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyAbilityUsageProfiles.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRockets.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRocketsData/AttachedRocketsData.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/VariantBatchThree/VariantBatchThreeWeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace VariantBatchThreeWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float Tolerance = 0.01f;
	const TCHAR* const MissingDisplayName = TEXT("NoWpTranslation");

	const TArray<EWeaponName>& GetNewWeapons()
	{
		static const TArray<EWeaponName> NewWeapons = {
			EWeaponName::MG151_15MM_Drilling,
			EWeaponName::Mortar_50MM_140Bunker,
			EWeaponName::MK103_30MM_PumaDual,
			EWeaponName::RadixiteSniper_50MM_Laser,
			EWeaponName::KwK30_Radixite_20MM,
			EWeaponName::KwK42_Radixite_50MM,
			EWeaponName::Rocket_80mm_PzIIIRack,
			EWeaponName::KwK37_75MM_Napalm,
			EWeaponName::FlameCharge_50MM_234,
			EWeaponName::Flak38_20MM_Drilling,
			EWeaponName::Flak41_50MM,
			EWeaponName::Rocket_150mm_Incendiary,
			EWeaponName::Rocket_200mm_PrecisionMissile
		};
		return NewWeapons;
	}

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}

	const FWeaponData* FindWeapon(ACPPGameState* GameState, const EWeaponName WeaponName)
	{
		return GameState ? GameState->GetWeaponDataOfPlayer(PlayerOne, WeaponName) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeWeaponRegistrationTest,
	"RTS.Weapons.VariantBatch.B3.Registration",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeWeaponRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchThreeWeaponTestsPrivate;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeWeaponKeyStatsTest,
	"RTS.Weapons.VariantBatch.B3.KeyStats",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeWeaponKeyStatsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchThreeWeaponTestsPrivate;
	using namespace DeveloperSettings::GameBalance::Weapons::Projectiles;
	using DeveloperSettings::GameBalance::Ranges::LightCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumCannonRange;

	ACPPGameState* GameState = GetGameStateDefaults();
	const FWeaponData* Drilling = FindWeapon(GameState, EWeaponName::MG151_15MM_Drilling);
	const FWeaponData* BunkerMortar = FindWeapon(GameState, EWeaponName::Mortar_50MM_140Bunker);
	const FWeaponData* PumaDual = FindWeapon(GameState, EWeaponName::MK103_30MM_PumaDual);
	const FWeaponData* Kwk31 = FindWeapon(GameState, EWeaponName::Kwk31_30MM);
	const FWeaponData* Sniper = FindWeapon(GameState, EWeaponName::RadixiteSniper_50MM_Laser);
	const FWeaponData* PumaGun = FindWeapon(GameState, EWeaponName::KwK39_1_50MM_Puma);
	const FWeaponData* PzIIFRadixite = FindWeapon(GameState, EWeaponName::KwK30_Radixite_20MM);
	const FWeaponData* KwK30 = FindWeapon(GameState, EWeaponName::KwK30_20MM);
	const FWeaponData* PzIIIRadixite = FindWeapon(GameState, EWeaponName::KwK42_Radixite_50MM);
	const FWeaponData* KwK42 = FindWeapon(GameState, EWeaponName::KwK42_L_50MM);
	const FWeaponData* Rack = FindWeapon(GameState, EWeaponName::Rocket_80mm_PzIIIRack);
	const FWeaponData* Napalm = FindWeapon(GameState, EWeaponName::KwK37_75MM_Napalm);
	const FWeaponData* FlameCharge = FindWeapon(GameState, EWeaponName::FlameCharge_50MM_234);
	const FWeaponData* KwK37 = FindWeapon(GameState, EWeaponName::KwK37_75MM);
	const FWeaponData* DrillingFlak = FindWeapon(GameState, EWeaponName::Flak38_20MM_Drilling);
	const FWeaponData* Flak41 = FindWeapon(GameState, EWeaponName::Flak41_50MM);
	const FWeaponData* MausAa = FindWeapon(GameState, EWeaponName::Maus_AA_88MM_Flak);
	const FWeaponData* Incendiary = FindWeapon(GameState, EWeaponName::Rocket_150mm_Incendiary);
	const FWeaponData* Precision = FindWeapon(GameState, EWeaponName::Rocket_200mm_PrecisionMissile);
	const FWeaponData* Panzerwerfer = FindWeapon(GameState, EWeaponName::Panzerwerfer_Small);
	if (not Drilling || not BunkerMortar || not PumaDual || not Kwk31 || not Sniper || not PumaGun
		|| not PzIIFRadixite || not KwK30 || not PzIIIRadixite || not KwK42 || not Rack || not Napalm
		|| not FlameCharge || not KwK37 || not DrillingFlak || not Flak41 || not MausAa || not Incendiary
		|| not Precision || not Panzerwerfer)
	{
		AddError(TEXT("A weapon entry is missing."));
		return false;
	}

	// Sd.Kfz. 140 AA / Bunker.
	TestEqual(TEXT("Drilling MG magazine"), Drilling->MagCapacity, 30);
	TestEqual(TEXT("Drilling MG reload"), Drilling->ReloadSpeed, 5.25f, Tolerance);
	TestEqual(TEXT("Drilling MG calibre"), Drilling->WeaponCalibre, 15.f, Tolerance);
	TestTrue(TEXT("Bunker mortar is HE only"), BunkerMortar->ShellTypes.Num() == 1
	         && BunkerMortar->ShellType == EWeaponShellType::Shell_HE);
	TestEqual(TEXT("Bunker mortar has the autocannon range"), BunkerMortar->Range, LightCannonRange, Tolerance);
	TestEqual(TEXT("Bunker mortar fired damage"), BunkerMortar->BaseDamage * HE_DamageMlt, 222.f, Tolerance);
	TestEqual(TEXT("Bunker mortar magazine"), BunkerMortar->MagCapacity, 3);
	TestEqual(TEXT("Bunker mortar reload"), BunkerMortar->ReloadSpeed, 7.f, Tolerance);

	// Puma.
	TestEqual(TEXT("Puma Dual magazine"), PumaDual->MagCapacity, 3);
	TestEqual(TEXT("Puma Dual damage as KwK 31"), PumaDual->BaseDamage, Kwk31->BaseDamage, Tolerance);
	TestEqual(TEXT("Sniper damage = 7 Puma shots"), Sniper->BaseDamage, 7.f * PumaGun->BaseDamage, Tolerance);
	TestEqual(TEXT("Sniper damage value"), Sniper->BaseDamage, 898.8f, Tolerance);
	TestEqual(TEXT("Sniper magazine"), Sniper->MagCapacity, 1);
	TestEqual(TEXT("Sniper reload"), Sniper->ReloadSpeed, 3.3f * 1.67f, Tolerance);
	TestEqual(TEXT("Sniper is a laser"), Sniper->DamageType, ERTSDamageType::Laser);
	TestFalse(TEXT("No radixite DoT on the sniper (none requested)"), Sniper->GetHasRadixiteDotOverride());

	// Pz II F radixite: fired values after the radixite shell multipliers.
	TestTrue(TEXT("Pz II F radixite only"), PzIIFRadixite->ShellTypes.Num() == 1
	         && PzIIFRadixite->ShellType == EWeaponShellType::Shell_Radixite);
	TestEqual(TEXT("Pz II F fired damage +30%"), PzIIFRadixite->BaseDamage * Radixite_DamageMlt,
	          KwK30->BaseDamage * 1.3f, Tolerance);
	TestEqual(TEXT("Pz II F fired accuracy as KwK 30"), PzIIFRadixite->Accuracy + Radixite_AccuracyDelta,
	          KwK30->Accuracy);
	TestEqual(TEXT("Pz II F fired max-range pen as KwK 30"),
	          PzIIFRadixite->ArmorPenMaxRange * Radixite_ArmorPenMaxRangeMlt, KwK30->ArmorPenMaxRange, Tolerance);
	TestEqual(TEXT("Pz II F reload"), PzIIFRadixite->ReloadSpeed, 7.5f, Tolerance);
	TestEqual(TEXT("Pz II F DoT 7.5 per second"), PzIIFRadixite->GetRadixiteDotDamagePerSecond(), 7.5f, Tolerance);
	TestEqual(TEXT("Pz II F DoT duration"), PzIIFRadixite->RadixiteDotDurationSeconds, 4.f, Tolerance);

	// Pz III J.
	TestEqual(TEXT("Pz III J fired radixite damage 156"), PzIIIRadixite->BaseDamage * Radixite_DamageMlt, 156.f,
	          Tolerance);
	TestEqual(TEXT("Pz III J radixite default"), PzIIIRadixite->ShellType, EWeaponShellType::Shell_Radixite);
	TestTrue(TEXT("Pz III J can fire HE"), PzIIIRadixite->ShellTypes.Contains(EWeaponShellType::Shell_HE));
	TestEqual(TEXT("Pz III J reload"), PzIIIRadixite->ReloadSpeed, 5.6f, Tolerance);
	TestEqual(TEXT("Pz III J DoT 10 per second"), PzIIIRadixite->GetRadixiteDotDamagePerSecond(), 10.f, Tolerance);
	TestEqual(TEXT("Pz III J fired accuracy as KwK 42"), PzIIIRadixite->Accuracy + Radixite_AccuracyDelta,
	          KwK42->Accuracy);
	TestEqual(TEXT("Rack rocket = half a KwK 42 shot"), Rack->BaseDamage, KwK42->BaseDamage * 0.5f, Tolerance);
	TestTrue(TEXT("Rack is HEAT only"), Rack->ShellTypes.Num() == 1 && Rack->ShellType == EWeaponShellType::Shell_HEAT);
	TestEqual(TEXT("Rack fired pen 90"), Rack->ArmorPen * HEAT_ArmorPenMlt, 90.f, Tolerance);
	TestEqual(TEXT("Rack magazine"), Rack->MagCapacity, 8);
	TestEqual(TEXT("Rack reload"), Rack->ReloadSpeed, 24.f, Tolerance);
	TestEqual(TEXT("Rack range"), Rack->Range, MediumCannonRange, Tolerance);

	// Sd.Kfz. 234/3 Incendiary.
	TestTrue(TEXT("Napalm Stummel is HE only"), Napalm->ShellTypes.Num() == 1
	         && Napalm->ShellType == EWeaponShellType::Shell_HE);
	TestTrue(TEXT("Napalm Stummel burns the ground"), Napalm->GroundFire.GetIsEnabled());
	TestEqual(TEXT("Napalm ground fire radius"), Napalm->GroundFire.RadiusCm, 350.f, Tolerance);
	TestEqual(TEXT("Napalm ground fire DPS"), Napalm->GroundFire.DamagePerSecond, 20.f, Tolerance);
	TestTrue(TEXT("Flame charge burns the ground"), FlameCharge->GroundFire.GetIsEnabled());
	TestEqual(TEXT("Flame charge ground fire duration"), FlameCharge->GroundFire.DurationSeconds, 7.f, Tolerance);
	TestEqual(TEXT("Flame charge reload"), FlameCharge->ReloadSpeed, 5.25f, Tolerance);
	TestEqual(TEXT("Flame charge has the Stummel's range"), FlameCharge->Range, KwK37->Range, Tolerance);
	TestEqual(TEXT("Flame charge magazine"), FlameCharge->MagCapacity, 1);

	// Pz III AA.
	TestEqual(TEXT("Drilling Flak magazine"), DrillingFlak->MagCapacity, 20);
	TestEqual(TEXT("Drilling Flak reload"), DrillingFlak->ReloadSpeed, 5.25f, Tolerance);
	TestEqual(TEXT("Drilling Flak calibre"), DrillingFlak->WeaponCalibre, 20.f, Tolerance);
	TestEqual(TEXT("FlaK 41 calibre"), Flak41->WeaponCalibre, 50.f, Tolerance);
	TestEqual(TEXT("FlaK 41 clip"), Flak41->MagCapacity, 5);
	TestEqual(TEXT("FlaK 41 reload"), Flak41->ReloadSpeed, 3.25f, Tolerance);
	TestEqual(TEXT("FlaK 41 range"), Flak41->Range, MediumCannonRange, Tolerance);
	TestTrue(TEXT("FlaK 41 hits softer than the 88 mm Maus AA Flak"), Flak41->BaseDamage < MausAa->BaseDamage);

	// Pz 38(t) Rockets.
	TestEqual(TEXT("Incendiary rocket = 3 base rockets"), Incendiary->BaseDamage, 3.f * Panzerwerfer->BaseDamage,
	          Tolerance);
	TestEqual(TEXT("Incendiary rocket damage value"), Incendiary->BaseDamage, 279.f, Tolerance);
	TestEqual(TEXT("Incendiary rocket is fire"), Incendiary->DamageType, ERTSDamageType::Fire);
	TestEqual(TEXT("Incendiary rocket fire shell"), Incendiary->ShellType, EWeaponShellType::Shell_Fire);
	TestEqual(TEXT("Incendiary magazine"), Incendiary->MagCapacity, 6);
	TestFalse(TEXT("Incendiary rocket leaves no ground fire (R4)"), Incendiary->GroundFire.GetIsEnabled());
	TestEqual(TEXT("Precision missile = 5 base rockets"), Precision->BaseDamage, 5.f * Panzerwerfer->BaseDamage,
	          Tolerance);
	TestEqual(TEXT("Precision missile damage value"), Precision->BaseDamage, 465.f, Tolerance);
	TestEqual(TEXT("Precision missile calibre"), Precision->WeaponCalibre, 200.f, Tolerance);
	TestEqual(TEXT("Precision missile salvo = base salvo damage"), Precision->BaseDamage * Precision->MagCapacity,
	          930.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeRocketDataTest,
	"RTS.Weapons.VariantBatch.B3.Sdkfz251Rockets",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeRocketDataTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchThreeWeaponTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}

	const FAttachedRocketsData Incendiary = GameState->GetAttachedRocketDataOfPlayer(
		PlayerOne, ERocketAbility::Sdkfz251IncendiaryRockets);
	const FAttachedRocketsData IncendiaryEnemy = GameState->GetAttachedRocketDataOfPlayer(
		EnemyPlayer, ERocketAbility::Sdkfz251IncendiaryRockets);
	TestEqual(TEXT("Incendiary reload"), Incendiary.ReloadSpeed, 60.f, Tolerance);
	TestEqual(TEXT("Incendiary interval"), Incendiary.Cooldown, 0.5f, Tolerance);
	TestEqual(TEXT("Incendiary enemy copy"), IncendiaryEnemy.ReloadSpeed, 60.f, Tolerance);
	TestEqual(TEXT("Incendiary is fire"), Incendiary.DamageType, ERTSDamageType::Fire);
	TestEqual(TEXT("Incendiary fire shell"), Incendiary.ShellType, EWeaponShellType::Shell_Fire);
	TestTrue(TEXT("Incendiary burns the ground"), Incendiary.GroundFire.GetIsEnabled());
	TestEqual(TEXT("Incendiary ground fire radius"), Incendiary.GroundFire.RadiusCm, 450.f, Tolerance);
	TestFalse(TEXT("Incendiary does not home"), Incendiary.bHomingMissiles);

	const FAttachedRocketsData Guided = GameState->GetAttachedRocketDataOfPlayer(
		PlayerOne, ERocketAbility::Sdkfz251GuidedMissiles);
	TestTrue(TEXT("Guided missiles home"), Guided.bHomingMissiles);
	TestEqual(TEXT("Guided cone"), Guided.HomingConeHalfAngleDegrees, 60.f, Tolerance);
	TestEqual(TEXT("Guided reload"), Guided.ReloadSpeed, 50.f, Tolerance);
	TestEqual(TEXT("Guided interval"), Guided.Cooldown, 0.9f, Tolerance);
	TestEqual(TEXT("Guided damage"), Guided.Damage, 280.f, Tolerance);
	TestEqual(TEXT("Guided pen"), Guided.ArmorPen, 170.f, Tolerance);
	TestFalse(TEXT("Guided leaves no ground fire"), Guided.GroundFire.GetIsEnabled());
	TestEqual(TEXT("Other rockets keep HEAT by default"),
	          GameState->GetAttachedRocketDataOfPlayer(PlayerOne, ERocketAbility::TigerIIRockets).ShellType,
	          EWeaponShellType::Shell_HEAT);

	const FUnitAbilityEntry HetzerAbility =
		FAbilityHelpers::GetRocketAbilityForRocketType(ERocketAbility::HetzerRockets);
	for (const ERocketAbility RocketAbility : {ERocketAbility::Sdkfz251IncendiaryRockets,
	                                           ERocketAbility::Sdkfz251GuidedMissiles})
	{
		const FUnitAbilityEntry Entry = FAbilityHelpers::GetRocketAbilityForRocketType(RocketAbility);
		TestEqual(TEXT("Fire rockets ability"), Entry.AbilityId, EAbilityID::IdFireRockets);
		TestEqual(TEXT("Large rocket card"), Entry.CustomType, HetzerAbility.CustomType);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeHomingConeTest,
	"RTS.Weapons.VariantBatch.B3.HomingCone",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeHomingConeTest::RunTest(const FString& Parameters)
{
	constexpr float HalfAngle = 60.f;
	constexpr float Range = 11000.f;
	const FVector Origin = FVector::ZeroVector;
	const FVector Heading = FVector::ForwardVector;
	TestTrue(TEXT("Straight ahead"),
	         UAttachedRockets::GetIsWithinHomingCone(Origin, Heading, FVector(5000.f, 0.f, 0.f), HalfAngle, Range));
	TestTrue(TEXT("55 degrees off"), UAttachedRockets::GetIsWithinHomingCone(
		         Origin, Heading, FRotator(0.f, 55.f, 0.f).RotateVector(FVector(5000.f, 0.f, 0.f)), HalfAngle, Range));
	TestFalse(TEXT("65 degrees off"), UAttachedRockets::GetIsWithinHomingCone(
		          Origin, Heading, FRotator(0.f, -65.f, 0.f).RotateVector(FVector(5000.f, 0.f, 0.f)), HalfAngle,
		          Range));
	TestFalse(TEXT("Behind"),
	          UAttachedRockets::GetIsWithinHomingCone(Origin, Heading, FVector(-5000.f, 0.f, 0.f), HalfAngle, Range));
	TestFalse(TEXT("Out of range"),
	          UAttachedRockets::GetIsWithinHomingCone(Origin, Heading, FVector(12000.f, 0.f, 0.f), HalfAngle, Range));
	TestTrue(TEXT("Height does not change the horizontal angle"), UAttachedRockets::GetIsWithinHomingCone(
		         Origin, Heading, FVector(3000.f, 0.f, 3000.f), HalfAngle, Range));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeRocketSocketOrderTest,
	"RTS.Weapons.VariantBatch.B3.RocketSocketOrder",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeRocketSocketOrderTest::RunTest(const FString& Parameters)
{
	TArray<FName> SocketNames = {
		FName(TEXT("RKT_10")), FName(TEXT("RKT_2")), FName(TEXT("RKT_0")), FName(TEXT("RKT_1"))
	};
	UWeaponStateRocketProjectile::SortLaunchSocketNamesByNumericSuffix(SocketNames);
	const TArray<FName> Expected = {
		FName(TEXT("RKT_0")), FName(TEXT("RKT_1")), FName(TEXT("RKT_2")), FName(TEXT("RKT_10"))
	};
	TestEqual(TEXT("Socket count"), SocketNames.Num(), Expected.Num());
	for (int32 Index = 0; Index < FMath::Min(SocketNames.Num(), Expected.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Socket %d"), Index), SocketNames[Index], Expected[Index]);
	}

	const FRocketWeaponSettings DefaultSettings;
	TestFalse(TEXT("Attached rockets are opt-in"), DefaultSettings.bSetupWithAttachedRocketsMesh);
	TestTrue(TEXT("Default rocket scale is 1"), DefaultSettings.RocketScale.Equals(FVector(1.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeRadixiteDotDataTest,
	"RTS.Weapons.VariantBatch.B3.RadixiteDotData",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeRadixiteDotDataTest::RunTest(const FString& Parameters)
{
	FWeaponData DefaultData;
	TestFalse(TEXT("No override by default"), DefaultData.GetHasRadixiteDotOverride());
	TestEqual(TEXT("No override DPS by default"), DefaultData.GetRadixiteDotDamagePerSecond(), 0.f);

	FWeaponData OnlyTotal;
	OnlyTotal.RadixiteDotTotalDamage = 30.f;
	TestFalse(TEXT("Total alone is no override"), OnlyTotal.GetHasRadixiteDotOverride());

	FWeaponData SourceData;
	SourceData.RadixiteDotTotalDamage = 50.f;
	SourceData.RadixiteDotDurationSeconds = 5.f;
	FWeaponData CopiedData;
	CopiedData.CopyWeaponDataValues(&SourceData);
	TestTrue(TEXT("CopyWeaponDataValues copies the override"), CopiedData.GetHasRadixiteDotOverride());
	TestEqual(TEXT("Copied DPS"), CopiedData.GetRadixiteDotDamagePerSecond(), 10.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeBlueprintApiTest,
	"RTS.Weapons.VariantBatch.B3.BlueprintApi",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeBlueprintApiTest::RunTest(const FString& Parameters)
{
	const UFunction* ReloadFinishedEvent =
		ACPPTurretsMaster::StaticClass()->FindFunctionByName(TEXT("BP_OnReloadFinished"));
	if (TestNotNull(TEXT("BP_OnReloadFinished exists"), ReloadFinishedEvent))
	{
		TestTrue(TEXT("BP_OnReloadFinished is a Blueprint event"),
		         ReloadFinishedEvent->HasAnyFunctionFlags(FUNC_BlueprintEvent));
	}
	const UFunction* ReverseSpeedSetter =
		UTrackPathFollowingComponent::StaticClass()->FindFunctionByName(TEXT("SetDesiredReverseSpeed"));
	if (TestNotNull(TEXT("SetDesiredReverseSpeed is a UFUNCTION"), ReverseSpeedSetter))
	{
		TestTrue(TEXT("SetDesiredReverseSpeed is BlueprintCallable"),
		         ReverseSpeedSetter->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}
	const UFunction* TurnRateSetter = ATankMaster::StaticClass()->FindFunctionByName(TEXT("SetVehicleTurnRate"));
	if (TestNotNull(TEXT("SetVehicleTurnRate is a UFUNCTION"), TurnRateSetter))
	{
		TestTrue(TEXT("SetVehicleTurnRate is BlueprintCallable"),
		         TurnRateSetter->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}
	TestNotNull(TEXT("VariantMobilityMultipliers property"),
	            ATankMaster::StaticClass()->FindPropertyByName(TEXT("VariantMobilityMultipliers")));

	const FTankVariantMobilityMultipliers DefaultMultipliers;
	TestEqual(TEXT("Default speed multiplier"), DefaultMultipliers.MaxSpeedMultiplier, 1.f);
	TestEqual(TEXT("Default reverse multiplier"), DefaultMultipliers.ReverseSpeedMultiplier, 1.f);
	TestEqual(TEXT("Default turn multiplier"), DefaultMultipliers.TurnRateMultiplier, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchThreeEnumKeysTest,
	"RTS.Weapons.VariantBatch.B3.EnumKeys",
	VariantBatchThreeWeaponTestsPrivate::TestFlags)

bool FVariantBatchThreeEnumKeysTest::RunTest(const FString& Parameters)
{
	// Card keys and saved Blueprint values: a reorder must fail here.
	TestEqual(TEXT("Maus_AA_88MM_Flak (batch 2 last)"), static_cast<int32>(EWeaponName::Maus_AA_88MM_Flak), 199);
	TestEqual(TEXT("MG151_15MM_Drilling"), static_cast<int32>(EWeaponName::MG151_15MM_Drilling), 200);
	TestEqual(TEXT("Mortar_50MM_140Bunker"), static_cast<int32>(EWeaponName::Mortar_50MM_140Bunker), 201);
	TestEqual(TEXT("MK103_30MM_PumaDual"), static_cast<int32>(EWeaponName::MK103_30MM_PumaDual), 202);
	TestEqual(TEXT("RadixiteSniper_50MM_Laser"), static_cast<int32>(EWeaponName::RadixiteSniper_50MM_Laser), 203);
	TestEqual(TEXT("KwK30_Radixite_20MM"), static_cast<int32>(EWeaponName::KwK30_Radixite_20MM), 204);
	TestEqual(TEXT("KwK42_Radixite_50MM"), static_cast<int32>(EWeaponName::KwK42_Radixite_50MM), 205);
	TestEqual(TEXT("Rocket_80mm_PzIIIRack"), static_cast<int32>(EWeaponName::Rocket_80mm_PzIIIRack), 206);
	TestEqual(TEXT("KwK37_75MM_Napalm"), static_cast<int32>(EWeaponName::KwK37_75MM_Napalm), 207);
	TestEqual(TEXT("FlameCharge_50MM_234"), static_cast<int32>(EWeaponName::FlameCharge_50MM_234), 208);
	TestEqual(TEXT("Flak38_20MM_Drilling"), static_cast<int32>(EWeaponName::Flak38_20MM_Drilling), 209);
	TestEqual(TEXT("Flak41_50MM"), static_cast<int32>(EWeaponName::Flak41_50MM), 210);
	TestEqual(TEXT("Rocket_150mm_Incendiary"), static_cast<int32>(EWeaponName::Rocket_150mm_Incendiary), 211);
	TestEqual(TEXT("Rocket_200mm_PrecisionMissile"),
	          static_cast<int32>(EWeaponName::Rocket_200mm_PrecisionMissile), 212);

	TestEqual(TEXT("Sdkfz251IncendiaryRockets key"),
	          static_cast<int32>(ERocketAbility::Sdkfz251IncendiaryRockets), 5);
	TestEqual(TEXT("Sdkfz251GuidedMissiles key"), static_cast<int32>(ERocketAbility::Sdkfz251GuidedMissiles), 6);
	TestEqual(TEXT("Sdkfz140BunkerOverwatch key"),
	          static_cast<int32>(EBehaviourAbilityType::Sdkfz140BunkerOverwatch), 28);
	TestEqual(TEXT("PzIIIRadixiteOvercharge key"),
	          static_cast<int32>(EBehaviourAbilityType::PzIIIRadixiteOvercharge), 29);
	// Reused, not new: the Overwatch and overcharge icons, the APCR and long-belt abilities.
	TestEqual(TEXT("APCR_Rounds key (existing)"), static_cast<int32>(EBehaviourAbilityType::APCR_Rounds), 19);
	TestEqual(TEXT("LongAmmoBelt key (existing)"), static_cast<int32>(EBehaviourAbilityType::LongAmmoBelt), 8);
	TestEqual(TEXT("OverwatchFireSupport icon (existing)"),
	          static_cast<int32>(EBehaviourIcon::OverwatchFireSupport), 22);
	TestEqual(TEXT("Pz38RailOvercharge icon (existing)"), static_cast<int32>(EBehaviourIcon::Pz38RailOvercharge), 83);

	TestEqual(TEXT("VariantC key"), static_cast<int32>(EUnitVariant::VariantC), 3);
	TestEqual(TEXT("Four variants per unit"), UnitVariants::MaxVariantsPerUnit, 4);
	TestEqual(TEXT("VariantC string"), Global_GetUnitVariantAsString(EUnitVariant::VariantC), FString(TEXT("VariantC")));

	TestTrue(TEXT("Bunker Overwatch is stationary combat"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::Sdkfz140BunkerOverwatch)
	         == EEnemyBehaviourUsage::CombatStationary);
	TestTrue(TEXT("Radixite Overcharge is used against any target"),
	         FEnemyAbilityUsageProfiles::GetBehaviourUsage(EBehaviourAbilityType::PzIIIRadixiteOvercharge)
	         == EEnemyBehaviourUsage::CombatAnyTarget);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
