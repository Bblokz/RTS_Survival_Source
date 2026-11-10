// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSFieldTrenchSpline.h"

#include "Algo/Reverse.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

namespace RTSFieldTrenchSplinePrivate
{
	// A new trench is drawn 10 m long along the actor's forward axis.
	constexpr float DefaultTrenchLengthCm = 1000.0f;
	constexpr float GroundTraceHalfHeight = 5000.0f;
	// The path this near a joined port is replaced by the port's axis, so the trench meets the port straight on.
	constexpr float PortApproachLengthCm = 400.0f;
	constexpr float PortApproachStepCm = 50.0f;
	// A free start faces the first point of the path this far away, as the planner's own start does.
	constexpr float StartAimDistanceCm = 300.0f;
	// A socket this much higher or lower than the end of the spline is not snapped to.
	constexpr float MaximumSnapHeightDifferenceCm = 600.0f;
	constexpr float MinimumTeeDistanceFromEndCm = 400.0f;
	constexpr float MinimumDistanceBetweenTeesCm = 600.0f;
	// A smaller gap left at a joined port is not closed with a piece.
	constexpr float MinimumClosingGapCm = 40.0f;
	// Closing straights are stretched at most this much; a longer gap takes more of them.
	constexpr float MaximumClosingStretch = 1.3f;
	constexpr float AnchorClaimRetrySeconds = 0.25f;
	// Anchors of a building expansion placed in the level come the tick after BeginPlay.
	constexpr int32 AnchorClaimTries = 20;

	// The trench before and after a Tee stops and starts this far from the middle of the Tee.
	float GetTeeHalfLengthCm()
	{
		return FieldTrenchLayout::GetPieceDefinition(EFieldTrenchPiece::Tee).LengthCm * 0.5f;
	}

	// The trenches that are rebuilding: each is rebuilt once per edit, however the joins between them run.
	TSet<const ARTSFieldTrenchSpline*> RebuildingTrenches;

	FVector2D GetYawDirection(const float Yaw)
	{
		float Sine = 0.0f;
		float Cosine = 1.0f;
		FMath::SinCos(&Sine, &Cosine, FMath::DegreesToRadians(Yaw));
		return FVector2D(Cosine, Sine);
	}

	float GetYawTowards(const FVector2D& From, const FVector2D& To)
	{
		const FVector2D Direction = To - From;
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}

	TArray<float> GetCumulativeDistances(const TArray<FVector>& Path)
	{
		TArray<float> Cumulative;
		Cumulative.Reserve(Path.Num());
		float Distance = 0.0f;
		for (int32 PointIndex = 0; PointIndex < Path.Num(); ++PointIndex)
		{
			if (PointIndex > 0)
			{
				Distance += FVector2D::Distance(FVector2D(Path[PointIndex - 1]), FVector2D(Path[PointIndex]));
			}
			Cumulative.Add(Distance);
		}
		return Cumulative;
	}

	FVector GetPointAtDistance(const TArray<FVector>& Path, const TArray<float>& Cumulative, const float Distance)
	{
		if (Distance <= 0.0f)
		{
			return Path[0];
		}
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			if (Cumulative[PointIndex] >= Distance)
			{
				const float SegmentLength = Cumulative[PointIndex] - Cumulative[PointIndex - 1];
				const float Alpha = SegmentLength > KINDA_SMALL_NUMBER
					? (Distance - Cumulative[PointIndex - 1]) / SegmentLength
					: 1.0f;
				return FMath::Lerp(Path[PointIndex - 1], Path[PointIndex], Alpha);
			}
		}
		return Path.Last();
	}

	// The part of the path between two distances along it, its ends interpolated.
	TArray<FVector> GetSubPath(
		const TArray<FVector>& Path,
		const TArray<float>& Cumulative,
		const float From,
		const float To)
	{
		TArray<FVector> SubPath;
		SubPath.Add(GetPointAtDistance(Path, Cumulative, From));
		for (int32 PointIndex = 0; PointIndex < Path.Num(); ++PointIndex)
		{
			if (Cumulative[PointIndex] > From && Cumulative[PointIndex] < To)
			{
				SubPath.Add(Path[PointIndex]);
			}
		}
		if (To > From)
		{
			SubPath.Add(GetPointAtDistance(Path, Cumulative, To));
		}
		return SubPath;
	}

	// The point of the path nearest to the location: how far along the path it is, how far from the location,
	// and on which side of the path the location lies (positive: right of the way the path runs).
	void FindNearestOnPath(
		const TArray<FVector>& Path,
		const TArray<float>& Cumulative,
		const FVector2D& Location,
		float& OutDistanceAlong,
		float& OutGap,
		float& OutSide)
	{
		OutDistanceAlong = 0.0f;
		OutGap = TNumericLimits<float>::Max();
		OutSide = 0.0f;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			const FVector2D SegmentStart(Path[PointIndex - 1]);
			const FVector2D Segment = FVector2D(Path[PointIndex]) - SegmentStart;
			const float SegmentLengthSquared = Segment.SizeSquared();
			const float Alpha = SegmentLengthSquared > KINDA_SMALL_NUMBER
				? FMath::Clamp(FVector2D::DotProduct(Location - SegmentStart, Segment) / SegmentLengthSquared, 0.0f, 1.0f)
				: 0.0f;
			const FVector2D Nearest = SegmentStart + Segment * Alpha;
			const float Gap = FVector2D::Distance(Location, Nearest);
			if (Gap < OutGap)
			{
				OutGap = Gap;
				OutDistanceAlong = Cumulative[PointIndex - 1] + (Cumulative[PointIndex] - Cumulative[PointIndex - 1]) * Alpha;
				OutSide = FVector2D::CrossProduct(Segment, Location - Nearest);
			}
		}
	}

	// Replaces the path near a port by points on the port's axis, so the trench comes straight out of it.
	void ReplacePathNearPort(TArray<FVector>& Path, const bool bAtStart, const FFieldTrenchSplinePort& Port)
	{
		if (Path.Num() < 2)
		{
			return;
		}
		const TArray<float> Cumulative = GetCumulativeDistances(Path);
		const float ApproachLength = FMath::Min(PortApproachLengthCm, Cumulative.Last() * 0.4f);
		const FVector2D PortLocation(Port.Location);
		const FVector2D PortOut = GetYawDirection(Port.Yaw);
		if (bAtStart)
		{
			Algo::Reverse(Path);
		}
		while (Path.Num() > 1 && FVector2D::Distance(FVector2D(Path.Last()), PortLocation) < ApproachLength)
		{
			Path.Pop();
		}
		for (float AlongAxis = ApproachLength; AlongAxis > 0.5f; AlongAxis -= PortApproachStepCm)
		{
			Path.Add(FVector(PortLocation + PortOut * AlongAxis, Port.Location.Z));
		}
		Path.Add(Port.Location);
		if (bAtStart)
		{
			Algo::Reverse(Path);
		}
	}

	float GetHeightNearest(const TArray<FVector>& Path, const FVector2D& GroundPoint)
	{
		float NearestHeight = Path.IsEmpty() ? 0.0f : Path[0].Z;
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (const FVector& Point : Path)
		{
			const float DistanceSquared = FVector2D::DistSquared(FVector2D(Point), GroundPoint);
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestHeight = Point.Z;
			}
		}
		return NearestHeight;
	}

	bool GetIsStraight(const EFieldTrenchPiece Piece)
	{
		const FFieldTrenchPieceDefinition& Definition = FieldTrenchLayout::GetPieceDefinition(Piece);
		return Definition.Ports.Num() == 2 && Definition.Ports[1].Yaw == 0.0f && Definition.Ports[1].Location.Y == 0.0f;
	}

	EFieldTrenchPiece GetStraightOfLength(const float NominalLength, const int32 PieceIndex)
	{
		if (NominalLength >= 500.0f)
		{
			return PieceIndex % 2 == 0 ? EFieldTrenchPiece::Straight_500_A : EFieldTrenchPiece::Straight_500_B;
		}
		if (NominalLength >= 250.0f)
		{
			constexpr EFieldTrenchPiece Variants[] = {
				EFieldTrenchPiece::Straight_250_A, EFieldTrenchPiece::Straight_250_B, EFieldTrenchPiece::Straight_250_C
			};
			return Variants[PieceIndex % 3];
		}
		return EFieldTrenchPiece::Straight_125;
	}

	EFieldTrenchSplineJoinKind GetEndJoinKind(const int32 End)
	{
		return End == 0 ? EFieldTrenchSplineJoinKind::TrenchStart : EFieldTrenchSplineJoinKind::TrenchEnd;
	}

	uint32 HashPieces(const TArray<FFieldTrenchPieceRuntime>& Pieces)
	{
		uint32 Hash = GetTypeHash(Pieces.Num());
		for (const FFieldTrenchPieceRuntime& Piece : Pieces)
		{
			const FFieldTrenchPiecePlacement& Placement = Piece.Placement;
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Placement.Piece)));
			Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(Placement.Location.X), FMath::RoundToInt32(Placement.Location.Y), FMath::RoundToInt32(Placement.Location.Z))));
			Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt32(Placement.Yaw * 10.0f)));
			Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt32(Placement.LengthScale * 1000.0f)));
		}
		return Hash;
	}
}

FFieldTrenchSplinePort FFieldTrenchSplinePort::Make(const FFieldTrenchOpenPort& Port, const float Height)
{
	FFieldTrenchSplinePort SplinePort;
	SplinePort.Location = FVector(Port.Location, Height);
	SplinePort.Yaw = Port.Yaw;
	SplinePort.bIsSet = true;
	return SplinePort;
}

ARTSFieldTrenchSpline::ARTSFieldTrenchSpline()
{
	TrenchPath = CreateDefaultSubobject<USplineComponent>(TEXT("TrenchPath"));
	TrenchPath->SetupAttachment(RootComponent);
	TrenchPath->SetClosedLoop(false);
	TrenchPath->SetCanEverAffectNavigation(false);
	TrenchPath->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrenchPath->SetHiddenInGame(true);
	TrenchPath->ClearSplinePoints(false);
	TrenchPath->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
	TrenchPath->AddSplinePoint(FVector(RTSFieldTrenchSplinePrivate::DefaultTrenchLengthCm, 0.0f, 0.0f),
	                           ESplineCoordinateSpace::Local, false);
	TrenchPath->UpdateSpline();
#if WITH_EDITORONLY_DATA
	TrenchPath->EditorUnselectedSplineSegmentColor = FLinearColor(1.0f, 0.55f, 0.1f);
	TrenchPath->bShouldVisualizeScale = false;
#endif
	bM_CreatePiecesAsConstructionScriptComponents = true;
}

void ARTSFieldTrenchSpline::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// An undone delete brings the trench back as the same object.
	bM_IsBeingRemoved = false;
	RebuildTrench();
}

void ARTSFieldTrenchSpline::Destroyed()
{
	bM_IsBeingRemoved = true;
	const UWorld* World = GetWorld();
	if (IsValid(World) && not World->IsGameWorld())
	{
		// The trenches it joined get their caps back, and the ones joined to it lose their join.
		TArray<ARTSFieldTrenchSpline*> AffectedTrenches = FindJoiners();
		for (const FFieldTrenchSplineJoin* Join : {&StartJoin, &EndJoin})
		{
			if (ARTSFieldTrenchSpline* Target = Cast<ARTSFieldTrenchSpline>(Join->Target.Get()))
			{
				AffectedTrenches.AddUnique(Target);
			}
		}
		for (ARTSFieldTrenchSpline* AffectedTrench : AffectedTrenches)
		{
			if (GetIsSplineTrenchUsable(AffectedTrench))
			{
				AffectedTrench->RebuildTrench();
			}
		}
	}
	Super::Destroyed();
}

void ARTSFieldTrenchSpline::BeginPlay()
{
	Super::BeginPlay();
	// A cap put on in game, when a joined pit goes, is an ordinary piece.
	bM_CreatePiecesAsConstructionScriptComponents = false;
	RefreshGroundIgnoredActors();
	if (StartJoin.Kind == EFieldTrenchSplineJoinKind::Anchor || EndJoin.Kind == EFieldTrenchSplineJoinKind::Anchor)
	{
		M_AnchorClaimTriesLeft = RTSFieldTrenchSplinePrivate::AnchorClaimTries;
		GetWorldTimerManager().SetTimerForNextTick(this, &ARTSFieldTrenchSpline::TryClaimJoinedAnchors);
	}
}

void ARTSFieldTrenchSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(M_AnchorClaimTimer);
	ReleaseClaimedAnchors();
	Super::EndPlay(EndPlayReason);
}

void ARTSFieldTrenchSpline::RebuildTrench()
{
	using namespace RTSFieldTrenchSplinePrivate;
	if (RebuildingTrenches.Contains(this))
	{
		return;
	}
	RebuildingTrenches.Add(this);
	ON_SCOPE_EXIT
	{
		RebuildingTrenches.Remove(this);
	};
	const UWorld* World = GetWorld();
	if (not IsValid(TrenchPath) || not IsValid(World))
	{
		return;
	}
	if (TrenchPath->IsClosedLoop())
	{
		// The planner lays an open trench; a loop is drawn as the open line through its points.
		TrenchPath->SetClosedLoop(false);
	}
	const uint32 HashBefore = HashPieces(GetPieces());
	const TArray<FVector> PathSamples = SampleTrenchPath();

	AActor* const OldTargets[] = {StartJoin.Target.Get(), EndJoin.Target.Get()};
	if (bSnapEnds && PathSamples.Num() >= 2)
	{
		StartJoin = FindJoin(0, PathSamples[0], FFieldTrenchSplineJoin());
		EndJoin = FindJoin(1, PathSamples.Last(), StartJoin);
	}
	else
	{
		StartJoin = FFieldTrenchSplineJoin();
		EndJoin = FFieldTrenchSplineJoin();
	}
	// The trenches this one joins, before and now, drop or put back their caps and Tees first; this trench then
	// plans onto their ports as they are now.
	TArray<ARTSFieldTrenchSpline*, TInlineAllocator<4>> Targets;
	for (AActor* Target : {OldTargets[0], OldTargets[1], StartJoin.Target.Get(), EndJoin.Target.Get()})
	{
		ARTSFieldTrenchSpline* TargetTrench = Cast<ARTSFieldTrenchSpline>(Target);
		if (TargetTrench != this && GetIsSplineTrenchUsable(TargetTrench))
		{
			Targets.AddUnique(TargetTrench);
		}
	}
	for (ARTSFieldTrenchSpline* TargetTrench : Targets)
	{
		TargetTrench->RebuildTrench();
	}

	if (PathSamples.Num() >= 2)
	{
		BuildPieces(PathSamples);
	}
	else
	{
		ClearAllPieces();
		M_Tees.Reset();
		M_StartOutwardPort = FFieldTrenchSplinePort();
		M_EndOutwardPort = FFieldTrenchSplinePort();
		PieceCount = 0;
		TrenchLength = 0.0f;
		SteepestPieceSlope = 0.0f;
	}
	if (HashPieces(GetPieces()) != HashBefore)
	{
		MarkPackageDirtyInEditor();
	}

	// The trenches joined to this one follow its ports and Tees.
	for (ARTSFieldTrenchSpline* Joiner : FindJoiners())
	{
		Joiner->RebuildTrench();
	}
}

void ARTSFieldTrenchSpline::BuildPieces(const TArray<FVector>& PathSamples)
{
	using namespace RTSFieldTrenchSplinePrivate;
	ClearAllPieces();
	M_Tees.Reset();
	M_StartOutwardPort = FFieldTrenchSplinePort();
	M_EndOutwardPort = FFieldTrenchSplinePort();
	RefreshGroundIgnoredActors();

	FFieldTrenchSplinePort JoinedPorts[2] = {GetJoinedPort(StartJoin, 0), GetJoinedPort(EndJoin, 1)};
	// A side join only holds when the other trench found room for its Tee.
	if (not JoinedPorts[0].bIsSet)
	{
		StartJoin = FFieldTrenchSplineJoin();
	}
	if (not JoinedPorts[1].bIsSet)
	{
		EndJoin = FFieldTrenchSplineJoin();
	}
	TArray<FVector> Path = PathSamples;
	if (JoinedPorts[0].bIsSet)
	{
		ReplacePathNearPort(Path, true, JoinedPorts[0]);
	}
	if (JoinedPorts[1].bIsSet)
	{
		ReplacePathNearPort(Path, false, JoinedPorts[1]);
	}
	const TArray<float> Cumulative = GetCumulativeDistances(Path);
	const float PathLength = Cumulative.Last();
	float SummedHeight = 0.0f;
	for (const FVector& Point : Path)
	{
		SummedHeight += Point.Z;
	}
	const float GroundHeight = SummedHeight / Path.Num();

	FFieldTrenchOpenPort StartPort;
	if (JoinedPorts[0].bIsSet)
	{
		StartPort = JoinedPorts[0].ToOpenPort();
	}
	else
	{
		StartPort.Location = FVector2D(Path[0]);
		const FVector* AimPoint = Path.FindByPredicate([&StartPort](const FVector& Point)
		{
			return FVector2D::Distance(StartPort.Location, FVector2D(Point)) >= StartAimDistanceCm;
		});
		StartPort.Yaw = GetYawTowards(StartPort.Location, FVector2D(AimPoint ? *AimPoint : Path.Last()));
		M_StartOutwardPort = FFieldTrenchSplinePort::Make(FieldTrenchLayout::GetReversedPort(StartPort), Path[0].Z);
	}

	// What other spline trenches join here: an end taken by another trench gets no cap, a side join a Tee.
	struct FTeeRequest
	{
		AActor* Joiner = nullptr;
		uint8 JoinerEnd = 0;
		float DistanceAlong = 0.0f;
		bool bIsRightSide = false;
	};
	bool bIsEndTakenByJoiner[2] = {false, false};
	TArray<FTeeRequest> TeeRequests;
	for (ARTSFieldTrenchSpline* Joiner : FindJoiners())
	{
		for (int32 JoinerEnd = 0; JoinerEnd < 2; ++JoinerEnd)
		{
			const FFieldTrenchSplineJoin& Join = Joiner->GetJoin(JoinerEnd);
			if (Join.Target.Get() != this)
			{
				continue;
			}
			if (Join.Kind == EFieldTrenchSplineJoinKind::TrenchStart)
			{
				bIsEndTakenByJoiner[0] = true;
			}
			else if (Join.Kind == EFieldTrenchSplineJoinKind::TrenchEnd)
			{
				bIsEndTakenByJoiner[1] = true;
			}
			else if (Join.Kind == EFieldTrenchSplineJoinKind::TrenchSide)
			{
				FTeeRequest& Request = TeeRequests.AddDefaulted_GetRef();
				Request.Joiner = Joiner;
				Request.JoinerEnd = static_cast<uint8>(JoinerEnd);
				float Gap = 0.0f;
				float Side = 0.0f;
				FindNearestOnPath(Path, Cumulative, FVector2D(Join.EndPoint), Request.DistanceAlong, Gap, Side);
				Request.bIsRightSide = Side > 0.0f;
			}
		}
	}
	TeeRequests.Sort([](const FTeeRequest& A, const FTeeRequest& B) { return A.DistanceAlong < B.DistanceAlong; });

	TArray<FFieldTrenchPiecePlacement> Pieces;
	FFieldTrenchOpenPort UnusedExit;
	if (not JoinedPorts[0].bIsSet && not bIsEndTakenByJoiner[0])
	{
		// The start cap's port faces along the trench, so it joins the port facing back out of the start.
		FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, FieldTrenchLayout::GetReversedPort(StartPort),
		                                    Pieces.AddDefaulted_GetRef(), UnusedExit);
	}
	const int32 FirstTrenchPiece = Pieces.Num();
	FFieldTrenchOpenPort OpenPort = StartPort;
	float SegmentStart = 0.0f;
	FFieldTrenchPlanOptions Options;
	Options.MaximumStraightLengthCm = MaximumStraightLength;
	Options.bAddEndCap = false;
	auto PlanSegmentTo = [&](const float SegmentEnd)
	{
		TArray<FVector2D> SegmentPath;
		for (const FVector& Point : GetSubPath(Path, Cumulative, SegmentStart, SegmentEnd))
		{
			SegmentPath.Add(FVector2D(Point));
		}
		// The segment starts where the trench has got to. Not added in front: a doubled first point stops the
		// planner's progress along the path, and it would aim back at the start.
		SegmentPath[0] = OpenPort.Location;
		Options.StartPort = OpenPort;
		FFieldTrenchOpenPort SegmentOpenEnd;
		Pieces.Append(PlanOnGround(SegmentPath, MaximumTrenchLength, GroundHeight, Options, SegmentOpenEnd));
		OpenPort = SegmentOpenEnd;
	};

	float LastTeeDistance = -TNumericLimits<float>::Max();
	for (const FTeeRequest& Request : TeeRequests)
	{
		if (Request.DistanceAlong < MinimumTeeDistanceFromEndCm ||
			Request.DistanceAlong > PathLength - MinimumTeeDistanceFromEndCm ||
			Request.DistanceAlong - LastTeeDistance < MinimumDistanceBetweenTeesCm)
		{
			// No room for it; the joiner caps its end instead.
			continue;
		}
		LastTeeDistance = Request.DistanceAlong;
		PlanSegmentTo(Request.DistanceAlong - GetTeeHalfLengthCm());
		// Entered from its first port the Tee branches to the left; from its second port, to the right.
		FFieldTrenchPiecePlacement& TeePlacement = Pieces.AddDefaulted_GetRef();
		FFieldTrenchOpenPort TeeExit;
		FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::Tee, Request.bIsRightSide ? 1 : 0, OpenPort, TeePlacement,
		                                    TeeExit);
		if (const TOptional<FFieldTrenchOpenPort> SidePort = FieldTrenchLayout::GetPlacedPort(TeePlacement, 2))
		{
			FFieldTrenchSplineTee& Tee = M_Tees.AddDefaulted_GetRef();
			Tee.Joiner = Request.Joiner;
			Tee.JoinerEnd = Request.JoinerEnd;
			Tee.SidePort = FFieldTrenchSplinePort::Make(SidePort.GetValue(), GetHeightNearest(Path, SidePort->Location));
		}
		OpenPort = TeeExit;
		SegmentStart = Request.DistanceAlong + GetTeeHalfLengthCm();
	}
	PlanSegmentTo(PathLength);

	if (JoinedPorts[1].bIsSet)
	{
		CloseOntoPort(Pieces, FirstTrenchPiece, OpenPort, JoinedPorts[1].ToOpenPort());
	}
	else
	{
		M_EndOutwardPort = FFieldTrenchSplinePort::Make(OpenPort, GetHeightNearest(Path, OpenPort.Location));
		if (not bIsEndTakenByJoiner[1])
		{
			FieldTrenchLayout::PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(),
			                                    UnusedExit);
		}
	}

	for (FFieldTrenchPiecePlacement& Placement : Pieces)
	{
		// The ground is looked for around the path right there, so a trench can climb a hill.
		Placement.Location.Z = GetHeightNearest(Path, FVector2D(Placement.Location));
		AddBuiltPiece(Placement);
	}
	PieceCount = GetPieceCount();
	TrenchLength = FieldTrenchLayout::GetTotalLengthCm(Pieces);
	SteepestPieceSlope = 0.0f;
	for (const FFieldTrenchPieceRuntime& Piece : GetPieces())
	{
		SteepestPieceSlope = FMath::Max(SteepestPieceSlope, Piece.SlopeDegrees);
	}
}

void ARTSFieldTrenchSpline::CloseOntoPort(
	TArray<FFieldTrenchPiecePlacement>& Pieces,
	const int32 FirstTrenchPiece,
	const FFieldTrenchOpenPort& OpenEnd,
	const FFieldTrenchOpenPort& TargetPort) const
{
	using namespace RTSFieldTrenchSplinePrivate;
	FFieldTrenchOpenPort From = OpenEnd;
	// The last straight is laid again from where it starts, so the stretch is spread over a whole piece.
	if (Pieces.Num() > FirstTrenchPiece && GetIsStraight(Pieces.Last().Piece) &&
		FMath::IsNearlyEqual(Pieces.Last().LengthScale, 1.0f))
	{
		From.Location = FVector2D(Pieces.Last().Location);
		From.Yaw = Pieces.Last().Yaw;
		Pieces.Pop();
	}
	const float Gap = FVector2D::Distance(From.Location, TargetPort.Location);
	if (Gap < MinimumClosingGapCm)
	{
		return;
	}
	TArray<float, TInlineAllocator<3>> NominalLengths;
	for (const float Length : {125.0f, 250.0f, 500.0f})
	{
		if (Length <= FMath::Max(125.0f, MaximumStraightLength))
		{
			NominalLengths.Add(Length);
		}
	}
	const int32 StraightCount = FMath::Max(1, FMath::CeilToInt32(Gap / (NominalLengths.Last() * MaximumClosingStretch)));
	const float EachLength = Gap / StraightCount;
	float NominalLength = NominalLengths[0];
	for (const float Length : NominalLengths)
	{
		if (FMath::Abs(FMath::Loge(EachLength / Length)) < FMath::Abs(FMath::Loge(EachLength / NominalLength)))
		{
			NominalLength = Length;
		}
	}
	const float Yaw = GetYawTowards(From.Location, TargetPort.Location);
	const FVector2D Direction = GetYawDirection(Yaw);
	for (int32 StraightIndex = 0; StraightIndex < StraightCount; ++StraightIndex)
	{
		FFieldTrenchPiecePlacement& Placement = Pieces.AddDefaulted_GetRef();
		Placement.Piece = GetStraightOfLength(NominalLength, Pieces.Num());
		Placement.Location = FVector(From.Location + Direction * EachLength * StraightIndex, 0.0f);
		Placement.Yaw = Yaw;
		Placement.LengthCm = EachLength;
		Placement.LengthScale = EachLength / NominalLength;
	}
}

FFieldTrenchSplineJoin ARTSFieldTrenchSpline::FindJoin(
	const int32 End,
	const FVector& EndPoint,
	const FFieldTrenchSplineJoin& OtherEnd) const
{
	using namespace RTSFieldTrenchSplinePrivate;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return FFieldTrenchSplineJoin();
	}
	// An end another trench joined stays its; this trench does not join anything there itself.
	for (const ARTSFieldTrenchSpline* Joiner : FindJoiners())
	{
		for (int32 JoinerEnd = 0; JoinerEnd < 2; ++JoinerEnd)
		{
			if (Joiner->GetJoin(JoinerEnd).Target.Get() == this && Joiner->GetJoin(JoinerEnd).Kind == GetEndJoinKind(End))
			{
				return FFieldTrenchSplineJoin();
			}
		}
	}
	const FVector2D EndPoint2D(EndPoint);
	FFieldTrenchSplineJoin BestEnd;
	float BestEndDistance = SnapRadius;
	FFieldTrenchSplineJoin BestSide;
	float BestSideDistance = SnapRadius;
	auto GetIsHeightClose = [&EndPoint](const float Height)
	{
		return FMath::Abs(Height - EndPoint.Z) <= MaximumSnapHeightDifferenceCm;
	};

	for (TObjectIterator<UFieldTrenchAnchorComponent> AnchorIterator; AnchorIterator; ++AnchorIterator)
	{
		const UFieldTrenchAnchorComponent* AnchorComponent = *AnchorIterator;
		AActor* AnchorOwner = IsValid(AnchorComponent) && not AnchorComponent->IsTemplate() &&
			AnchorComponent->GetWorld() == World
				? AnchorComponent->GetOwner()
				: nullptr;
		if (not IsValid(AnchorOwner) || AnchorOwner->IsActorBeingDestroyed())
		{
			continue;
		}
		TArray<FFieldTrenchAnchorSocketPort> SocketPorts;
		UFieldTrenchAnchorComponent::GetAnchorSocketPorts(*AnchorOwner, SocketPorts);
		for (const FFieldTrenchAnchorSocketPort& SocketPort : SocketPorts)
		{
			const float Distance = FVector2D::Distance(EndPoint2D, SocketPort.Port.Location);
			if (Distance > BestEndDistance || not GetIsHeightClose(SocketPort.Height))
			{
				continue;
			}
			const bool bIsOtherEnd = OtherEnd.Kind == EFieldTrenchSplineJoinKind::Anchor &&
				OtherEnd.Target.Get() == AnchorOwner && OtherEnd.AnchorSocket == SocketPort.SocketName;
			if (bIsOtherEnd || GetIsAnchorTaken(*AnchorOwner, SocketPort.SocketName))
			{
				continue;
			}
			BestEndDistance = Distance;
			BestEnd = FFieldTrenchSplineJoin();
			BestEnd.Kind = EFieldTrenchSplineJoinKind::Anchor;
			BestEnd.Target = AnchorOwner;
			BestEnd.AnchorSocket = SocketPort.SocketName;
		}
	}

	for (TActorIterator<ARTSFieldTrenchSpline> TrenchIterator(World); TrenchIterator; ++TrenchIterator)
	{
		ARTSFieldTrenchSpline* Other = *TrenchIterator;
		if (Other == this || not GetIsSplineTrenchUsable(Other))
		{
			continue;
		}
		for (int32 OtherEndIndex = 0; OtherEndIndex < 2; ++OtherEndIndex)
		{
			const FFieldTrenchSplinePort& Port = Other->GetOutwardPort(OtherEndIndex);
			if (not Port.bIsSet || Other->GetJoin(OtherEndIndex).Kind != EFieldTrenchSplineJoinKind::None)
			{
				continue;
			}
			const float Distance = FVector2D::Distance(EndPoint2D, FVector2D(Port.Location));
			if (Distance > BestEndDistance || not GetIsHeightClose(Port.Location.Z))
			{
				continue;
			}
			const EFieldTrenchSplineJoinKind Kind = GetEndJoinKind(OtherEndIndex);
			const bool bIsOtherEnd = OtherEnd.Kind == Kind && OtherEnd.Target.Get() == Other;
			if (bIsOtherEnd || GetIsTrenchEndTaken(*Other, OtherEndIndex))
			{
				continue;
			}
			BestEndDistance = Distance;
			BestEnd = FFieldTrenchSplineJoin();
			BestEnd.Kind = Kind;
			BestEnd.Target = Other;
		}
		const USplineComponent* OtherPath = Other->GetTrenchPath();
		if (not IsValid(OtherPath))
		{
			continue;
		}
		const float InputKey = OtherPath->FindInputKeyClosestToWorldLocation(EndPoint);
		const FVector Nearest = OtherPath->GetLocationAtSplineInputKey(InputKey, ESplineCoordinateSpace::World);
		const float Distance = FVector2D::Distance(EndPoint2D, FVector2D(Nearest));
		const float DistanceAlong = OtherPath->GetDistanceAlongSplineAtSplineInputKey(InputKey);
		const bool bHasRoomForTee = DistanceAlong >= MinimumTeeDistanceFromEndCm &&
			DistanceAlong <= OtherPath->GetSplineLength() - MinimumTeeDistanceFromEndCm;
		if (Distance <= BestSideDistance && bHasRoomForTee && GetIsHeightClose(Nearest.Z))
		{
			BestSideDistance = Distance;
			BestSide = FFieldTrenchSplineJoin();
			BestSide.Kind = EFieldTrenchSplineJoinKind::TrenchSide;
			BestSide.Target = Other;
		}
	}
	// Ports beat the side of a trench: a trench end near a pit joins the pit.
	FFieldTrenchSplineJoin Join = BestEnd.Kind != EFieldTrenchSplineJoinKind::None ? BestEnd : BestSide;
	Join.EndPoint = EndPoint;
	return Join;
}

FFieldTrenchSplinePort ARTSFieldTrenchSpline::GetJoinedPort(const FFieldTrenchSplineJoin& Join, const int32 End) const
{
	if (not Join.IsSet())
	{
		return FFieldTrenchSplinePort();
	}
	if (Join.Kind == EFieldTrenchSplineJoinKind::Anchor)
	{
		TArray<FFieldTrenchAnchorSocketPort> SocketPorts;
		UFieldTrenchAnchorComponent::GetAnchorSocketPorts(*Join.Target, SocketPorts);
		for (const FFieldTrenchAnchorSocketPort& SocketPort : SocketPorts)
		{
			if (SocketPort.SocketName == Join.AnchorSocket)
			{
				return FFieldTrenchSplinePort::Make(SocketPort.Port, SocketPort.Height);
			}
		}
		return FFieldTrenchSplinePort();
	}
	const ARTSFieldTrenchSpline* Other = Cast<ARTSFieldTrenchSpline>(Join.Target.Get());
	if (not GetIsSplineTrenchUsable(Other))
	{
		return FFieldTrenchSplinePort();
	}
	if (Join.Kind == EFieldTrenchSplineJoinKind::TrenchSide)
	{
		return Other->GetTeePortFor(this, End);
	}
	const int32 OtherEnd = Join.Kind == EFieldTrenchSplineJoinKind::TrenchStart ? 0 : 1;
	return Other->GetJoin(OtherEnd).Kind == EFieldTrenchSplineJoinKind::None
		? Other->GetOutwardPort(OtherEnd)
		: FFieldTrenchSplinePort();
}

FFieldTrenchSplinePort ARTSFieldTrenchSpline::GetTeePortFor(const AActor* Joiner, const int32 JoinerEnd) const
{
	const FFieldTrenchSplineTee* Tee = M_Tees.FindByPredicate([Joiner, JoinerEnd](const FFieldTrenchSplineTee& Candidate)
	{
		return Candidate.Joiner.Get() == Joiner && Candidate.JoinerEnd == JoinerEnd;
	});
	return Tee ? Tee->SidePort : FFieldTrenchSplinePort();
}

bool ARTSFieldTrenchSpline::GetIsTrenchEndTaken(const ARTSFieldTrenchSpline& Target, const int32 TargetEnd) const
{
	const EFieldTrenchSplineJoinKind Kind = RTSFieldTrenchSplinePrivate::GetEndJoinKind(TargetEnd);
	for (TActorIterator<ARTSFieldTrenchSpline> TrenchIterator(GetWorld()); TrenchIterator; ++TrenchIterator)
	{
		const ARTSFieldTrenchSpline* Other = *TrenchIterator;
		if (Other == this || not GetIsSplineTrenchUsable(Other))
		{
			continue;
		}
		for (int32 OtherEnd = 0; OtherEnd < 2; ++OtherEnd)
		{
			if (Other->GetJoin(OtherEnd).Target.Get() == &Target && Other->GetJoin(OtherEnd).Kind == Kind)
			{
				return true;
			}
		}
	}
	return false;
}

bool ARTSFieldTrenchSpline::GetIsAnchorTaken(const AActor& AnchorOwner, const FName SocketName) const
{
	for (TActorIterator<ARTSFieldTrenchSpline> TrenchIterator(GetWorld()); TrenchIterator; ++TrenchIterator)
	{
		const ARTSFieldTrenchSpline* Other = *TrenchIterator;
		if (Other == this || not GetIsSplineTrenchUsable(Other))
		{
			continue;
		}
		for (int32 OtherEnd = 0; OtherEnd < 2; ++OtherEnd)
		{
			const FFieldTrenchSplineJoin& Join = Other->GetJoin(OtherEnd);
			if (Join.Kind == EFieldTrenchSplineJoinKind::Anchor && Join.Target.Get() == &AnchorOwner &&
				Join.AnchorSocket == SocketName)
			{
				return true;
			}
		}
	}
	return false;
}

TArray<ARTSFieldTrenchSpline*> ARTSFieldTrenchSpline::FindJoiners() const
{
	TArray<ARTSFieldTrenchSpline*> Joiners;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return Joiners;
	}
	for (TActorIterator<ARTSFieldTrenchSpline> TrenchIterator(World); TrenchIterator; ++TrenchIterator)
	{
		ARTSFieldTrenchSpline* Other = *TrenchIterator;
		if (Other != this && GetIsSplineTrenchUsable(Other) &&
			(Other->StartJoin.Target.Get() == this || Other->EndJoin.Target.Get() == this))
		{
			Joiners.Add(Other);
		}
	}
	return Joiners;
}

void ARTSFieldTrenchSpline::RefreshGroundIgnoredActors()
{
	M_ExtraGroundIgnoredActors.Reset();
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Other trenches block the landscape channel; at a join their pieces lie right where this one looks.
	for (TActorIterator<ARTSFieldTrench> TrenchIterator(World); TrenchIterator; ++TrenchIterator)
	{
		if (*TrenchIterator != this)
		{
			M_ExtraGroundIgnoredActors.Add(*TrenchIterator);
		}
	}
	// Near an anchor the ground lies under its owner's walls.
	for (const FFieldTrenchSplineJoin* Join : {&StartJoin, &EndJoin})
	{
		if (Join->Kind == EFieldTrenchSplineJoinKind::Anchor && IsValid(Join->Target))
		{
			M_ExtraGroundIgnoredActors.Add(Join->Target.Get());
		}
	}
}

void ARTSFieldTrenchSpline::MarkPackageDirtyInEditor()
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (IsValid(World) && not World->IsGameWorld())
	{
		MarkPackageDirty();
	}
#endif
}

void ARTSFieldTrenchSpline::TryClaimJoinedAnchors()
{
	bool bIsWaitingForAnchors = false;
	for (int32 End = 0; End < 2; ++End)
	{
		const FFieldTrenchSplineJoin& Join = GetJoin(End);
		if (Join.Kind != EFieldTrenchSplineJoinKind::Anchor || not IsValid(Join.Target) || M_ClaimedAnchors[End].IsSet())
		{
			continue;
		}
		UFieldTrenchAnchorComponent* AnchorComponent = Join.Target->FindComponentByClass<UFieldTrenchAnchorComponent>();
		if (not IsValid(AnchorComponent))
		{
			continue;
		}
		if (not AnchorComponent->GetAreAnchorsAvailable())
		{
			bIsWaitingForAnchors = true;
			continue;
		}
		for (int32 AnchorIndex = 0; AnchorIndex < AnchorComponent->GetAnchorCount(); ++AnchorIndex)
		{
			if (AnchorComponent->GetAnchorSocketName(AnchorIndex) != Join.AnchorSocket)
			{
				continue;
			}
			if (AnchorComponent->TryClaimAnchor(AnchorIndex, *this))
			{
				M_ClaimedAnchors[End].Component = AnchorComponent;
				M_ClaimedAnchors[End].AnchorIndex = AnchorIndex;
				// The trench is there from the start: the neck shows instead of the plug.
				AnchorComponent->SetAnchorJoined(AnchorIndex, true);
			}
			break;
		}
	}
	if (bIsWaitingForAnchors && --M_AnchorClaimTriesLeft > 0)
	{
		GetWorldTimerManager().SetTimer(M_AnchorClaimTimer, this, &ARTSFieldTrenchSpline::TryClaimJoinedAnchors,
		                                RTSFieldTrenchSplinePrivate::AnchorClaimRetrySeconds, false);
	}
}

void ARTSFieldTrenchSpline::ReleaseClaimedAnchors()
{
	for (FFieldTrenchAnchorHandle& Claimed : M_ClaimedAnchors)
	{
		if (UFieldTrenchAnchorComponent* AnchorComponent = Claimed.Get())
		{
			AnchorComponent->ReleaseAnchor(Claimed.AnchorIndex, *this);
		}
		Claimed.Reset();
	}
}

void ARTSFieldTrenchSpline::OnStartAnchorLost(const FFieldTrenchOpenPort& AnchorPort, const float AnchorHeight)
{
	bool bWasJoinedThere = false;
	for (FFieldTrenchAnchorHandle& Claimed : M_ClaimedAnchors)
	{
		const UFieldTrenchAnchorComponent* AnchorComponent = Claimed.Get();
		if (IsValid(AnchorComponent) &&
			AnchorComponent->GetAnchorPort(Claimed.AnchorIndex).Location.Equals(AnchorPort.Location, 1.0f))
		{
			// The anchor already let go of this trench.
			Claimed.Reset();
			bWasJoinedThere = true;
		}
	}
	if (not bWasJoinedThere)
	{
		return;
	}
	FFieldTrenchPiecePlacement EndCap;
	FFieldTrenchOpenPort UnusedExit;
	FieldTrenchLayout::PlacePieceOnPort(
		EFieldTrenchPiece::End, 0, FieldTrenchLayout::GetReversedPort(AnchorPort), EndCap, UnusedExit);
	EndCap.Location.Z = AnchorHeight;
	AddBuiltPiece(EndCap);
}

void ARTSFieldTrenchSpline::SnapSplinePointsToGround()
{
	UWorld* World = GetWorld();
	if (not IsValid(TrenchPath) || not IsValid(World))
	{
		return;
	}
	TrenchPath->Modify();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FieldTrenchSplineSnap), false, this);
	for (TActorIterator<ARTSFieldTrench> TrenchIterator(World); TrenchIterator; ++TrenchIterator)
	{
		QueryParams.AddIgnoredActor(*TrenchIterator);
	}
	const int32 PointCount = TrenchPath->GetNumberOfSplinePoints();
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const FVector Point = TrenchPath->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
		const FVector TraceStart = Point + FVector::UpVector * RTSFieldTrenchSplinePrivate::GroundTraceHalfHeight;
		const FVector TraceEnd = Point - FVector::UpVector * RTSFieldTrenchSplinePrivate::GroundTraceHalfHeight;
		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			TrenchPath->SetLocationAtSplinePoint(
				PointIndex, GroundHit.ImpactPoint, ESplineCoordinateSpace::World, false);
		}
	}
	TrenchPath->UpdateSpline();
	RebuildTrench();
}

TArray<FVector> ARTSFieldTrenchSpline::SampleTrenchPath() const
{
	TArray<FVector> Samples;
	const float SplineLength = TrenchPath->GetSplineLength();
	if (SplineLength <= KINDA_SMALL_NUMBER)
	{
		return Samples;
	}
	const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt32(SplineLength / FMath::Max(10.0f, PathSampleSpacing)));
	Samples.Reserve(SegmentCount + 1);
	for (int32 SampleIndex = 0; SampleIndex <= SegmentCount; ++SampleIndex)
	{
		const float Distance = SplineLength * SampleIndex / SegmentCount;
		Samples.Add(TrenchPath->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
	}
	return Samples;
}

bool ARTSFieldTrenchSpline::GetIsSplineTrenchUsable(const ARTSFieldTrenchSpline* Trench)
{
	return IsValid(Trench) && not Trench->IsTemplate() && not Trench->IsActorBeingDestroyed() &&
		not Trench->bM_IsBeingRemoved;
}
