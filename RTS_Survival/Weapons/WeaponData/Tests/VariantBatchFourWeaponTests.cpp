// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the unit variant batch 4 C++ (Wespe, Pz III M, Hetzer, JagdPanther): the new weapon entries (stored and fired
// values), the Ashmaker11 data-key fix, the weapon-name filter on weapon behaviours, the attached-rockets disable flag,
// the replace-in-slot option, and the new enum values. Editor-only and dev-test-only, like the batch 2 and 3 tests.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRockets.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/VariantBatchFour/VariantBatchFourWeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponNameConversion.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace VariantBatchFourWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float Tolerance = 0.01f;
	const TCHAR* const MissingDisplayName = TEXT("NoWpTranslation");

	const TArray<EWeaponName>& GetNewWeapons()
	{
		static const TArray<EWeaponName> NewWeapons = {
			EWeaponName::LeFH_Radixite_200MM,
			EWeaponName::Laser_60MM_PzIIIM,
			EWeaponName::PanzerSchreck_PzIIIM,
			EWeaponName::Flamm15_HetzerHull,
			EWeaponName::Mortar_120MM_Hetzer,
			EWeaponName::Pak43_88MM_APCR_Pfeil,
			EWeaponName::sFH18_150MM_Bollwerk,
			EWeaponName::KwK_Long60MM_APCR_Flanker
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

	/**
	 * The values a weapon fires with for a shell type. GLOBAL_GetWeaponDataForShellType is a header-static defined only
	 * in WeaponData.cpp, so a test in another unity unit cannot call it; its shell multipliers are mirrored here.
	 */
	FWeaponData GetFiredData(const FWeaponData& StoredData, const EWeaponShellType ShellType)
	{
		using namespace DeveloperSettings::GameBalance::Weapons::Projectiles;
		FWeaponData FiredData = StoredData;
		FiredData.ShellType = ShellType;
		switch (ShellType)
		{
		case EWeaponShellType::Shell_APHEBC:
			FiredData.ArmorPenMaxRange = FMath::Lerp(FiredData.ArmorPen, FiredData.ArmorPenMaxRange,
			                                         APHEBC_ArmorPenLerpFactor);
			break;
		case EWeaponShellType::Shell_HE:
			FiredData.ArmorPen *= HE_ArmorPenMlt;
			FiredData.ArmorPenMaxRange = FiredData.ArmorPen;
			FiredData.TNTExplosiveGrams *= HE_TNTExplosiveGramsMlt;
			FiredData.BaseDamage *= HE_DamageMlt;
			FiredData.ShrapnelDamage *= HE_ShrapnelDamageMlt;
			FiredData.ShrapnelRange *= HE_ShrapnelRangeMlt;
			FiredData.ShrapnelPen *= HE_ShrapnelPenMlt;
			FiredData.ShrapnelParticles *= HE_ShrapnelParticlesMlt;
			FiredData.ProjectileMovementSpeed *= HE_ProjectileSpeedMlt;
			break;
		case EWeaponShellType::Shell_HEAT:
			FiredData.ArmorPen *= HEAT_ArmorPenMlt;
			FiredData.ArmorPenMaxRange = FiredData.ArmorPen;
			FiredData.ProjectileMovementSpeed *= HEAT_ProjectileSpeedMlt;
			break;
		case EWeaponShellType::Shell_Radixite:
			FiredData.BaseDamage *= Radixite_DamageMlt;
			FiredData.ArmorPenMaxRange *= Radixite_ArmorPenMaxRangeMlt;
			FiredData.Accuracy += Radixite_AccuracyDelta;
			break;
		case EWeaponShellType::Shell_APCR:
			FiredData.ArmorPen *= APCR_ArmorPenMlt;
			FiredData.ArmorPenMaxRange *= APCR_ArmorPenMlt;
			FiredData.BaseDamage *= APCR_DamageMlt;
			FiredData.ShrapnelParticles = 0;
			FiredData.ProjectileMovementSpeed *= APCR_ProjectileSpeedMlt;
			break;
		default:
			// AP, APHE and railgun shells fire their stored values.
			break;
		}
		return FiredData;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourWeaponRegistrationTest,
	"RTS.Weapons.VariantBatch.B4.Registration",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourWeaponRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchFourWeaponTestsPrivate;

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
		// The Blueprint side stores weapons as names: the name must round-trip through the enum.
		const FName BlueprintName = Global_GetWeaponNameAsFName(WeaponName);
		TestEqual(*FString::Printf(TEXT("%s Blueprint name matches the enum string"), *EnumString),
		          BlueprintName.ToString(), EnumString);
		TestEqual(*FString::Printf(TEXT("%s Blueprint name resolves"), *EnumString),
		          Global_GetWeaponNameFromFName(BlueprintName), WeaponName);
		TestTrue(*FString::Printf(TEXT("%s default shell is in its shell list"), *EnumString),
		         PlayerData->ShellTypes.Contains(PlayerData->ShellType));
		TestTrue(*FString::Printf(TEXT("%s has a range"), *EnumString), PlayerData->Range > 0.f);
		TestTrue(*FString::Printf(TEXT("%s has damage"), *EnumString), PlayerData->BaseDamage > 0.f);
		TestTrue(*FString::Printf(TEXT("%s has a magazine"), *EnumString), PlayerData->MagCapacity > 0);
		TestTrue(*FString::Printf(TEXT("%s has a reload"), *EnumString), PlayerData->ReloadSpeed > 0.f);
		TestEqual(*FString::Printf(TEXT("%s enemy copy hits the same"), *EnumString), EnemyData->BaseDamage,
		          PlayerData->BaseDamage, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourWeaponKeyStatsTest,
	"RTS.Weapons.VariantBatch.B4.KeyStats",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourWeaponKeyStatsTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchFourWeaponTestsPrivate;
	using namespace DeveloperSettings::GameBalance::Weapons::Projectiles;
	using DeveloperSettings::GameBalance::Ranges::MediumCannonRange;
	using DeveloperSettings::GameBalance::Ranges::MediumLaserWeaponRange;
	using DeveloperSettings::GameBalance::Ranges::HeavyArtilleryRange;
	using DeveloperSettings::GameBalance::Weapons::DamagePerMM;

	ACPPGameState* GameState = GetGameStateDefaults();
	const FWeaponData* RadixiteHowitzer = FindWeapon(GameState, EWeaponName::LeFH_Radixite_200MM);
	const FWeaponData* LeFH = FindWeapon(GameState, EWeaponName::LeFH_18_105MM);
	const FWeaponData* PzIIIMLaser = FindWeapon(GameState, EWeaponName::Laser_60MM_PzIIIM);
	const FWeaponData* KwK39 = FindWeapon(GameState, EWeaponName::KwK39_1_50MM);
	const FWeaponData* PzIIIMSchreck = FindWeapon(GameState, EWeaponName::PanzerSchreck_PzIIIM);
	const FWeaponData* PitSchreck = FindWeapon(GameState, EWeaponName::PanzerSchreck_Pit);
	const FWeaponData* HullFlamm = FindWeapon(GameState, EWeaponName::Flamm15_HetzerHull);
	const FWeaponData* Flamm15 = FindWeapon(GameState, EWeaponName::Flamm15);
	const FWeaponData* HetzerMortar = FindWeapon(GameState, EWeaponName::Mortar_120MM_Hetzer);
	const FWeaponData* Mortar120 = FindWeapon(GameState, EWeaponName::Mortar_120MM);
	const FWeaponData* Pfeil = FindWeapon(GameState, EWeaponName::Pak43_88MM_APCR_Pfeil);
	const FWeaponData* Pak43 = FindWeapon(GameState, EWeaponName::Pak43_88MM);
	const FWeaponData* Bollwerk = FindWeapon(GameState, EWeaponName::sFH18_150MM_Bollwerk);
	const FWeaponData* TowedSfh = FindWeapon(GameState, EWeaponName::sFH18_150MM_TW);
	const FWeaponData* Flanker = FindWeapon(GameState, EWeaponName::KwK_Long60MM_APCR_Flanker);
	const FWeaponData* Bison = FindWeapon(GameState, EWeaponName::PzI_Bison_L60_50MM);
	if (not RadixiteHowitzer || not LeFH || not PzIIIMLaser || not KwK39 || not PzIIIMSchreck || not PitSchreck
		|| not HullFlamm || not Flamm15 || not HetzerMortar || not Mortar120 || not Pfeil || not Pak43
		|| not Bollwerk || not TowedSfh || not Flanker || not Bison)
	{
		AddError(TEXT("A weapon entry is missing."));
		return false;
	}

	// Wespe Radixite Howitzer: fired radixite = 1.5x the base Wespe's fired HE; same pen, accuracy and arc speed.
	const FWeaponData LeFHFired = GetFiredData(*LeFH, EWeaponShellType::Shell_HE);
	const FWeaponData RadixiteFired = GetFiredData(*RadixiteHowitzer, EWeaponShellType::Shell_Radixite);
	TestEqual(TEXT("Radixite howitzer default shell"), RadixiteHowitzer->ShellType, EWeaponShellType::Shell_Radixite);
	TestTrue(TEXT("Radixite howitzer can load HE (W3)"),
	         RadixiteHowitzer->ShellTypes.Num() == 2 && RadixiteHowitzer->ShellTypes.Contains(EWeaponShellType::Shell_HE));
	TestEqual(TEXT("Radixite howitzer stored damage"), RadixiteHowitzer->BaseDamage, 1486.8f, 0.1f);
	TestEqual(TEXT("Radixite howitzer fired damage = 1.5x base fired"), RadixiteFired.BaseDamage,
	          LeFHFired.BaseDamage * VariantBatchFourWeapons::RadixiteHowitzerDamageInBaseShells, 0.1f);
	TestEqual(TEXT("Radixite howitzer fired damage value"), RadixiteFired.BaseDamage, 1784.16f, 0.1f);
	TestEqual(TEXT("Radixite howitzer fired pen as base HE"), RadixiteFired.ArmorPen, LeFHFired.ArmorPen, Tolerance);
	TestEqual(TEXT("Radixite howitzer fired max-range pen as base HE"), RadixiteFired.ArmorPenMaxRange,
	          LeFHFired.ArmorPenMaxRange, 0.05f);
	TestEqual(TEXT("Radixite howitzer fired accuracy as base"), RadixiteFired.Accuracy, LeFHFired.Accuracy);
	TestEqual(TEXT("Radixite howitzer arc speed as base HE"), RadixiteFired.ProjectileMovementSpeed,
	          LeFHFired.ProjectileMovementSpeed, 0.1f);
	TestEqual(TEXT("Radixite howitzer reload 2x base"), RadixiteHowitzer->ReloadSpeed, LeFH->ReloadSpeed * 2.f,
	          Tolerance);
	TestEqual(TEXT("Radixite howitzer reload value"), RadixiteHowitzer->ReloadSpeed, 10.f, Tolerance);
	TestEqual(TEXT("Radixite howitzer calibre"), RadixiteHowitzer->WeaponCalibre, 200.f, Tolerance);
	TestEqual(TEXT("Radixite howitzer range as base"), RadixiteHowitzer->Range, LeFH->Range, Tolerance);
	TestEqual(TEXT("Radixite howitzer DoT 10 per second"), RadixiteHowitzer->GetRadixiteDotDamagePerSecond(), 10.f,
	          Tolerance);
	TestEqual(TEXT("Radixite howitzer DoT duration"), RadixiteHowitzer->RadixiteDotDurationSeconds, 6.f, Tolerance);
	// HE uses the same stored values with the HE multipliers (documented as TUNABLE in CPP_IMPL_DONE_B4.md).
	const FWeaponData RadixiteHowitzerHeFired = GetFiredData(*RadixiteHowitzer, EWeaponShellType::Shell_HE);
	TestTrue(TEXT("Radixite howitzer HE outdamages the base HE"),
	         RadixiteHowitzerHeFired.BaseDamage > LeFHFired.BaseDamage);

	// Pz III M Overcharged laser.
	TestEqual(TEXT("Pz III M laser = 3 KwK 39/1 shots"), PzIIIMLaser->BaseDamage, 3.f * KwK39->BaseDamage, Tolerance);
	TestEqual(TEXT("Pz III M laser damage value"), PzIIIMLaser->BaseDamage, 385.2f, Tolerance);
	TestEqual(TEXT("Pz III M laser reload = 4 KwK 39/1 reloads"), PzIIIMLaser->ReloadSpeed, 14.8f, Tolerance);
	TestEqual(TEXT("Pz III M laser magazine"), PzIIIMLaser->MagCapacity, 1);
	TestEqual(TEXT("Pz III M laser calibre"), PzIIIMLaser->WeaponCalibre, 60.f, Tolerance);
	TestEqual(TEXT("Pz III M laser is a laser"), PzIIIMLaser->DamageType, ERTSDamageType::Laser);
	TestEqual(TEXT("Pz III M laser range"), PzIIIMLaser->Range, MediumLaserWeaponRange, Tolerance);

	// Pz III M Spaced Armor Panzerschreck.
	TestEqual(TEXT("Pz III M Panzerschreck has medium cannon range"), PzIIIMSchreck->Range, MediumCannonRange,
	          Tolerance);
	TestTrue(TEXT("Pz III M Panzerschreck outranges the pit"), PzIIIMSchreck->Range > PitSchreck->Range);
	TestEqual(TEXT("Pz III M Panzerschreck hits as the pit"), PzIIIMSchreck->BaseDamage, PitSchreck->BaseDamage,
	          Tolerance);
	TestEqual(TEXT("Pz III M Panzerschreck pen as the pit"), PzIIIMSchreck->ArmorPen, PitSchreck->ArmorPen, Tolerance);

	// Hetzer Flame / Mortar.
	TestEqual(TEXT("Hetzer hull flamer damage"), HullFlamm->BaseDamage, 16.75f, Tolerance);
	TestEqual(TEXT("Hetzer hull flamer range as Flamm 15"), HullFlamm->Range, Flamm15->Range, Tolerance);
	TestEqual(TEXT("Hetzer hull flamer is fire"), HullFlamm->DamageType, ERTSDamageType::Fire);
	TestEqual(TEXT("Hetzer hull flamer reload as Flamm 15"), HullFlamm->ReloadSpeed, Flamm15->ReloadSpeed, Tolerance);
	TestTrue(TEXT("Hetzer mortar is HE only"), HetzerMortar->ShellTypes.Num() == 1
	         && HetzerMortar->ShellType == EWeaponShellType::Shell_HE);
	TestEqual(TEXT("Hetzer mortar damage x1.5"), HetzerMortar->BaseDamage, Mortar120->BaseDamage * 1.5f, Tolerance);
	TestEqual(TEXT("Hetzer mortar TNT x1.4"), HetzerMortar->TNTExplosiveGrams, Mortar120->TNTExplosiveGrams * 1.4f,
	          Tolerance);
	TestEqual(TEXT("Hetzer mortar shrapnel damage follows TNT"), HetzerMortar->ShrapnelDamage,
	          Mortar120->ShrapnelDamage * 1.4f, Tolerance);
	TestEqual(TEXT("Hetzer mortar reload"), HetzerMortar->ReloadSpeed, 7.f, Tolerance);
	TestEqual(TEXT("Hetzer mortar range as base mortar"), HetzerMortar->Range, Mortar120->Range, Tolerance);
	TestEqual(TEXT("Hetzer mortar magazine"), HetzerMortar->MagCapacity, 1);

	// JagdPanther Pfeil.
	TestTrue(TEXT("Pfeil is APCR only"), Pfeil->ShellTypes.Num() == 1 && Pfeil->ShellType == EWeaponShellType::Shell_APCR);
	TestEqual(TEXT("Pfeil has no TNT"), Pfeil->TNTExplosiveGrams, 0.f, Tolerance);
	TestTrue(TEXT("Pfeil has no shrapnel"), Pfeil->ShrapnelRange == 0.f && Pfeil->ShrapnelDamage == 0.f
	         && Pfeil->ShrapnelParticles == 0 && Pfeil->ShrapnelPen == 0.f);
	TestEqual(TEXT("Pfeil stored damage = 250 / APCR mlt"), Pfeil->BaseDamage,
	          250.f / DeveloperSettings::GameBalance::Weapons::Projectiles::APCR_DamageMlt, Tolerance);
	TestEqual(TEXT("Pfeil pen"), Pfeil->ArmorPen, 255.f, Tolerance);
	TestEqual(TEXT("Pfeil max-range pen"), Pfeil->ArmorPenMaxRange, 250.f, Tolerance);
	TestEqual(TEXT("Pfeil accuracy"), Pfeil->Accuracy, 97);
	TestEqual(TEXT("Pfeil reload as PaK 43"), Pfeil->ReloadSpeed, Pak43->ReloadSpeed, Tolerance);
	TestEqual(TEXT("Pfeil range = PaK 43 x1.1"), Pfeil->Range, Pak43->Range * 1.1f, 10.f);
	TestEqual(TEXT("Pfeil shell speed as PaK 43 (APCR x1.4 at fire)"), Pfeil->ProjectileMovementSpeed,
	          Pak43->ProjectileMovementSpeed, Tolerance);
	const FWeaponData PfeilFired = GetFiredData(*Pfeil, EWeaponShellType::Shell_APCR);
	TestTrue(TEXT("Pfeil fired pen beats the PaK 43 APHE"), PfeilFired.ArmorPen > Pak43->ArmorPen);
	TestEqual(TEXT("Pfeil fired damage per shot = 250"), PfeilFired.BaseDamage, 250.f, Tolerance);

	// JagdPanther Bollwerk.
	TestTrue(TEXT("Bollwerk sFH is HE only"), Bollwerk->ShellTypes.Num() == 1
	         && Bollwerk->ShellType == EWeaponShellType::Shell_HE);
	TestEqual(TEXT("Bollwerk range = heavy artillery"), Bollwerk->Range,
	          static_cast<float>(RTSFunctionLibrary::RoundToNearestMultipleOf(
		          FMath::RoundToInt(HeavyArtilleryRange), 100)), Tolerance);
	TestEqual(TEXT("Bollwerk reload"), Bollwerk->ReloadSpeed, 16.f, Tolerance);
	TestEqual(TEXT("Bollwerk accuracy"), Bollwerk->Accuracy, 82);
	TestEqual(TEXT("Bollwerk damage as the towed sFH 18"), Bollwerk->BaseDamage, TowedSfh->BaseDamage, Tolerance);
	TestEqual(TEXT("Bollwerk fired pen 61"), GetFiredData(*Bollwerk, EWeaponShellType::Shell_HE).ArmorPen, 61.f,
	          0.05f);

	// Jaguar Flanker: the Bison's long 50mm scaled to 60mm, APCR only, one round, 9 s reload per gun.
	TestTrue(TEXT("Flanker is APCR only"), Flanker->ShellTypes.Num() == 1
	         && Flanker->ShellType == EWeaponShellType::Shell_APCR);
	TestEqual(TEXT("Flanker calibre"), Flanker->WeaponCalibre, 60.f, Tolerance);
	TestEqual(TEXT("Flanker has no TNT"), Flanker->TNTExplosiveGrams, 0.f, Tolerance);
	TestTrue(TEXT("Flanker has no shrapnel"), Flanker->ShrapnelRange == 0.f && Flanker->ShrapnelDamage == 0.f
	         && Flanker->ShrapnelParticles == 0 && Flanker->ShrapnelPen == 0.f);
	TestEqual(TEXT("Flanker magazine"), Flanker->MagCapacity, 1);
	TestEqual(TEXT("Flanker reload 9 s"), Flanker->ReloadSpeed, 9.f, Tolerance);
	TestEqual(TEXT("Flanker range as the Bison"), Flanker->Range, Bison->Range, Tolerance);
	TestEqual(TEXT("Flanker pen = Bison x 60/50"), Flanker->ArmorPen, Bison->ArmorPen * 1.2f, Tolerance);
	TestEqual(TEXT("Flanker fired damage = Bison fired APHE x 60/50"),
	          GetFiredData(*Flanker, EWeaponShellType::Shell_APCR).BaseDamage,
	          GetFiredData(*Bison, EWeaponShellType::Shell_APHE).BaseDamage * 1.2f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourAshmakerKeyTest,
	"RTS.Weapons.VariantBatch.B4.AshmakerDataKey",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourAshmakerKeyTest::RunTest(const FString& Parameters)
{
	using namespace VariantBatchFourWeaponTestsPrivate;

	ACPPGameState* GameState = GetGameStateDefaults();
	const FWeaponData* Ashmaker = FindWeapon(GameState, EWeaponName::Ashmaker11);
	const FWeaponData* Flamm15 = FindWeapon(GameState, EWeaponName::Flamm15);
	if (not TestNotNull(TEXT("Ashmaker11 has its own data entry"), Ashmaker)
		|| not TestNotNull(TEXT("Flamm15 data"), Flamm15))
	{
		return false;
	}
	TestEqual(TEXT("Ashmaker11 entry names Ashmaker11"), Ashmaker->WeaponName, EWeaponName::Ashmaker11);
	TestEqual(TEXT("Ashmaker11 damage"), Ashmaker->BaseDamage, 18.f, Tolerance);
	TestEqual(TEXT("Ashmaker11 calibre"), Ashmaker->WeaponCalibre, 40.f, Tolerance);
	TestEqual(TEXT("Flamm15 entry names Flamm15"), Flamm15->WeaponName, EWeaponName::Flamm15);
	TestEqual(TEXT("Flamm15 damage unchanged"), Flamm15->BaseDamage, 25.f, Tolerance);
	TestEqual(TEXT("Flamm15 calibre unchanged"), Flamm15->WeaponCalibre, 50.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourBehaviourWeaponFilterTest,
	"RTS.Weapons.VariantBatch.B4.BehaviourWeaponNameFilter",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourBehaviourWeaponFilterTest::RunTest(const FString& Parameters)
{
	const TArray<FName> NoFilter;
	TestTrue(TEXT("Empty filter applies to the laser"),
	         UBehaviourWeapon::GetIsWeaponNameInFilter(NoFilter, EWeaponName::Laser_60MM_PzIIIM));
	TestTrue(TEXT("Empty filter applies to the coax"),
	         UBehaviourWeapon::GetIsWeaponNameInFilter(NoFilter, EWeaponName::Ger_TankMG_7_6MM));

	const TArray<FName> LaserOnly = {FName(TEXT("Laser_60MM_PzIIIM"))};
	TestTrue(TEXT("Laser filter applies to the laser"),
	         UBehaviourWeapon::GetIsWeaponNameInFilter(LaserOnly, EWeaponName::Laser_60MM_PzIIIM));
	TestFalse(TEXT("Laser filter skips the coax"),
	          UBehaviourWeapon::GetIsWeaponNameInFilter(LaserOnly, EWeaponName::Ger_TankMG_7_6MM));
	TestFalse(TEXT("Laser filter skips the Strahlkanone"),
	          UBehaviourWeapon::GetIsWeaponNameInFilter(LaserOnly, EWeaponName::Strahlkanone39));

	const FProperty* FilterProperty = UBehaviourWeapon::StaticClass()->FindPropertyByName(TEXT("WeaponNamesToApplyTo"));
	if (TestNotNull(TEXT("WeaponNamesToApplyTo property"), FilterProperty))
	{
		TestTrue(TEXT("WeaponNamesToApplyTo is editable on Blueprint defaults"),
		         FilterProperty->HasAnyPropertyFlags(CPF_Edit) && FilterProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
		TestTrue(TEXT("WeaponNamesToApplyTo has the weapon name dropdown"), FilterProperty->HasMetaData(TEXT("GetOptions")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourComponentOptionsTest,
	"RTS.Weapons.VariantBatch.B4.ComponentOptions",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourComponentOptionsTest::RunTest(const FString& Parameters)
{
	const FProperty* DisabledProperty = UAttachedRockets::StaticClass()->FindPropertyByName(TEXT("bDisabled"));
	if (TestNotNull(TEXT("UAttachedRockets::bDisabled property"), DisabledProperty))
	{
		TestTrue(TEXT("bDisabled is editable on Blueprint defaults"), DisabledProperty->HasAnyPropertyFlags(CPF_Edit));
	}
	const UAttachedRockets* RocketDefaults = GetDefault<UAttachedRockets>();
	if (TestNotNull(TEXT("UAttachedRockets defaults"), RocketDefaults))
	{
		TestFalse(TEXT("Attached rockets are enabled by default"), RocketDefaults->GetIsDisabled());
	}

	const FProperty* ReplaceProperty =
		FApplyBehaviourAbilitySettings::StaticStruct()->FindPropertyByName(TEXT("bReplaceAbilityInPreferredSlot"));
	TestNotNull(TEXT("bReplaceAbilityInPreferredSlot property"), ReplaceProperty);
	const FApplyBehaviourAbilitySettings DefaultSettings;
	TestFalse(TEXT("Replace-in-slot is opt-in"), DefaultSettings.bReplaceAbilityInPreferredSlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVariantBatchFourEnumKeysTest,
	"RTS.Weapons.VariantBatch.B4.EnumKeys",
	VariantBatchFourWeaponTestsPrivate::TestFlags)

bool FVariantBatchFourEnumKeysTest::RunTest(const FString& Parameters)
{
	// Card keys and saved Blueprint values: a reorder must fail here.
	TestEqual(TEXT("Rocket_200mm_PrecisionMissile (batch 3 last)"),
	          static_cast<int32>(EWeaponName::Rocket_200mm_PrecisionMissile), 212);
	TestEqual(TEXT("LeFH_Radixite_200MM"), static_cast<int32>(EWeaponName::LeFH_Radixite_200MM), 213);
	TestEqual(TEXT("Laser_60MM_PzIIIM"), static_cast<int32>(EWeaponName::Laser_60MM_PzIIIM), 214);
	TestEqual(TEXT("PanzerSchreck_PzIIIM"), static_cast<int32>(EWeaponName::PanzerSchreck_PzIIIM), 215);
	TestEqual(TEXT("Flamm15_HetzerHull"), static_cast<int32>(EWeaponName::Flamm15_HetzerHull), 216);
	TestEqual(TEXT("Mortar_120MM_Hetzer"), static_cast<int32>(EWeaponName::Mortar_120MM_Hetzer), 217);
	TestEqual(TEXT("Pak43_88MM_APCR_Pfeil"), static_cast<int32>(EWeaponName::Pak43_88MM_APCR_Pfeil), 218);
	TestEqual(TEXT("sFH18_150MM_Bollwerk"), static_cast<int32>(EWeaponName::sFH18_150MM_Bollwerk), 219);
	TestEqual(TEXT("L_11_76MM_SMK (previous last)"), static_cast<int32>(EWeaponName::L_11_76MM_SMK), 223);
	TestEqual(TEXT("KwK_Long60MM_APCR_Flanker"), static_cast<int32>(EWeaponName::KwK_Long60MM_APCR_Flanker), 224);

	TestEqual(TEXT("Flak88GroundRole (previous last mode)"), static_cast<int32>(EModeAbilityType::Flak88GroundRole), 2);
	TestEqual(TEXT("WespeCamouflagedNest mode key"), static_cast<int32>(EModeAbilityType::WespeCamouflagedNest), 3);
	TestEqual(TEXT("FlareLaunch (previous last aim)"), static_cast<int32>(EAimAbilityType::FlareLaunch), 5);
	TestEqual(TEXT("JagdPantherBollwerkBarrage aim key"),
	          static_cast<int32>(EAimAbilityType::JagdPantherBollwerkBarrage), 6);
	TestEqual(TEXT("HeArtilleryBarrage aim key (existing, Wespe base)"),
	          static_cast<int32>(EAimAbilityType::HeArtilleryBarrage), 2);
	TestEqual(TEXT("TwinAutocannonOverload icon (previous last)"),
	          static_cast<int32>(EBehaviourIcon::TwinAutocannonOverload), 90);
	TestEqual(TEXT("JagdPantherBollwerkBarrage icon"),
	          static_cast<int32>(EBehaviourIcon::JagdPantherBollwerkBarrage), 91);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
