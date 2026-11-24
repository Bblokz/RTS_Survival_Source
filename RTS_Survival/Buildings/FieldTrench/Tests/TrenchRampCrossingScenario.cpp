// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrenchRampCrossingScenario.h"

#if RTS_WITH_MAP_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTrenchRamp, Log, All);

namespace TrenchRampScenarioPrivate
{
	const TCHAR* const LocationSwitch = TEXT("TrenchRampAt=");
	const TCHAR* const VehicleRoot = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/");
	const TCHAR* const PanzerIVPath = TEXT("TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const KuebelPath = TEXT("TrackBasedVehicle/Ger_ArmoredCar/Keubel/Ger_ArmoredCar_Keubel.Ger_ArmoredCar_Keubel_C");
	const TCHAR* const PumaPath = TEXT("TrackBasedVehicle/Ger_ArmoredCar/Puma/Ger_ArmoredCar_Puma.Ger_ArmoredCar_Puma_C");
	const TCHAR* const HalfTrackPath = TEXT("TrackBasedVehicle/Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250.Ger_ArmoredCar_Sdkfz250_C");
	const TCHAR* const HarvesterPath = TEXT("TrackBasedVehicle/Ger_Light/Panzer1/Ger_Harvester_Panzer1.Ger_Harvester_Panzer1_C");
	const TCHAR* const HQTruckPath = TEXT("BaseAndNomadic/ChildBlueprints/SKDFZ-7/Ger_HQ_Truck.Ger_HQ_Truck_C");
	const TCHAR* const T34Path = TEXT("TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const BA11Path = TEXT("TrackBasedVehicle/Rus_ArmoredCar/BA-11/Rus_ArmoredCar_BA11.Rus_ArmoredCar_BA11_C");

	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float SettleSeconds = 4.f;
	constexpr float DriveTimeoutSeconds = 75.f;
	// The parked tank is watched this long after the trench under it is finished.
	constexpr float AfterDugSeconds = 4.f;
	constexpr float DigSecondsPerMetre = 0.25f;
	constexpr float TraceLogIntervalSeconds = 1.f;

	// Layout, in centimetres. Trenches run along +Y; vehicles cross along +X. Lanes sit side by side along +Y.
	constexpr float StraightLength = 500.f;
	constexpr int32 SquareStraights = 1;
	constexpr int32 ObliqueStraights = 3;
	constexpr float LaneHalfLength = 2200.f;
	constexpr float LaneWidth = 2000.f;
	constexpr float ObliqueLaneWidth = 3800.f;
	constexpr float ObliqueHalfLength = 1600.f;
	// Far enough from the player's lanes that the enemy vehicles have nobody to fight.
	constexpr float EnemyLanesOffset = 11000.f;

	// Open, even landscape that holds every lane is searched for around the test spot.
	constexpr float AreaSearchRadius = 60000.f;
	constexpr float AreaSearchStep = 3000.f;
	constexpr float AreaSampleStep = 650.f;
	constexpr float AreaHalfAlong = LaneHalfLength + 400.f;
	constexpr float AreaMaxHeightDifference = 150.f;

	constexpr float GoalTolerance = 450.f;
	constexpr float GroundTraceHalfHeight = 50000.f;
	// The ramp's lid lies about 130 cm over the ground: a vehicle that went over rose at least this much; one that
	// drove through the walls stayed level.
	constexpr float MinimumRiseOverTrench = 70.f;
	// Never tipped further than this from upright.
	constexpr float MinimumUpZ = 0.5f;
	// The parked tank ends up on the ramp's lid, not inside the trench.
	constexpr float MinimumParkedFinalRise = 70.f;

	FString GetClassPath(const TCHAR* RelativePath)
	{
		return FString(VehicleRoot) + RelativePath;
	}
}

void FTrenchRampCrossingScenario::Start()
{
	EnterPhase(ETrenchRampTestPhase::WaitingForGameStart);
	RTSFunctionLibrary::PushModalDialogSuppression();
	MakeLanes();
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP started lanes=%d"), M_Lanes.Num());
}

void FTrenchRampCrossingScenario::MakeLanes()
{
	using namespace TrenchRampScenarioPrivate;
	const EFieldTrenchKit German = EFieldTrenchKit::German;
	const EFieldTrenchKit Soviet = EFieldTrenchKit::Soviet;
	M_Lanes = {
		{TEXT("tracked_panzer4"), PanzerIVPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("wheeled_kuebel"), KuebelPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("control_kuebel_no_trench"), KuebelPath, PlayerOwner, German, ETrenchRampLaneKind::Control},
		{TEXT("wheeled_puma"), PumaPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("halftrack_sdkfz250"), HalfTrackPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("harvester_panzer1"), HarvesterPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("hq_truck"), HQTruckPath, PlayerOwner, German, ETrenchRampLaneKind::Square},
		{TEXT("parked_panzer4_on_dug_trench"), PanzerIVPath, PlayerOwner, German, ETrenchRampLaneKind::Parked},
		{TEXT("tracked_panzer4_oblique_45"), PanzerIVPath, PlayerOwner, German, ETrenchRampLaneKind::Oblique},
		{TEXT("enemy_t34_soviet_kit"), T34Path, EnemyOwner, Soviet, ETrenchRampLaneKind::Square},
		{TEXT("enemy_wheeled_ba11_soviet_kit"), BA11Path, EnemyOwner, Soviet, ETrenchRampLaneKind::Square},
	};
}

FVector FTrenchRampCrossingScenario::GetSiteOffset(const int32 LaneIndex) const
{
	using namespace TrenchRampScenarioPrivate;
	if (not M_Lanes.IsValidIndex(LaneIndex))
	{
		return FVector::ZeroVector;
	}
	const uint8 Owner = M_Lanes[LaneIndex].Owner;
	float Across = 0.f;
	for (int32 Index = 0; Index < LaneIndex; ++Index)
	{
		if (M_Lanes[Index].Owner == Owner)
		{
			Across += M_Lanes[Index].Kind == ETrenchRampLaneKind::Oblique ? ObliqueLaneWidth : LaneWidth;
		}
	}
	if (M_Lanes[LaneIndex].Kind == ETrenchRampLaneKind::Oblique)
	{
		Across += (ObliqueLaneWidth - LaneWidth) * 0.5f;
	}
	return FVector(Owner == EnemyOwner ? EnemyLanesOffset : 0.f, Across, 0.f);
}

void FTrenchRampCrossingScenario::Tick(URTSCoverFinderWorldSubsystem& Host, const float DeltaTime)
{
	if (M_Phase == ETrenchRampTestPhase::Inactive || M_Phase == ETrenchRampTestPhase::Done)
	{
		return;
	}
	UWorld* World = Host.GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_World = World;
	if (M_Phase != ETrenchRampTestPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
	}
	switch (M_Phase)
	{
	case ETrenchRampTestPhase::WaitingForGameStart:
		TickWaitingForGameStart(*World);
		break;
	case ETrenchRampTestPhase::WaitingForVehicles:
		TickWaitingForVehicles(*World);
		break;
	case ETrenchRampTestPhase::Driving:
		TickDriving(*World);
		break;
	default:
		break;
	}
}

void FTrenchRampCrossingScenario::EnterPhase(const ETrenchRampTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = M_World.IsValid() ? M_World->GetTimeSeconds() : 0.f;
}

float FTrenchRampCrossingScenario::GetGameSecondsInPhase() const
{
	return M_World.IsValid() ? M_World->GetTimeSeconds() - M_PhaseStartGameSeconds : 0.f;
}

double FTrenchRampCrossingScenario::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

void FTrenchRampCrossingScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details);
}

void FTrenchRampCrossingScenario::Finish()
{
	EnterPhase(ETrenchRampTestPhase::Done);
	RTSFunctionLibrary::PopModalDialogSuppression();
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	FPlatformMisc::RequestExit(false, TEXT("TrenchRampCrossingScenario"));
}

void FTrenchRampCrossingScenario::TickWaitingForGameStart(UWorld& World)
{
	using namespace TrenchRampScenarioPrivate;
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
	FVector SearchOrigin;
	if (not FGrenadeTestHelpers::FindTestSpot(World, LocationSwitch, SearchOrigin) ||
		not FindOpenArea(World, SearchOrigin, M_Anchor))
	{
		Check(false, TEXT("open_test_area_found"), SearchOrigin.ToCompactString());
		Finish();
		return;
	}
	for (int32 LaneIndex = 0; LaneIndex < M_Lanes.Num(); ++LaneIndex)
	{
		SetUpLane(World, LaneIndex);
	}
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP layout anchor=%s"), *M_Anchor.ToCompactString());
	EnterPhase(ETrenchRampTestPhase::WaitingForVehicles);
}

void FTrenchRampCrossingScenario::SetUpLane(UWorld& World, const int32 LaneIndex)
{
	using namespace TrenchRampScenarioPrivate;
	const FTrenchRampLane& Lane = M_Lanes[LaneIndex];
	FVector Centre = M_Anchor + GetSiteOffset(LaneIndex);
	Centre.Z = GetGroundZ(World, Centre);
	// Enemy lanes run the other way, toward the player's side.
	const float Direction = Lane.Owner == EnemyOwner ? -1.f : 1.f;
	const FVector Across(LaneHalfLength * Direction, 0.f, 0.f);
	switch (Lane.Kind)
	{
	case ETrenchRampLaneKind::Square:
		SpawnTrench(World, Centre, 90.f, SquareStraights, Lane.Kit, false);
		SpawnVehicle(World, Lane, Centre, Centre - Across, Centre + Across);
		break;
	case ETrenchRampLaneKind::Oblique:
		SpawnTrench(World, Centre, 90.f, ObliqueStraights, Lane.Kit, false);
		SpawnVehicle(World, Lane, Centre, Centre - FVector(ObliqueHalfLength, ObliqueHalfLength, 0.f),
		             Centre + FVector(ObliqueHalfLength, ObliqueHalfLength, 0.f));
		break;
	case ETrenchRampLaneKind::Control:
		SpawnVehicle(World, Lane, Centre, Centre - Across, Centre + Across);
		break;
	case ETrenchRampLaneKind::Parked:
		// The trench is dug under it once it has settled.
		SpawnVehicle(World, Lane, Centre, Centre, Centre);
		break;
	}
}

ARTSFieldTrench* FTrenchRampCrossingScenario::SpawnTrench(UWorld& World, const FVector& Centre, const float Yaw,
                                                          const int32 Straights, const EFieldTrenchKit Kit,
                                                          const bool bDigIt)
{
	using namespace TrenchRampScenarioPrivate;
	const FVector Direction = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Start = Centre - Direction * (Straights * StraightLength * 0.5f);
	TArray<FFieldTrenchPiecePlacement> Pieces;
	FFieldTrenchOpenPort OpenPort;
	OpenPort.Location = FVector2D(Start);
	// The start cap joins a port facing back out of the trench.
	OpenPort.Yaw = FRotator::NormalizeAxis(Yaw + 180.f);
	FFieldTrenchOpenPort UnusedExit;
	FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
	OpenPort.Yaw = Yaw;
	for (int32 StraightIndex = 0; StraightIndex < Straights; ++StraightIndex)
	{
		FieldTrenchLayout::PlacePieceOnPort(
			EFieldTrenchPiece::Straight_500_A, 0, OpenPort, Pieces.AddDefaulted_GetRef(), OpenPort);
	}
	FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
	for (FFieldTrenchPiecePlacement& Piece : Pieces)
	{
		Piece.Location.Z = GetGroundZ(World, Piece.Location);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARTSFieldTrench* Trench = World.SpawnActor<ARTSFieldTrench>(
		ARTSFieldTrench::StaticClass(), FTransform(Start), SpawnParameters);
	if (not IsValid(Trench))
	{
		Check(false, TEXT("trench_spawned"), Centre.ToCompactString());
		return nullptr;
	}
	Trench->SetKit(Kit);
	M_Trenches.Add(Trench);
	if (bDigIt)
	{
		Trench->SetDigSecondsPerMetre(DigSecondsPerMetre);
		Trench->StartConstruction(Pieces, PlayerOwner, {});
		return Trench;
	}
	for (const FFieldTrenchPiecePlacement& Piece : Pieces)
	{
		Trench->AddBuiltPiece(Piece);
	}
	return Trench;
}

void FTrenchRampCrossingScenario::SpawnVehicle(UWorld& World, const FTrenchRampLane& Lane, const FVector& SiteCentre,
                                               const FVector& Start, const FVector& Goal)
{
	using namespace TrenchRampScenarioPrivate;
	FVector GroundStart = Start;
	FGrenadeTestHelpers::DropToGround(World, GroundStart);
	const bool bIsParked = Lane.Kind == ETrenchRampLaneKind::Parked;
	const float Yaw = bIsParked ? 0.f : (Goal - Start).GetSafeNormal2D().Rotation().Yaw;
	const FString ClassPath = GetClassPath(Lane.ClassPath);
	ATrackedTankMaster* Tank = Cast<ATrackedTankMaster>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, *ClassPath, FTransform(FRotator(0.f, Yaw, 0.f), GroundStart), Lane.Owner));
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP spawned %s=%s owner=%d kit=%s at %s"),
	       Lane.Label, *GetNameSafe(Tank), Lane.Owner, *UEnum::GetValueAsString(Lane.Kit),
	       *GroundStart.ToCompactString());
	if (not IsValid(Tank))
	{
		Check(false, TEXT("vehicle_spawned"), Lane.Label);
		return;
	}
	// Nobody stops to fight on the way over.
	Tank->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
	FTrenchRampTestVehicle& TestVehicle = M_Vehicles.AddDefaulted_GetRef();
	TestVehicle.Vehicle = Tank;
	TestVehicle.Lane = Lane;
	TestVehicle.SiteCentre = SiteCentre;
	TestVehicle.GoalLocation = Goal;
}

void FTrenchRampCrossingScenario::TickWaitingForVehicles(UWorld& World)
{
	using namespace TrenchRampScenarioPrivate;
	if (GetGameSecondsInPhase() < SettleSeconds)
	{
		return;
	}
	for (FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		if (const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get())
		{
			TestVehicle.SettledZ = Tank->GetActorLocation().Z;
		}
	}
	CheckRamps();
	OrderVehicles();
	for (const FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		if (TestVehicle.Lane.Kind == ETrenchRampLaneKind::Parked)
		{
			M_DugTrench = SpawnTrench(World, TestVehicle.SiteCentre, 90.f, SquareStraights, TestVehicle.Lane.Kit, true);
		}
	}
	EnterPhase(ETrenchRampTestPhase::Driving);
}

void FTrenchRampCrossingScenario::CheckRamps()
{
	int32 Pieces = 0;
	int32 RampsWithHulls = 0;
	for (const TWeakObjectPtr<ARTSFieldTrench>& TrenchPointer : M_Trenches)
	{
		const ARTSFieldTrench* Trench = TrenchPointer.Get();
		if (not IsValid(Trench))
		{
			continue;
		}
		for (const FFieldTrenchPieceRuntime& Piece : Trench->GetPieces())
		{
			++Pieces;
			const UStaticMeshComponent* Ramp = Piece.RampComponent;
			const bool bHasHulls = IsValid(Ramp) && IsValid(Ramp->GetStaticMesh()) &&
				Ramp->GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly && not Ramp->IsVisible();
			RampsWithHulls += bHasHulls ? 1 : 0;
		}
	}
	Check(Pieces > 0 && RampsWithHulls == Pieces, TEXT("built_pieces_have_physics_only_hidden_ramps"),
	      FString::Printf(TEXT("%d of %d"), RampsWithHulls, Pieces));
}

void FTrenchRampCrossingScenario::OrderVehicles()
{
	for (FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		if (TestVehicle.Lane.Kind == ETrenchRampLaneKind::Parked || not IsValid(Tank))
		{
			continue;
		}
		const FRotator Facing = (TestVehicle.GoalLocation - Tank->GetActorLocation()).GetSafeNormal2D().Rotation();
		ICommands* Commands = Tank;
		const ECommandQueueError Error = Commands->MoveToLocation(TestVehicle.GoalLocation, true, Facing, false);
		Check(Error == ECommandQueueError::NoError, TEXT("move_ordered"), TestVehicle.Lane.Label);
	}
}

void FTrenchRampCrossingScenario::TickDriving(UWorld& World)
{
	using namespace TrenchRampScenarioPrivate;
	const float SecondsSinceOrders = GetGameSecondsInPhase();
	bool bAllArrived = true;
	for (FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		UpdateVehicle(TestVehicle, SecondsSinceOrders);
		bAllArrived &= TestVehicle.Lane.Kind == ETrenchRampLaneKind::Parked || TestVehicle.ArrivalSeconds >= 0.f;
	}
	if (M_DugTrenchFinishedSeconds < 0.f && GetIsDugTrenchFinished())
	{
		M_DugTrenchFinishedSeconds = SecondsSinceOrders;
		UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP dug trench finished after %.1f s"),
		       SecondsSinceOrders);
	}
	const bool bParkedWatched = M_DugTrenchFinishedSeconds >= 0.f &&
		SecondsSinceOrders - M_DugTrenchFinishedSeconds > AfterDugSeconds;
	if ((bAllArrived && bParkedWatched) || SecondsSinceOrders > DriveTimeoutSeconds)
	{
		Check(M_DugTrenchFinishedSeconds >= 0.f, TEXT("dug_trench_finished"),
		      FString::Printf(TEXT("after %.1f s"), M_DugTrenchFinishedSeconds));
		CheckVehicles();
		Finish();
	}
}

void FTrenchRampCrossingScenario::UpdateVehicle(FTrenchRampTestVehicle& TestVehicle,
                                                const float SecondsSinceOrders) const
{
	using namespace TrenchRampScenarioPrivate;
	const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
	if (not IsValid(Tank))
	{
		return;
	}
	const FVector Location = Tank->GetActorLocation();
	const float Rise = Location.Z - TestVehicle.SettledZ;
	TestVehicle.HighestRise = FMath::Max(TestVehicle.HighestRise, Rise);
	TestVehicle.LowestUpZ = FMath::Min(TestVehicle.LowestUpZ, Tank->GetActorUpVector().Z);
	const bool bDrives = TestVehicle.Lane.Kind != ETrenchRampLaneKind::Parked;
	if (bDrives && TestVehicle.ArrivalSeconds < 0.f &&
		FVector::Dist2D(Location, TestVehicle.GoalLocation) < GoalTolerance)
	{
		TestVehicle.ArrivalSeconds = SecondsSinceOrders;
	}
	if (SecondsSinceOrders - TestVehicle.LastTraceLogSeconds < TraceLogIntervalSeconds)
	{
		return;
	}
	TestVehicle.LastTraceLogSeconds = SecondsSinceOrders;
	// Signed distance along the lane from the trench centre line: negative before it, positive past it.
	const FVector LaneDirection = (TestVehicle.GoalLocation - TestVehicle.SiteCentre).GetSafeNormal2D();
	const float Along = FVector::DotProduct(Location - TestVehicle.SiteCentre, LaneDirection);
	UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP trace %s t=%.0f along=%.0f rise=%.0f speed=%.0f up_z=%.2f"),
	       TestVehicle.Lane.Label, SecondsSinceOrders, Along, Rise, Tank->GetVelocity().Size2D(),
	       Tank->GetActorUpVector().Z);
}

void FTrenchRampCrossingScenario::CheckVehicles()
{
	using namespace TrenchRampScenarioPrivate;
	// A class whose control lane (open ground) never arrived cannot drive in this map at all: its trench lane says
	// nothing about the ramp and is skipped instead of failed.
	TSet<FString> ClassesThatCannotDrive;
	for (const FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		if (TestVehicle.Lane.Kind == ETrenchRampLaneKind::Control && TestVehicle.ArrivalSeconds < 0.f)
		{
			ClassesThatCannotDrive.Add(TestVehicle.Lane.ClassPath);
		}
	}
	for (const FTrenchRampTestVehicle& TestVehicle : M_Vehicles)
	{
		const ATrackedTankMaster* Tank = TestVehicle.Vehicle.Get();
		const float FinalRise = IsValid(Tank) ? Tank->GetActorLocation().Z - TestVehicle.SettledZ : 0.f;
		if (TestVehicle.Lane.Kind != ETrenchRampLaneKind::Control &&
			ClassesThatCannotDrive.Contains(TestVehicle.Lane.ClassPath))
		{
			UE_LOG(LogRTSTrenchRamp, Display,
			       TEXT("RTS_TRENCH_RAMP check=SKIP %s its control lane on open ground never arrived either"),
			       TestVehicle.Lane.Label);
			continue;
		}
		const FString Details = FString::Printf(
			TEXT("%s highest_rise=%.0f final_rise=%.0f lowest_up_z=%.2f arrival_s=%.1f"),
			TestVehicle.Lane.Label, TestVehicle.HighestRise, FinalRise, TestVehicle.LowestUpZ,
			TestVehicle.ArrivalSeconds);
		Check(IsValid(Tank), TEXT("vehicle_alive"), Details);
		Check(TestVehicle.LowestUpZ >= MinimumUpZ, TEXT("vehicle_stayed_upright"), Details);
		switch (TestVehicle.Lane.Kind)
		{
		case ETrenchRampLaneKind::Parked:
			Check(FinalRise >= MinimumParkedFinalRise, TEXT("parked_tank_lifted_onto_ramp"), Details);
			break;
		case ETrenchRampLaneKind::Control:
			// Not a ramp check: it only tells whether the vehicle class can drive here at all.
			UE_LOG(LogRTSTrenchRamp, Display, TEXT("RTS_TRENCH_RAMP control %s %s"),
			       TestVehicle.ArrivalSeconds >= 0.f ? TEXT("arrived") : TEXT("NEVER_ARRIVED"), *Details);
			break;
		default:
			Check(TestVehicle.ArrivalSeconds >= 0.f, TEXT("vehicle_crossed_trench"), Details);
			Check(TestVehicle.HighestRise >= MinimumRiseOverTrench, TEXT("vehicle_went_over_not_through"), Details);
			break;
		}
	}
}

bool FTrenchRampCrossingScenario::GetIsDugTrenchFinished() const
{
	const ARTSFieldTrench* Trench = M_DugTrench.Get();
	return IsValid(Trench) && Trench->GetPieceCount() > 0 && Trench->GetBuiltPieceCount() == Trench->GetPieceCount();
}

bool FTrenchRampCrossingScenario::FindOpenArea(UWorld& World, const FVector& SearchOrigin, FVector& OutAnchor) const
{
	using namespace TrenchRampScenarioPrivate;
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
		bool bIsOpen = true;
		for (int32 LaneIndex = 0; bIsOpen && LaneIndex < M_Lanes.Num(); ++LaneIndex)
		{
			bIsOpen = GetIsOpenSite(World, Candidate + FVector2D(GetSiteOffset(LaneIndex)));
		}
		float AnchorZ = 0.f;
		if (bIsOpen && GetLandscapeZ(World, Candidate, AnchorZ))
		{
			OutAnchor = FVector(Candidate, AnchorZ);
			return true;
		}
	}
	return false;
}

bool FTrenchRampCrossingScenario::GetIsOpenSite(UWorld& World, const FVector2D& Centre) const
{
	using namespace TrenchRampScenarioPrivate;
	float CentreZ = 0.f;
	if (not GetLandscapeZ(World, Centre, CentreZ))
	{
		return false;
	}
	const float HalfAcross = LaneWidth * 0.5f;
	for (float Along = -AreaHalfAlong; Along <= AreaHalfAlong; Along += AreaSampleStep)
	{
		for (float Across = -HalfAcross; Across <= HalfAcross; Across += AreaSampleStep)
		{
			float SampleZ = 0.f;
			if (not GetLandscapeZ(World, Centre + FVector2D(Along, Across), SampleZ) ||
				FMath::Abs(SampleZ - CentreZ) > AreaMaxHeightDifference)
			{
				return false;
			}
		}
	}
	return true;
}

bool FTrenchRampCrossingScenario::GetLandscapeZ(UWorld& World, const FVector2D& Location, float& OutZ) const
{
	using namespace TrenchRampScenarioPrivate;
	// The first thing a visibility trace from above meets must be the landscape: no building, rock or tree there.
	FHitResult GroundHit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TrenchRampArea), false);
	if (not World.LineTraceSingleByChannel(GroundHit, FVector(Location, GroundTraceHalfHeight),
	                                       FVector(Location, -GroundTraceHalfHeight), ECC_Visibility, Params) ||
		not IsValid(Cast<ALandscapeProxy>(GroundHit.GetActor())))
	{
		return false;
	}
	OutZ = GroundHit.ImpactPoint.Z;
	return true;
}

float FTrenchRampCrossingScenario::GetGroundZ(UWorld& World, const FVector& Location) const
{
	using namespace TrenchRampScenarioPrivate;
	FHitResult GroundHit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TrenchRampGround), false);
	if (World.LineTraceSingleByChannel(GroundHit, FVector(FVector2D(Location), GroundTraceHalfHeight),
	                                   FVector(FVector2D(Location), -GroundTraceHalfHeight),
	                                   COLLISION_TRACE_LANDSCAPE, Params))
	{
		return GroundHit.ImpactPoint.Z;
	}
	return Location.Z;
}

#endif
