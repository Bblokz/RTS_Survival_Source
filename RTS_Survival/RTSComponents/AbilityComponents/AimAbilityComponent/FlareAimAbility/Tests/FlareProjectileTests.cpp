// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if WITH_DEV_AUTOMATION_TESTS && RTS_WITH_MAP_TESTS

#include "Misc/AutomationTest.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/Tests/FlareAbilityTestAccess.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"

namespace FlareProjectileTests
{
	constexpr float Tolerance = 1.f;
	constexpr float TickSeconds = 1.f / 30.f;
	const FVector Launch(0.f, 0.f, 230.f);
	const FVector Ground(4000.f, 1500.f, 50.f);

	/** @brief A bare game world for spawning flares; the flares are ticked by hand. */
	struct FScopedFlareWorld
	{
		TWeakObjectPtr<UWorld> World;

		~FScopedFlareWorld()
		{
			UWorld* TestWorld = World.Get();
			if (IsValid(TestWorld))
			{
				GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool Initialize()
		{
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (not World.IsValid())
			{
				return false;
			}
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			return true;
		}

		AFlareProjectile* LaunchFlare(const FFlareProjectileSettings& Settings, const uint8 OwningPlayer) const
		{
			AFlareProjectile* Flare = World->SpawnActor<AFlareProjectile>(Launch, FRotator::ZeroRotator);
			if (IsValid(Flare))
			{
				Flare->LaunchFlare(Launch, FVector(0.3f, 0.f, 1.f), Ground, Settings, OwningPlayer);
			}
			return Flare;
		}
	};

	FFlareProjectileSettings MakeSettings()
	{
		FFlareProjectileSettings Settings;
		Settings.ApexHeight = 1200.f;
		Settings.HangTimeSeconds = 6.f;
		Settings.VisionRadius = 2750.f;
		Settings.AscentSpeed = 4000.f;
		return Settings;
	}

	/** @return Seconds ticked until the flare left the phase (or MaxSeconds). */
	float TickWhileInPhase(AFlareProjectile& Flare, const EFlareProjectilePhase Phase, const float MaxSeconds)
	{
		float Seconds = 0.f;
		while (Flare.GetPhase() == Phase && Seconds < MaxSeconds)
		{
			FFlareAbilityTestAccess::Tick(Flare, TickSeconds);
			Seconds += TickSeconds;
		}
		return Seconds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlareFlightPathTest, "RTS.Abilities.Flare.FlightPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlareFlightPathTest::RunTest(const FString& Parameters)
{
	using namespace FlareProjectileTests;
	const FFlareFlightPath Path = FFlareFlightPath::Make(Launch, FVector(0.3f, 0.f, 1.f), Ground, 1200.f, 4000.f, 6.f);

	TestTrue(TEXT("Starts at the launch socket"), Path.GetLocationAtTime(0.f).Equals(Launch, Tolerance));
	TestTrue(TEXT("Apex is ApexHeight above the ground at the target"),
	         Path.Apex.Equals(Ground + FVector(0.f, 0.f, 1200.f), Tolerance));
	TestTrue(TEXT("Reaches the apex when the climb ends"),
	         Path.GetLocationAtTime(Path.AscentSeconds).Equals(Path.Apex, Tolerance));
	TestEqual(TEXT("Hangs for HangTimeSeconds"), Path.HangSeconds, 6.f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Lands at the target"),
	         Path.GetLocationAtTime(Path.GetTotalSeconds()).Equals(Path.Landing, Tolerance));
	TestTrue(TEXT("Landing is on the ground under the target"),
	         FVector::Dist2D(Path.Landing, Ground) < Tolerance && FMath::Abs(Path.Landing.Z - Ground.Z) < 50.f);

	// The climb never goes above the apex and never falls back; the descent never climbs.
	float PreviousZ = -BIG_NUMBER;
	bool bClimbIsMonotonic = true;
	bool bStaysBelowApex = true;
	for (float Seconds = 0.f; Seconds <= Path.AscentSeconds; Seconds += 0.01f)
	{
		const float Z = Path.GetLocationAtTime(Seconds).Z;
		bClimbIsMonotonic &= Z >= PreviousZ - KINDA_SMALL_NUMBER;
		bStaysBelowApex &= Z <= Path.Apex.Z + KINDA_SMALL_NUMBER;
		PreviousZ = Z;
	}
	TestTrue(TEXT("Climb only goes up"), bClimbIsMonotonic);
	TestTrue(TEXT("Climb never overshoots the apex"), bStaysBelowApex);

	bool bDescentIsMonotonic = true;
	for (float Seconds = Path.AscentSeconds; Seconds <= Path.GetTotalSeconds(); Seconds += 0.05f)
	{
		const float Z = Path.GetLocationAtTime(Seconds).Z;
		bDescentIsMonotonic &= Z <= PreviousZ + KINDA_SMALL_NUMBER;
		PreviousZ = Z;
	}
	TestTrue(TEXT("Parachute only sinks"), bDescentIsMonotonic);
	const float HalfwayZ = Path.GetLocationAtTime(Path.AscentSeconds + Path.HangSeconds * 0.5f).Z;
	TestEqual(TEXT("Sinks at a steady speed"), HalfwayZ, static_cast<float>(0.5 * (Path.Apex.Z + Path.Landing.Z)), Tolerance);

	TestTrue(TEXT("Leaves the tube along the tube"),
	         Path.GetDirectionAtTime(0.f).Equals(FVector(0.3f, 0.f, 1.f).GetSafeNormal(), 0.05f));
	TestTrue(TEXT("Hangs straight down while burning"),
	         Path.GetDirectionAtTime(Path.AscentSeconds + 1.f).Equals(-FVector::UpVector, KINDA_SMALL_NUMBER));

	const float ExpectedMinimumSeconds = FVector::Dist(Launch, Path.Apex) / 4000.f;
	TestTrue(TEXT("Climb time follows the climb speed"), Path.AscentSeconds >= ExpectedMinimumSeconds &&
	         Path.AscentSeconds < ExpectedMinimumSeconds * 1.5f);

	// A steep tube with a far target would put the curve's control point above the apex.
	const FFlareFlightPath Steep = FFlareFlightPath::Make(Launch, FVector::UpVector, FVector(9000.f, 0.f, 0.f), 600.f,
	                                                      4000.f, 6.f);
	TestTrue(TEXT("Steep launch to a far target stays below the apex"), Steep.Control.Z <= Steep.Apex.Z);

	// A tube pointing down still sends the flare up.
	const FFlareFlightPath Down = FFlareFlightPath::Make(Launch, FVector(1.f, 0.f, -1.f), Ground, 1200.f, 4000.f, 6.f);
	TestTrue(TEXT("A downward tube is tilted upwards"), Down.GetDirectionAtTime(0.f).Z >= 0.34f);

	// A target far below the unit still gets an apex above the launcher.
	const FFlareFlightPath Below = FFlareFlightPath::Make(Launch, FVector::UpVector, FVector(500.f, 0.f, -5000.f), 300.f,
	                                                      4000.f, 6.f);
	TestTrue(TEXT("Apex is above the launcher"), Below.Apex.Z >= Launch.Z + 300.f - Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlareProjectileLifeTest, "RTS.Abilities.Flare.ProjectileLife",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlareProjectileLifeTest::RunTest(const FString& Parameters)
{
	using namespace FlareProjectileTests;
	FScopedFlareWorld Fixture;
	if (not TestTrue(TEXT("Test world"), Fixture.Initialize()))
	{
		return false;
	}
	const FFlareProjectileSettings Settings = MakeSettings();
	AFlareProjectile* Flare = Fixture.LaunchFlare(Settings, 1);
	if (not TestNotNull(TEXT("Flare spawned"), Flare))
	{
		return false;
	}
	const FFlareFlightPath& Path = Flare->GetFlightPath();
	TestTrue(TEXT("Climbs after launch"), Flare->GetPhase() == EFlareProjectilePhase::Ascending);
	TestTrue(TEXT("Ticks"), Flare->IsActorTickEnabled());
	TestFalse(TEXT("No parachute while climbing"), FFlareAbilityTestAccess::GetParachute(*Flare)->IsVisible());
	TestFalse(TEXT("No vision while climbing"), FFlareAbilityTestAccess::GetIsGivingVision(*Flare));

	const float ClimbSeconds = TickWhileInPhase(*Flare, EFlareProjectilePhase::Ascending, 30.f);
	TestEqual(TEXT("Ignites when the climb ends"), ClimbSeconds, Path.AscentSeconds, TickSeconds * 1.5f);
	TestTrue(TEXT("Burns after the climb"), Flare->GetPhase() == EFlareProjectilePhase::Burning);
	TestEqual(TEXT("Ignites at its apex height"), static_cast<float>(Flare->GetActorLocation().Z), static_cast<float>(Path.Apex.Z), 15.f);

	const UFowComp* FowComponent = FFlareAbilityTestAccess::GetFowComponent(*Flare);
	TestTrue(TEXT("Player flare gives player vision"), FowComponent->GetFowBehaviour() == EFowBehaviour::Fow_Active);
	TestEqual(TEXT("Vision radius from the settings"), FowComponent->GetVisionRadius(), Settings.VisionRadius);
	TestTrue(TEXT("Parachute opens"), FFlareAbilityTestAccess::GetParachute(*Flare)->IsVisible());
	TestTrue(TEXT("Light is on"), FFlareAbilityTestAccess::GetLight(*Flare)->IsVisible());

	float PreviousZ = Flare->GetActorLocation().Z;
	bool bOnlySinks = true;
	float BurnSeconds = 0.f;
	while (Flare->GetPhase() == EFlareProjectilePhase::Burning && BurnSeconds < 60.f)
	{
		FFlareAbilityTestAccess::Tick(*Flare, TickSeconds);
		BurnSeconds += TickSeconds;
		const float Z = Flare->GetActorLocation().Z;
		bOnlySinks &= Z <= PreviousZ + KINDA_SMALL_NUMBER;
		PreviousZ = Z;
	}
	TestTrue(TEXT("Sinks while burning"), bOnlySinks);
	TestEqual(TEXT("Burns for HangTimeSeconds"), BurnSeconds, Settings.HangTimeSeconds, TickSeconds * 2.f);
	TestTrue(TEXT("Burns out"), Flare->GetPhase() == EFlareProjectilePhase::BurntOut);
	TestTrue(TEXT("Burns out on the ground"), Flare->GetActorLocation().Equals(Path.Landing, Tolerance));
	TestFalse(TEXT("No parachute after burning out"), FFlareAbilityTestAccess::GetParachute(*Flare)->IsVisible());
	TestTrue(TEXT("Removes itself after burning out"), Flare->GetLifeSpan() > 0.f);

	TickWhileInPhase(*Flare, EFlareProjectilePhase::BurntOut, 3.f);
	TestEqual(TEXT("Light faded out"), FFlareAbilityTestAccess::GetLight(*Flare)->Intensity, 0.f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlareProjectileEnemyTest, "RTS.Abilities.Flare.EnemyVision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlareProjectileEnemyTest::RunTest(const FString& Parameters)
{
	using namespace FlareProjectileTests;
	FScopedFlareWorld Fixture;
	if (not TestTrue(TEXT("Test world"), Fixture.Initialize()))
	{
		return false;
	}
	AFlareProjectile* Flare = Fixture.LaunchFlare(MakeSettings(), 2);
	if (not TestNotNull(TEXT("Flare spawned"), Flare))
	{
		return false;
	}
	TickWhileInPhase(*Flare, EFlareProjectilePhase::Ascending, 30.f);
	TestTrue(TEXT("An AI flare gives the AI vision, never the player"),
	         FFlareAbilityTestAccess::GetFowComponent(*Flare)->GetFowBehaviour() ==
	         EFowBehaviour::Fow_PassiveEnemyVision);
	return true;
}

#endif
