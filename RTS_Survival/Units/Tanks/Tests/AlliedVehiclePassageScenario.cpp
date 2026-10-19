// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AlliedVehiclePassageScenario.h"

#if RTS_WITH_MAP_TESTS

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAlliedPassage, Log, All);

namespace AlliedPassageScenarioPrivate
{
	const TCHAR* const LocationSwitch = TEXT("AlliedPassageAt=");
	// Only checks that every vehicle the map holds stands on its ground, on any map; spawns nothing.
	const TCHAR* const GroundProbeSwitch = TEXT("AlliedPassageGroundProbe");
	// Game seconds the map's vehicles get to drop onto, or through, the ground before they are measured.
	constexpr float GroundProbeSettleSeconds = 8.f;
	const TCHAR* const HarvesterClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer1/"
		"Ger_Harvester_Panzer1.Ger_Harvester_Panzer1_C");
	const TCHAR* const AlliedTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/"
		"Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const HQTruckClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/"
		"Ger_HQ_Truck.Ger_HQ_Truck_C");
	const TCHAR* const EnemyTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/"
		"Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");

	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;
	// The hull channel of enemy vehicles, spelled out so the scenario also builds without the passage change.
	constexpr ECollisionChannel EnemyHullChannel = ECC_GameTraceChannel10;
	const FName HullBlockerName(TEXT("HullBlocker"));

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float MaxVehicleLoadSeconds = 30.f;
	constexpr float SettleSeconds = 3.f;
	constexpr float DriveTimeoutSeconds = 90.f;
	// Pushed allies keep driving a moment after the truck reached its spot; their moves are measured that long.
	constexpr float AfterDeploySeconds = 6.f;
	// The harvester sent onto the enemy tank settles beside it; it is watched this long.
	constexpr float EnemyLaneWatchSeconds = 30.f;

	// Layout around the HQ truck, in centimetres.
	constexpr float HarvesterLaneOffset = 6000.f;
	constexpr float EnemyLaneOffset = 11000.f;
	constexpr float LaneHalfLength = 2200.f;
	// Halfway between the far lane's harvester and the enemy tank it is sent to.
	constexpr float LaneParkedTankAlong = -LaneHalfLength * 0.5f;
	constexpr float NomadicDriveDistance = 3500.f;
	constexpr float NomadicParkedFractions[] = {0.35f, 0.7f, 1.f};
	constexpr float BroadsideYawOffset = 90.f;
	// The test area is flat within AreaMaxHeightDifference; deeper than this a vehicle sank through the ground.
	constexpr float MaxSinkBelowGround = 600.f;
	const FVector GoalNavExtent(600.f, 600.f, 3000.f);

	// Open flat ground that holds every lane is searched for around the test spot.
	constexpr float AreaSearchRadius = 60000.f;
	constexpr float AreaSearchStep = 3000.f;
	constexpr float AreaSampleStep = 700.f;
	// The rectangle the lanes, the parked vehicles and the HQ building take up, with room to spare.
	constexpr float AreaMinAlong = -LaneHalfLength - 1000.f;
	constexpr float AreaMaxAlong = NomadicDriveDistance + 2500.f;
	constexpr float AreaMinAcross = -HarvesterLaneOffset - 1000.f;
	constexpr float AreaMaxAcross = EnemyLaneOffset + 1000.f;
	constexpr float AreaMaxHeightDifference = 400.f;
	constexpr float AreaTraceHalfHeight = 50000.f;
	constexpr float AreaClearanceHeight = 450.f;
	constexpr float AreaClearanceRadius = 400.f;
	constexpr float AreaNavTolerance = 60.f;
	const FVector AreaNavExtent(200.f, 200.f, 600.f);

	constexpr float GoalTolerance = 400.f;
	constexpr float StalledSpeed = 30.f;
	// Starting off takes a moment; standing still before that is not being held up.
	constexpr float StallGraceSeconds = 3.f;

	constexpr float HarvesterMaxArrivalSeconds = 40.f;
	constexpr float HarvesterMaxStalledSeconds = 4.f;
	// Hulls this deep inside each other drove through each other instead of around.
	constexpr float PassedThroughPenetration = 50.f;
	// Solid hulls touch by a few centimetres while pressing; deeper than this the enemy did not block.
	constexpr float EnemyMaxPenetration = 40.f;
	// A parked ally that a harvester drives through is not shoved off its spot.
	constexpr float ParkedAllyMaxShove = 200.f;

	constexpr float NomadicMaxDeploySeconds = 60.f;
	constexpr float NomadicMaxDeployGoalDistance = 600.f;
	constexpr float PushedAllyMinimumMove = 250.f;
}

void FAlliedVehiclePassageScenario::Start()
{
	EnterPhase(EAlliedPassagePhase::WaitingForGameStart);
	RTSFunctionLibrary::PushModalDialogSuppression();
	UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE started"));
}

void FAlliedVehiclePassageScenario::Tick(URTSCoverFinderWorldSubsystem& Host, const float DeltaTime)
{
	if (M_Phase == EAlliedPassagePhase::Inactive || M_Phase == EAlliedPassagePhase::Done)
	{
		return;
	}
	UWorld* World = Host.GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_World = World;
	if (M_Phase != EAlliedPassagePhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
	}
	switch (M_Phase)
	{
	case EAlliedPassagePhase::WaitingForGameStart:
		TickWaitingForGameStart(*World);
		break;
	case EAlliedPassagePhase::WaitingForVehicles:
		TickWaitingForVehicles(*World);
		break;
	case EAlliedPassagePhase::Driving:
		TickDriving(*World, DeltaTime);
		break;
	case EAlliedPassagePhase::ProbingMapVehicles:
		TickProbingMapVehicles(*World);
		break;
	default:
		break;
	}
}

void FAlliedVehiclePassageScenario::EnterPhase(const EAlliedPassagePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = M_World.IsValid() ? M_World->GetTimeSeconds() : 0.f;
}

float FAlliedVehiclePassageScenario::GetGameSecondsInPhase() const
{
	return M_World.IsValid() ? M_World->GetTimeSeconds() - M_PhaseStartGameSeconds : 0.f;
}

double FAlliedVehiclePassageScenario::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float FAlliedVehiclePassageScenario::GetSecondsSinceOrders() const
{
	return M_Phase == EAlliedPassagePhase::Driving ? GetGameSecondsInPhase() : 0.f;
}

void FAlliedVehiclePassageScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details);
}

void FAlliedVehiclePassageScenario::Finish()
{
	EnterPhase(EAlliedPassagePhase::Done);
	RTSFunctionLibrary::PopModalDialogSuppression();
	UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	FPlatformMisc::RequestExit(false, TEXT("AlliedVehiclePassageScenario"));
}

void FAlliedVehiclePassageScenario::TickWaitingForGameStart(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	if (GetRealSecondsInPhase() < HudBuildSeconds)
	{
		return;
	}
	if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		if (GetRealSecondsInPhase() > MaxStartSeconds)
		{
			Check(false, TEXT("game_started"), TEXT("no player controller"));
			Finish();
		}
		return;
	}
	if (FParse::Param(FCommandLine::Get(), GroundProbeSwitch))
	{
		EnterPhase(EAlliedPassagePhase::ProbingMapVehicles);
		return;
	}
	if (not FindAnchor(World))
	{
		Check(false, TEXT("hq_truck_spawned"), TEXT("no open test area, or the truck could not be spawned"));
		Finish();
		return;
	}
	SpawnHarvesterLanes(World);
	SpawnNomadicLane(World);
	EnterPhase(EAlliedPassagePhase::WaitingForVehicles);
}

void FAlliedVehiclePassageScenario::TickProbingMapVehicles(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	if (GetGameSecondsInPhase() < GroundProbeSettleSeconds)
	{
		return;
	}
	int32 ProbedVehicles = 0;
	for (TActorIterator<ATrackedTankMaster> TankIterator(&World); TankIterator; ++TankIterator)
	{
		ProbeVehicleGround(World, **TankIterator);
		++ProbedVehicles;
	}
	Check(ProbedVehicles > 0, TEXT("map_has_vehicles"), FString::Printf(TEXT("count=%d"), ProbedVehicles));
	Finish();
}

void FAlliedVehiclePassageScenario::ProbeVehicleGround(UWorld& World, const ATrackedTankMaster& Tank)
{
	using namespace AlliedPassageScenarioPrivate;
	const USkeletalMeshComponent* Chassis = Tank.GetTankMesh();
	const URTSComponent* RTSComponent = Tank.GetRTSComponent();
	const FVector Location = Tank.GetActorLocation();
	FHitResult GroundHit;
	FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(AlliedPassageProbe), false, &Tank);
	const bool bFoundLandscape = World.LineTraceSingleByChannel(
		GroundHit, FVector(FVector2D(Location), AreaTraceHalfHeight), FVector(FVector2D(Location), -AreaTraceHalfHeight),
		COLLISION_TRACE_LANDSCAPE, GroundQuery);
	const float HeightAboveGround = bFoundLandscape ? Location.Z - GroundHit.ImpactPoint.Z : 0.f;
	const FString Details = FString::Printf(
		TEXT("%s owner=%d height_above_ground=%.0f ground=%s chassis_object=%d blocks_static=%d simulating=%d"),
		*Tank.GetName(), IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1, HeightAboveGround,
		bFoundLandscape ? *GetNameSafe(GroundHit.GetActor()) : TEXT("none"),
		IsValid(Chassis) ? static_cast<int32>(Chassis->GetCollisionObjectType()) : -1,
		IsValid(Chassis) ? static_cast<int32>(Chassis->GetCollisionResponseToChannel(ECC_WorldStatic)) : -1,
		IsValid(Chassis) && Chassis->IsSimulatingPhysics());
	// Below the ground under it: it dropped through. No ground at all below it is just as wrong.
	Check(bFoundLandscape && HeightAboveGround > -MaxSinkBelowGround, TEXT("map_vehicle_on_ground"), Details);
}

bool FAlliedVehiclePassageScenario::FindAnchor(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	FVector SearchOrigin;
	if (not FGrenadeTestHelpers::FindTestSpot(World, LocationSwitch, SearchOrigin))
	{
		return false;
	}
	// The map's own trucks stand deployed among buildings around the player start (and never finish deploying
	// without rendering); the scenario brings its own truck to open ground that holds every lane.
	if (not FindOpenTestArea(World, SearchOrigin, M_Anchor))
	{
		Check(false, TEXT("open_test_area_found"), SearchOrigin.ToCompactString());
		return false;
	}
	FHitResult GroundHit;
	if (World.LineTraceSingleByChannel(GroundHit, M_Anchor + FVector::UpVector * AreaTraceHalfHeight,
	                                   M_Anchor - FVector::UpVector * AreaTraceHalfHeight, ECC_Visibility) &&
		IsValid(GroundHit.GetComponent()))
	{
		UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE ground %s blocks player=%d enemy=%d"),
		       *GroundHit.GetComponent()->GetName(),
		       static_cast<int32>(GroundHit.GetComponent()->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER)),
		       static_cast<int32>(GroundHit.GetComponent()->GetCollisionResponseToChannel(COLLISION_OBJ_ENEMY)));
	}
	M_LaneForward = FVector::ForwardVector;
	M_LaneRight = FVector::RightVector;
	M_Nomadic = Cast<ANomadicVehicle>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, HQTruckClassPath, FTransform(M_LaneForward.Rotation(), M_Anchor), PlayerOwner));
	UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE hq=%s at %s, searched from %s"),
	       *GetNameSafe(M_Nomadic.Get()), *M_Anchor.ToCompactString(), *SearchOrigin.ToCompactString());
	return M_Nomadic.IsValid();
}

bool FAlliedVehiclePassageScenario::FindOpenTestArea(UWorld& World, const FVector& SearchOrigin,
                                                     FVector& OutAnchor) const
{
	using namespace AlliedPassageScenarioPrivate;
	TArray<FVector2D> Candidates;
	for (float OffsetX = -AreaSearchRadius; OffsetX <= AreaSearchRadius; OffsetX += AreaSearchStep)
	{
		for (float OffsetY = -AreaSearchRadius; OffsetY <= AreaSearchRadius; OffsetY += AreaSearchStep)
		{
			Candidates.Add(FVector2D(SearchOrigin) + FVector2D(OffsetX, OffsetY));
		}
	}
	const FVector2D Origin2D(SearchOrigin);
	Candidates.Sort([Origin2D](const FVector2D& Left, const FVector2D& Right)
	{
		return FVector2D::DistSquared(Left, Origin2D) < FVector2D::DistSquared(Right, Origin2D);
	});
	for (const FVector2D& Candidate : Candidates)
	{
		if (GetIsOpenTestArea(World, Candidate, OutAnchor))
		{
			return true;
		}
	}
	return false;
}

bool FAlliedVehiclePassageScenario::GetIsOpenTestArea(UWorld& World, const FVector2D& Candidate,
                                                      FVector& OutAnchor) const
{
	using namespace AlliedPassageScenarioPrivate;
	float AnchorGroundZ = 0.f;
	if (not GetOpenGroundZ(World, Candidate, AnchorGroundZ))
	{
		return false;
	}
	for (float Along = AreaMinAlong; Along <= AreaMaxAlong; Along += AreaSampleStep)
	{
		for (float Across = AreaMinAcross; Across <= AreaMaxAcross; Across += AreaSampleStep)
		{
			float GroundZ = 0.f;
			if (not GetOpenGroundZ(World, Candidate + FVector2D(Along, Across), GroundZ) ||
				FMath::Abs(GroundZ - AnchorGroundZ) > AreaMaxHeightDifference)
			{
				return false;
			}
		}
	}
	OutAnchor = FVector(Candidate, AnchorGroundZ);
	return true;
}

bool FAlliedVehiclePassageScenario::GetOpenGroundZ(UWorld& World, const FVector2D& Location, float& OutGroundZ) const
{
	using namespace AlliedPassageScenarioPrivate;
	const FVector TraceTop(Location, AreaTraceHalfHeight);
	const FVector TraceBottom(Location, -AreaTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AlliedPassageGround), false);
	if (not World.LineTraceSingleByChannel(GroundHit, TraceTop, TraceBottom, ECC_Visibility, TraceParams) ||
		not IsValid(Cast<ALandscapeProxy>(GroundHit.GetActor())))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	FVector OnNavMesh = GroundHit.ImpactPoint;
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, OnNavMesh, AreaNavExtent) ||
		FVector::DistSquared2D(OnNavMesh, GroundHit.ImpactPoint) > FMath::Square(AreaNavTolerance))
	{
		return false;
	}
	// Nothing but the ground may stand there: no buildings, trees, rocks or units.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_Vehicle);
	ObjectTypes.AddObjectTypesToQuery(ECC_Destructible);
	ObjectTypes.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	ObjectTypes.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	World.OverlapMultiByObjectType(
		Overlaps,
		GroundHit.ImpactPoint + FVector(0.f, 0.f, AreaClearanceHeight),
		FQuat::Identity,
		ObjectTypes,
		FCollisionShape::MakeSphere(AreaClearanceRadius));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (not IsValid(Cast<ALandscapeProxy>(Overlap.GetActor())))
		{
			return false;
		}
	}
	return true;
}

FVector FAlliedVehiclePassageScenario::GetLanePoint(const float AlongLane, const float AcrossLane) const
{
	return M_Anchor + M_LaneForward * AlongLane + M_LaneRight * AcrossLane;
}

int32 FAlliedVehiclePassageScenario::SpawnVehicle(
	UWorld& World,
	const TCHAR* ClassPath,
	const FVector& Location,
	const float Yaw,
	const uint8 OwningPlayer,
	const TCHAR* Label)
{
	ATrackedTankMaster* Tank = Cast<ATrackedTankMaster>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, ClassPath, FTransform(FRotator(0.f, Yaw, 0.f), Location), OwningPlayer));
	UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE spawned %s=%s owner=%d at %s"),
	       Label, IsValid(Tank) ? *Tank->GetName() : TEXT("nothing"), OwningPlayer,
	       IsValid(Tank) ? *Tank->GetActorLocation().ToCompactString() : TEXT("-"));
	if (not IsValid(Tank))
	{
		Check(false, TEXT("vehicle_spawned"), Label);
		return INDEX_NONE;
	}
	FAlliedPassageTestVehicle& TestVehicle = M_Vehicles.AddDefaulted_GetRef();
	TestVehicle.Vehicle = Tank;
	TestVehicle.Label = Label;
	return M_Vehicles.Num() - 1;
}

int32 FAlliedVehiclePassageScenario::AddPair(const int32 FirstIndex, const int32 SecondIndex)
{
	if (FirstIndex == INDEX_NONE || SecondIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	FAlliedPassageTestPair& Pair = M_Pairs.AddDefaulted_GetRef();
	Pair.FirstIndex = FirstIndex;
	Pair.SecondIndex = SecondIndex;
	return M_Pairs.Num() - 1;
}

void FAlliedVehiclePassageScenario::SpawnHarvesterLanes(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	const float LaneYaw = M_LaneForward.Rotation().Yaw;
	const float ReverseYaw = LaneYaw + 180.f;
	// Left lane: two harvesters drive head-on through each other.
	M_HeadOnHarvesterA = SpawnVehicle(World, HarvesterClassPath, GetLanePoint(-LaneHalfLength, -HarvesterLaneOffset),
	                                  LaneYaw, PlayerOwner, TEXT("head_on_harvester_a"));
	M_HeadOnHarvesterB = SpawnVehicle(World, HarvesterClassPath, GetLanePoint(LaneHalfLength, -HarvesterLaneOffset),
	                                  ReverseYaw, PlayerOwner, TEXT("head_on_harvester_b"));
	// Right lane: a harvester and an allied tank drive head-on through each other. Nothing parks in this lane:
	// an idle vehicle cuts the navmesh, and both would plan around it on different sides and never meet.
	M_LaneHarvester = SpawnVehicle(World, HarvesterClassPath, GetLanePoint(-LaneHalfLength, HarvesterLaneOffset),
	                               LaneYaw, PlayerOwner, TEXT("lane_harvester"));
	M_OncomingTank = SpawnVehicle(World, AlliedTankClassPath, GetLanePoint(LaneHalfLength, HarvesterLaneOffset),
	                              ReverseYaw, PlayerOwner, TEXT("oncoming_tank"));
	// Far lane: a harvester passes a parked ally broadside, then is sent onto the spot of a parked enemy tank and
	// must stay out of its hull.
	M_EnemyLaneHarvester = SpawnVehicle(World, HarvesterClassPath, GetLanePoint(-LaneHalfLength, EnemyLaneOffset),
	                                    LaneYaw, PlayerOwner, TEXT("enemy_lane_harvester"));
	M_LaneParkedTank = SpawnVehicle(World, AlliedTankClassPath,
	                                GetLanePoint(LaneParkedTankAlong, EnemyLaneOffset),
	                                LaneYaw + BroadsideYawOffset, PlayerOwner, TEXT("lane_parked_tank"));
	M_EnemyTank = SpawnVehicle(World, EnemyTankClassPath, GetLanePoint(0.f, EnemyLaneOffset),
	                           LaneYaw + BroadsideYawOffset, EnemyOwner, TEXT("enemy_tank"));

	M_PairHeadOnHarvesters = AddPair(M_HeadOnHarvesterA, M_HeadOnHarvesterB);
	M_PairHarvesterOncomingTank = AddPair(M_LaneHarvester, M_OncomingTank);
	M_PairHarvesterParkedTank = AddPair(M_EnemyLaneHarvester, M_LaneParkedTank);
	M_PairHarvesterEnemy = AddPair(M_EnemyLaneHarvester, M_EnemyTank);
}

void FAlliedVehiclePassageScenario::SpawnNomadicLane(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	M_NomadicGoal = GetLanePoint(NomadicDriveDistance, 0.f);
	FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_NomadicGoal, GoalNavExtent);

	FAlliedPassageTestVehicle& NomadicEntry = M_Vehicles.AddDefaulted_GetRef();
	NomadicEntry.Vehicle = M_Nomadic.Get();
	NomadicEntry.Label = TEXT("hq_truck");
	M_NomadicIndex = M_Vehicles.Num() - 1;

	// Idle allies parked across the truck's way and on its building spot.
	const float BroadsideYaw = M_LaneForward.Rotation().Yaw + BroadsideYawOffset;
	for (const float Fraction : NomadicParkedFractions)
	{
		const FVector ParkedLocation = M_Anchor + (M_NomadicGoal - M_Anchor) * Fraction;
		const int32 ParkedIndex = SpawnVehicle(World, AlliedTankClassPath, ParkedLocation, BroadsideYaw, PlayerOwner,
		                                       *FString::Printf(TEXT("nomadic_parked_%.0f"), Fraction * 100.f));
		if (ParkedIndex != INDEX_NONE)
		{
			M_NomadicParkedTanks.Add(ParkedIndex);
		}
	}
}

void FAlliedVehiclePassageScenario::TickWaitingForVehicles(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	if (not GetAreVehiclesReady())
	{
		if (GetGameSecondsInPhase() > MaxVehicleLoadSeconds)
		{
			Check(false, TEXT("vehicles_ready"), TEXT("chassis or controller missing"));
			ReportVehicles();
			Finish();
		}
		return;
	}
	SilenceVehicles();
	if (GetGameSecondsInPhase() < SettleSeconds)
	{
		return;
	}
	CheckVehiclesStayOnGround(TEXT("ready"));
	CheckCollisionSetup();
	OrderVehicles(World);
	EnterPhase(EAlliedPassagePhase::Driving);
}

void FAlliedVehiclePassageScenario::CheckVehiclesStayOnGround(const TCHAR* Moment)
{
	using namespace AlliedPassageScenarioPrivate;
	for (const FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		const USkeletalMeshComponent* Chassis = IsValid(Tank) ? Tank->GetTankMesh() : nullptr;
		if (not IsValid(Chassis))
		{
			continue;
		}
		const float HeightAboveAnchor = Tank->GetActorLocation().Z - M_Anchor.Z;
		const FString Details = FString::Printf(
			TEXT("%s %s height=%.0f object=%d blocks_static=%d simulating=%d"),
			Moment, *TestVehicle.Label, HeightAboveAnchor, static_cast<int32>(Chassis->GetCollisionObjectType()),
			static_cast<int32>(Chassis->GetCollisionResponseToChannel(ECC_WorldStatic)),
			Chassis->IsSimulatingPhysics());
		// Every lane is on flat ground; a vehicle far below it fell through the landscape.
		Check(HeightAboveAnchor > -MaxSinkBelowGround, TEXT("vehicle_stays_on_ground"), Details);
	}
}

bool FAlliedVehiclePassageScenario::GetAreVehiclesReady()
{
	for (FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		if (not IsValid(Tank) || not IsValid(Tank->GetController()) || not IsValid(Tank->GetTankMesh()))
		{
			return false;
		}
		if (not TestVehicle.Footprint.GetIsValid())
		{
			TestVehicle.Footprint = RTSVehicleFootprint::BuildFromChassisMesh(Tank->GetTankMesh());
		}
		if (not TestVehicle.Footprint.GetIsValid())
		{
			return false;
		}
	}
	return true;
}

void FAlliedVehiclePassageScenario::SilenceVehicles() const
{
	// Nothing shoots: hits would move or kill the vehicles the checks are about.
	for (const FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		Tank->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
		Tank->SetTurretsDisabled();
	}
}

void FAlliedVehiclePassageScenario::CheckCollisionSetup()
{
	using namespace AlliedPassageScenarioPrivate;
	auto FindHullBlocker = [](const ATrackedTankMaster* Tank) -> const UBoxComponent*
	{
		TInlineComponentArray<UBoxComponent*> Boxes(Tank);
		for (const UBoxComponent* Box : Boxes)
		{
			if (IsValid(Box) && Box->GetFName() == HullBlockerName)
			{
				return Box;
			}
		}
		return nullptr;
	};
	const ATrackedTankMaster* Harvester = M_Vehicles.IsValidIndex(M_LaneHarvester)
		                                      ? M_Vehicles[M_LaneHarvester].Vehicle.Get()
		                                      : nullptr;
	const ATrackedTankMaster* AlliedTank = M_Vehicles.IsValidIndex(M_OncomingTank)
		                                       ? M_Vehicles[M_OncomingTank].Vehicle.Get()
		                                       : nullptr;
	const ATrackedTankMaster* EnemyTank = M_Vehicles.IsValidIndex(M_EnemyTank)
		                                      ? M_Vehicles[M_EnemyTank].Vehicle.Get()
		                                      : nullptr;
	if (not IsValid(Harvester) || not IsValid(AlliedTank) || not IsValid(EnemyTank))
	{
		Check(false, TEXT("collision_setup_vehicles"), TEXT("missing"));
		return;
	}
	const USkeletalMeshComponent* HarvesterChassis = Harvester->GetTankMesh();
	Check(HarvesterChassis->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER) != ECR_Block,
	      TEXT("harvester_chassis_passes_allied_chassis"),
	      FString::Printf(TEXT("response=%d"),
	                      static_cast<int32>(HarvesterChassis->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER))));
	Check(HarvesterChassis->GetCollisionResponseToChannel(COLLISION_OBJ_ENEMY) == ECR_Block,
	      TEXT("harvester_chassis_blocks_enemy_chassis"), FString());

	const UBoxComponent* HarvesterHull = FindHullBlocker(Harvester);
	const UBoxComponent* AlliedHull = FindHullBlocker(AlliedTank);
	const UBoxComponent* EnemyHull = FindHullBlocker(EnemyTank);
	Check(IsValid(HarvesterHull) && IsValid(AlliedHull) && IsValid(EnemyHull), TEXT("hull_blockers_exist"), FString());
	if (not IsValid(HarvesterHull) || not IsValid(AlliedHull) || not IsValid(EnemyHull))
	{
		return;
	}
	// Two bodies collide when each blocks the object channel of the other.
	auto GetDoBlock = [](const UPrimitiveComponent* First, const UPrimitiveComponent* Second)
	{
		return First->GetCollisionResponseToChannel(Second->GetCollisionObjectType()) == ECR_Block &&
			Second->GetCollisionResponseToChannel(First->GetCollisionObjectType()) == ECR_Block;
	};
	Check(not GetDoBlock(HarvesterHull, AlliedHull), TEXT("harvester_hull_passes_allied_hull"), FString());
	Check(GetDoBlock(HarvesterHull, EnemyHull), TEXT("harvester_hull_blocks_enemy_hull"),
	      FString::Printf(TEXT("enemy_hull_channel=%d"), static_cast<int32>(EnemyHull->GetCollisionObjectType())));
	Check(GetDoBlock(AlliedHull, EnemyHull), TEXT("tank_hull_blocks_enemy_hull"), FString());
	Check(EnemyHull->GetCollisionObjectType() == EnemyHullChannel, TEXT("enemy_hull_on_enemy_hull_channel"),
	      FString::Printf(TEXT("channel=%d"), static_cast<int32>(EnemyHull->GetCollisionObjectType())));
}

void FAlliedVehiclePassageScenario::OrderMove(const int32 VehicleIndex, const FVector& Goal)
{
	if (not M_Vehicles.IsValidIndex(VehicleIndex))
	{
		return;
	}
	FAlliedPassageTestVehicle& TestVehicle = M_Vehicles[VehicleIndex];
	ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
	if (not IsValid(Tank))
	{
		return;
	}
	TestVehicle.GoalLocation = Goal;
	TestVehicle.bHasGoal = true;
	const FRotator Facing = (Goal - Tank->GetActorLocation()).GetSafeNormal2D().Rotation();
	ICommands* Commands = Tank;
	const ECommandQueueError Error = Commands->MoveToLocation(Goal, true, Facing, false);
	Check(Error == ECommandQueueError::NoError, TEXT("move_ordered"), TestVehicle.Label);
}

void FAlliedVehiclePassageScenario::OrderVehicles(UWorld& World)
{
	using namespace AlliedPassageScenarioPrivate;
	for (FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		TestVehicle.StartLocation = TestVehicle.Vehicle.IsValid()
			                            ? TestVehicle.Vehicle->GetActorLocation()
			                            : FVector::ZeroVector;
	}
	auto GetProjectedLanePoint = [this, &World](const float AlongLane, const float AcrossLane)
	{
		FVector Point = GetLanePoint(AlongLane, AcrossLane);
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, Point, GoalNavExtent);
		return Point;
	};
	OrderMove(M_HeadOnHarvesterA, GetProjectedLanePoint(LaneHalfLength, -HarvesterLaneOffset));
	OrderMove(M_HeadOnHarvesterB, GetProjectedLanePoint(-LaneHalfLength, -HarvesterLaneOffset));
	OrderMove(M_LaneHarvester, GetProjectedLanePoint(LaneHalfLength, HarvesterLaneOffset));
	OrderMove(M_OncomingTank, GetProjectedLanePoint(-LaneHalfLength, HarvesterLaneOffset));
	if (M_Vehicles.IsValidIndex(M_EnemyTank) && M_Vehicles[M_EnemyTank].Vehicle.IsValid())
	{
		OrderMove(M_EnemyLaneHarvester, M_Vehicles[M_EnemyTank].Vehicle->GetActorLocation());
	}

	ANomadicVehicle* Nomadic = M_Nomadic.Get();
	if (not IsValid(Nomadic))
	{
		return;
	}
	M_Vehicles[M_NomadicIndex].GoalLocation = M_NomadicGoal;
	M_Vehicles[M_NomadicIndex].bHasGoal = true;
	ICommands* NomadicCommands = Nomadic;
	const ECommandQueueError Error = NomadicCommands->CreateBuildingAtLocation(
		M_NomadicGoal, Nomadic->GetActorRotation(), true);
	Check(Error == ECommandQueueError::NoError, TEXT("hq_deploy_ordered"),
	      FString::Printf(TEXT("goal=%s"), *M_NomadicGoal.ToCompactString()));
}

FVector2D FAlliedVehiclePassageScenario::GetHullCenter(const FAlliedPassageTestVehicle& TestVehicle) const
{
	const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
	if (not IsValid(Tank))
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D Forward = FVector2D(Tank->GetActorForwardVector()).GetSafeNormal();
	return TestVehicle.Footprint.GetWorldCenter(FVector2D(Tank->GetActorLocation()), Forward);
}

bool FAlliedVehiclePassageScenario::GetHasArrived(const int32 VehicleIndex) const
{
	return M_Vehicles.IsValidIndex(VehicleIndex) && M_Vehicles[VehicleIndex].ArrivalSeconds >= 0.f;
}

void FAlliedVehiclePassageScenario::TickDriving(UWorld& World, const float DeltaTime)
{
	using namespace AlliedPassageScenarioPrivate;
	SampleVehicles(DeltaTime);
	SamplePairs();
	SampleNomadicDeploy();
	if (not GetIsDrivingDone() && GetGameSecondsInPhase() < DriveTimeoutSeconds)
	{
		return;
	}
	ReportVehicles();
	EvaluateResults();
	Finish();
}

void FAlliedVehiclePassageScenario::SampleVehicles(const float DeltaTime)
{
	using namespace AlliedPassageScenarioPrivate;
	const float SecondsSinceOrders = GetSecondsSinceOrders();
	for (FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		const FVector Location = Tank->GetActorLocation();
		TestVehicle.FarthestFromStart = FMath::Max(
			TestVehicle.FarthestFromStart,
			static_cast<float>(FVector::Dist2D(Location, TestVehicle.StartLocation)));
		if (not TestVehicle.bHasGoal || TestVehicle.ArrivalSeconds >= 0.f)
		{
			continue;
		}
		if (FVector::DistSquared2D(Location, TestVehicle.GoalLocation) <= FMath::Square(GoalTolerance))
		{
			TestVehicle.ArrivalSeconds = SecondsSinceOrders;
			UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE arrived %s after %.1f s"),
			       *TestVehicle.Label, SecondsSinceOrders);
			continue;
		}
		const bool bIsStalled = Tank->GetVelocity().SizeSquared2D() < FMath::Square(StalledSpeed);
		if (bIsStalled && SecondsSinceOrders > StallGraceSeconds)
		{
			TestVehicle.StalledSeconds += DeltaTime;
		}
	}
}

void FAlliedVehiclePassageScenario::SamplePairs()
{
	for (FAlliedPassageTestPair& Pair : M_Pairs)
	{
		const FAlliedPassageTestVehicle& First = M_Vehicles[Pair.FirstIndex];
		const FAlliedPassageTestVehicle& Second = M_Vehicles[Pair.SecondIndex];
		if (not First.Vehicle.IsValid() || not Second.Vehicle.IsValid())
		{
			continue;
		}
		const float Penetration = RTSVehicleFootprint::GetPenetrationDepth2D(
			GetHullCenter(First),
			FVector2D(First.Vehicle->GetActorForwardVector()).GetSafeNormal(),
			First.Footprint,
			GetHullCenter(Second),
			FVector2D(Second.Vehicle->GetActorForwardVector()).GetSafeNormal(),
			Second.Footprint);
		Pair.DeepestPenetration = FMath::Max(Pair.DeepestPenetration, Penetration);
	}
}

void FAlliedVehiclePassageScenario::SampleNomadicDeploy()
{
	const ANomadicVehicle* Nomadic = M_Nomadic.Get();
	if (M_NomadicDeploySeconds >= 0.f || not IsValid(Nomadic) || Nomadic->GetIsInTruckMode())
	{
		return;
	}
	M_NomadicDeploySeconds = GetSecondsSinceOrders();
	M_NomadicDeployGoalDistance = FVector::Dist2D(Nomadic->GetActorLocation(), M_NomadicGoal);
	// Gap between the hull circles of the truck at its goal and the nearest ally that was parked on its way.
	const FAlliedPassageTestVehicle& NomadicEntry = M_Vehicles[M_NomadicIndex];
	float NearestGap = MAX_flt;
	for (const int32 ParkedIndex : M_NomadicParkedTanks)
	{
		const FAlliedPassageTestVehicle& Parked = M_Vehicles[ParkedIndex];
		if (not Parked.Vehicle.IsValid())
		{
			continue;
		}
		const float Gap = FVector2D::Distance(GetHullCenter(Parked), FVector2D(M_NomadicGoal)) -
			Parked.Footprint.GetCornerRadius() - NomadicEntry.Footprint.GetCornerRadius();
		NearestGap = FMath::Min(NearestGap, Gap);
	}
	M_NomadicDeployNearestParkedGap = NearestGap;
	UE_LOG(LogRTSAlliedPassage, Display,
	       TEXT("RTS_ALLIED_PASSAGE hq left truck mode after %.1f s, %.0f cm from its goal, nearest parked gap %.0f"),
	       M_NomadicDeploySeconds, M_NomadicDeployGoalDistance, M_NomadicDeployNearestParkedGap);
}

bool FAlliedVehiclePassageScenario::GetIsDrivingDone() const
{
	using namespace AlliedPassageScenarioPrivate;
	const bool bHarvesterLanesDone = GetHasArrived(M_HeadOnHarvesterA) && GetHasArrived(M_HeadOnHarvesterB) &&
		GetHasArrived(M_LaneHarvester) && GetHasArrived(M_OncomingTank) &&
		GetSecondsSinceOrders() >= EnemyLaneWatchSeconds;
	const bool bNomadicDone = M_NomadicDeploySeconds >= 0.f &&
		GetSecondsSinceOrders() >= M_NomadicDeploySeconds + AfterDeploySeconds;
	return bHarvesterLanesDone && bNomadicDone;
}

void FAlliedVehiclePassageScenario::ReportVehicles() const
{
	for (const FAlliedPassageTestVehicle& TestVehicle : M_Vehicles)
	{
		const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		UE_LOG(LogRTSAlliedPassage, Display,
		       TEXT("RTS_ALLIED_PASSAGE vehicle %s at %s goal %s arrived %.1f stalled %.1f farthest_from_start %.0f"),
		       *TestVehicle.Label, IsValid(Tank) ? *Tank->GetActorLocation().ToCompactString() : TEXT("gone"),
		       TestVehicle.bHasGoal ? *TestVehicle.GoalLocation.ToCompactString() : TEXT("-"),
		       TestVehicle.ArrivalSeconds, TestVehicle.StalledSeconds, TestVehicle.FarthestFromStart);
	}
	for (const FAlliedPassageTestPair& Pair : M_Pairs)
	{
		UE_LOG(LogRTSAlliedPassage, Display, TEXT("RTS_ALLIED_PASSAGE pair %s/%s deepest_penetration %.0f"),
		       *M_Vehicles[Pair.FirstIndex].Label, *M_Vehicles[Pair.SecondIndex].Label, Pair.DeepestPenetration);
	}
}

void FAlliedVehiclePassageScenario::EvaluateResults()
{
	CheckVehiclesStayOnGround(TEXT("end"));
	EvaluateHarvesterResults();
	EvaluateNomadicResults();
}

void FAlliedVehiclePassageScenario::EvaluateHarvesterResults()
{
	using namespace AlliedPassageScenarioPrivate;
	for (const int32 HarvesterIndex : {M_HeadOnHarvesterA, M_HeadOnHarvesterB, M_LaneHarvester})
	{
		if (not M_Vehicles.IsValidIndex(HarvesterIndex))
		{
			continue;
		}
		const FAlliedPassageTestVehicle& Harvester = M_Vehicles[HarvesterIndex];
		const FString Details = FString::Printf(TEXT("%s arrival=%.1f s stalled=%.1f s"),
		                                        *Harvester.Label, Harvester.ArrivalSeconds, Harvester.StalledSeconds);
		Check(Harvester.ArrivalSeconds >= 0.f && Harvester.ArrivalSeconds <= HarvesterMaxArrivalSeconds,
		      TEXT("harvester_arrived_in_time"), Details);
		Check(Harvester.StalledSeconds <= HarvesterMaxStalledSeconds, TEXT("harvester_not_held_up"), Details);
	}
	if (M_Pairs.IsValidIndex(M_PairHeadOnHarvesters))
	{
		const float Penetration = M_Pairs[M_PairHeadOnHarvesters].DeepestPenetration;
		Check(Penetration >= PassedThroughPenetration, TEXT("harvesters_drove_through_each_other"),
		      FString::Printf(TEXT("deepest=%.0f cm"), Penetration));
	}
	if (M_Pairs.IsValidIndex(M_PairHarvesterOncomingTank))
	{
		const float Penetration = M_Pairs[M_PairHarvesterOncomingTank].DeepestPenetration;
		Check(Penetration >= PassedThroughPenetration, TEXT("harvester_and_allied_tank_drove_through_each_other"),
		      FString::Printf(TEXT("deepest=%.0f cm"), Penetration));
	}
	if (M_Vehicles.IsValidIndex(M_LaneParkedTank))
	{
		const float Shove = M_Vehicles[M_LaneParkedTank].FarthestFromStart;
		const float Penetration = M_Pairs.IsValidIndex(M_PairHarvesterParkedTank)
			                          ? M_Pairs[M_PairHarvesterParkedTank].DeepestPenetration
			                          : 0.f;
		Check(Shove <= ParkedAllyMaxShove, TEXT("parked_ally_not_shoved_by_harvester"),
		      FString::Printf(TEXT("moved=%.0f cm deepest=%.0f cm"), Shove, Penetration));
	}
	if (M_Pairs.IsValidIndex(M_PairHarvesterEnemy))
	{
		const float Penetration = M_Pairs[M_PairHarvesterEnemy].DeepestPenetration;
		Check(Penetration <= EnemyMaxPenetration, TEXT("enemy_tank_blocks_harvester"),
		      FString::Printf(TEXT("deepest=%.0f cm"), Penetration));
	}
}

void FAlliedVehiclePassageScenario::EvaluateNomadicResults()
{
	using namespace AlliedPassageScenarioPrivate;
	Check(M_NomadicDeploySeconds >= 0.f && M_NomadicDeploySeconds <= NomadicMaxDeploySeconds,
	      TEXT("hq_reached_building_spot_in_time"), FString::Printf(TEXT("after=%.1f s"), M_NomadicDeploySeconds));
	Check(M_NomadicDeployGoalDistance >= 0.f && M_NomadicDeployGoalDistance <= NomadicMaxDeployGoalDistance,
	      TEXT("hq_drove_onto_building_spot"), FString::Printf(TEXT("distance=%.0f cm"), M_NomadicDeployGoalDistance));
	Check(M_NomadicDeployNearestParkedGap >= 0.f, TEXT("building_spot_free_of_allies"),
	      FString::Printf(TEXT("nearest_gap=%.0f cm"), M_NomadicDeployNearestParkedGap));
	for (const int32 ParkedIndex : M_NomadicParkedTanks)
	{
		const FAlliedPassageTestVehicle& Parked = M_Vehicles[ParkedIndex];
		Check(Parked.FarthestFromStart >= PushedAllyMinimumMove, TEXT("ally_in_hq_way_pushed_aside"),
		      FString::Printf(TEXT("%s moved=%.0f cm"), *Parked.Label, Parked.FarthestFromStart));
	}
}

#endif
