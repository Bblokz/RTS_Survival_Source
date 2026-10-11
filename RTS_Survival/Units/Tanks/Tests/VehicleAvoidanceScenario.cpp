#include "VehicleAvoidanceScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSVehicleAvoidance, Log, All);

namespace VehicleAvoidanceScenarioPrivate
{
	constexpr int32 PlayerOwner = 1;
	constexpr float WarmUpSeconds = 4.f;
	constexpr float CollectIntervalSeconds = 1.f;
	constexpr float ReportIntervalSeconds = 10.f;
	constexpr float TraceIntervalSeconds = 4.f;
	constexpr float ScreenshotIntervalSeconds = 12.f;
	// Late enough for a hundred vehicles to be under way, and long enough to average out single frames.
	constexpr float StatDumpStartSeconds = 55.f;
	constexpr int32 StatDumpFrames = 1200;
	constexpr float OverviewCameraHeight = 5500.f;
	constexpr float ObservedTraceMinimumAge = 30.f;
	// Hulls resting against each other touch by a few centimetres; only deeper than this counts as clipping.
	constexpr float CountedOverlapDepth = 20.f;
	// Deeper than this two hulls are well inside each other; every such pair is reported once.
	constexpr float DeepOverlapDepth = 20.f;
	// No vehicle drives this far in one frame.
	constexpr float JumpDistance = 60.f;
	// A vehicle that tips this much further than any before it is named in the log.
	constexpr float TiltReportStepDegrees = 8.f;
	constexpr float GroundTraceHalfHeight = 1500.f;
	constexpr float StalledSpeed = 15.f;
	constexpr float DrivePhaseSeconds = 150.f;
	// A phase may end early once every vehicle has been idle this long.
	constexpr float DrivePhaseMinimumSeconds = 8.f;
	constexpr float FormationCenterOffset = 2500.f;
	constexpr float BlockDistanceFromCenter = 4000.f;
	constexpr float FormationYaw = 90.f;
	// Vehicles accept a goal from a few metres away; further off than this they did not get to their slot.
	constexpr float GoalLocationTolerance = 400.f;
	constexpr float GoalYawTolerance = 5.f;
	constexpr float SpawnSpacingScale = 3.f;
	constexpr float SpawnHeightOffset = 60.f;

	FVehicleAvoidancePairStats& GetPairStats(
		FVehicleAvoidanceStats& Stats,
		const int32 OwnerA,
		const int32 OwnerB)
	{
		if (OwnerA != OwnerB)
		{
			return Stats.HostilePairs;
		}
		return OwnerA == PlayerOwner ? Stats.PlayerPairs : Stats.EnemyPairs;
	}

	FVector2D GetGroundForward(const ATrackedTankMaster& Tank)
	{
		const FVector2D Forward(Tank.GetActorForwardVector());
		return Forward.GetSafeNormal();
	}

	/** One hull box in the world: its middle, its three axes and how far it reaches along each. */
	struct FWorldHullBox
	{
		FVector Center;
		FVector Axes[3];
		FVector Extent;

		FWorldHullBox(const ATrackedTankMaster& Tank, const FBox& HullBox)
		{
			const FQuat Rotation = Tank.GetActorQuat();
			Center = Tank.GetActorLocation() + Rotation.RotateVector(HullBox.GetCenter());
			Axes[0] = Rotation.GetForwardVector();
			Axes[1] = Rotation.GetRightVector();
			Axes[2] = Rotation.GetUpVector();
			Extent = HullBox.GetExtent();
		}

		float GetReachAlong(const FVector& Axis) const
		{
			return Extent.X * FMath::Abs(FVector::DotProduct(Axes[0], Axis)) +
				Extent.Y * FMath::Abs(FVector::DotProduct(Axes[1], Axis)) +
				Extent.Z * FMath::Abs(FVector::DotProduct(Axes[2], Axis));
		}
	};

	/** @return How deep two hull boxes are inside each other in space; zero when any axis separates them. */
	float GetHullBoxPenetrationDepth(
		const ATrackedTankMaster& FirstTank,
		const FBox& FirstHullBox,
		const ATrackedTankMaster& SecondTank,
		const FBox& SecondHullBox)
	{
		const FWorldHullBox First(FirstTank, FirstHullBox);
		const FWorldHullBox Second(SecondTank, SecondHullBox);
		const FVector CenterDelta = Second.Center - First.Center;
		float SmallestOverlap = MAX_flt;
		const auto TestAxis = [&](const FVector& Axis)
		{
			if (Axis.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				return true;
			}
			const FVector UnitAxis = Axis.GetUnsafeNormal();
			const float Overlap = First.GetReachAlong(UnitAxis) + Second.GetReachAlong(UnitAxis) -
				FMath::Abs(FVector::DotProduct(CenterDelta, UnitAxis));
			SmallestOverlap = FMath::Min(SmallestOverlap, Overlap);
			return Overlap > 0.f;
		};
		for (int32 FirstAxis = 0; FirstAxis < 3; ++FirstAxis)
		{
			if (not TestAxis(First.Axes[FirstAxis]) || not TestAxis(Second.Axes[FirstAxis]))
			{
				return 0.f;
			}
			for (int32 SecondAxis = 0; SecondAxis < 3; ++SecondAxis)
			{
				if (not TestAxis(FVector::CrossProduct(First.Axes[FirstAxis], Second.Axes[SecondAxis])))
				{
					return 0.f;
				}
			}
		}
		return SmallestOverlap;
	}

	bool GetIsOnMovementCommand(ATrackedTankMaster& Tank)
	{
		const UCommandData* CommandData = Tank.GetIsValidCommandData();
		return IsValid(CommandData) && CommandData->GetIsCurrentCommandAMovementCommand();
	}

	// Done with the move and the turn it was ordered; a tank that then starts shooting at something is still done.
	bool GetHasFinishedOrder(ATrackedTankMaster& Tank)
	{
		if (Tank.GetIsUnitIdle())
		{
			return true;
		}
		const UCommandData* CommandData = Tank.GetIsValidCommandData();
		return IsValid(CommandData) && not CommandData->GetIsCurrentCommandAMovementCommand() &&
			CommandData->GetCurrentlyActiveCommandType() != EAbilityID::IdRotateTowards;
	}
}

void FVehicleAvoidanceScenario::Start()
{
	M_Phase = EVehicleAvoidancePhase::WarmingUp;
	bM_DriveVehicles = FParse::Param(FCommandLine::Get(), TEXT("VehicleAvoidanceDrive"));
	FParse::Value(FCommandLine::Get(), TEXT("VehicleAvoidanceSeconds="), M_ObserveSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("VehicleAvoidanceSpawn="), M_ExtraVehiclesToSpawn);
	bM_TraceVehicles = FParse::Param(FCommandLine::Get(), TEXT("VehicleAvoidanceTrace"));
	bM_TakeScreenshots = FParse::Param(FCommandLine::Get(), TEXT("VehicleAvoidanceScreenshots"));
	bM_DumpStats = FParse::Param(FCommandLine::Get(), TEXT("VehicleAvoidancePerf"));
	UE_LOG(LogRTSVehicleAvoidance, Display, TEXT("RTS_VEHICLE_AVOIDANCE scheduled drive=%d"), bM_DriveVehicles ? 1 : 0);
}

namespace VehicleAvoidanceScenarioPrivate
{
	// Shows what the hull of a vehicle class is made of for the physics, once per class.
	void LogChassisCollision(const ATrackedTankMaster& Tank)
	{
		static TSet<FName> LoggedClasses;
		const USkeletalMeshComponent* ChassisMesh = Tank.GetTankMesh();
		if (not IsValid(ChassisMesh) || LoggedClasses.Contains(Tank.GetClass()->GetFName()))
		{
			return;
		}
		LoggedClasses.Add(Tank.GetClass()->GetFName());
		const UPhysicsAsset* PhysicsAsset = ChassisMesh->GetPhysicsAsset();
		UE_LOG(
			LogRTSVehicleAvoidance,
			Display,
			TEXT("RTS_VEHICLE_AVOIDANCE chassis class=%s object_type=%d response_player=%d response_enemy=%d collision_enabled=%d simulating=%d bodies=%d mass=%.0f"),
			*Tank.GetClass()->GetName(),
			static_cast<int32>(ChassisMesh->GetCollisionObjectType()),
			static_cast<int32>(ChassisMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel9)),
			static_cast<int32>(ChassisMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel6)),
			static_cast<int32>(ChassisMesh->GetCollisionEnabled()),
			ChassisMesh->IsSimulatingPhysics() ? 1 : 0,
			IsValid(PhysicsAsset) ? PhysicsAsset->SkeletalBodySetups.Num() : -1,
			ChassisMesh->GetMass());
		for (const UActorComponent* Component : Tank.GetComponents())
		{
			const UBoxComponent* HullBlocker = Cast<UBoxComponent>(Component);
			if (not IsValid(HullBlocker) || HullBlocker->GetFName() != TEXT("HullBlocker"))
			{
				continue;
			}
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE hull_blocker class=%s welded=%d extent=%s relative=%s collision_enabled=%d"),
				*Tank.GetClass()->GetName(),
				HullBlocker->IsWelded() ? 1 : 0,
				*HullBlocker->GetScaledBoxExtent().ToCompactString(),
				*HullBlocker->GetRelativeLocation().ToCompactString(),
				static_cast<int32>(HullBlocker->GetCollisionEnabled()));
		}
		if (not IsValid(PhysicsAsset))
		{
			return;
		}
		for (const USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (not IsValid(BodySetup))
			{
				continue;
			}
			const FBox BodyBox = BodySetup->AggGeom.CalcAABB(FTransform::Identity);
			const FBodyInstance* BodyInstance = ChassisMesh->GetBodyInstance(BodySetup->BoneName);
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE body bone=%s boxes=%d spheres=%d capsules=%d convex=%d extent=%s center=%s physics_type=%d instance=%d instance_response_player=%d"),
				*BodySetup->BoneName.ToString(),
				BodySetup->AggGeom.BoxElems.Num(),
				BodySetup->AggGeom.SphereElems.Num(),
				BodySetup->AggGeom.SphylElems.Num(),
				BodySetup->AggGeom.ConvexElems.Num(),
				*BodyBox.GetExtent().ToCompactString(),
				*BodyBox.GetCenter().ToCompactString(),
				static_cast<int32>(BodySetup->PhysicsType),
				BodyInstance != nullptr ? 1 : 0,
				BodyInstance != nullptr
					? static_cast<int32>(BodyInstance->GetResponseToChannel(ECC_GameTraceChannel9))
					: -1);
		}
	}
}

void FVehicleAvoidanceScenario::CollectVehicles(UWorld& World)
{
	for (TActorIterator<ATrackedTankMaster> TankIterator(&World); TankIterator; ++TankIterator)
	{
		ATrackedTankMaster* Tank = *TankIterator;
		if (not IsValid(Tank) || Tank->IsHidden() || not IsValid(Tank->GetRTSComponent()))
		{
			continue;
		}
		const bool bIsKnown = M_Vehicles.ContainsByPredicate([Tank](const FVehicleAvoidanceTestVehicle& Vehicle)
		{
			return Vehicle.Tank.Get() == Tank;
		});
		if (bIsKnown)
		{
			continue;
		}
		const FRTSVehicleFootprint Footprint = RTSVehicleFootprint::BuildFromChassisMesh(Tank->GetTankMesh());
		if (not Footprint.GetIsValid())
		{
			continue;
		}

		VehicleAvoidanceScenarioPrivate::LogChassisCollision(*Tank);
		FVehicleAvoidanceTestVehicle Vehicle;
		Vehicle.Tank = Tank;
		Vehicle.Footprint = Footprint;
		Vehicle.HullBox = RTSVehicleFootprint::GetChassisCollisionBox(
			Tank->GetTankMesh(),
			FTransform(FQuat::Identity, FVector::ZeroVector, Tank->GetTankMesh()->GetComponentScale()));
		Vehicle.OwningPlayer = Tank->GetRTSComponent()->GetOwningPlayer();
		Vehicle.FirstSeenSeconds = World.GetTimeSeconds();
		M_Vehicles.Add(Vehicle);
		M_LastLocations.Add(Tank->GetActorLocation());
		UE_LOG(
			LogRTSVehicleAvoidance,
			Display,
			TEXT("RTS_VEHICLE_AVOIDANCE vehicle name=%s owner=%d half_length=%.0f half_width=%.0f formation_radius=%.0f"),
			*Tank->GetName(),
			Vehicle.OwningPlayer,
			Footprint.HalfLength,
			Footprint.HalfWidth,
			Tank->GetRTSComponent()->GetFormationUnitInnerRadius());
	}
}

void FVehicleAvoidanceScenario::SpawnExtraVehicles(UWorld& World)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	TArray<ATrackedTankMaster*> Templates;
	FVector Centroid = FVector::ZeroVector;
	float LargestRadius = 0.f;
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank) || Vehicle.OwningPlayer != PlayerOwner)
		{
			continue;
		}
		Templates.Add(Tank);
		Centroid += Tank->GetActorLocation();
		LargestRadius = FMath::Max(LargestRadius, Vehicle.Footprint.GetCornerRadius());
	}
	if (Templates.IsEmpty() || M_ExtraVehiclesToSpawn <= 0)
	{
		return;
	}
	Centroid /= Templates.Num();

	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(M_ExtraVehiclesToSpawn)));
	const float Spacing = LargestRadius * SpawnSpacingScale;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 SpawnIndex = 0; SpawnIndex < M_ExtraVehiclesToSpawn; ++SpawnIndex)
	{
		const ATrackedTankMaster* Template = Templates[SpawnIndex % Templates.Num()];
		// Behind the vehicles that are already there, so nothing spawns inside another hull.
		const FVector SpawnLocation = Centroid + FVector(
			-FormationCenterOffset - (SpawnIndex / Columns) * Spacing,
			((SpawnIndex % Columns) - Columns * 0.5f) * Spacing,
			SpawnHeightOffset);
		World.SpawnActor<ATrackedTankMaster>(
			Template->GetClass(),
			FTransform(Template->GetActorRotation(), SpawnLocation),
			SpawnParameters);
	}
	UE_LOG(LogRTSVehicleAvoidance, Display, TEXT("RTS_VEHICLE_AVOIDANCE spawned extra=%d"), M_ExtraVehiclesToSpawn);
	M_ExtraVehiclesToSpawn = 0;
}

void FVehicleAvoidanceScenario::EnterPhase(
	UWorld& World,
	const EVehicleAvoidancePhase NewPhase,
	const float DurationSeconds)
{
	M_Phase = NewPhase;
	M_Stats = FVehicleAvoidanceStats();
	M_Stats.StartSeconds = World.GetTimeSeconds();
	M_PhaseEndSeconds = M_Stats.StartSeconds + DurationSeconds;
	M_NextReportSeconds = M_Stats.StartSeconds + VehicleAvoidanceScenarioPrivate::ReportIntervalSeconds;
	for (int32 VehicleIndex = 0; VehicleIndex < M_Vehicles.Num(); ++VehicleIndex)
	{
		// Driving done before the phase began is not a jump inside it.
		if (const ATrackedTankMaster* Tank = M_Vehicles[VehicleIndex].Tank.Get())
		{
			M_LastLocations[VehicleIndex] = Tank->GetActorLocation();
		}
	}
	for (FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		Vehicle.bHasGoal = false;
		Vehicle.ArrivalSeconds = -1.f;
	}
}

void FVehicleAvoidanceScenario::ReportNewDeepOverlap(
	const UWorld& World,
	const int32 FirstIndex,
	const int32 SecondIndex,
	const float Depth)
{
	const uint64 PairKey = (static_cast<uint64>(FirstIndex) << 32) | static_cast<uint64>(SecondIndex);
	if (M_ReportedDeepOverlaps.Contains(PairKey))
	{
		return;
	}
	M_ReportedDeepOverlaps.Add(PairKey);
	const ATrackedTankMaster* FirstTank = M_Vehicles[FirstIndex].Tank.Get();
	const ATrackedTankMaster* SecondTank = M_Vehicles[SecondIndex].Tank.Get();
	UE_LOG(
		LogRTSVehicleAvoidance,
		Display,
		TEXT("RTS_VEHICLE_AVOIDANCE deep_overlap depth=%.0f tilt=%.0f/%.0f first=%s age=%.1f at=(%.0f,%.0f,%.0f) speed=%.0f second=%s age=%.1f at=(%.0f,%.0f,%.0f) speed=%.0f"),
		Depth,
		FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FirstTank->GetActorUpVector().Z, -1.0, 1.0))),
		FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(SecondTank->GetActorUpVector().Z, -1.0, 1.0))),
		*FirstTank->GetName(),
		World.GetTimeSeconds() - M_Vehicles[FirstIndex].FirstSeenSeconds,
		FirstTank->GetActorLocation().X,
		FirstTank->GetActorLocation().Y,
		FirstTank->GetActorLocation().Z,
		FirstTank->GetVelocity().Size(),
		*SecondTank->GetName(),
		World.GetTimeSeconds() - M_Vehicles[SecondIndex].FirstSeenSeconds,
		SecondTank->GetActorLocation().X,
		SecondTank->GetActorLocation().Y,
		SecondTank->GetActorLocation().Z,
		SecondTank->GetVelocity().Size());
}

void FVehicleAvoidanceScenario::SamplePairs(const UWorld& World, const float DeltaTime)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	M_Stats.PlayerPairs.OverlappingPairsNow = 0;
	M_Stats.EnemyPairs.OverlappingPairsNow = 0;
	M_Stats.HostilePairs.OverlappingPairsNow = 0;
	for (int32 FirstIndex = 0; FirstIndex < M_Vehicles.Num(); ++FirstIndex)
	{
		const ATrackedTankMaster* FirstTank = M_Vehicles[FirstIndex].Tank.Get();
		if (not IsValid(FirstTank))
		{
			continue;
		}
		const FVector2D FirstForward = GetGroundForward(*FirstTank);
		const FVector2D FirstCenter = M_Vehicles[FirstIndex].Footprint.GetWorldCenter(
			FVector2D(FirstTank->GetActorLocation()),
			FirstForward);
		for (int32 SecondIndex = FirstIndex + 1; SecondIndex < M_Vehicles.Num(); ++SecondIndex)
		{
			const ATrackedTankMaster* SecondTank = M_Vehicles[SecondIndex].Tank.Get();
			if (not IsValid(SecondTank))
			{
				continue;
			}
			const FVector2D SecondForward = GetGroundForward(*SecondTank);
			const float Depth = RTSVehicleFootprint::GetPenetrationDepth2D(
				FirstCenter,
				FirstForward,
				M_Vehicles[FirstIndex].Footprint,
				M_Vehicles[SecondIndex].Footprint.GetWorldCenter(
					FVector2D(SecondTank->GetActorLocation()),
					SecondForward),
				SecondForward,
				M_Vehicles[SecondIndex].Footprint);
			if (Depth < CountedOverlapDepth)
			{
				continue;
			}
			const float DepthInSpace = GetHullBoxPenetrationDepth(
				*FirstTank,
				M_Vehicles[FirstIndex].HullBox,
				*SecondTank,
				M_Vehicles[SecondIndex].HullBox);
			if (DepthInSpace < CountedOverlapDepth)
			{
				continue;
			}
			FVehicleAvoidancePairStats& PairStats = GetPairStats(
				M_Stats,
				M_Vehicles[FirstIndex].OwningPlayer,
				M_Vehicles[SecondIndex].OwningPlayer);
			PairStats.OverlapPairSeconds += DeltaTime;
			PairStats.DeepestOverlap = FMath::Max(PairStats.DeepestOverlap, DepthInSpace);
			if (Depth > DeepOverlapDepth)
			{
				ReportNewDeepOverlap(World, FirstIndex, SecondIndex, DepthInSpace);
			}
			++PairStats.OverlappingPairsNow;
		}
	}
}

void FVehicleAvoidanceScenario::SampleFrame(UWorld& World, const float DeltaTime)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	const float PhaseSeconds = World.GetTimeSeconds() - M_Stats.StartSeconds;
	for (int32 VehicleIndex = 0; VehicleIndex < M_Vehicles.Num(); ++VehicleIndex)
	{
		FVehicleAvoidanceTestVehicle& Vehicle = M_Vehicles[VehicleIndex];
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		const FVector Location = Tank->GetActorLocation();
		const float FrameDistance = FVector::Dist2D(Location, M_LastLocations[VehicleIndex]);
		M_LastLocations[VehicleIndex] = Location;
		if (FrameDistance > JumpDistance)
		{
			++M_Stats.Jumps;
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE jump name=%s distance=%.0f age=%.1f"),
				*Tank->GetName(),
				FrameDistance,
				World.GetTimeSeconds() - Vehicle.FirstSeenSeconds);
		}
		else
		{
			M_Stats.DistanceDriven += FrameDistance;
		}
		const float TiltDegrees = static_cast<float>(
			FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Tank->GetActorUpVector().Z, -1.0, 1.0))));
		if (TiltDegrees > M_Stats.LastReportedTiltDegrees + TiltReportStepDegrees)
		{
			M_Stats.LastReportedTiltDegrees = TiltDegrees;
			// The height of the ground under it tells a tank on a slope or an obstacle from one on top of another.
			FHitResult GroundHit;
			FCollisionQueryParams GroundQuery;
			GroundQuery.AddIgnoredActor(Tank);
			const bool bFoundGround = World.LineTraceSingleByChannel(
				GroundHit,
				Location + FVector(0.f, 0.f, GroundTraceHalfHeight),
				Location - FVector(0.f, 0.f, GroundTraceHalfHeight),
				ECC_GameTraceChannel2,
				GroundQuery);
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE tilt name=%s degrees=%.0f ground_z=%.0f ground_slope=%.0f ground=%s age=%.1f at=(%.0f,%.0f,%.0f) speed=%.0f mass=%.0f"),
				*Tank->GetName(),
				TiltDegrees,
				bFoundGround ? GroundHit.ImpactPoint.Z : -99999.f,
				bFoundGround ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(GroundHit.ImpactNormal.Z, -1.0, 1.0))) : -1.f,
				bFoundGround && GroundHit.GetActor() != nullptr ? *GroundHit.GetActor()->GetName() : TEXT("none"),
				World.GetTimeSeconds() - Vehicle.FirstSeenSeconds,
				Location.X,
				Location.Y,
				Location.Z,
				Tank->GetVelocity().Size(),
				IsValid(Tank->GetTankMesh()) ? Tank->GetTankMesh()->GetMass() : 0.f);
		}
		M_Stats.SteepestTiltDegrees = FMath::Max(M_Stats.SteepestTiltDegrees, TiltDegrees);

		const bool bIsIdle = Tank->GetIsUnitIdle();
		if (bIsIdle && not Vehicle.bWasIdle)
		{
			++M_Stats.Arrivals;
			Vehicle.ArrivalSeconds = PhaseSeconds;
		}
		Vehicle.bWasIdle = bIsIdle;
		if (bIsIdle || not GetIsOnMovementCommand(*Tank))
		{
			continue;
		}
		M_Stats.MovingVehicleSeconds += DeltaTime;
		if (Tank->GetVelocity().SizeSquared2D() < FMath::Square(StalledSpeed))
		{
			M_Stats.StalledVehicleSeconds += DeltaTime;
		}
	}
	SamplePairs(World, DeltaTime);
}

void FVehicleAvoidanceScenario::ReportProgress(const UWorld& World, const TCHAR* Label) const
{
	int32 LivingCount = 0;
	int32 BusyCount = 0;
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		++LivingCount;
		BusyCount += Tank->GetIsUnitIdle() ? 0 : 1;
	}
	UE_LOG(
		LogRTSVehicleAvoidance,
		Display,
		TEXT("RTS_VEHICLE_AVOIDANCE %s t=%.0f vehicles=%d busy=%d arrivals=%d overlapping_now(pp/ee/pe)=%d/%d/%d overlap_pair_seconds(pp/ee/pe)=%.1f/%.1f/%.1f deepest(pp/ee/pe)=%.0f/%.0f/%.0f stalled_s=%.1f moving_s=%.1f distance_m=%.0f tilt_deg=%.1f jumps=%d"),
		Label,
		World.GetTimeSeconds() - M_Stats.StartSeconds,
		LivingCount,
		BusyCount,
		M_Stats.Arrivals,
		M_Stats.PlayerPairs.OverlappingPairsNow,
		M_Stats.EnemyPairs.OverlappingPairsNow,
		M_Stats.HostilePairs.OverlappingPairsNow,
		M_Stats.PlayerPairs.OverlapPairSeconds,
		M_Stats.EnemyPairs.OverlapPairSeconds,
		M_Stats.HostilePairs.OverlapPairSeconds,
		M_Stats.PlayerPairs.DeepestOverlap,
		M_Stats.EnemyPairs.DeepestOverlap,
		M_Stats.HostilePairs.DeepestOverlap,
		M_Stats.StalledVehicleSeconds,
		M_Stats.MovingVehicleSeconds,
		M_Stats.DistanceDriven / 100.0,
		M_Stats.SteepestTiltDegrees,
		M_Stats.Jumps);
}

void FVehicleAvoidanceScenario::UpdateOverviewCamera(UWorld& World)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	APlayerController* PlayerController = World.GetFirstPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	// Above where most vehicles that are under way are; the picture shows whether hulls keep apart there.
	FVector Centroid = FVector::ZeroVector;
	int32 VehicleCount = 0;
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (IsValid(Tank) && Vehicle.OwningPlayer == PlayerOwner)
		{
			Centroid += Tank->GetActorLocation();
			++VehicleCount;
		}
	}
	if (VehicleCount == 0)
	{
		return;
	}
	Centroid /= VehicleCount;
	if (not M_OverviewCamera.IsValid())
	{
		M_OverviewCamera = World.SpawnActor<ACameraActor>();
	}
	if (not M_OverviewCamera.IsValid())
	{
		return;
	}
	M_OverviewCamera->SetActorLocationAndRotation(
		Centroid + FVector(0.f, 0.f, OverviewCameraHeight),
		FRotator(-90.f, 0.f, 0.f));
	// The game's own camera takes the view back; it is handed to the overview camera again every frame.
	PlayerController->SetViewTarget(M_OverviewCamera.Get());

	const float NowSeconds = World.GetTimeSeconds();
	if (NowSeconds < M_NextScreenshotSeconds)
	{
		return;
	}
	M_NextScreenshotSeconds = NowSeconds + ScreenshotIntervalSeconds;
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(
		TEXT("VehicleAvoidance_%s_%02d.png"),
		*World.GetName(),
		M_ScreenshotCount++);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSVehicleAvoidance, Display, TEXT("RTS_VEHICLE_AVOIDANCE screenshot file=%s"), *FileName);
}

void FVehicleAvoidanceScenario::ReportBusyVehicles(const UWorld& World) const
{
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		// On a map that orders its own vehicles around, only those of the player that are at it for a while.
		const bool bIsFollowed = M_Phase == EVehicleAvoidancePhase::Observing
			? Vehicle.OwningPlayer == VehicleAvoidanceScenarioPrivate::PlayerOwner &&
			World.GetTimeSeconds() - Vehicle.FirstSeenSeconds > VehicleAvoidanceScenarioPrivate::ObservedTraceMinimumAge
			: Vehicle.bHasGoal;
		if (not IsValid(Tank) || not bIsFollowed || VehicleAvoidanceScenarioPrivate::GetHasFinishedOrder(*Tank))
		{
			continue;
		}
		const AAITankMaster* TankController = Tank->GetAIController();
		const UTrackPathFollowingComponent* PathFollowing = IsValid(TankController)
			? Cast<UTrackPathFollowingComponent>(TankController->GetPathFollowingComponent())
			: nullptr;
		if (not IsValid(PathFollowing))
		{
			continue;
		}
		const FVehicleAvoidanceState& State = PathFollowing->GetVehicleAvoidanceState();
		const UCommandData* CommandData = Tank->GetIsValidCommandData();
		UE_LOG(
			LogRTSVehicleAvoidance,
			Display,
			TEXT("RTS_VEHICLE_AVOIDANCE trace t=%.0f name=%s command=%d status=%d at=(%.0f,%.0f) yaw=%.0f speed=%.0f to_goal=%.0f mode=%d cap=%.0f push=%.2f waited=%.1f goal_blocked=%.1f blocked=%.1f blocker=%s gap=%.0f"),
			World.GetTimeSeconds() - M_Stats.StartSeconds,
			*Tank->GetName(),
			IsValid(CommandData) ? static_cast<int32>(CommandData->GetCurrentlyActiveCommandType()) : -1,
			static_cast<int32>(PathFollowing->GetStatus()),
			Tank->GetActorLocation().X,
			Tank->GetActorLocation().Y,
			Tank->GetActorRotation().Yaw,
			Tank->GetVelocity().Size2D(),
			FVector::Dist2D(
				Tank->GetActorLocation(),
				Vehicle.bHasGoal ? Vehicle.GoalLocation : State.LastGoalLocation),
			static_cast<int32>(State.Mode),
			State.LastSpeedCap < MAX_flt ? State.LastSpeedCap : -1.f,
			State.LastPush,
			State.WaitingSeconds,
			State.GoalBlockedSeconds,
			State.BlockedSeconds,
			State.LastBlocker.IsValid() ? *State.LastBlocker->GetName() : TEXT("none"),
			State.LastBlocker.IsValid() ? State.LastBlockerGap : -1.f);
	}
}

void FVehicleAvoidanceScenario::ReportPhaseResult(const UWorld& World, const TCHAR* PhaseName) const
{
	using namespace VehicleAvoidanceScenarioPrivate;
	ReportProgress(World, *FString::Printf(TEXT("RESULT phase=%s"), PhaseName));
	ReportBusyVehicles(World);

	int32 GoalCount = 0;
	int32 InterruptedCount = 0;
	int32 FinishedCount = 0;
	int32 OnSlotCount = 0;
	int32 FacingCount = 0;
	float WorstLocationError = 0.f;
	float WorstYawError = 0.f;
	float SlowestArrival = 0.f;
	double TotalLocationError = 0.0;
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank) || not Vehicle.bHasGoal)
		{
			continue;
		}
		// A tank that the game gave something else to do (it starts a fight) no longer carries out the order;
		// where and how it stands says nothing about the order then.
		const UCommandData* CommandData = Tank->GetIsValidCommandData();
		const bool bWasGivenOtherWork = not Tank->GetIsUnitIdle() && IsValid(CommandData) &&
			not CommandData->GetIsCurrentCommandAMovementCommand() &&
			CommandData->GetCurrentlyActiveCommandType() != EAbilityID::IdRotateTowards;
		if (bWasGivenOtherWork)
		{
			++InterruptedCount;
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE interrupted phase=%s name=%s command=%d"),
				PhaseName,
				*Tank->GetName(),
				static_cast<int32>(CommandData->GetCurrentlyActiveCommandType()));
			continue;
		}
		++GoalCount;
		const bool bIsFinished = GetHasFinishedOrder(*Tank);
		FinishedCount += bIsFinished ? 1 : 0;
		const float LocationError = FVector::Dist2D(Tank->GetActorLocation(), Vehicle.GoalLocation);
		const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Tank->GetActorRotation().Yaw, Vehicle.GoalYaw));
		OnSlotCount += LocationError <= GoalLocationTolerance ? 1 : 0;
		FacingCount += YawError <= GoalYawTolerance ? 1 : 0;
		WorstLocationError = FMath::Max(WorstLocationError, LocationError);
		WorstYawError = FMath::Max(WorstYawError, YawError);
		TotalLocationError += LocationError;
		SlowestArrival = FMath::Max(SlowestArrival, Vehicle.ArrivalSeconds);
		if (not bIsFinished || LocationError > GoalLocationTolerance || YawError > GoalYawTolerance)
		{
			const AAITankMaster* TankController = Tank->GetAIController();
			const UTrackPathFollowingComponent* PathFollowing = IsValid(TankController)
				? Cast<UTrackPathFollowingComponent>(TankController->GetPathFollowingComponent())
				: nullptr;
			UE_LOG(
				LogRTSVehicleAvoidance,
				Display,
				TEXT("RTS_VEHICLE_AVOIDANCE miss phase=%s name=%s finished=%d location_error=%.0f yaw_error=%.1f move_end=%d last_goal_error=%.0f path_end_error=%.0f displaced_others=%d"),
				PhaseName,
				*Tank->GetName(),
				bIsFinished ? 1 : 0,
				LocationError,
				YawError,
				IsValid(PathFollowing) ? static_cast<int32>(PathFollowing->GetVehicleAvoidanceState().LastMoveEnd) : -1,
				IsValid(PathFollowing)
					? FVector::Dist2D(PathFollowing->GetVehicleAvoidanceState().LastGoalLocation, Vehicle.GoalLocation)
					: -1.f,
				IsValid(PathFollowing) ? PathFollowing->GetVehicleAvoidanceState().LastPathEndToGoalDistance : -1.f,
				IsValid(PathFollowing) ? PathFollowing->GetVehicleAvoidanceState().DisplacementsAsked : -1);
		}
	}
	if (GoalCount == 0)
	{
		return;
	}
	UE_LOG(
		LogRTSVehicleAvoidance,
		Display,
		TEXT("RTS_VEHICLE_AVOIDANCE GOALS phase=%s ordered=%d interrupted=%d finished=%d on_slot=%d facing=%d worst_location_error=%.0f avg_location_error=%.0f worst_yaw_error=%.1f slowest_arrival_s=%.1f"),
		PhaseName,
		GoalCount,
		InterruptedCount,
		FinishedCount,
		OnSlotCount,
		FacingCount,
		WorstLocationError,
		TotalLocationError / GoalCount,
		WorstYawError,
		SlowestArrival);
}

bool FVehicleAvoidanceScenario::GetAreAllDrivenVehiclesIdle() const
{
	for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (IsValid(Tank) && Vehicle.bHasGoal && not VehicleAvoidanceScenarioPrivate::GetHasFinishedOrder(*Tank))
		{
			return false;
		}
	}
	return true;
}

void FVehicleAvoidanceScenario::OrderSamePoint()
{
	for (FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = Vehicle.Tank.Get();
		if (not IsValid(Tank) || Vehicle.OwningPlayer != VehicleAvoidanceScenarioPrivate::PlayerOwner)
		{
			continue;
		}
		// A tank that goes after every enemy it sees drops the order; the test is about carrying the order out.
		Tank->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
		Vehicle.bHasGoal = true;
		Vehicle.GoalLocation = M_DriveCenter;
		Vehicle.GoalYaw = Tank->GetActorRotation().Yaw;
		Tank->MoveToLocation(M_DriveCenter, true, FRotator(0.f, Vehicle.GoalYaw, 0.f));
	}
}

TArray<int32> FVehicleAvoidanceScenario::GetPlayerVehicleIndices() const
{
	TArray<int32> VehicleIndices;
	for (int32 VehicleIndex = 0; VehicleIndex < M_Vehicles.Num(); ++VehicleIndex)
	{
		const bool bIsPlayerVehicle = M_Vehicles[VehicleIndex].Tank.IsValid() &&
			M_Vehicles[VehicleIndex].OwningPlayer == VehicleAvoidanceScenarioPrivate::PlayerOwner;
		if (bIsPlayerVehicle)
		{
			VehicleIndices.Add(VehicleIndex);
		}
	}
	return VehicleIndices;
}

void FVehicleAvoidanceScenario::OrderVehicleToSlot(const int32 VehicleIndex, const FVector& Slot, const float Yaw)
{
	FVehicleAvoidanceTestVehicle& Vehicle = M_Vehicles[VehicleIndex];
	Vehicle.bHasGoal = true;
	Vehicle.GoalLocation = Slot;
	Vehicle.GoalYaw = Yaw;
	// Forced final rotation is what the player's rotation arrow gives.
	Vehicle.Tank->MoveToLocation(Slot, true, FRotator(0.f, Yaw, 0.f), true);
}

TArray<FVector> FVehicleAvoidanceScenario::BuildFormationSlots(
	const TArray<int32>& VehicleIndices,
	const FVector& Center,
	const float Yaw) const
{
	using namespace VehicleAvoidanceScenarioPrivate;
	// Same spacing the player's rectangle formation uses, so the slots are as tight as in a real order.
	constexpr float FormationSpacingDivisor = 1.5f;
	float LargestFormationRadius = 0.f;
	for (const int32 VehicleIndex : VehicleIndices)
	{
		const ATrackedTankMaster* Tank = M_Vehicles[VehicleIndex].Tank.Get();
		LargestFormationRadius = FMath::Max(
			LargestFormationRadius,
			Tank->GetRTSComponent()->GetFormationUnitInnerRadius());
	}
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(VehicleIndices.Num()))));
	const float ColumnSpacing = LargestFormationRadius + LargestFormationRadius / FormationSpacingDivisor;
	const float RowSpacing = 2.f * LargestFormationRadius / FormationSpacingDivisor;

	TArray<FVector> Slots;
	for (int32 SlotIndex = 0; SlotIndex < VehicleIndices.Num(); ++SlotIndex)
	{
		const float ColumnOffset = ((SlotIndex % Columns) - (Columns - 1) * 0.5f) * ColumnSpacing;
		const float RowOffset = -(SlotIndex / Columns) * RowSpacing;
		Slots.Add(Center + Right * ColumnOffset + Forward * RowOffset);
	}
	return Slots;
}

void FVehicleAvoidanceScenario::OrderFormation()
{
	using namespace VehicleAvoidanceScenarioPrivate;
	const TArray<int32> VehicleIndices = GetPlayerVehicleIndices();
	const TArray<FVector> Slots = BuildFormationSlots(
		VehicleIndices,
		M_DriveCenter + FVector(FormationCenterOffset, 0.f, 0.f),
		FormationYaw);
	for (int32 OrderIndex = 0; OrderIndex < VehicleIndices.Num(); ++OrderIndex)
	{
		OrderVehicleToSlot(VehicleIndices[OrderIndex], Slots[OrderIndex], FormationYaw);
	}
}

void FVehicleAvoidanceScenario::OrderGroupsToBlocks(const bool bSwapBlocks, const bool bMirrorSlots)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	constexpr int32 GroupCount = 2;
	const TArray<int32> VehicleIndices = GetPlayerVehicleIndices();
	TArray<int32> Groups[GroupCount];
	for (int32 OrderIndex = 0; OrderIndex < VehicleIndices.Num(); ++OrderIndex)
	{
		Groups[OrderIndex % GroupCount].Add(VehicleIndices[OrderIndex]);
	}
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const int32 BlockIndex = bSwapBlocks ? GroupCount - 1 - GroupIndex : GroupIndex;
		// The blocks face each other, so a group that trades blocks has to turn around when it gets there.
		const float BlockSide = BlockIndex == 0 ? 1.f : -1.f;
		const float Yaw = BlockIndex == 0 ? 180.f : 0.f;
		const TArray<int32>& Group = Groups[GroupIndex];
		const TArray<FVector> Slots = BuildFormationSlots(
			Group,
			M_DriveCenter + FVector(BlockSide * BlockDistanceFromCenter, 0.f, 0.f),
			Yaw);
		for (int32 OrderIndex = 0; OrderIndex < Group.Num(); ++OrderIndex)
		{
			const int32 SlotIndex = bMirrorSlots ? Group.Num() - 1 - OrderIndex : OrderIndex;
			OrderVehicleToSlot(Group[OrderIndex], Slots[SlotIndex], Yaw);
		}
	}
}

void FVehicleAvoidanceScenario::AdvanceDrivePhase(UWorld& World)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	if (M_Phase == EVehicleAvoidancePhase::DrivingSamePoint)
	{
		ReportPhaseResult(World, TEXT("same_point"));
		EnterPhase(World, EVehicleAvoidancePhase::DrivingFormation, DrivePhaseSeconds);
		OrderFormation();
		return;
	}
	if (M_Phase == EVehicleAvoidancePhase::DrivingFormation)
	{
		ReportPhaseResult(World, TEXT("formation"));
		EnterPhase(World, EVehicleAvoidancePhase::DrivingSplit, DrivePhaseSeconds);
		OrderGroupsToBlocks(false, false);
		return;
	}
	if (M_Phase == EVehicleAvoidancePhase::DrivingSplit)
	{
		ReportPhaseResult(World, TEXT("split"));
		EnterPhase(World, EVehicleAvoidancePhase::DrivingSwap, DrivePhaseSeconds);
		OrderGroupsToBlocks(true, false);
		return;
	}
	if (M_Phase == EVehicleAvoidancePhase::DrivingSwap)
	{
		ReportPhaseResult(World, TEXT("swap"));
		EnterPhase(World, EVehicleAvoidancePhase::DrivingShuffle, DrivePhaseSeconds);
		OrderGroupsToBlocks(true, true);
		return;
	}
	ReportPhaseResult(World, TEXT("shuffle"));
	M_Phase = EVehicleAvoidancePhase::Inactive;
	UE_LOG(LogRTSVehicleAvoidance, Display, TEXT("RTS_VEHICLE_AVOIDANCE RESULT DONE map=%s"), *World.GetName());
}

void FVehicleAvoidanceScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem, const float DeltaTime)
{
	using namespace VehicleAvoidanceScenarioPrivate;
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == EVehicleAvoidancePhase::Inactive || not IsValid(World))
	{
		return;
	}
	// The map boots paused behind the start-game widget.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}

	const float NowSeconds = World->GetTimeSeconds();
	if (NowSeconds >= M_NextCollectSeconds)
	{
		CollectVehicles(*World);
		M_NextCollectSeconds = NowSeconds + CollectIntervalSeconds;
	}
	if (M_Phase == EVehicleAvoidancePhase::WarmingUp)
	{
		if (M_Vehicles.Num() < 2)
		{
			M_PhaseEndSeconds = NowSeconds + WarmUpSeconds;
			return;
		}
		if (M_ExtraVehiclesToSpawn > 0)
		{
			SpawnExtraVehicles(*World);
			M_PhaseEndSeconds = NowSeconds + WarmUpSeconds;
			return;
		}
		if (NowSeconds < M_PhaseEndSeconds)
		{
			return;
		}
		if (not bM_DriveVehicles)
		{
			EnterPhase(*World, EVehicleAvoidancePhase::Observing, M_ObserveSeconds);
			return;
		}
		int32 PlayerVehicleCount = 0;
		M_DriveCenter = FVector::ZeroVector;
		for (const FVehicleAvoidanceTestVehicle& Vehicle : M_Vehicles)
		{
			if (Vehicle.Tank.IsValid() && Vehicle.OwningPlayer == PlayerOwner)
			{
				M_DriveCenter += Vehicle.Tank->GetActorLocation();
				++PlayerVehicleCount;
			}
		}
		M_DriveCenter /= FMath::Max(1, PlayerVehicleCount);
		EnterPhase(*World, EVehicleAvoidancePhase::DrivingSamePoint, DrivePhaseSeconds);
		OrderSamePoint();
		return;
	}

	SampleFrame(*World, DeltaTime);
	if (bM_TakeScreenshots)
	{
		UpdateOverviewCamera(*World);
	}
	const bool bShouldDumpStats = bM_DumpStats && not bM_HasDumpedStats && GEngine != nullptr &&
		NowSeconds - M_Stats.StartSeconds >= StatDumpStartSeconds;
	if (bShouldDumpStats)
	{
		bM_HasDumpedStats = true;
		ReportProgress(*World, TEXT("stat_dump_start"));
		GEngine->Exec(World, *FString::Printf(TEXT("stat dumpave -num=%d -ms=0.001"), StatDumpFrames));
	}
	if (NowSeconds >= M_NextReportSeconds)
	{
		ReportProgress(*World, TEXT("progress"));
		M_NextReportSeconds = NowSeconds + ReportIntervalSeconds;
	}
	if (bM_TraceVehicles && NowSeconds >= M_NextTraceSeconds)
	{
		ReportBusyVehicles(*World);
		M_NextTraceSeconds = NowSeconds + TraceIntervalSeconds;
	}
	if (M_Phase == EVehicleAvoidancePhase::Observing)
	{
		if (NowSeconds < M_PhaseEndSeconds)
		{
			return;
		}
		ReportPhaseResult(*World, TEXT("observe"));
		M_Phase = EVehicleAvoidancePhase::Inactive;
		UE_LOG(LogRTSVehicleAvoidance, Display, TEXT("RTS_VEHICLE_AVOIDANCE RESULT DONE map=%s"), *World->GetName());
		return;
	}

	const bool bMayEndEarly = NowSeconds - M_Stats.StartSeconds >= DrivePhaseMinimumSeconds &&
		GetAreAllDrivenVehiclesIdle();
	if (bMayEndEarly || NowSeconds >= M_PhaseEndSeconds)
	{
		AdvanceDrivePhase(*World);
	}
}

#endif // RTS_WITH_MAP_TESTS
