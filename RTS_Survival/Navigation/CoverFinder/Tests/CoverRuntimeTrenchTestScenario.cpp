#include "CoverRuntimeTrenchTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "CoverTrenchTestScenario.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverRuntimeTrenchTest, Log, All);

namespace CoverRuntimeTrenchTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float CoverTimeoutSeconds = 10.0f;
	// The dug trench is about 9 m long at one second of digging per metre.
	constexpr float DigTimeoutSeconds = 60.0f;
	// Beside the squad, so the squad's walk into the trench is short; the dug trench well away from it.
	const FVector BuiltTrenchOffsetFromSquad(600.0f, -300.0f, 0.0f);
	// Where the dug trench may go; the first spot flat enough for the HQ's trench placement is taken.
	const FVector DugTrenchOffsetsFromSquad[] = {
		FVector(600.0f, 2500.0f, 0.0f), FVector(600.0f, -2500.0f, 0.0f), FVector(-1500.0f, 0.0f, 0.0f),
		FVector(2500.0f, 0.0f, 0.0f), FVector(-1500.0f, 2500.0f, 0.0f), FVector(-1500.0f, -2500.0f, 0.0f)
	};
	constexpr float DugTrenchMaximumSlopeDegrees = 15.0f;
	constexpr float DugTrenchDragLength = 750.0f;
	const FVector NavigationProjectionExtent(500.0f, 500.0f, 1000.0f);

	// How far a published point may lie from where the socket says it should.
	constexpr float MaximumSnapError = 2.0f;
	// The walk-to point is on the ground over the socket; the sockets lie 60 cm below the ground.
	constexpr float MaximumApproachFlatOffset = 40.0f;
	constexpr float SocketDepthBelowGround = 60.0f;
	// The navigation mesh only follows the ground roughly, more so on a slope.
	constexpr float MaximumApproachHeightError = 30.0f;

	TArray<FTransform> GetBuiltCargoSockets(const ARTSFieldTrench& Trench)
	{
		TArray<FTransform> CargoSockets;
		for (const FFieldTrenchPieceRuntime& Piece : Trench.GetPieces())
		{
			if (Piece.State != EFieldTrenchPieceState::Built || not IsValid(Piece.Component))
			{
				continue;
			}
			for (const FName& SocketName : Piece.Component->GetAllSocketNames())
			{
				if (SocketName.ToString().Contains(TEXT("cargo")))
				{
					CargoSockets.Add(Piece.Component->GetSocketTransform(SocketName, RTS_World));
				}
			}
		}
		return CargoSockets;
	}

	TArray<FFieldTrenchPiecePlacement> MakeStraightsWithEnds(const FVector& Start, const float Yaw)
	{
		TArray<FFieldTrenchPiecePlacement> Pieces;
		FFieldTrenchOpenPort OpenPort;
		OpenPort.Location = FVector2D(Start);
		// The start cap joins a port facing back out of the trench.
		OpenPort.Yaw = FRotator::NormalizeAxis(Yaw + 180.0f);
		FFieldTrenchOpenPort UnusedExit;
		FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
		OpenPort.Yaw = Yaw;
		// The layout of the hand-placed trench on TestFieldTrench, with room for a whole squad.
		FieldTrenchLayout::PlacePieceOnPort(
			EFieldTrenchPiece::Straight_500_A, 0, OpenPort, Pieces.AddDefaulted_GetRef(), OpenPort);
		FieldTrenchLayout::PlacePieceOnPort(
			EFieldTrenchPiece::Straight_250_A, 0, OpenPort, Pieces.AddDefaulted_GetRef(), OpenPort);
		FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
		for (FFieldTrenchPiecePlacement& Piece : Pieces)
		{
			Piece.Location.Z = Start.Z;
		}
		return Pieces;
	}

	bool TryProjectToNavigation(UWorld& World, const FVector& Location, FVector& OutLocation)
	{
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		FNavLocation Projected;
		if (not IsValid(NavigationSystem) ||
			not NavigationSystem->ProjectPointToNavigation(Location, Projected, NavigationProjectionExtent))
		{
			return false;
		}
		OutLocation = Projected.Location;
		return true;
	}
}

void FCoverRuntimeTrenchTestScenario::Start()
{
	M_Phase = ECoverRuntimeTrenchTestPhase::WaitingForCoverScan;
	UE_LOG(LogRTSCoverRuntimeTrenchTest, Display, TEXT("RTS_COVER_RUNTIME_TRENCH_TEST scheduled"));
}

void FCoverRuntimeTrenchTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(
		LogRTSCoverRuntimeTrenchTest,
		Display,
		TEXT("RTS_COVER_RUNTIME_TRENCH_TEST check=%s %s %s"),
		bCondition ? TEXT("PASS") : TEXT("FAIL"),
		Description,
		*Details);
}

void FCoverRuntimeTrenchTestScenario::EnterPhase(
	const UWorld& World,
	const ECoverRuntimeTrenchTestPhase NewPhase,
	const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseDeadlineWorldSeconds = World.GetTimeSeconds() + TimeoutSeconds;
}

bool FCoverRuntimeTrenchTestScenario::GetHasPhaseTimedOut(const UWorld& World) const
{
	return World.GetTimeSeconds() >= M_PhaseDeadlineWorldSeconds;
}

void FCoverRuntimeTrenchTestScenario::Finish(const UWorld& World, const int32 TrenchScenarioFailedChecks)
{
	M_Phase = ECoverRuntimeTrenchTestPhase::Inactive;
	const int32 TotalFailedChecks = M_FailedCheckCount + TrenchScenarioFailedChecks;
	UE_LOG(
		LogRTSCoverRuntimeTrenchTest,
		Display,
		TEXT("RTS_COVER_RUNTIME_TRENCH_TEST RESULT %s failed_checks=%d (runtime=%d stand_up_cycle=%d) map=%s"),
		TotalFailedChecks == 0 ? TEXT("PASS") : TEXT("FAIL"),
		TotalFailedChecks,
		M_FailedCheckCount,
		TrenchScenarioFailedChecks,
		*World.GetName());
}

void FCoverRuntimeTrenchTestScenario::Tick(
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	FCoverTrenchTestScenario& TrenchScenario)
{
	using namespace CoverRuntimeTrenchTestScenarioPrivate;
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ECoverRuntimeTrenchTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ECoverRuntimeTrenchTestPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (not CoverSubsystem.GetHasCompletedFullScan())
		{
			return;
		}
		const TMap<ERTSResourceType, int32> PieceCost = ARTSFieldTrench::GetCostOfLength(
			{{ERTSResourceType::Resource_Radixite, 4}, {ERTSResourceType::Resource_Metal, 1}}, 65.0f);
		Check(
			PieceCost.FindRef(ERTSResourceType::Resource_Radixite) == 3 &&
			PieceCost.FindRef(ERTSResourceType::Resource_Metal) == 1,
			TEXT("a piece costs its length times the price per metre, at least one of each resource"),
			FString::Printf(
				TEXT("radixite=%d metal=%d"),
				PieceCost.FindRef(ERTSResourceType::Resource_Radixite),
				PieceCost.FindRef(ERTSResourceType::Resource_Metal)));
		if (not SpawnTrenches(*World))
		{
			Finish(*World, 0);
			return;
		}
		EnterPhase(*World, ECoverRuntimeTrenchTestPhase::WaitingForBuiltTrenchCover, CoverTimeoutSeconds);
		return;
	}
	ARTSFieldTrench* BuiltTrench = M_BuiltTrench.Get();
	ARTSFieldTrench* DugTrench = M_DugTrench.Get();
	if (not IsValid(BuiltTrench) || not IsValid(DugTrench))
	{
		Check(false, TEXT("the runtime trenches still exist"));
		Finish(*World, 0);
		return;
	}
	if (M_Phase == ECoverRuntimeTrenchTestPhase::WaitingForBuiltTrenchCover)
	{
		if (not CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT(""), false) && not GetHasPhaseTimedOut(*World))
		{
			return;
		}
		CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT("a trench spawned at runtime publishes its cargo sockets"), true);
		// The far end cap goes; its socket has to leave the published cover with it.
		BuiltTrench->RemovePiece(BuiltTrench->GetPieceCount() - 1);
		EnterPhase(*World, ECoverRuntimeTrenchTestPhase::WaitingForRemovedPiece, CoverTimeoutSeconds);
		return;
	}
	if (M_Phase == ECoverRuntimeTrenchTestPhase::WaitingForRemovedPiece)
	{
		if (not CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT(""), false) && not GetHasPhaseTimedOut(*World))
		{
			return;
		}
		CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT("removing a piece withdraws its sockets"), true);
		const TArray<FFieldTrenchPiecePlacement> Pieces = MakeStraightsWithEnds(BuiltTrench->GetActorLocation(), 90.0f);
		BuiltTrench->AddBuiltPiece(Pieces.Last());
		EnterPhase(*World, ECoverRuntimeTrenchTestPhase::WaitingForDugTrench, DigTimeoutSeconds);
		return;
	}
	if (M_Phase == ECoverRuntimeTrenchTestPhase::WaitingForDugTrench)
	{
		const bool bIsDug = DugTrench->GetPieceCount() > 0 && DugTrench->GetBuiltPieceCount() == DugTrench->GetPieceCount();
		const bool bIsPublished = bIsDug &&
			CheckPublishedCover(CoverSubsystem, *DugTrench, TEXT(""), false) &&
			CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT(""), false);
		if (not bIsPublished && not GetHasPhaseTimedOut(*World))
		{
			return;
		}
		Check(
			bIsDug,
			TEXT("a planned trench is dug piece by piece"),
			FString::Printf(TEXT("built=%d of %d"), DugTrench->GetBuiltPieceCount(), DugTrench->GetPieceCount()));
		CheckPublishedCover(CoverSubsystem, *BuiltTrench, TEXT("a piece added again publishes its socket again"), true);
		CheckPublishedCover(CoverSubsystem, *DugTrench, TEXT("a dug trench publishes the sockets of every dug piece"), true);
		TrenchScenario.Start(BuiltTrench);
		M_Phase = ECoverRuntimeTrenchTestPhase::SoldierInRuntimeTrench;
		return;
	}
	if (not TrenchScenario.GetIsRunning())
	{
		Finish(*World, TrenchScenario.GetFailedCheckCount());
	}
}

bool FCoverRuntimeTrenchTestScenario::SpawnTrenches(UWorld& World)
{
	using namespace CoverRuntimeTrenchTestScenarioPrivate;
	const ASquadController* PlayerSquad = nullptr;
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator && PlayerSquad == nullptr; ++SquadIterator)
	{
		const URTSComponent* SquadRTSComponent = IsValid(*SquadIterator) ? SquadIterator->GetRTSComponent() : nullptr;
		if (IsValid(SquadRTSComponent) && SquadRTSComponent->GetOwningPlayer() == PlayerOwnedTeam)
		{
			PlayerSquad = *SquadIterator;
		}
	}
	FVector BuiltTrenchStart = FVector::ZeroVector;
	const bool bFoundGround = IsValid(PlayerSquad) &&
		TryProjectToNavigation(World, PlayerSquad->GetActorLocation() + BuiltTrenchOffsetFromSquad, BuiltTrenchStart);
	Check(bFoundGround, TEXT("a player squad with navigation mesh beside it exists"));
	if (not bFoundGround)
	{
		return false;
	}
	// What the HQ's trench placement spawns.
	UClass* TrenchClass = ARTSFieldTrench::StaticClass();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARTSFieldTrench* BuiltTrench = World.SpawnActor<ARTSFieldTrench>(
		TrenchClass, FTransform(BuiltTrenchStart), SpawnParameters);
	ARTSFieldTrench* DugTrench = World.SpawnActor<ARTSFieldTrench>(
		TrenchClass, FTransform(BuiltTrenchStart), SpawnParameters);
	if (not IsValid(BuiltTrench) || not IsValid(DugTrench))
	{
		Check(false, TEXT("the runtime trenches spawn"));
		return false;
	}
	M_BuiltTrench = BuiltTrench;
	M_DugTrench = DugTrench;
	for (const FFieldTrenchPiecePlacement& Piece : MakeStraightsWithEnds(BuiltTrenchStart, 90.0f))
	{
		BuiltTrench->AddBuiltPiece(Piece);
	}
	// Planned on the ground as the HQ's placement plans it: the straights get as short as uneven ground needs,
	// and ground steeper than the placement allows is passed over.
	FVector DugTrenchStart = FVector::ZeroVector;
	TArray<FFieldTrenchPiecePlacement> DugPieces;
	float DugTrenchSlope = 0.0f;
	for (const FVector& Offset : DugTrenchOffsetsFromSquad)
	{
		if (not TryProjectToNavigation(World, PlayerSquad->GetActorLocation() + Offset, DugTrenchStart))
		{
			continue;
		}
		const FVector2D DragStart(DugTrenchStart);
		DugPieces = DugTrench->PlanOnGround(
			{DragStart, DragStart + FVector2D(DugTrenchDragLength * 0.5f, 0.0f), DragStart + FVector2D(DugTrenchDragLength, 0.0f)},
			10000.0f,
			DugTrenchStart.Z);
		DugTrench->SetPlannedPieces(DugPieces);
		DugTrenchSlope = DugTrench->GetSteepestPlannedSlopeDegrees();
		if (DugTrenchSlope <= DugTrenchMaximumSlopeDegrees)
		{
			break;
		}
	}
	Check(
		DugTrenchSlope <= DugTrenchMaximumSlopeDegrees,
		TEXT("ground flat enough for a dragged trench is found"),
		FString::Printf(TEXT("steepest_piece_deg=%.1f at %s"), DugTrenchSlope, *DugTrenchStart.ToCompactString()));
	const bool bPlannedCapsAtBothEnds = DugPieces.Num() >= 3 &&
		DugPieces[0].Piece == EFieldTrenchPiece::End && DugPieces.Last().Piece == EFieldTrenchPiece::End;
	Check(
		bPlannedCapsAtBothEnds,
		TEXT("a dragged trench is planned with an end cap at both ends"),
		FString::Printf(TEXT("pieces=%d length_cm=%.0f"), DugPieces.Num(), FieldTrenchLayout::GetTotalLengthCm(DugPieces)));
	DugTrench->StartConstruction({}, PlayerOwnedTeam, {});
	UE_LOG(
		LogRTSCoverRuntimeTrenchTest,
		Display,
		TEXT("RTS_COVER_RUNTIME_TRENCH_TEST spawned built=%s at %s dug=%s at %s pieces=%d class=%s"),
		*BuiltTrench->GetName(),
		*BuiltTrenchStart.ToCompactString(),
		*DugTrench->GetName(),
		*DugTrenchStart.ToCompactString(),
		DugPieces.Num(),
		*GetNameSafe(TrenchClass));
	return true;
}

bool FCoverRuntimeTrenchTestScenario::CheckPublishedCover(
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const ARTSFieldTrench& Trench,
	const TCHAR* Description,
	const bool bReport)
{
	using namespace CoverRuntimeTrenchTestScenarioPrivate;
	const TArray<FTransform> CargoSockets = GetBuiltCargoSockets(Trench);
	TArray<const FRTSCoverPoint*> TrenchPoints;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		if (CoverPoint.CoverType == ERTSCoverType::TrenchStandUp && CoverSubsystem.ResolveBlockingProvider(CoverPoint) == &Trench)
		{
			TrenchPoints.Add(&CoverPoint);
		}
	}
	int32 MatchedSocketCount = 0;
	int32 WrongPointCount = 0;
	float WorstApproachHeightError = 0.0f;
	for (const FTransform& CargoSocket : CargoSockets)
	{
		const FVector SocketLocation = CargoSocket.GetLocation();
		const FRTSCoverPoint* const* MatchingPoint = TrenchPoints.FindByPredicate([&SocketLocation](const FRTSCoverPoint* Point)
		{
			return FVector::Dist(Point->SnapLocation, SocketLocation) <= MaximumSnapError;
		});
		if (MatchingPoint == nullptr)
		{
			continue;
		}
		++MatchedSocketCount;
		const FRTSCoverPoint& Point = **MatchingPoint;
		const float ApproachHeightError = FMath::Abs(Point.Location.Z - (SocketLocation.Z + SocketDepthBelowGround));
		WorstApproachHeightError = FMath::Max(WorstApproachHeightError, ApproachHeightError);
		const bool bIsRightPoint = Point.bSnapSoldierToSocket &&
			FVector::Dist(Point.GetHoldLocation(), SocketLocation) <= MaximumSnapError &&
			FVector::Dist2D(Point.Location, SocketLocation) <= MaximumApproachFlatOffset &&
			ApproachHeightError <= MaximumApproachHeightError;
		WrongPointCount += bIsRightPoint ? 0 : 1;
		if (bReport && not bIsRightPoint)
		{
			UE_LOG(
				LogRTSCoverRuntimeTrenchTest,
				Display,
				TEXT("RTS_COVER_RUNTIME_TRENCH_TEST wrong_point socket=%s walk_to=%s flat_offset_cm=%.1f height_error_cm=%.1f snap=%d"),
				*SocketLocation.ToCompactString(),
				*Point.Location.ToCompactString(),
				FVector::Dist2D(Point.Location, SocketLocation),
				ApproachHeightError,
				Point.bSnapSoldierToSocket ? 1 : 0);
		}
	}
	const bool bPasses = not CargoSockets.IsEmpty() && MatchedSocketCount == CargoSockets.Num() &&
		TrenchPoints.Num() == CargoSockets.Num() && WrongPointCount == 0;
	if (bReport)
	{
		Check(
			bPasses,
			Description,
			FString::Printf(
				TEXT("trench=%s built_pieces=%d cargo_sockets=%d published_points=%d matched=%d wrong=%d worst_approach_height_error_cm=%.1f"),
				*Trench.GetName(),
				Trench.GetBuiltPieceCount(),
				CargoSockets.Num(),
				TrenchPoints.Num(),
				MatchedSocketCount,
				WrongPointCount,
				WorstApproachHeightError));
	}
	return bPasses;
}

#else

void FCoverRuntimeTrenchTestScenario::Start()
{
}

void FCoverRuntimeTrenchTestScenario::Tick(URTSCoverFinderWorldSubsystem&, FCoverTrenchTestScenario&)
{
}

#endif
