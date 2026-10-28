// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the BM-13 Katyusha C++: the rail rocket arc solver (the rocket lands on its target for any valid distance,
// height and elevation, impossible arcs are rejected), the Katyusha_M13_132MM weapon entry (stored and fired values),
// the Tank_BM13 unit data, and the FRailRocketWeaponSettings defaults.
// Guarded by WITH_DEV_AUTOMATION_TESTS only (no WITH_EDITOR) so the Development game target compiles it too.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/Projectile/RailRocketArc/RailRocketArcSolver.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponNameConversion.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace RailRocketWeaponTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 PlayerOne = 1;
	constexpr float Tolerance = 0.01f;
	// Landing checks: centimetres of float error over a ~100 m arc.
	constexpr float LandingToleranceCm = 2.0f;
	constexpr float WorldGravity = 980.0f;

	/** The game state CDO: its constructor fills every unit and weapon data map, no world needed. */
	ACPPGameState* GetGameStateDefaults()
	{
		return GetMutableDefault<ACPPGameState>();
	}

	/**
	 * The HE values the rocket is fired with. GLOBAL_GetWeaponDataForShellType is a header-static defined only in
	 * WeaponData.cpp, so outside that translation unit the HE multipliers are applied here the same way.
	 */
	FWeaponData GetFiredData(const FWeaponData& StoredData)
	{
		using namespace DeveloperSettings::GameBalance::Weapons::Projectiles;
		FWeaponData FiredData = StoredData;
		FiredData.TNTExplosiveGrams *= HE_TNTExplosiveGramsMlt;
		FiredData.BaseDamage *= HE_DamageMlt;
		FiredData.ShrapnelRange *= HE_ShrapnelRangeMlt;
		FiredData.ProjectileMovementSpeed *= HE_ProjectileSpeedMlt;
		return FiredData;
	}

	/** Flat distance and height reached after FlightTime on a ballistic arc with the given speed and elevation. */
	FVector2D GetArcPointAtTime(const float LaunchSpeed, const float ElevationDegrees, const float Gravity,
	                            const float FlightTime)
	{
		const float ElevationRadians = FMath::DegreesToRadians(ElevationDegrees);
		const float FlatDistance = LaunchSpeed * FMath::Cos(ElevationRadians) * FlightTime;
		const float Height = LaunchSpeed * FMath::Sin(ElevationRadians) * FlightTime
			- 0.5f * Gravity * FlightTime * FlightTime;
		return FVector2D(FlatDistance, Height);
	}

	/** World position of a free-flight solution after its flight time. */
	FVector GetFreeFlightLanding(const FRailRocketArcRequest& Request, const FRailRocketFreeFlightSolution& Solution)
	{
		const float Time = Solution.FlightTime;
		return Request.RailExitLocation + Solution.LaunchVelocity * Time
			- FVector(0.0f, 0.0f, 0.5f * Solution.EffectiveGravity * Time * Time);
	}

	FRailRocketArcRequest MakeRequest(const float RailElevationDegrees, const FVector& TargetLocation)
	{
		const FRailRocketWeaponSettings Defaults;
		FRailRocketArcRequest Request;
		Request.RailExitLocation = FVector::ZeroVector;
		Request.RailDirection = FRotator(RailElevationDegrees, 0.0f, 0.0f).Vector();
		Request.TargetLocation = TargetLocation;
		Request.WorldGravity = WorldGravity;
		Request.ArcGravityScale = Defaults.ArcGravityScale;
		Request.MinElevationDegrees = Defaults.MinArcElevationDegrees;
		Request.MaxElevationDegrees = Defaults.MaxArcElevationDegrees;
		Request.MinLaunchSpeed = 0.0f;
		Request.MaxLaunchSpeed = 0.0f;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRailRocketFixedElevationSolveTest,
	"RTS.Weapons.RailRocket.FixedElevationSolve",
	RailRocketWeaponTestsPrivate::TestFlags)

bool FRailRocketFixedElevationSolveTest::RunTest(const FString& Parameters)
{
	using namespace RailRocketWeaponTestsPrivate;
	const TArray<float> Distances = {1000.f, 4000.f, 7600.f};
	const TArray<float> HeightDeltas = {-300.f, 0.f, 400.f};
	const TArray<float> Elevations = {10.f, 15.f, 30.f, 45.f, 60.f};
	const float Gravity = WorldGravity * 1.25f;

	for (const float Distance : Distances)
	{
		for (const float HeightDelta : HeightDeltas)
		{
			for (const float Elevation : Elevations)
			{
				const FString Case = FString::Printf(TEXT("d=%.0f dz=%.0f theta=%.0f"), Distance, HeightDelta, Elevation);
				const bool bArcExists = Distance * FMath::Tan(FMath::DegreesToRadians(Elevation)) - HeightDelta >= 1.0f;
				float LaunchSpeed = 0.f;
				float FlightTime = 0.f;
				const bool bSolved = FRailRocketArcSolver::SolveFixedElevationLaunch(
					Distance, HeightDelta, Elevation, Gravity, LaunchSpeed, FlightTime);
				TestTrue(FString::Printf(TEXT("Solvable: %s"), *Case), bSolved == bArcExists);
				if (not bSolved)
				{
					continue;
				}
				const FVector2D Landing = GetArcPointAtTime(LaunchSpeed, Elevation, Gravity, FlightTime);
				TestEqual(FString::Printf(TEXT("Lands at distance: %s"), *Case), static_cast<float>(Landing.X), Distance,
				          LandingToleranceCm);
				TestEqual(FString::Printf(TEXT("Lands at height: %s"), *Case), static_cast<float>(Landing.Y), HeightDelta,
				          LandingToleranceCm);
			}
		}
	}

	float LaunchSpeed = 0.f;
	float FlightTime = 0.f;
	TestFalse(TEXT("Rejects zero distance"),
	          FRailRocketArcSolver::SolveFixedElevationLaunch(0.f, 0.f, 30.f, Gravity, LaunchSpeed, FlightTime));
	TestFalse(TEXT("Rejects zero gravity"),
	          FRailRocketArcSolver::SolveFixedElevationLaunch(4000.f, 0.f, 30.f, 0.f, LaunchSpeed, FlightTime));
	TestFalse(TEXT("Rejects a target above the elevation line"),
	          FRailRocketArcSolver::SolveFixedElevationLaunch(1000.f, 500.f, 10.f, Gravity, LaunchSpeed, FlightTime));
	TestFalse(TEXT("Rejects a vertical launch"),
	          FRailRocketArcSolver::SolveFixedElevationLaunch(4000.f, 0.f, 89.9f, Gravity, LaunchSpeed, FlightTime));

	float FallTime = 0.f;
	TestTrue(TEXT("Fall time exists"), FRailRocketArcSolver::GetTimeToFallToHeight(1000.f, -200.f, Gravity, FallTime));
	TestEqual(TEXT("Fall time reaches the height"),
	          1000.f * FallTime - 0.5f * Gravity * FallTime * FallTime, -200.f, LandingToleranceCm);
	TestFalse(TEXT("Cannot reach a height above the apex"),
	          FRailRocketArcSolver::GetTimeToFallToHeight(100.f, 5000.f, Gravity, FallTime));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRailRocketFreeFlightSolveTest,
	"RTS.Weapons.RailRocket.FreeFlightSolve",
	RailRocketWeaponTestsPrivate::TestFlags)

bool FRailRocketFreeFlightSolveTest::RunTest(const FString& Parameters)
{
	using namespace RailRocketWeaponTestsPrivate;
	constexpr float RailElevation = 30.f;
	const FVector Target(6000.f, 0.f, -150.f);

	// Rail pitched inside the limits, target straight ahead: leaves exactly along the rail and lands on the target.
	{
		const FRailRocketArcRequest Request = MakeRequest(RailElevation, Target);
		FRailRocketFreeFlightSolution Solution;
		if (not TestTrue(TEXT("Rail-aligned arc solves"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution)))
		{
			return false;
		}
		TestEqual(TEXT("Arc leaves along the rail"),
		          static_cast<float>(FVector::DotProduct(Solution.LaunchVelocity.GetSafeNormal(), Request.RailDirection)),
		          1.f, 0.0001f);
		TestEqual(TEXT("Keeps the rail elevation"), Solution.ElevationDegrees, RailElevation, Tolerance);
		TestTrue(TEXT("Lands on the target"),
		         GetFreeFlightLanding(Request, Solution).Equals(Target, LandingToleranceCm));
		TestEqual(TEXT("Gravity scale is the designer arc scale"), Solution.GravityScale, Request.ArcGravityScale,
		          Tolerance);
	}

	// A flat rail is lifted to the minimum elevation; the arc still lands on the target.
	{
		const FRailRocketArcRequest Request = MakeRequest(5.f, Target);
		FRailRocketFreeFlightSolution Solution;
		TestTrue(TEXT("Clamped arc solves"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution));
		TestEqual(TEXT("Clamped to the minimum elevation"), Solution.ElevationDegrees, Request.MinElevationDegrees,
		          Tolerance);
		TestTrue(TEXT("Clamped arc lands on the target"),
		         GetFreeFlightLanding(Request, Solution).Equals(Target, LandingToleranceCm));
	}

	// Elevation jitter is re-solved: a different arc, the same impact point.
	{
		FRailRocketArcRequest Request = MakeRequest(RailElevation, Target);
		Request.ElevationOffsetDegrees = 1.5f;
		FRailRocketFreeFlightSolution Solution;
		TestTrue(TEXT("Jittered arc solves"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution));
		TestTrue(TEXT("Jittered arc lands on the target"),
		         GetFreeFlightLanding(Request, Solution).Equals(Target, LandingToleranceCm));
	}

	// A short shot keeps the rail speed and falls faster on the same path instead of braking at the rail exit.
	{
		FRailRocketArcRequest Request = MakeRequest(RailElevation, FVector(600.f, 0.f, 0.f));
		Request.MinLaunchSpeed = 2500.f;
		FRailRocketFreeFlightSolution Solution;
		TestTrue(TEXT("Short arc solves"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution));
		TestEqual(TEXT("Short arc flies at the rail speed"), static_cast<float>(Solution.LaunchVelocity.Size()),
		          Request.MinLaunchSpeed,
		          Tolerance);
		TestTrue(TEXT("Short arc uses more gravity"), Solution.GravityScale > Request.ArcGravityScale);
		TestTrue(TEXT("Short arc lands on the target"),
		         GetFreeFlightLanding(Request, Solution).Equals(Request.TargetLocation, LandingToleranceCm));
	}

	// Pitch wobble is not aimed out, but the explosion is timed on the wobbled arc (no air burst high above ground).
	{
		FRailRocketArcRequest Request = MakeRequest(RailElevation, Target);
		Request.ExitPitchWobbleDegrees = 1.0f;
		FRailRocketFreeFlightSolution Solution;
		TestTrue(TEXT("Wobbled arc solves"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution));
		TestEqual(TEXT("Wobbled arc ends at the target height"),
		          static_cast<float>(GetFreeFlightLanding(Request, Solution).Z), static_cast<float>(Target.Z),
		          LandingToleranceCm);
	}

	// Rejections: speed cap exceeded, target above the rail line, target straight above the rail exit.
	{
		FRailRocketArcRequest CappedRequest = MakeRequest(RailElevation, Target);
		CappedRequest.MaxLaunchSpeed = 100.f;
		FRailRocketFreeFlightSolution Solution;
		TestFalse(TEXT("Rejects an arc above the speed cap"),
		          FRailRocketArcSolver::SolveFreeFlight(CappedRequest, Solution));
		const FRailRocketArcRequest HighTargetRequest = MakeRequest(15.f, FVector(1000.f, 0.f, 2000.f));
		TestFalse(TEXT("Rejects a target above the elevation line"),
		          FRailRocketArcSolver::SolveFreeFlight(HighTargetRequest, Solution));
		const FRailRocketArcRequest OverheadRequest = MakeRequest(RailElevation, FVector(0.f, 0.f, -100.f));
		TestFalse(TEXT("Rejects a target with no flat distance"),
		          FRailRocketArcSolver::SolveFreeFlight(OverheadRequest, Solution));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKatyushaWeaponDataTest,
	"RTS.Weapons.RailRocket.KatyushaWeaponData",
	RailRocketWeaponTestsPrivate::TestFlags)

bool FKatyushaWeaponDataTest::RunTest(const FString& Parameters)
{
	using namespace RailRocketWeaponTestsPrivate;
	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	const FWeaponData* Katyusha = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Katyusha_M13_132MM);
	if (not TestNotNull(TEXT("Katyusha weapon data"), Katyusha))
	{
		return false;
	}

	// Card keys and saved Blueprint values: assumes the uncommitted ISU-130 gun stays at 225.
	TestEqual(TEXT("Katyusha enum key"), static_cast<int32>(EWeaponName::Katyusha_M13_132MM), 226);
	TestEqual(TEXT("Display name"), Global_GetWeaponDisplayName(EWeaponName::Katyusha_M13_132MM),
	          FString(TEXT("M-13 Rockets")));
	TestEqual(TEXT("Enum string"), Global_GetWeaponEnumAsString(EWeaponName::Katyusha_M13_132MM),
	          FString(TEXT("Katyusha_M13_132MM")));
	TestEqual(TEXT("Blueprint FName resolves"),
	          static_cast<int32>(Global_GetWeaponNameFromFName(FName(TEXT("Katyusha_M13_132MM")))),
	          static_cast<int32>(EWeaponName::Katyusha_M13_132MM));

	TestEqual(TEXT("Sixteen rockets"), Katyusha->MagCapacity, 16);
	TestTrue(TEXT("Ripple cooldown 0.4-0.5 s"), Katyusha->BaseCooldown >= 0.4f && Katyusha->BaseCooldown <= 0.5f);
	TestTrue(TEXT("Long reload 30-40 s"), Katyusha->ReloadSpeed >= 30.f && Katyusha->ReloadSpeed <= 40.f);
	TestTrue(TEXT("HE only"), Katyusha->ShellType == EWeaponShellType::Shell_HE
	         && Katyusha->ShellTypes.Num() == 1 && Katyusha->ShellTypes[0] == EWeaponShellType::Shell_HE);
	TestEqual(TEXT("Calibre"), Katyusha->WeaponCalibre, 132.f, Tolerance);
	TestTrue(TEXT("Area fire accuracy"), Katyusha->Accuracy < 30);

	using DeveloperSettings::GameBalance::Ranges::MediumArtilleryRange;
	using DeveloperSettings::GameBalance::Ranges::HowitzerArtilleryRange;
	TestTrue(TEXT("Range between the Nebelwerfer and the howitzers"),
	         Katyusha->Range > MediumArtilleryRange && Katyusha->Range < HowitzerArtilleryRange);

	using DeveloperSettings::GameBalance::Weapons::ShrapnelRangePerMM;
	using DeveloperSettings::GameBalance::Weapons::MortarAOEMlt;
	const FWeaponData Fired = GetFiredData(*Katyusha);
	TestEqual(TEXT("Fired TNT"), Fired.TNTExplosiveGrams, 300.f, Tolerance);
	TestEqual(TEXT("Fired shrapnel range: mortar AOE"), Fired.ShrapnelRange,
	          132.f * ShrapnelRangePerMM * MortarAOEMlt, Tolerance);
	const FWeaponData* Mortar120 = GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Mortar_120MM);
	if (TestNotNull(TEXT("Mortar 120 data"), Mortar120))
	{
		const FWeaponData FiredMortar = GetFiredData(*Mortar120);
		TestTrue(TEXT("Each rocket out-hits the 120mm mortar"), Fired.BaseDamage > FiredMortar.BaseDamage);
		TestTrue(TEXT("Each rocket out-splashes the 120mm mortar"), Fired.ShrapnelRange > FiredMortar.ShrapnelRange);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBM13UnitDataTest,
	"RTS.Weapons.RailRocket.BM13UnitData",
	RailRocketWeaponTestsPrivate::TestFlags)

bool FBM13UnitDataTest::RunTest(const FString& Parameters)
{
	using namespace RailRocketWeaponTestsPrivate;
	TestTrue(TEXT("BM-13 is an armored car (wheeled)"), Global_GetIsArmoredCar(ETankSubtype::Tank_BM13));
	TestFalse(TEXT("BM-13 is not a light tank"), Global_GetIsLightTank(ETankSubtype::Tank_BM13));
	TestFalse(TEXT("BM-13 is not a medium tank"), Global_GetIsMediumTank(ETankSubtype::Tank_BM13));
	TestFalse(TEXT("BM-13 is not a heavy tank"), Global_GetIsHeavyTank(ETankSubtype::Tank_BM13));

	ACPPGameState* GameState = GetGameStateDefaults();
	if (not TestNotNull(TEXT("Game state defaults"), GameState))
	{
		return false;
	}
	using namespace DeveloperSettings::GameBalance::UnitCosts::ArmoredCars;
	const FTankData BM13Data = GameState->GetTankDataOfPlayer(PlayerOne, ETankSubtype::Tank_BM13);
	const FTankData ZilData = GameState->GetTankDataOfPlayer(PlayerOne, ETankSubtype::Tank_ZilAmmo);
	TestTrue(TEXT("Sturdier than the Zil supply truck"), BM13Data.MaxHealth > ZilData.MaxHealth);
	TestEqual(TEXT("Radixite cost"), BM13Data.Cost.ResourceCosts.FindRef(ERTSResourceType::Resource_Radixite),
	          BM13_Radixite);
	TestEqual(TEXT("Vehicle parts cost"),
	          BM13Data.Cost.ResourceCosts.FindRef(ERTSResourceType::Resource_VehicleParts), BM13_VehicleParts);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRailRocketSettingsDefaultsTest,
	"RTS.Weapons.RailRocket.SettingsDefaults",
	RailRocketWeaponTestsPrivate::TestFlags)

bool FRailRocketSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace RailRocketWeaponTestsPrivate;
	const FRailRocketWeaponSettings Defaults;
	TestTrue(TEXT("Rail has length"), Defaults.RailLength > 0.f);
	TestTrue(TEXT("Rail speed is a slower fraction"),
	         Defaults.RailExitSpeedMultiplier > 0.f && Defaults.RailExitSpeedMultiplier <= 1.f);
	TestTrue(TEXT("Elevation limits ordered"), Defaults.MinArcElevationDegrees < Defaults.MaxArcElevationDegrees);
	TestTrue(TEXT("Arc gravity positive"), Defaults.ArcGravityScale > 0.f);
	TestTrue(TEXT("Speed cap above the projectile speed"), Defaults.MaxArcSpeedMultiplier > 1.f);
	TestTrue(TEXT("Small elevation jitter"), Defaults.ElevationJitterDegrees >= 0.f
	         && Defaults.ElevationJitterDegrees <= 5.f);
	TestTrue(TEXT("Small exit wobble"), Defaults.RailExitWobbleDegrees >= 0.f
	         && Defaults.RailExitWobbleDegrees <= 3.f);
	TestTrue(TEXT("Scatter not negative"), Defaults.TargetScatterRadius >= 0.f);

	// The Katyusha at full range off a 30 degree rail: stays under the cap and lands within a few seconds.
	ACPPGameState* GameState = GetGameStateDefaults();
	const FWeaponData* Katyusha = GameState
		                              ? GameState->GetWeaponDataOfPlayer(PlayerOne, EWeaponName::Katyusha_M13_132MM)
		                              : nullptr;
	if (not TestNotNull(TEXT("Katyusha weapon data"), Katyusha))
	{
		return false;
	}
	constexpr float MaxFullRangeFlightSeconds = 6.f;
	const FWeaponData Fired = GetFiredData(*Katyusha);
	FRailRocketArcRequest Request = MakeRequest(30.f, FVector(Fired.Range, 0.f, 0.f));
	Request.MinLaunchSpeed = Fired.ProjectileMovementSpeed * Defaults.RailExitSpeedMultiplier;
	Request.MaxLaunchSpeed = Fired.ProjectileMovementSpeed * Defaults.MaxArcSpeedMultiplier;
	FRailRocketFreeFlightSolution Solution;
	TestTrue(TEXT("Full range arc solves under the cap"), FRailRocketArcSolver::SolveFreeFlight(Request, Solution));
	TestTrue(TEXT("Full range flight is short"), Solution.FlightTime > 0.f
	         && Solution.FlightTime < MaxFullRangeFlightSeconds);

	// Blueprint / Python fill these by name.
	const TArray<FName> PropertyNames = {
		TEXT("RocketSettings"), TEXT("RailLength"), TEXT("RailExitSpeedMultiplier"), TEXT("MinArcElevationDegrees"),
		TEXT("MaxArcElevationDegrees"), TEXT("ArcGravityScale"), TEXT("MaxArcSpeedMultiplier"),
		TEXT("ElevationJitterDegrees"), TEXT("TargetScatterRadius"), TEXT("RailExitWobbleDegrees")
	};
	for (const FName& PropertyName : PropertyNames)
	{
		TestNotNull(*FString::Printf(TEXT("Property %s"), *PropertyName.ToString()),
		            FRailRocketWeaponSettings::StaticStruct()->FindPropertyByName(PropertyName));
	}
	TestNotNull(TEXT("Init struct carries the settings"),
	            FInitWeaponStateRailRocket::StaticStruct()->FindPropertyByName(TEXT("RailRocketSettings")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
