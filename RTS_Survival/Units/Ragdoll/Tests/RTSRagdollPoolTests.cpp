// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the vehicle crew ragdoll pool: prewarm, waking bodies where the vehicle died (not under the map), the launch
// surviving the wake, landing on the ground, the return to the pool, the per-side looks and the simulation budget.
// Each test runs its own game world with a flat ground box and ticks it by hand, so it runs under -nullrhi.
#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"
#include "RTS_Survival/Units/Ragdoll/RTSPooledRagdoll.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSubsystem.h"

namespace RTSRagdollPoolTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr float FrameSeconds = 1.f / 60.f;
	// Long enough for the staggered prewarm to fill both pools.
	constexpr float PrewarmSeconds = 1.5f;
	constexpr float GroundHalfExtentCm = 5000.f;
	constexpr float GroundHalfHeightCm = 50.f;
	const FVector VehicleTopCentre(0.f, 0.f, 250.f);
	constexpr float VehicleSpreadRadiusCm = 100.f;
	// Spread plus a standing body's height: a woken body further away was not moved to the vehicle.
	constexpr float MaxWakeDistanceCm = 300.f;
	// Parked bodies wait 300 m below the map.
	constexpr float ParkedBelowZ = -10000.f;
	// A cook-off throws at 600-900 cm/s upward, so even the slowest body climbs far more than this.
	constexpr float MinCookOffRiseCm = 100.f;
	constexpr float FlightSeconds = 1.5f;
	// Body centres sit above the ground by their capsule radius; below this they went through it.
	constexpr float MinBodyCentreZ = -20.f;
	// Time after the throw by which every body must be back in the pool, on top of the configured lifetime.
	constexpr float ReturnMarginSeconds = 1.f;

	/** @brief A game world with a ground box and the ragdoll subsystem begun, ticked by hand. */
	struct FRagdollTestWorld
	{
		UWorld* World = nullptr;

		FRagdollTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World == nullptr)
			{
				return;
			}
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			SpawnGround();
			World->BeginPlay();
		}

		~FRagdollTestWorld()
		{
			if (World == nullptr)
			{
				return;
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		URTSRagdollSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<URTSRagdollSubsystem>() : nullptr;
		}

		/** Ticks frame by frame; the timer manager ticks once per GFrameCounter, so every frame advances it. */
		void TickFor(const float Seconds, const TFunctionRef<void()> AfterEachFrame = [] {}) const
		{
			const int32 Frames = FMath::CeilToInt(Seconds / FrameSeconds);
			for (int32 FrameIndex = 0; FrameIndex < Frames; ++FrameIndex)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, FrameSeconds);
				AfterEachFrame();
			}
		}

		TArray<ARTSPooledRagdoll*> GetAllBodies() const
		{
			TArray<ARTSPooledRagdoll*> Bodies;
			for (TActorIterator<ARTSPooledRagdoll> It(World); It; ++It)
			{
				Bodies.Add(*It);
			}
			return Bodies;
		}

		/** Woken bodies are the visible ones; parked bodies are hidden. */
		TArray<ARTSPooledRagdoll*> GetWokenBodies() const
		{
			TArray<ARTSPooledRagdoll*> Bodies = GetAllBodies();
			Bodies.RemoveAll([](const ARTSPooledRagdoll* Body) { return Body->IsHidden(); });
			return Bodies;
		}

	private:
		void SpawnGround() const
		{
			AActor* Ground = World->SpawnActor<AActor>();
			if (Ground == nullptr)
			{
				return;
			}
			UBoxComponent* GroundBox = NewObject<UBoxComponent>(Ground);
			Ground->AddInstanceComponent(GroundBox);
			Ground->SetRootComponent(GroundBox);
			GroundBox->SetBoxExtent(FVector(GroundHalfExtentCm, GroundHalfExtentCm, GroundHalfHeightCm));
			GroundBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GroundBox->SetCollisionObjectType(ECC_WorldStatic);
			GroundBox->SetCollisionResponseToAllChannels(ECR_Block);
			GroundBox->RegisterComponent();
			// The top of the box is the ground at Z = 0.
			Ground->SetActorLocation(FVector(0.f, 0.f, -GroundHalfHeightCm));
		}
	};

	USkeletalMeshComponent* GetCrewMesh(const ARTSPooledRagdoll* Body)
	{
		return Body ? Body->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}

	/** World positions of the physics bodies, not the component: the bug that left bodies parked moved only the actor. */
	TArray<FVector> GetPhysicsBodyLocations(const ARTSPooledRagdoll* Body)
	{
		TArray<FVector> Locations;
		const USkeletalMeshComponent* CrewMesh = GetCrewMesh(Body);
		if (CrewMesh == nullptr)
		{
			return Locations;
		}
		for (const FBodyInstance* BodyInstance : CrewMesh->Bodies)
		{
			if (BodyInstance != nullptr && BodyInstance->IsValidBodyInstance())
			{
				Locations.Add(BodyInstance->GetUnrealWorldTransform().GetLocation());
			}
		}
		return Locations;
	}

	float GetLowestBodyZ(const ARTSPooledRagdoll* Body)
	{
		float LowestZ = TNumericLimits<float>::Max();
		for (const FVector& Location : GetPhysicsBodyLocations(Body))
		{
			LowestZ = FMath::Min(LowestZ, static_cast<float>(Location.Z));
		}
		return LowestZ;
	}

	float GetHighestBodyZ(const ARTSPooledRagdoll* Body)
	{
		float HighestZ = TNumericLimits<float>::Lowest();
		for (const FVector& Location : GetPhysicsBodyLocations(Body))
		{
			HighestZ = FMath::Max(HighestZ, static_cast<float>(Location.Z));
		}
		return HighestZ;
	}

	FRTSCrewBodyRequest MakeRequest(const ERTSCrewSide Side, const bool bIsCookOff)
	{
		FRTSCrewBodyRequest Request;
		Request.TopCentre = VehicleTopCentre;
		Request.SpreadRadius = VehicleSpreadRadiusCm;
		Request.Side = Side;
		Request.bIsCookOff = bIsCookOff;
		return Request;
	}

	bool GetIsLookOfSide(const UMaterialInterface* Material, const FRTSCrewBodyType& CrewType)
	{
		for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : CrewType.Materials)
		{
			if (SoftMaterial.Get() == Material)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSRagdollPoolPrewarmTest,
	"RTS.Ragdoll.CrewPool.PrewarmFillsBothSides",
	RTSRagdollPoolTestsPrivate::TestFlags)

bool FRTSRagdollPoolPrewarmTest::RunTest(const FString& Parameters)
{
	using namespace RTSRagdollPoolTestsPrivate;
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (not TestNotNull(TEXT("Ragdoll subsystem exists in a game world"), Subsystem))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);

	TestEqual(TEXT("German pool is full"), Subsystem->GetParkedBodyCount(ERTSCrewSide::German),
	          Settings->PoolSizePerSide);
	TestEqual(TEXT("Soviet pool is full"), Subsystem->GetParkedBodyCount(ERTSCrewSide::Soviet),
	          Settings->PoolSizePerSide);
	const TArray<ARTSPooledRagdoll*> Bodies = TestWorld.GetAllBodies();
	TestEqual(TEXT("Exactly the pooled bodies were spawned"), Bodies.Num(), Settings->PoolSizePerSide * 2);
	int32 GermanBodies = 0;
	int32 SovietBodies = 0;
	for (const ARTSPooledRagdoll* Body : Bodies)
	{
		const USkeletalMeshComponent* CrewMesh = GetCrewMesh(Body);
		if (not TestNotNull(TEXT("Pooled body has its crew mesh"), CrewMesh))
		{
			continue;
		}
		TestTrue(TEXT("Parked body is hidden"), Body->IsHidden());
		TestTrue(TEXT("Parked body waits below the map"), Body->GetActorLocation().Z < ParkedBelowZ);
		TestFalse(TEXT("Parked body does not simulate"), CrewMesh->IsSimulatingPhysics());
		// Bodies only exist while collision is on; without them waking would rebuild them at 5 times the cost.
		TestTrue(TEXT("Parked body keeps its physics bodies ready"), CrewMesh->Bodies.Num() > 0);
		GermanBodies += CrewMesh->GetSkeletalMeshAsset() == Settings->GermanCrew.Mesh.Get() ? 1 : 0;
		SovietBodies += CrewMesh->GetSkeletalMeshAsset() == Settings->SovietCrew.Mesh.Get() ? 1 : 0;
	}
	TestEqual(TEXT("German pool uses the Scavenger mesh"), GermanBodies, Settings->PoolSizePerSide);
	TestEqual(TEXT("Soviet pool uses the Mosin soldier mesh"), SovietBodies, Settings->PoolSizePerSide);
	TestEqual(TEXT("Nothing simulates after prewarm"), Subsystem->GetSimulatingRagdollCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSRagdollPoolCookOffLifecycleTest,
	"RTS.Ragdoll.CrewPool.CookOffBodiesFlyLandAndReturn",
	RTSRagdollPoolTestsPrivate::TestFlags)

bool FRTSRagdollPoolCookOffLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RTSRagdollPoolTestsPrivate;
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (not TestNotNull(TEXT("Ragdoll subsystem exists in a game world"), Subsystem))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);

	const int32 Thrown = Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::German, true));
	TestTrue(TEXT("A cook-off throws its configured body count"),
	         Thrown >= Settings->CookOffDeath.MinBodies && Thrown <= Settings->CookOffDeath.MaxBodies);
	TestEqual(TEXT("Thrown bodies leave the pool"), Subsystem->GetParkedBodyCount(ERTSCrewSide::German),
	          Settings->PoolSizePerSide - Thrown);
	TestEqual(TEXT("Thrown bodies count as simulating"), Subsystem->GetSimulatingRagdollCount(), Thrown);

	const TArray<ARTSPooledRagdoll*> WokenBodies = TestWorld.GetWokenBodies();
	if (not TestEqual(TEXT("Every thrown body is visible"), WokenBodies.Num(), Thrown))
	{
		return false;
	}
	TMap<const ARTSPooledRagdoll*, float> StartTopZ;
	for (const ARTSPooledRagdoll* Body : WokenBodies)
	{
		const USkeletalMeshComponent* CrewMesh = GetCrewMesh(Body);
		TestTrue(TEXT("Woken body simulates"), CrewMesh && CrewMesh->IsSimulatingPhysics());
		TestTrue(TEXT("German body wears a Scavenger look"),
		         CrewMesh && GetIsLookOfSide(CrewMesh->GetMaterial(0), Settings->GermanCrew));
		const TArray<FVector> Locations = GetPhysicsBodyLocations(Body);
		TestTrue(TEXT("Woken body has physics bodies"), Locations.Num() > 0);
		for (const FVector& Location : Locations)
		{
			TestTrue(TEXT("Physics bodies wake at the vehicle, not where they were parked"),
			         FVector::Dist(Location, VehicleTopCentre) < MaxWakeDistanceCm);
		}
		StartTopZ.Add(Body, GetHighestBodyZ(Body));
	}

	TMap<const ARTSPooledRagdoll*, float> PeakTopZ = StartTopZ;
	TestWorld.TickFor(FlightSeconds, [&PeakTopZ]()
	{
		for (TPair<const ARTSPooledRagdoll*, float>& Peak : PeakTopZ)
		{
			Peak.Value = FMath::Max(Peak.Value, GetHighestBodyZ(Peak.Key));
		}
	});
	for (const ARTSPooledRagdoll* Body : WokenBodies)
	{
		// Without the launch the body only falls. A launch in the wake frame passed here too, so this does not prove
		// the next-tick launch is needed.
		TestTrue(TEXT("The launch throws the body up"),
		         PeakTopZ[Body] - StartTopZ[Body] > MinCookOffRiseCm);
		TestTrue(TEXT("The body lands on the ground instead of falling through"),
		         GetLowestBodyZ(Body) > MinBodyCentreZ);
	}

	TestWorld.TickFor(Settings->MaxSimulationSeconds + Settings->SinkSeconds + ReturnMarginSeconds);
	TestEqual(TEXT("Nothing simulates once the bodies are back"), Subsystem->GetSimulatingRagdollCount(), 0);
	TestEqual(TEXT("German pool is full again"), Subsystem->GetParkedBodyCount(ERTSCrewSide::German),
	          Settings->PoolSizePerSide);
	TestEqual(TEXT("No body stays visible"), TestWorld.GetWokenBodies().Num(), 0);
	for (const ARTSPooledRagdoll* Body : WokenBodies)
	{
		TestTrue(TEXT("Returned body is parked below the map"), Body->GetActorLocation().Z < ParkedBelowZ);
		TestFalse(TEXT("Returned body stopped simulating"), GetCrewMesh(Body)->IsSimulatingPhysics());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSRagdollPoolSovietCrewTest,
	"RTS.Ragdoll.CrewPool.SovietDeathThrowsMosinSoldiers",
	RTSRagdollPoolTestsPrivate::TestFlags)

bool FRTSRagdollPoolSovietCrewTest::RunTest(const FString& Parameters)
{
	using namespace RTSRagdollPoolTestsPrivate;
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (not TestNotNull(TEXT("Ragdoll subsystem exists in a game world"), Subsystem))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);

	const int32 Thrown = Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::Soviet, false));
	TestTrue(TEXT("An ordinary death throws its configured body count"),
	         Thrown >= Settings->OrdinaryDeath.MinBodies && Thrown <= Settings->OrdinaryDeath.MaxBodies);
	TestEqual(TEXT("The German pool is untouched"), Subsystem->GetParkedBodyCount(ERTSCrewSide::German),
	          Settings->PoolSizePerSide);
	for (const ARTSPooledRagdoll* Body : TestWorld.GetWokenBodies())
	{
		const USkeletalMeshComponent* CrewMesh = GetCrewMesh(Body);
		TestTrue(TEXT("Soviet body is a Mosin soldier"),
		         CrewMesh && CrewMesh->GetSkeletalMeshAsset() == Settings->SovietCrew.Mesh.Get());
	}
	TestWorld.TickFor(FlightSeconds);
	for (const ARTSPooledRagdoll* Body : TestWorld.GetWokenBodies())
	{
		TestTrue(TEXT("Soviet body lands on the ground"), GetLowestBodyZ(Body) > MinBodyCentreZ);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSRagdollBudgetTest,
	"RTS.Ragdoll.Budget.PressureAndCapLimitBodies",
	RTSRagdollPoolTestsPrivate::TestFlags)

bool FRTSRagdollBudgetTest::RunTest(const FString& Parameters)
{
	using namespace RTSRagdollPoolTestsPrivate;
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (not TestNotNull(TEXT("Ragdoll subsystem exists in a game world"), Subsystem))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);

	// Falling soldiers count against the same budget as crew bodies.
	for (int32 Soldier = 0; Soldier < Settings->PressureSimulatingRagdolls; ++Soldier)
	{
		Subsystem->NotifySoldierRagdollStarted();
	}
	TestTrue(TEXT("Below the cap a soldier may still ragdoll"), Subsystem->GetCanStartRagdoll());
	TestEqual(TEXT("Under pressure a cook-off throws a single body"),
	          Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::German, true)), 1);

	const int32 SoldiersToCap = Settings->MaxSimulatingRagdolls - Subsystem->GetSimulatingRagdollCount();
	for (int32 Soldier = 0; Soldier < SoldiersToCap; ++Soldier)
	{
		Subsystem->NotifySoldierRagdollStarted();
	}
	TestFalse(TEXT("At the cap soldiers keep their death montage"), Subsystem->GetCanStartRagdoll());
	const int32 RefusedBefore = Subsystem->GetBodiesRefused();
	const int32 ParkedBefore = Subsystem->GetParkedBodyCount(ERTSCrewSide::Soviet);
	TestEqual(TEXT("At the cap a death throws nothing"),
	          Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::Soviet, true)), 0);
	TestTrue(TEXT("The refused bodies are counted"),
	         Subsystem->GetBodiesRefused() - RefusedBefore >= Settings->CookOffDeath.MinBodies);
	TestEqual(TEXT("A refused request leaves the pool alone"), Subsystem->GetParkedBodyCount(ERTSCrewSide::Soviet),
	          ParkedBefore);

	const int32 SoldiersStarted = Settings->PressureSimulatingRagdolls + SoldiersToCap;
	// One extra end, as a soldier destroyed twice must not push the count below zero.
	for (int32 Soldier = 0; Soldier <= SoldiersStarted; ++Soldier)
	{
		Subsystem->NotifySoldierRagdollEnded();
	}
	TestEqual(TEXT("Only the crew body still counts once the soldiers ended"),
	          Subsystem->GetSimulatingRagdollCount(), 1);
	TestTrue(TEXT("Ragdolls may start again"), Subsystem->GetCanStartRagdoll());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSRagdollPoolNeverSpawnsTest,
	"RTS.Ragdoll.CrewPool.EmptyPoolNeverSpawns",
	RTSRagdollPoolTestsPrivate::TestFlags)

bool FRTSRagdollPoolNeverSpawnsTest::RunTest(const FString& Parameters)
{
	using namespace RTSRagdollPoolTestsPrivate;
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (not TestNotNull(TEXT("Ragdoll subsystem exists in a game world"), Subsystem))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);
	const int32 BodiesBefore = TestWorld.GetAllBodies().Num();

	// Every request throws at least one body until the pool or the cap runs out.
	int32 Requests = 0;
	while (Subsystem->GetParkedBodyCount(ERTSCrewSide::German) > 0 && Subsystem->GetCanStartRagdoll()
		&& Requests < Settings->PoolSizePerSide)
	{
		Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::German, true));
		++Requests;
	}
	TestEqual(TEXT("The German pool is drained"), Subsystem->GetParkedBodyCount(ERTSCrewSide::German), 0);
	TestEqual(TEXT("An empty pool throws nothing"),
	          Subsystem->RequestCrewBodies(MakeRequest(ERTSCrewSide::German, true)), 0);
	TestEqual(TEXT("No body was spawned during play"), TestWorld.GetAllBodies().Num(), BodiesBefore);
	TestTrue(TEXT("The Soviet pool still has bodies"), Subsystem->GetParkedBodyCount(ERTSCrewSide::Soviet) > 0);
	return true;
}

#endif
