// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSBarbedWireLine.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Player/ConstructionPreview/BuildingGridOverlay/BuildingGridOverlay.h"
#include "RTS_Survival/Player/ConstructionPreview/MoveUnitsFromConstruction/FMoveUnitsFromConstruction.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldConstruction.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace RTSBarbedWireLinePrivate
{
	constexpr float GroundTraceHalfHeight = 2000.0f;
	// The ground is sampled this far to either side of the wire to lean the piece with a side slope; about the depth
	// of the coil.
	constexpr float GroundSampleHalfDepth = 60.0f;
	// Piece ends this close together continue each other: a line started on another line's end joins it there.
	constexpr float JoinedEndTolerance = 75.0f;
	// A closing line whose length is this little more than whole pieces gets no extra piece for it.
	constexpr float ClosingPieceTolerance = 0.05f;
	// Units are looked for up to this far above the ground under a piece; the box is twice as high.
	constexpr float ClearSpotQueryHalfHeight = 100.0f;
	// Obstacles are looked for in a box over the coil, from this far above the ground under the piece.
	constexpr float ObstacleQueryGroundClearance = 25.0f;
	constexpr float ObstacleQueryHalfHeight = 75.0f;
	constexpr float ObstacleQueryHalfDepth = 45.0f;
	const FName PlacementOkayParameter(TEXT("PlacementOkay"));
	const TCHAR* const DefaultPieceClass = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_BarbedWire_FieldConstruction."
		"Ger_BarbedWire_FieldConstruction_C");
	const TCHAR* const DefaultPieceMesh = TEXT(
		"/Game/RTS_Survival/Meshes/Military/Barriers/SM_RTS_Barrier_BarbWire.SM_RTS_Barrier_BarbWire");
	const TCHAR* const DefaultPlannedPieceMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev.MI_ConstructionPrev");

	float GetPathLength(const TArray<FVector2D>& Path)
	{
		float Length = 0.0f;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			Length += FVector2D::Distance(Path[PointIndex - 1], Path[PointIndex]);
		}
		return Length;
	}

	/** @return The way the path runs at its end; X when it has no length. */
	FVector2D GetEndDirection(const TArray<FVector2D>& Path)
	{
		for (int32 PointIndex = Path.Num() - 1; PointIndex >= 1; --PointIndex)
		{
			const FVector2D Segment = Path[PointIndex] - Path[PointIndex - 1];
			if (not Segment.IsNearlyZero())
			{
				return Segment.GetSafeNormal();
			}
		}
		return FVector2D(1.0f, 0.0f);
	}

	/** @return The point this far along the path; beyond its end the path is continued the way it ends. */
	FVector2D GetPointAlongPath(const TArray<FVector2D>& Path, const float Distance)
	{
		float Walked = 0.0f;
		for (int32 PointIndex = 1; PointIndex < Path.Num(); ++PointIndex)
		{
			const float SegmentLength = FVector2D::Distance(Path[PointIndex - 1], Path[PointIndex]);
			if (Walked + SegmentLength >= Distance && SegmentLength > KINDA_SMALL_NUMBER)
			{
				const float Alpha = (Distance - Walked) / SegmentLength;
				return FMath::Lerp(Path[PointIndex - 1], Path[PointIndex], Alpha);
			}
			Walked += SegmentLength;
		}
		return Path.Last() + GetEndDirection(Path) * FMath::Max(0.0f, Distance - Walked);
	}
}

ARTSBarbedWireLine::ARTSBarbedWireLine()
{
	using namespace RTSBarbedWireLinePrivate;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PieceClass = TSoftClassPtr<AFieldConstruction>(FSoftObjectPath(DefaultPieceClass));
	PieceMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(DefaultPieceMesh));
	PlannedPieceMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultPlannedPieceMaterial));
}

TArray<FBarbedWireSpan> ARTSBarbedWireLine::PlanAlongPath(
	const TArray<FVector2D>& Path,
	const float PieceLength,
	const float MaximumLength,
	const bool bEndOnPathEnd)
{
	using namespace RTSBarbedWireLinePrivate;
	TArray<FBarbedWireSpan> Spans;
	const float PathLength = GetPathLength(Path);
	if (Path.IsEmpty() || PieceLength <= KINDA_SMALL_NUMBER)
	{
		return Spans;
	}
	const int32 MaximumPieces = FMath::Max(1, FMath::FloorToInt32((MaximumLength + KINDA_SMALL_NUMBER) / PieceLength));
	const int32 ClosingPieceCount = FMath::Max(1, FMath::CeilToInt32(PathLength / PieceLength - ClosingPieceTolerance));
	// A line too long to close within the maximum is laid as a free line instead.
	const bool bCloses = bEndOnPathEnd && PathLength > 1.0f && ClosingPieceCount <= MaximumPieces;
	const int32 PieceCount = bCloses
		                         ? ClosingPieceCount
		                         : FMath::Clamp(FMath::RoundToInt32(PathLength / PieceLength), 1, MaximumPieces);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const bool bIsClosingPiece = bCloses && PieceIndex == PieceCount - 1;
		// The closing piece is pulled back from the end of the path, so it ends there and overlaps the one before it.
		const float StartDistance = bIsClosingPiece
			                            ? FMath::Max(0.0f, PathLength - PieceLength)
			                            : PieceIndex * PieceLength;
		const float EndDistance = bIsClosingPiece ? PathLength : StartDistance + PieceLength;
		FBarbedWireSpan& Span = Spans.AddDefaulted_GetRef();
		Span.Start = GetPointAlongPath(Path, StartDistance);
		Span.End = GetPointAlongPath(Path, EndDistance);
	}
	return Spans;
}

void ARTSBarbedWireLine::SetPlannedPieces(const TArray<FBarbedWireSpan>& Spans, const float GroundHeight)
{
	if (bM_IsConstructionStarted)
	{
		RTSFunctionLibrary::ReportError("Barbed wire line " + GetName() + ": the plan cannot change once it is built.");
		return;
	}
	for (int32 LeftoverIndex = M_Pieces.Num() - 1; LeftoverIndex >= Spans.Num(); --LeftoverIndex)
	{
		DestroyGhost(M_Pieces[LeftoverIndex]);
		M_Pieces.RemoveAt(LeftoverIndex);
	}
	float AroundHeight = GroundHeight;
	for (int32 SpanIndex = 0; SpanIndex < Spans.Num(); ++SpanIndex)
	{
		FBarbedWirePieceRuntime& Piece = M_Pieces.IsValidIndex(SpanIndex)
			                                 ? M_Pieces[SpanIndex]
			                                 : M_Pieces.AddDefaulted_GetRef();
		if (not IsValid(Piece.GhostComponent))
		{
			Piece.GhostComponent = CreateGhostComponent();
		}
		Piece.Span = Spans[SpanIndex];
		Piece.State = EBarbedWirePieceState::Planned;
		Piece.BuildProgress = 0.0f;
		FitPieceToGround(Piece, AroundHeight);
		// The ground is followed from piece to piece, so a long line over a hill still finds it.
		if (Piece.bHasGround)
		{
			AroundHeight = Piece.GroundTransform.GetLocation().Z;
		}
		ShowPieceState(Piece);
	}
}

void ARTSBarbedWireLine::SetPlanIsValid(const bool bIsValid)
{
	if (UMaterialInterface* PlannedMaterial = GetPlannedMaterial(); IsValid(PlannedMaterial))
	{
		M_PlannedMaterial->SetScalarParameterValue(
			RTSBarbedWireLinePrivate::PlacementOkayParameter, bIsValid ? 1.0f : 0.0f);
	}
}

void ARTSBarbedWireLine::StartConstruction(const int32 OwningPlayer, const TMap<ERTSResourceType, int32>& CostPerMetre)
{
	SetPlanIsValid(true);
	bM_IsConstructionStarted = true;
	M_OwningPlayer = OwningPlayer;
	M_CostPerMetre = CostPerMetre;
	M_TimeUntilRetry = 0.0f;
	M_ClearSpotWaitSeconds = 0.0f;
	SetActorTickEnabled(FindPieceToBuild() != INDEX_NONE);
	ScheduleRemovalIfEmpty();
}

int32 ARTSBarbedWireLine::GetBuiltPieceCount() const
{
	int32 BuiltCount = 0;
	for (const FBarbedWirePieceRuntime& Piece : M_Pieces)
	{
		BuiltCount += Piece.State == EBarbedWirePieceState::Built ? 1 : 0;
	}
	return BuiltCount;
}

float ARTSBarbedWireLine::GetSteepestPlannedSlopeDegrees() const
{
	float SteepestSlope = 0.0f;
	for (const FBarbedWirePieceRuntime& Piece : M_Pieces)
	{
		if (Piece.State == EBarbedWirePieceState::Planned || Piece.State == EBarbedWirePieceState::Building)
		{
			SteepestSlope = FMath::Max(SteepestSlope, Piece.SlopeDegrees);
		}
	}
	return SteepestSlope;
}

bool ARTSBarbedWireLine::GetIsPieceOnObstacle(const FBarbedWirePieceRuntime& Piece) const
{
	using namespace RTSBarbedWireLinePrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	// Above the ground under the piece, so bumps in it do not count; the box leans with the piece.
	const FQuat PieceRotation = Piece.GroundTransform.GetRotation();
	const FVector QueryCentre = Piece.GroundTransform.GetLocation() + PieceRotation.GetUpVector() *
		(ObstacleQueryGroundClearance + ObstacleQueryHalfHeight);
	const FCollisionShape QueryBox = FCollisionShape::MakeBox(FVector(
		PieceLength * 0.5f, ObstacleQueryHalfDepth, ObstacleQueryHalfHeight));
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, QueryCentre, PieceRotation, COLLISION_OBJ_BUILDING_PLACEMENT, QueryBox,
	                             FCollisionQueryParams(SCENE_QUERY_STAT(BarbedWireObstacle), false, this));
	const UClass* WireClass = PieceClass.Get();
	return Overlaps.ContainsByPredicate([WireClass](const FOverlapResult& Overlap)
	{
		const AActor* OverlappedActor = Overlap.GetActor();
		if (not IsValid(OverlappedActor))
		{
			return false;
		}
		// Soldiers step out of the way (vehicles ignore this channel; a nomadic building is a vehicle and must count),
		// wire may cross or join wire, and the building grid only exists while placing.
		const bool bIsUnit = OverlappedActor->IsA<ACharacter>();
		const bool bIsWire = OverlappedActor->IsA<ARTSBarbedWireLine>() ||
			(IsValid(WireClass) && OverlappedActor->IsA(WireClass));
		return not bIsUnit && not bIsWire && not OverlappedActor->IsA<ABuildingGridOverlay>();
	});
}

void ARTSBarbedWireLine::GetOpenEndsWithinLine(TArray<FVector>& OutOpenEnds) const
{
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		const FBarbedWirePieceRuntime& Piece = M_Pieces[PieceIndex];
		if (not Piece.GetIsStanding())
		{
			continue;
		}
		const float GroundHeight = Piece.GroundTransform.GetLocation().Z;
		const bool bIsStartOpen = PieceIndex == 0 || not M_Pieces[PieceIndex - 1].GetIsStanding();
		const bool bIsEndOpen = PieceIndex == M_Pieces.Num() - 1 || not M_Pieces[PieceIndex + 1].GetIsStanding();
		if (bIsStartOpen)
		{
			OutOpenEnds.Add(FVector(Piece.Span.Start, GroundHeight));
		}
		if (bIsEndOpen)
		{
			OutOpenEnds.Add(FVector(Piece.Span.End, GroundHeight));
		}
	}
}

bool ARTSBarbedWireLine::FindOpenEndNear(
	UWorld* World,
	const FVector2D& Location,
	const float Radius,
	const int32 OwningPlayer,
	const ARTSBarbedWireLine* IgnoredLine,
	FVector& OutEnd)
{
	using namespace RTSBarbedWireLinePrivate;
	if (not IsValid(World))
	{
		return false;
	}
	// Every end of every standing piece, to see whether a candidate is continued by another line.
	TArray<TPair<const ARTSBarbedWireLine*, FVector2D>> PieceEnds;
	TArray<TPair<const ARTSBarbedWireLine*, FVector>> Candidates;
	for (TActorIterator<ARTSBarbedWireLine> LineIterator(World); LineIterator; ++LineIterator)
	{
		const ARTSBarbedWireLine* Line = *LineIterator;
		if (not IsValid(Line) || Line == IgnoredLine || not Line->GetIsUnderConstruction() ||
			Line->GetOwningPlayer() != OwningPlayer)
		{
			continue;
		}
		for (const FBarbedWirePieceRuntime& Piece : Line->GetPieces())
		{
			if (Piece.GetIsStanding())
			{
				PieceEnds.Emplace(Line, Piece.Span.Start);
				PieceEnds.Emplace(Line, Piece.Span.End);
			}
		}
		TArray<FVector> OpenEnds;
		Line->GetOpenEndsWithinLine(OpenEnds);
		for (const FVector& OpenEnd : OpenEnds)
		{
			if (FVector2D::Distance(FVector2D(OpenEnd), Location) <= Radius)
			{
				Candidates.Emplace(Line, OpenEnd);
			}
		}
	}
	float NearestDistance = TNumericLimits<float>::Max();
	for (const TPair<const ARTSBarbedWireLine*, FVector>& Candidate : Candidates)
	{
		const FVector2D CandidateEnd(Candidate.Value);
		const bool bIsContinuedByOtherLine = PieceEnds.ContainsByPredicate(
			[&Candidate, &CandidateEnd](const TPair<const ARTSBarbedWireLine*, FVector2D>& PieceEnd)
			{
				return PieceEnd.Key != Candidate.Key &&
					FVector2D::Distance(PieceEnd.Value, CandidateEnd) <= JoinedEndTolerance;
			});
		const float Distance = FVector2D::Distance(CandidateEnd, Location);
		if (bIsContinuedByOtherLine || Distance >= NearestDistance)
		{
			continue;
		}
		NearestDistance = Distance;
		OutEnd = Candidate.Value;
	}
	return NearestDistance < TNumericLimits<float>::Max();
}

TMap<ERTSResourceType, int32> ARTSBarbedWireLine::GetCostOfLength(
	const TMap<ERTSResourceType, int32>& CostPerMetre,
	const float LengthCm)
{
	TMap<ERTSResourceType, int32> Cost;
	for (const TPair<ERTSResourceType, int32>& ResourcePrice : CostPerMetre)
	{
		if (ResourcePrice.Value <= 0)
		{
			continue;
		}
		Cost.Add(ResourcePrice.Key, FMath::Max(1, FMath::RoundToInt32(ResourcePrice.Value * LengthCm / 100.0f)));
	}
	return Cost;
}

void ARTSBarbedWireLine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	// The wire actors stand on their own; they only stop telling this line about their death.
	for (const FBarbedWirePieceRuntime& Piece : M_Pieces)
	{
		AFieldConstruction* WireActor = Piece.WireActor.Get();
		if (not IsValid(WireActor))
		{
			continue;
		}
		WireActor->OnDestructableEnvActorDied.RemoveAll(this);
		WireActor->OnDestroyed.RemoveDynamic(this, &ARTSBarbedWireLine::OnWireActorDestroyed);
	}
	Super::EndPlay(EndPlayReason);
}

void ARTSBarbedWireLine::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickBuilding(DeltaSeconds);
}

UStaticMeshComponent* ARTSBarbedWireLine::CreateGhostComponent()
{
	UStaticMeshComponent* GhostComponent = NewObject<UStaticMeshComponent>(this);
	GhostComponent->SetMobility(EComponentMobility::Movable);
	// Only a marking: soldiers, vehicles and shells pass it; the wire actor does the rest once the piece stands.
	GhostComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostComponent->SetCanEverAffectNavigation(false);
	GhostComponent->SetReceivesDecals(false);
	GhostComponent->SetStaticMesh(GetPieceMesh());
	GhostComponent->SetupAttachment(RootComponent);
	GhostComponent->RegisterComponent();
	AddInstanceComponent(GhostComponent);
	return GhostComponent;
}

UStaticMesh* ARTSBarbedWireLine::GetPieceMesh()
{
	if (IsValid(M_LoadedPieceMesh))
	{
		return M_LoadedPieceMesh;
	}
	M_LoadedPieceMesh = PieceMesh.LoadSynchronous();
	if (not IsValid(M_LoadedPieceMesh))
	{
		RTSFunctionLibrary::ReportError("Barbed wire line " + GetName() + ": could not load the piece mesh " +
			PieceMesh.ToString() + "; the planned wire is invisible.");
	}
	return M_LoadedPieceMesh;
}

UMaterialInterface* ARTSBarbedWireLine::GetPlannedMaterial()
{
	if (IsValid(M_PlannedMaterial))
	{
		return M_PlannedMaterial;
	}
	UMaterialInterface* BaseMaterial = PlannedPieceMaterial.LoadSynchronous();
	if (not IsValid(BaseMaterial))
	{
		return nullptr;
	}
	M_PlannedMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	M_PlannedMaterial->SetScalarParameterValue(RTSBarbedWireLinePrivate::PlacementOkayParameter, 1.0f);
	return M_PlannedMaterial;
}

void ARTSBarbedWireLine::FitPieceToGround(FBarbedWirePieceRuntime& Piece, const float AroundHeight) const
{
	using namespace RTSBarbedWireLinePrivate;
	const FBarbedWireSpan& Span = Piece.Span;
	FVector2D Along = Span.End - Span.Start;
	Along = Along.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : Along.GetSafeNormal();
	// A quarter turn of yaw: to the right of the wire.
	const FVector2D Side(-Along.Y, Along.X);
	const FVector2D Centre = Span.GetCentre();
	FVector GroundCentre;
	FVector GroundStart;
	FVector GroundEnd;
	FVector GroundLeft;
	FVector GroundRight;
	Piece.bHasGround = TryTraceGround(Centre, AroundHeight, GroundCentre);
	const bool bHasSlope = Piece.bHasGround &&
		TryTraceGround(Span.Start, AroundHeight, GroundStart) &&
		TryTraceGround(Span.End, AroundHeight, GroundEnd) &&
		TryTraceGround(Centre - Side * GroundSampleHalfDepth, AroundHeight, GroundLeft) &&
		TryTraceGround(Centre + Side * GroundSampleHalfDepth, AroundHeight, GroundRight);
	FRotator GroundRotation(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.0f);
	if (bHasSlope)
	{
		GroundRotation = FRotationMatrix::MakeFromXY(GroundEnd - GroundStart, GroundRight - GroundLeft).Rotator();
	}
	Piece.GroundTransform = FTransform(GroundRotation, Piece.bHasGround ? GroundCentre : FVector(Centre, AroundHeight));
	const FVector PieceUp = GroundRotation.Quaternion().GetUpVector();
	Piece.SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(PieceUp.Z, -1.0f, 1.0f)));
}

bool ARTSBarbedWireLine::TryTraceGround(const FVector2D& GroundPoint, const float AroundHeight, FVector& OutGround) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector TraceStart(GroundPoint, AroundHeight + RTSBarbedWireLinePrivate::GroundTraceHalfHeight);
	const FVector TraceEnd(GroundPoint, AroundHeight - RTSBarbedWireLinePrivate::GroundTraceHalfHeight);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BarbedWireGround), false, this);
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return false;
	}
	OutGround = GroundHit.ImpactPoint;
	return true;
}

void ARTSBarbedWireLine::ShowPieceState(FBarbedWirePieceRuntime& Piece)
{
	UStaticMeshComponent* GhostComponent = Piece.GhostComponent;
	if (not IsValid(GhostComponent))
	{
		return;
	}
	const bool bIsPlanned = Piece.State == EBarbedWirePieceState::Planned;
	UMaterialInterface* PlannedMaterial = bIsPlanned ? GetPlannedMaterial() : nullptr;
	const int32 MaterialCount = GhostComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		// Without an override the slot shows the mesh's own material again.
		GhostComponent->SetMaterial(MaterialIndex, PlannedMaterial);
	}
	GhostComponent->SetCastShadow(not bIsPlanned);
	const float SinkDepth = Piece.State == EBarbedWirePieceState::Building
		                        ? (1.0f - Piece.BuildProgress) * BuildStartDepth
		                        : 0.0f;
	FTransform ShownTransform = Piece.GroundTransform;
	ShownTransform.AddToTranslation(-Piece.GroundTransform.GetRotation().GetUpVector() * SinkDepth);
	GhostComponent->SetWorldTransform(ShownTransform);
}

int32 ARTSBarbedWireLine::FindPieceToBuild() const
{
	return M_Pieces.IndexOfByPredicate([](const FBarbedWirePieceRuntime& Piece)
	{
		return Piece.State == EBarbedWirePieceState::Planned || Piece.State == EBarbedWirePieceState::Building;
	});
}

void ARTSBarbedWireLine::TickBuilding(const float DeltaSeconds)
{
	const int32 PieceIndex = FindPieceToBuild();
	if (PieceIndex == INDEX_NONE)
	{
		SetActorTickEnabled(false);
		return;
	}
	FBarbedWirePieceRuntime& Piece = M_Pieces[PieceIndex];
	if (Piece.State == EBarbedWirePieceState::Planned && not TryStartPiece(Piece, DeltaSeconds))
	{
		return;
	}
	const float BuildSeconds = BuildSecondsPerMetre * PieceLength / 100.0f;
	Piece.BuildProgress = BuildSeconds > KINDA_SMALL_NUMBER
		                      ? FMath::Min(1.0f, Piece.BuildProgress + DeltaSeconds / BuildSeconds)
		                      : 1.0f;
	if (Piece.BuildProgress >= 1.0f)
	{
		FinishPiece(PieceIndex);
		return;
	}
	ShowPieceState(Piece);
}

bool ARTSBarbedWireLine::TryStartPiece(FBarbedWirePieceRuntime& Piece, const float DeltaSeconds)
{
	if (bM_IsWaitingForClearSpot)
	{
		M_ClearSpotWaitSeconds += DeltaSeconds;
	}
	M_TimeUntilRetry -= DeltaSeconds;
	if (M_TimeUntilRetry > 0.0f)
	{
		return false;
	}
	const bool bHasWaitedLongEnough = M_ClearSpotWaitSeconds >= MaximumClearSpotWaitSeconds;
	if (not bHasWaitedLongEnough && GetIsSpotTakenAndClearOwnUnits(Piece))
	{
		bM_IsWaitingForClearSpot = true;
		M_TimeUntilRetry = RetrySeconds;
		return false;
	}
	bM_IsWaitingForClearSpot = false;
	// Checked after the spot: a piece is paid for only when it can be strung at once.
	if (not TryPayForPiece())
	{
		bM_IsWaitingForResources = true;
		M_TimeUntilRetry = RetrySeconds;
		return false;
	}
	bM_IsWaitingForResources = false;
	M_ClearSpotWaitSeconds = 0.0f;
	Piece.State = EBarbedWirePieceState::Building;
	Piece.BuildProgress = 0.0f;
	return true;
}

bool ARTSBarbedWireLine::GetIsSpotTakenAndClearOwnUnits(const FBarbedWirePieceRuntime& Piece) const
{
	using namespace RTSBarbedWireLinePrivate;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FQuat PieceRotation = Piece.GroundTransform.GetRotation();
	const FVector QueryCentre = Piece.GroundTransform.GetLocation() + PieceRotation.GetUpVector() *
		ClearSpotQueryHalfHeight;
	const FCollisionShape QueryBox = FCollisionShape::MakeBox(FVector(
		PieceLength * 0.5f, SpotHalfDepth, ClearSpotQueryHalfHeight));
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	ObjectQuery.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	ObjectQuery.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY_HULL);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, QueryCentre, PieceRotation, ObjectQuery, QueryBox,
	                                FCollisionQueryParams(SCENE_QUERY_STAT(BarbedWireClearSpot), false, this));
	const bool bIsTaken = Overlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
	{
		return Cast<IRTSNavAgentInterface>(Overlap.GetActor()) != nullptr;
	});
	if (bIsTaken)
	{
		FMoveUnitsFromConstruction::MoveOverlappingUnitsForPlayer1AwayFromBounds(
			this, GetPieceWorldBounds(Piece), FRotator::ZeroRotator);
	}
	return bIsTaken;
}

FBox ARTSBarbedWireLine::GetPieceWorldBounds(const FBarbedWirePieceRuntime& Piece) const
{
	using namespace RTSBarbedWireLinePrivate;
	const FBox LocalBox(
		FVector(-PieceLength * 0.5f, -SpotHalfDepth, 0.0f),
		FVector(PieceLength * 0.5f, SpotHalfDepth, ClearSpotQueryHalfHeight * 2.0f));
	return LocalBox.TransformBy(Piece.GroundTransform);
}

bool ARTSBarbedWireLine::TryPayForPiece() const
{
	const TMap<ERTSResourceType, int32> PieceCost = GetCostOfLength(M_CostPerMetre, PieceLength);
	if (PieceCost.IsEmpty())
	{
		return true;
	}
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		return false;
	}
	// Checked first: paying reports every shortfall to the player, and a waiting line asks twice a second.
	if (ResourceManager->GetCanPayForCost(PieceCost) != EPlayerError::Error_None)
	{
		return false;
	}
	return ResourceManager->PayForCosts(PieceCost);
}

void ARTSBarbedWireLine::FinishPiece(const int32 PieceIndex)
{
	FBarbedWirePieceRuntime& Piece = M_Pieces[PieceIndex];
	UWorld* World = GetWorld();
	UClass* WireClass = PieceClass.LoadSynchronous();
	if (not IsValid(World) || not IsValid(WireClass))
	{
		RTSFunctionLibrary::ReportError("Barbed wire line " + GetName() + ": could not load the wire piece class " +
			PieceClass.ToString() + "; the piece is dropped.");
		MarkPieceDestroyed(PieceIndex);
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFieldConstruction* WireActor = World->SpawnActor<AFieldConstruction>(
		WireClass, Piece.GroundTransform, SpawnParameters);
	if (not IsValid(WireActor) || WireActor->IsActorBeingDestroyed())
	{
		// A vehicle that stood on the spot when the piece was strung crushed it while it spawned.
		MarkPieceDestroyed(PieceIndex);
		return;
	}
	Piece.WireActor = WireActor;
	Piece.State = EBarbedWirePieceState::Built;
	Piece.BuildProgress = 1.0f;
	DestroyGhost(Piece);
	BindToWireActor(PieceIndex, *WireActor);
	// Soldiers that walked onto the spot while it was strung now stand in wire; they step out of it.
	FMoveUnitsFromConstruction::MoveOverlappingUnitsForPlayer1AwayFromBounds(
		this, GetPieceWorldBounds(Piece), FRotator::ZeroRotator);
}

void ARTSBarbedWireLine::BindToWireActor(const int32 PieceIndex, AFieldConstruction& WireActor)
{
	WireActor.OnDestructableEnvActorDied.AddWeakLambda(this, [this, PieceIndex]()
	{
		OnWireActorDied(PieceIndex);
	});
	WireActor.OnDestroyed.AddUniqueDynamic(this, &ARTSBarbedWireLine::OnWireActorDestroyed);
}

void ARTSBarbedWireLine::OnWireActorDied(const int32 PieceIndex)
{
	if (not M_Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	// The gap opens for infantry at once, not only when the collapsed wreck is removed.
	if (const AFieldConstruction* WireActor = M_Pieces[PieceIndex].WireActor.Get())
	{
		if (URTSNavCollision* NavCollision = WireActor->FindComponentByClass<URTSNavCollision>())
		{
			NavCollision->EnableAffectNavmesh(false);
		}
	}
	MarkPieceDestroyed(PieceIndex);
}

void ARTSBarbedWireLine::OnWireActorDestroyed(AActor* DestroyedActor)
{
	constexpr bool bEvenIfPendingKill = true;
	const int32 PieceIndex = M_Pieces.IndexOfByPredicate([DestroyedActor](const FBarbedWirePieceRuntime& Piece)
	{
		return Piece.WireActor.Get(bEvenIfPendingKill) == DestroyedActor;
	});
	if (PieceIndex != INDEX_NONE)
	{
		MarkPieceDestroyed(PieceIndex);
	}
}

void ARTSBarbedWireLine::MarkPieceDestroyed(const int32 PieceIndex)
{
	if (not M_Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	FBarbedWirePieceRuntime& Piece = M_Pieces[PieceIndex];
	if (Piece.State == EBarbedWirePieceState::Destroyed)
	{
		return;
	}
	Piece.State = EBarbedWirePieceState::Destroyed;
	DestroyGhost(Piece);
	ScheduleRemovalIfEmpty();
}

void ARTSBarbedWireLine::DestroyGhost(FBarbedWirePieceRuntime& Piece) const
{
	if (IsValid(Piece.GhostComponent) && Piece.GhostComponent->GetOwner() == this)
	{
		Piece.GhostComponent->DestroyComponent();
	}
	Piece.GhostComponent = nullptr;
}

void ARTSBarbedWireLine::ScheduleRemovalIfEmpty()
{
	if (bM_IsRemovalScheduled || not bM_IsConstructionStarted || GetHasStandingPiece())
	{
		return;
	}
	bM_IsRemovalScheduled = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &ARTSBarbedWireLine::RemoveIfEmpty);
}

void ARTSBarbedWireLine::RemoveIfEmpty()
{
	bM_IsRemovalScheduled = false;
	if (not GetHasStandingPiece())
	{
		Destroy();
	}
}

bool ARTSBarbedWireLine::GetHasStandingPiece() const
{
	return M_Pieces.ContainsByPredicate([](const FBarbedWirePieceRuntime& Piece)
	{
		return Piece.GetIsStanding();
	});
}
