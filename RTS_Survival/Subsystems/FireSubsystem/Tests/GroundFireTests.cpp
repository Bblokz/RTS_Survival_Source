// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the lingering ground fire of incendiary projectiles: the weapon data that switches it on, and the fire
// manager's pooled, merged and capped burning areas. Editor-only and dev-test-only: UBT defines
// WITH_DEV_AUTOMATION_TESTS=0 for Shipping and Test and WITH_EDITOR=0 for every game target, so this translation unit
// is empty in the shipping binary.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FirePoolSettings/RTSFirePoolSettings.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireManager/RTSFireManager.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

/** @brief Drives the fire manager's ground fires with explicit times, so no world has to tick. */
struct FRTSGroundFireTestAccess
{
	static FRTSGroundFireRequestResult Request(ARTSFireManager& Manager, const FRTSGroundFireSettings& Settings,
	                                           const FVector& Location, const int32 OwningPlayer,
	                                           const float TimeSeconds)
	{
		return Manager.RequestGroundFireAtTime(Settings, Location, OwningPlayer, TimeSeconds);
	}

	static void Tick(ARTSFireManager& Manager, const float TimeSeconds)
	{
		Manager.TickGroundFires(TimeSeconds);
	}

	static const FRTSGroundFireArea* FindArea(const ARTSFireManager& Manager, const int32 AreaHandle)
	{
		return Manager.M_GroundFireAreas.FindByPredicate([AreaHandle](const FRTSGroundFireArea& Area)
		{
			return Area.AreaHandle == AreaHandle;
		});
	}
};

namespace GroundFireTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr int32 TestPoolSize = 3;
	constexpr float Tolerance = 0.01f;
	const TCHAR* const GroundFireSystemPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/Fire/RTS_GroundFire_0.RTS_GroundFire_0");

	/** The game state CDO: its constructor fills every weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}

	FRTSGroundFireSettings MakeSettings(const float RadiusCm = 450.f, const float DurationSeconds = 9.f)
	{
		FRTSGroundFireSettings Settings;
		Settings.RadiusCm = RadiusCm;
		Settings.DurationSeconds = DurationSeconds;
		Settings.DamagePerSecond = 25.f;
		return Settings;
	}

	/** @brief A bare world with one fire manager whose only pool is a small GroundFire pool of the real asset. */
	struct FScopedFireManager
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ARTSFireManager> Manager;
		TStrongObjectPtr<URTSFirePoolSettings> Settings;

		~FScopedFireManager()
		{
			UWorld* TestWorld = World.Get();
			if (IsValid(TestWorld))
			{
				GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool Initialize(const int32 MaxActiveGroundFires)
		{
			// Initialise as preview so no gameplay world subsystem starts; the manager is spawned by hand.
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (not World.IsValid())
			{
				return false;
			}
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());

			Settings.Reset(NewObject<URTSFirePoolSettings>(GetTransientPackage()));
			FFireSettingsForType GroundFirePool;
			GroundFirePool.FireSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(GroundFireSystemPath));
			GroundFirePool.PoolSize = TestPoolSize;
			Settings->FireSettingsByType.Reset();
			Settings->FireSettingsByType.Add(ERTSFireType::GroundFire, GroundFirePool);
			Settings->MaxActiveGroundFires = MaxActiveGroundFires;
			Settings->GroundFireMergeDistanceFraction = 0.75f;

			Manager = World->SpawnActor<ARTSFireManager>();
			if (not Manager.IsValid())
			{
				return false;
			}
			Manager->InitFireManager(Settings.Get());
			return Manager->GetFreeFireCount(ERTSFireType::GroundFire) == TestPoolSize;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFireIncendiaryWeaponTest, "RTS.Weapons.GroundFire.IncendiaryWeaponHasSettings",
	GroundFireTestsPrivate::TestFlags)

bool FGroundFireIncendiaryWeaponTest::RunTest(const FString& Parameters)
{
	using namespace GroundFireTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	for (const int32 Player : {PlayerOne, EnemyPlayer})
	{
		const FWeaponData* Data = GameState->GetWeaponDataOfPlayer(Player, EWeaponName::Rocket_320mm_Incendiary);
		if (not TestNotNull(FString::Printf(TEXT("Player %d has incendiary rocket data"), Player), Data))
		{
			return false;
		}
		const FRTSGroundFireSettings& GroundFire = Data->GroundFire;
		TestTrue(FString::Printf(TEXT("Player %d incendiary ground fire is enabled"), Player), GroundFire.GetIsEnabled());
		TestTrue(TEXT("Radius 4-5 m"), GroundFire.RadiusCm >= 400.f && GroundFire.RadiusCm <= 500.f);
		TestTrue(TEXT("Burns 8-10 s"), GroundFire.DurationSeconds >= 8.f && GroundFire.DurationSeconds <= 10.f);
		TestTrue(TEXT("Full burn deals less than a direct hit"),
		         GroundFire.DamagePerSecond * GroundFire.DurationSeconds < Data->BaseDamage);
		TestTrue(TEXT("Burning patch is inside the rocket's splash"), GroundFire.RadiusCm <= Data->ShrapnelRange);
	}

	// Clones and behaviour upgrades go through CopyWeaponDataValues; the ground fire must survive them.
	const FWeaponData* Source = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Rocket_320mm_Incendiary);
	FWeaponData Copy;
	Copy.CopyWeaponDataValues(Source);
	TestTrue(TEXT("CopyWeaponDataValues keeps the ground fire"), Copy.GroundFire.GetIsEnabled());
	TestEqual(TEXT("CopyWeaponDataValues keeps the radius"), Copy.GroundFire.RadiusCm, Source->GroundFire.RadiusCm,
	          Tolerance);
	TestEqual(TEXT("CopyWeaponDataValues keeps the damage"), Copy.GroundFire.DamagePerSecond,
	          Source->GroundFire.DamagePerSecond, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFireOtherWeaponsTest, "RTS.Weapons.GroundFire.OtherWeaponsHaveNone",
	GroundFireTestsPrivate::TestFlags)

bool FGroundFireOtherWeaponsTest::RunTest(const FString& Parameters)
{
	using namespace GroundFireTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	if (not TestNotNull(TEXT("EWeaponName is reflected"), WeaponEnum))
	{
		return false;
	}

	int32 CheckedWeapons = 0;
	TArray<FString> WeaponsWithGroundFire;
	// NumEnums includes the generated _MAX entry, which has no weapon data and is skipped by the null check.
	for (int32 EnumIndex = 0; EnumIndex < WeaponEnum->NumEnums(); ++EnumIndex)
	{
		const EWeaponName WeaponName = static_cast<EWeaponName>(WeaponEnum->GetValueByIndex(EnumIndex));
		for (const int32 Player : {PlayerOne, EnemyPlayer})
		{
			const FWeaponData* Data = GameState->GetWeaponDataOfPlayer(Player, WeaponName);
			if (Data == nullptr)
			{
				continue;
			}
			++CheckedWeapons;
			if (Data->GroundFire.GetIsEnabled() && WeaponName != EWeaponName::Rocket_320mm_Incendiary)
			{
				WeaponsWithGroundFire.Add(WeaponEnum->GetNameStringByIndex(EnumIndex));
			}
		}
	}

	TestTrue(TEXT("Weapon data was checked"), CheckedWeapons > 100);
	TestEqual(TEXT("Only the incendiary rocket leaves a ground fire: ") + FString::Join(WeaponsWithGroundFire, TEXT(", ")),
	          WeaponsWithGroundFire.Num(), 0);
	TestFalse(TEXT("Default FRTSGroundFireSettings is off"), FRTSGroundFireSettings().GetIsEnabled());
	TestFalse(TEXT("Default FWeaponData has no ground fire"), FWeaponData().GroundFire.GetIsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFireConfigTest, "RTS.Weapons.GroundFire.PoolConfigured",
	GroundFireTestsPrivate::TestFlags)

bool FGroundFireConfigTest::RunTest(const FString& Parameters)
{
	const URTSFirePoolSettings* Settings = GetDefault<URTSFirePoolSettings>();
	const FFireSettingsForType* GroundFirePool = Settings->FireSettingsByType.Find(ERTSFireType::GroundFire);
	if (not TestNotNull(TEXT("DefaultGame.ini has a GroundFire pool"), GroundFirePool))
	{
		return false;
	}
	TestTrue(TEXT("GroundFire pool holds a fire for every allowed ground fire"),
	         GroundFirePool->PoolSize >= Settings->MaxActiveGroundFires);
	TestNotNull(TEXT("GroundFire Niagara system loads"), GroundFirePool->FireSystem.LoadSynchronous());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFireLifecycleTest, "RTS.Weapons.GroundFire.PooledAreaLifecycle",
	GroundFireTestsPrivate::TestFlags)

bool FGroundFireLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GroundFireTestsPrivate;
	FScopedFireManager Scope;
	if (not TestTrue(TEXT("Fire manager with a GroundFire pool"), Scope.Initialize(TestPoolSize)))
	{
		return false;
	}
	ARTSFireManager& Manager = *Scope.Manager;
	const FRTSGroundFireSettings Settings = MakeSettings();

	FRTSGroundFireSettings Disabled = Settings;
	Disabled.DamagePerSecond = 0.f;
	TestEqual(TEXT("A disabled ground fire starts nothing"),
	          FRTSGroundFireTestAccess::Request(Manager, Disabled, FVector::ZeroVector, PlayerOne, 0.f).AreaHandle,
	          static_cast<int32>(INDEX_NONE));

	const FRTSGroundFireRequestResult Result =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(100.f, 200.f, 0.f), PlayerOne, 0.f);
	TestNotEqual(TEXT("The impact starts a ground fire"), Result.AreaHandle, static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("A first fire is not a merge"), Result.bMergedIntoExistingArea);
	TestEqual(TEXT("Exactly one area burns"), Manager.GetActiveGroundFireCount(), 1);
	TestEqual(TEXT("Exactly one pooled GroundFire is active"), Manager.GetActiveFireCount(ERTSFireType::GroundFire), 1);
	TestEqual(TEXT("It came from the pool"), Manager.GetFreeFireCount(ERTSFireType::GroundFire), TestPoolSize - 1);

	FRTSGroundFireTestAccess::Tick(Manager, Settings.DurationSeconds * 0.5f);
	TestEqual(TEXT("Still burning halfway"), Manager.GetActiveGroundFireCount(), 1);
	const FRTSGroundFireArea* Area = FRTSGroundFireTestAccess::FindArea(Manager, Result.AreaHandle);
	if (TestNotNull(TEXT("Area is findable"), Area))
	{
		TestEqual(TEXT("Damage is owed from the last tick on"), Area->LastDamageTimeSeconds,
		          Settings.DurationSeconds * 0.5f, Tolerance);
	}

	FRTSGroundFireTestAccess::Tick(Manager, Settings.DurationSeconds + 0.1f);
	TestEqual(TEXT("Burnt out after its lifetime"), Manager.GetActiveGroundFireCount(), 0);
	TestEqual(TEXT("Its flames are stopped"), Manager.GetActiveFireCount(ERTSFireType::GroundFire), 0);
	TestEqual(TEXT("Its flames are back in the pool"), Manager.GetFreeFireCount(ERTSFireType::GroundFire),
	          TestPoolSize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundFireMergeAndCapTest, "RTS.Weapons.GroundFire.MergeAndCap",
	GroundFireTestsPrivate::TestFlags)

bool FGroundFireMergeAndCapTest::RunTest(const FString& Parameters)
{
	using namespace GroundFireTestsPrivate;
	constexpr int32 MaxFires = 2;
	FScopedFireManager Scope;
	if (not TestTrue(TEXT("Fire manager with a GroundFire pool"), Scope.Initialize(MaxFires)))
	{
		return false;
	}
	ARTSFireManager& Manager = *Scope.Manager;
	const FRTSGroundFireSettings Settings = MakeSettings();

	// One salvo: three rockets 0.8 s apart, within 2 m of each other.
	const FRTSGroundFireRequestResult First =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(0.f, 0.f, 0.f), PlayerOne, 0.f);
	const FRTSGroundFireRequestResult Second =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(150.f, 80.f, 0.f), PlayerOne, 0.8f);
	const FRTSGroundFireRequestResult Third =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(-120.f, 140.f, 30.f), PlayerOne, 1.6f);
	TestTrue(TEXT("Second rocket merges"), Second.bMergedIntoExistingArea);
	TestTrue(TEXT("Third rocket merges"), Third.bMergedIntoExistingArea);
	TestEqual(TEXT("Merged into the first area"), Third.AreaHandle, First.AreaHandle);
	TestEqual(TEXT("One salvo, one burning area"), Manager.GetActiveGroundFireCount(), 1);
	TestEqual(TEXT("One salvo, one pooled flame"), Manager.GetActiveFireCount(ERTSFireType::GroundFire), 1);
	const FRTSGroundFireArea* Area = FRTSGroundFireTestAccess::FindArea(Manager, First.AreaHandle);
	if (TestNotNull(TEXT("Merged area is findable"), Area))
	{
		TestEqual(TEXT("Merging refreshes the burn time"), Area->EndTimeSeconds, 1.6f + Settings.DurationSeconds,
		          Tolerance);
		TestEqual(TEXT("Merging never stacks damage"), Area->Settings.DamagePerSecond, Settings.DamagePerSecond,
		          Tolerance);
	}

	// The other side's fire on the same spot is its own area (it damages the other units).
	const FRTSGroundFireRequestResult EnemyFire =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(0.f, 0.f, 0.f), EnemyPlayer, 2.f);
	TestFalse(TEXT("Fires of different players do not merge"), EnemyFire.bMergedIntoExistingArea);
	TestEqual(TEXT("Two areas"), Manager.GetActiveGroundFireCount(), 2);

	// A far-away fire at the cap replaces the area closest to burning out (the player's, ending at 10.6 s).
	const FRTSGroundFireRequestResult FarFire =
		FRTSGroundFireTestAccess::Request(Manager, Settings, FVector(5000.f, 0.f, 0.f), PlayerOne, 3.f);
	TestFalse(TEXT("Far fire does not merge"), FarFire.bMergedIntoExistingArea);
	TestTrue(TEXT("At the cap the oldest fire is replaced"), FarFire.bReplacedOldestArea);
	TestEqual(TEXT("Never more areas than the cap"), Manager.GetActiveGroundFireCount(), MaxFires);
	TestEqual(TEXT("Never more flames than the cap"), Manager.GetActiveFireCount(ERTSFireType::GroundFire), MaxFires);
	TestNull(TEXT("The replaced area is gone"), FRTSGroundFireTestAccess::FindArea(Manager, First.AreaHandle));
	TestNotNull(TEXT("The enemy fire still burns"), FRTSGroundFireTestAccess::FindArea(Manager, EnemyFire.AreaHandle));

	FRTSGroundFireTestAccess::Tick(Manager, 3.f + Settings.DurationSeconds + 0.1f);
	TestEqual(TEXT("All burnt out"), Manager.GetActiveGroundFireCount(), 0);
	TestEqual(TEXT("Pool fully returned"), Manager.GetFreeFireCount(ERTSFireType::GroundFire), TestPoolSize);
	return true;
}

#endif
