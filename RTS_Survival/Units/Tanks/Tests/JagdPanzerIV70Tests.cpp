// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the Jagdpanzer IV/70 (Tank_PzIV_70): classification, balance data, its Pak 42 main gun and its place in the
// live training layout. Editor-only and dev-test-only: UBT defines WITH_DEV_AUTOMATION_TESTS=0 for Shipping and Test
// and WITH_EDITOR=0 for every game target, so this translation unit is empty in the shipping binary.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutDataAsset.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/UnitData/VehicleData.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "Engine/DataTable.h"

namespace JagdPanzerIV70TestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr ETankSubtype JagdPanzer = ETankSubtype::Tank_PzIV_70;
	constexpr ETankSubtype StuG = ETankSubtype::Tank_Stug;
	constexpr EWeaponName MainGun = EWeaponName::Pak42_L70_75MM;
	// The Pak 42 is the Panther's KwK 42 in a casemate: same ballistics, its own reload and accuracy.
	constexpr EWeaponName PantherGun = EWeaponName::KwK42_75MM;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float ExpectedMaxSpeedKmh = 24.f;
	constexpr float ExpectedReverseSpeedKmh = 12.f;
	constexpr float ExpectedPak42ReloadSeconds = 7.8f;
	constexpr int32 ExpectedPak42Accuracy = 95;
	constexpr int32 ExperienceWorthMultiple = 5;
	constexpr float Tolerance = 0.001f;
	const TCHAR* const ExpectedWeaponEnumString = TEXT("Pak42_L70_75MM");
	const TCHAR* const ExpectedWeaponDisplayName = TEXT("Pak 42 L/70");

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}

	int32 GetResourceCost(const FTankData& TankData, const ERTSResourceType ResourceType)
	{
		const int32* Cost = TankData.Cost.ResourceCosts.Find(ResourceType);
		return Cost ? *Cost : 0;
	}

	TArray<EAbilityID> GetAbilityIds(const FTankData& TankData)
	{
		TArray<EAbilityID> AbilityIds;
		for (const FUnitAbilityEntry& Entry : TankData.Abilities)
		{
			AbilityIds.Add(Entry.AbilityId);
		}
		return AbilityIds;
	}

	bool GetBuildingHasTankSlot(const FTrainingLayoutBuilding& Building, const ETankSubtype TankSubtype)
	{
		for (const FTrainingLayoutSubSection& SubSection : Building.SubSections)
		{
			const bool bHasSlot = SubSection.Slots.ContainsByPredicate([TankSubtype](const FTrainingLayoutSlot& Slot)
			{
				return Slot.UnitType == EAllUnitType::UNType_Tank && Slot.TankSubtype == TankSubtype;
			});
			if (bHasSlot)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJagdPanzerIV70ClassificationTest,
	"RTS.Units.JagdPanzerIV70.Classification",
	JagdPanzerIV70TestsPrivate::TestFlags)

bool FJagdPanzerIV70ClassificationTest::RunTest(const FString& Parameters)
{
	using namespace JagdPanzerIV70TestsPrivate;

	TestTrue(TEXT("Jagdpanzer IV/70 is a medium tank"), Global_GetIsMediumTank(JagdPanzer));
	TestTrue(TEXT("Jagdpanzer IV/70 is a tank destroyer"), Global_GetIsTankDestroyer(JagdPanzer));
	TestFalse(TEXT("Jagdpanzer IV/70 is not a heavy tank"), Global_GetIsHeavyTank(JagdPanzer));
	TestFalse(TEXT("Jagdpanzer IV/70 has a display name"), Global_GetTankDisplayName(JagdPanzer).IsEmpty());
	TestFalse(TEXT("Jagdpanzer IV/70 has a training option name"),
	          UTrainingOptionLibrary::GetTrainingOptionName(EAllUnitType::UNType_Tank,
	                                                        static_cast<uint8>(JagdPanzer)).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJagdPanzerIV70TankDataTest,
	"RTS.Units.JagdPanzerIV70.TankData",
	JagdPanzerIV70TestsPrivate::TestFlags)

bool FJagdPanzerIV70TankDataTest::RunTest(const FString& Parameters)
{
	using namespace JagdPanzerIV70TestsPrivate;
	using namespace DeveloperSettings::GameBalance;

	const ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}

	const FTankData PlayerData = GameState->GetTankDataOfPlayer(PlayerOne, JagdPanzer);
	const FTankData EnemyData = GameState->GetTankDataOfPlayer(EnemyPlayer, JagdPanzer);
	const FTankData StuGData = GameState->GetTankDataOfPlayer(PlayerOne, StuG);

	TestEqual(TEXT("Max health is the medium tank base"), PlayerData.MaxHealth, UnitHealth::MediumTankHealthBase, Tolerance);
	TestTrue(TEXT("Thicker front than the StuG: more health"), PlayerData.MaxHealth > StuGData.MaxHealth);
	TestEqual(TEXT("Enemy copy has the same health"), EnemyData.MaxHealth, PlayerData.MaxHealth, Tolerance);
	TestEqual(TEXT("Max speed km/h"), PlayerData.VehicleMaxSpeedKmh, ExpectedMaxSpeedKmh, Tolerance);
	TestEqual(TEXT("Reverse speed km/h"), PlayerData.VehicleReverseSpeedKmh, ExpectedReverseSpeedKmh, Tolerance);
	TestTrue(TEXT("Slower than the StuG"), PlayerData.VehicleMaxSpeedKmh < StuGData.VehicleMaxSpeedKmh);
	TestTrue(TEXT("Has a vision radius"), PlayerData.VisionRadius > 0.f);

	TestEqual(TEXT("Radixite cost"), GetResourceCost(PlayerData, ERTSResourceType::Resource_Radixite),
	          UnitCosts::MediumTanks::PzIV70_Radixite);
	TestEqual(TEXT("Vehicle parts cost"), GetResourceCost(PlayerData, ERTSResourceType::Resource_VehicleParts),
	          UnitCosts::MediumTanks::PzIV70_VehicleParts);
	TestTrue(TEXT("Costs more radixite than the StuG"),
	         GetResourceCost(PlayerData, ERTSResourceType::Resource_Radixite)
	         > GetResourceCost(StuGData, ERTSResourceType::Resource_Radixite));

	TestTrue(TEXT("Has abilities"), PlayerData.Abilities.Num() > 0);
	TestEqual(TEXT("Same tank destroyer abilities as the StuG"), GetAbilityIds(PlayerData), GetAbilityIds(StuGData));
	TestTrue(TEXT("Has experience levels"), PlayerData.ExperienceLevels.Num() > 0);
	TestTrue(TEXT("Worth more experience than the StuG"), PlayerData.ExperienceWorth > StuGData.ExperienceWorth);
	TestEqual(TEXT("Experience worth is a multiple of 5"),
	          FMath::RoundToInt(PlayerData.ExperienceWorth) % ExperienceWorthMultiple, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJagdPanzerIV70MainGunTest,
	"RTS.Units.JagdPanzerIV70.MainGun",
	JagdPanzerIV70TestsPrivate::TestFlags)

bool FJagdPanzerIV70MainGunTest::RunTest(const FString& Parameters)
{
	using namespace JagdPanzerIV70TestsPrivate;

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}

	const FWeaponData* Pak42 = GameState->GetWeaponDataOfPlayer(PlayerOne, MainGun);
	const FWeaponData* EnemyPak42 = GameState->GetWeaponDataOfPlayer(EnemyPlayer, MainGun);
	const FWeaponData* KwK42 = GameState->GetWeaponDataOfPlayer(PlayerOne, PantherGun);
	if (not TestNotNull(TEXT("Player Pak 42 data"), Pak42)
		|| not TestNotNull(TEXT("Enemy Pak 42 data"), EnemyPak42)
		|| not TestNotNull(TEXT("Panther KwK 42 data"), KwK42))
	{
		return false;
	}

	TestEqual(TEXT("Data names its own weapon"), Pak42->WeaponName, MainGun);
	TestEqual(TEXT("Enum string"), Global_GetWeaponEnumAsString(MainGun), FString(ExpectedWeaponEnumString));
	TestEqual(TEXT("Display name"), Global_GetWeaponDisplayName(MainGun), FString(ExpectedWeaponDisplayName));

	TestEqual(TEXT("Reload seconds"), Pak42->ReloadSpeed, ExpectedPak42ReloadSeconds, Tolerance);
	TestEqual(TEXT("Accuracy"), Pak42->Accuracy, ExpectedPak42Accuracy);
	TestTrue(TEXT("Loads slower than the Panther's KwK 42"), Pak42->ReloadSpeed > KwK42->ReloadSpeed);

	TestEqual(TEXT("Calibre as the KwK 42"), Pak42->WeaponCalibre, KwK42->WeaponCalibre, Tolerance);
	TestEqual(TEXT("Armor pen as the KwK 42"), Pak42->ArmorPen, KwK42->ArmorPen, Tolerance);
	TestEqual(TEXT("Armor pen at max range as the KwK 42"), Pak42->ArmorPenMaxRange, KwK42->ArmorPenMaxRange, Tolerance);
	TestEqual(TEXT("Damage as the KwK 42"), Pak42->BaseDamage, KwK42->BaseDamage, Tolerance);
	TestEqual(TEXT("Range as the KwK 42"), Pak42->Range, KwK42->Range, Tolerance);
	TestEqual(TEXT("Projectile speed as the KwK 42"), Pak42->ProjectileMovementSpeed, KwK42->ProjectileMovementSpeed, Tolerance);
	TestEqual(TEXT("Enemy copy reloads the same"), EnemyPak42->ReloadSpeed, Pak42->ReloadSpeed, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJagdPanzerIV70TrainingLayoutTest,
	"RTS.Units.JagdPanzerIV70.TrainingLayout",
	JagdPanzerIV70TestsPrivate::TestFlags)

bool FJagdPanzerIV70TrainingLayoutTest::RunTest(const FString& Parameters)
{
	using namespace JagdPanzerIV70TestsPrivate;

	// The asset picked in Project Settings > Training Layout, not the class defaults: a saved asset keeps its own
	// slots, so a unit added to the code defaults after the asset was saved never reaches the game.
	const UTrainingLayoutDataAsset* TrainingLayout = UTrainingLayoutSettings::Get()->LoadTrainingLayout();
	if (not TestNotNull(TEXT("Training layout asset"), TrainingLayout))
	{
		return false;
	}

	const FTrainingLayoutBuilding* MediumFactory = TrainingLayout->FindBuildingLayout(
		ERTSFaction::GerBreakthroughDoctrine, ENomadicSubtype::Nomadic_GerMedTankFactory);
	if (not TestNotNull(TEXT("Breakthrough medium tank factory layout"), MediumFactory))
	{
		return false;
	}
	TestTrue(TEXT("Breakthrough medium tank factory trains the Jagdpanzer IV/70"),
	         GetBuildingHasTankSlot(*MediumFactory, JagdPanzer));

	const UDataTable* IconTable = TrainingLayout->TrainingOptionIconTable.LoadSynchronous();
	if (not TestNotNull(TEXT("Training option icon table"), IconTable))
	{
		return false;
	}
	const FName RowName(*UTrainingOptionLibrary::GetTrainingOptionName(EAllUnitType::UNType_Tank,
	                                                                     static_cast<uint8>(JagdPanzer)));
	TestNotNull(*FString::Printf(TEXT("Icon table row '%s'"), *RowName.ToString()), IconTable->FindRowUnchecked(RowName));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
