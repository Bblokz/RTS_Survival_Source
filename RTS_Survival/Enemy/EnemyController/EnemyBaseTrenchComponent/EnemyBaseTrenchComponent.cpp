// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyBaseTrenchComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorSubsystem.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"

namespace EnemyBaseTrenchPrivate
{
	// The enemy AI plays as player 2.
	constexpr int32 EnemyPlayerIndex = 2;
	// A plan that does not fit is shortened and planned again at most this often.
	constexpr int32 MaximumShortenAttempts = 8;
	constexpr float LengthTolerance = 1.0f;
	// Base points of two think steps closer than this are the same base.
	constexpr float SameBaseDistance = 3000.0f;
	// The arc's sections follow it through a point every this many centimetres.
	constexpr float ArcPointSpacing = 300.0f;
	// The box a piece is checked with: as wide as the trench over its sandbags, from just above the ground (which
	// strays at most 20 cm from a piece) to a man's height, so the ground itself never counts.
	constexpr float PieceCheckHalfWidth = 200.0f;
	constexpr float PieceCheckBottom = 60.0f;
	constexpr float PieceCheckTop = 260.0f;
	constexpr float GroundTraceHalfHeight = 5000.0f;
	constexpr float CentimetresPerMetre = 100.0f;
}

UEnemyBaseTrenchComponent::UEnemyBaseTrenchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyBaseTrenchComponent::StartBaseTrenchDigging()
{
	UWorld* World = GetWorld();
	if (not Settings.bEnabled || bM_IsDigging || not IsValid(World))
	{
		return;
	}
	bM_IsDigging = true;
	World->GetTimerManager().SetTimer(M_ThinkTimerHandle, this, &UEnemyBaseTrenchComponent::ThinkStep,
	                                  Settings.ThinkIntervalSeconds, true, 0.0f);
}

void UEnemyBaseTrenchComponent::StopBaseTrenchDigging()
{
	bM_IsDigging = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ThinkTimerHandle);
	}
}

void UEnemyBaseTrenchComponent::AddFortifiedBase(
	const FVector& Centre,
	const float PerimeterRadius,
	const FVector2D ThreatDirection)
{
	FEnemyTrenchBase& Base = M_Bases.AddDefaulted_GetRef();
	Base.Centre = Centre;
	Base.PerimeterRadius = FMath::Max(0.0f, PerimeterRadius);
	Base.ThreatDirection = ThreatDirection.GetSafeNormal();
	Base.bIsAddedByDesigner = true;
	AbsorbPitOnlyBases(M_Bases.Num() - 1);
}

void UEnemyBaseTrenchComponent::AbsorbPitOnlyBases(const int32 BaseIndex)
{
	if (not M_Bases.IsValidIndex(BaseIndex))
	{
		return;
	}
	// Removed from the back, so the indices before them stay valid; the base itself may move down.
	int32 AbsorbingIndex = BaseIndex;
	for (int32 OtherIndex = M_Bases.Num() - 1; OtherIndex >= 0; --OtherIndex)
	{
		const FEnemyTrenchBase& Other = M_Bases[OtherIndex];
		const bool bIsPitOnly = Other.PerimeterRadius <= 0.0f && not Other.bIsAddedByDesigner;
		if (OtherIndex == AbsorbingIndex || not bIsPitOnly ||
			FVector::Dist2D(Other.Centre, M_Bases[AbsorbingIndex].Centre) > Settings.BaseGroupingRadius)
		{
			continue;
		}
		FEnemyTrenchBase& Absorbing = M_Bases[AbsorbingIndex];
		Absorbing.TrenchCount += Other.TrenchCount;
		Absorbing.DugMetres += Other.DugMetres;
		Absorbing.NextDigTimeSeconds = FMath::Max(Absorbing.NextDigTimeSeconds, Other.NextDigTimeSeconds);
		M_Bases.RemoveAt(OtherIndex);
		AbsorbingIndex -= OtherIndex < AbsorbingIndex ? 1 : 0;
	}
}

void UEnemyBaseTrenchComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBaseTrenchDigging();
	Super::EndPlay(EndPlayReason);
}

void UEnemyBaseTrenchComponent::ThinkStep()
{
	const UWorld* World = GetWorld();
	const UFieldTrenchAnchorSubsystem* AnchorSubsystem = IsValid(World)
		? World->GetSubsystem<UFieldTrenchAnchorSubsystem>()
		: nullptr;
	if (not Settings.bEnabled || not IsValid(AnchorSubsystem))
	{
		return;
	}
	UpdateBasesFromStrategicAI();
	TArray<FFieldTrenchAnchorHandle> FreeAnchors;
	AnchorSubsystem->GetFreeAnchorsOfPlayer(EnemyBaseTrenchPrivate::EnemyPlayerIndex, FreeAnchors);
	FreeAnchors.RemoveAll([this](const FFieldTrenchAnchorHandle& Anchor)
	{
		return GetIsAnchorFailed(Anchor);
	});
	AddBasesForLonelyPits(FreeAnchors);
	for (int32 BaseIndex = 0; BaseIndex < M_Bases.Num(); ++BaseIndex)
	{
		if (CountLiveTrenches() >= Settings.MaxTrenches)
		{
			return;
		}
		(void)TryDigForBase(BaseIndex, FreeAnchors);
	}
}

void UEnemyBaseTrenchComponent::UpdateBasesFromStrategicAI()
{
	const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner());
	const UEnemyStrategicAIComponent* StrategicAI = IsValid(EnemyController)
		? EnemyController->GetEnemyStrategicAIComponent()
		: nullptr;
	if (not IsValid(StrategicAI))
	{
		return;
	}
	for (const FEnemyBasePointCoreBuildings& BasePoint : StrategicAI->GetStrategicAIBlackboard().EnemyBasePoints)
	{
		// The arc stands clear of the base's farthest production building.
		float FarthestBuilding = 0.0f;
		for (const FVector& BuildingLocation : BasePoint.CoreBuildingLocations)
		{
			FarthestBuilding = FMath::Max(FarthestBuilding, FVector::Dist2D(BuildingLocation, BasePoint.BaseLocation));
		}
		const float PerimeterRadius = FMath::Clamp(FarthestBuilding + Settings.PerimeterMargin,
		                                           Settings.MinimumPerimeterRadius, Settings.MaximumPerimeterRadius);
		const int32 KnownBase = FindBaseNear(BasePoint.BaseLocation, EnemyBaseTrenchPrivate::SameBaseDistance);
		if (KnownBase != INDEX_NONE)
		{
			// A base whose arc is planned keeps it, so its sections stay where the first ones were dug.
			FEnemyTrenchBase& Base = M_Bases[KnownBase];
			if (Base.PerimeterSections.IsEmpty() && not Base.bIsAddedByDesigner)
			{
				Base.Centre = BasePoint.BaseLocation;
				Base.PerimeterRadius = PerimeterRadius;
				AbsorbPitOnlyBases(KnownBase);
			}
			continue;
		}
		FEnemyTrenchBase& NewBase = M_Bases.AddDefaulted_GetRef();
		NewBase.Centre = BasePoint.BaseLocation;
		NewBase.PerimeterRadius = PerimeterRadius;
		// Pits seen before the base search found their base were fortified alone so far; they join it now.
		AbsorbPitOnlyBases(M_Bases.Num() - 1);
	}
}

int32 UEnemyBaseTrenchComponent::FindBaseNear(const FVector& Location, const float Radius) const
{
	int32 NearestBase = INDEX_NONE;
	float NearestDistanceSquared = FMath::Square(Radius);
	for (int32 BaseIndex = 0; BaseIndex < M_Bases.Num(); ++BaseIndex)
	{
		const float DistanceSquared = FVector::DistSquared2D(M_Bases[BaseIndex].Centre, Location);
		if (DistanceSquared <= NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestBase = BaseIndex;
		}
	}
	return NearestBase;
}

void UEnemyBaseTrenchComponent::AddBasesForLonelyPits(const TArray<FFieldTrenchAnchorHandle>& FreeAnchors)
{
	for (const FFieldTrenchAnchorHandle& Anchor : FreeAnchors)
	{
		const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
		if (not IsValid(AnchorComponent))
		{
			continue;
		}
		const FVector AnchorLocation = AnchorComponent->GetAnchorLocation(Anchor.AnchorIndex);
		if (FindBaseNear(AnchorLocation, Settings.BaseGroupingRadius) != INDEX_NONE)
		{
			continue;
		}
		FEnemyTrenchBase& PitBase = M_Bases.AddDefaulted_GetRef();
		PitBase.Centre = AnchorLocation;
	}
}

bool UEnemyBaseTrenchComponent::TryDigForBase(const int32 BaseIndex, const TArray<FFieldTrenchAnchorHandle>& FreeAnchors)
{
	// Nothing below adds bases, so the reference stays valid.
	FEnemyTrenchBase& Base = M_Bases[BaseIndex];
	const float Now = GetWorld()->GetTimeSeconds();
	if (not GetCanBaseDig(Base, Now))
	{
		return false;
	}
	// The pits first: they are what the base defends with, and their trenches are short.
	for (const FFieldTrenchAnchorHandle& Anchor : FreeAnchors)
	{
		const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
		if (not IsValid(AnchorComponent) || not AnchorComponent->GetIsAnchorFree(Anchor.AnchorIndex) ||
			FindBaseNear(AnchorComponent->GetAnchorLocation(Anchor.AnchorIndex), Settings.BaseGroupingRadius) !=
			BaseIndex)
		{
			continue;
		}
		if (TryDigFromAnchor(Base, Anchor))
		{
			Base.NextDigTimeSeconds = Now + Settings.DigCooldownSeconds;
			return true;
		}
		M_FailedAnchors.Add(Anchor);
	}
	if (not Settings.bDigPerimeterTrenches || Base.PerimeterRadius <= 0.0f)
	{
		return false;
	}
	if (TryDigPerimeterSection(Base))
	{
		Base.NextDigTimeSeconds = Now + Settings.DigCooldownSeconds;
		return true;
	}
	return false;
}

bool UEnemyBaseTrenchComponent::GetCanBaseDig(const FEnemyTrenchBase& Base, const float Now) const
{
	return Now >= Base.NextDigTimeSeconds && Base.TrenchCount < Settings.MaxTrenchesPerBase &&
		Base.DugMetres < Settings.MaxTrenchMetresPerBase;
}

bool UEnemyBaseTrenchComponent::TryDigFromAnchor(FEnemyTrenchBase& Base, const FFieldTrenchAnchorHandle& Anchor)
{
	const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
	if (not IsValid(AnchorComponent))
	{
		return false;
	}
	// Straight out of the port: the crew entrance of the pit leads into it.
	const FFieldTrenchOpenPort& Port = AnchorComponent->GetAnchorPort(Anchor.AnchorIndex);
	const float PortYawRadians = FMath::DegreesToRadians(Port.Yaw);
	const FVector2D Out(FMath::Cos(PortYawRadians), FMath::Sin(PortYawRadians));
	return TryLayTrench(Base, {Port.Location, Port.Location + Out * Settings.PitTrenchLength}, Anchor);
}

bool UEnemyBaseTrenchComponent::TryDigPerimeterSection(FEnemyTrenchBase& Base)
{
	if (Base.PerimeterSections.IsEmpty() && not TryPlanPerimeterSections(Base))
	{
		// The player's HQ is not known yet; the arc is planned on a later think step.
		return false;
	}
	// A section that does not fit at all (a building, a cliff) is skipped; the next one is tried in this step.
	while (Base.PerimeterSections.IsValidIndex(Base.NextPerimeterSection))
	{
		const TArray<FVector2D> Section = Base.PerimeterSections[Base.NextPerimeterSection];
		++Base.NextPerimeterSection;
		if (TryLayTrench(Base, Section, FFieldTrenchAnchorHandle()))
		{
			return true;
		}
	}
	return false;
}

bool UEnemyBaseTrenchComponent::TryPlanPerimeterSections(FEnemyTrenchBase& Base) const
{
	using namespace EnemyBaseTrenchPrivate;
	const FVector2D ThreatDirection = GetThreatDirection(Base);
	if (ThreatDirection.IsNearlyZero() || Base.PerimeterRadius <= 0.0f)
	{
		return false;
	}
	const float ArcLength = FMath::DegreesToRadians(Settings.PerimeterArcDegrees) * Base.PerimeterRadius;
	const float SectionWithGap = Settings.PerimeterSectionLength + Settings.PerimeterGapLength;
	// One more section than gaps: the arc starts and ends with a section.
	const int32 SectionCount = FMath::Max(1, FMath::FloorToInt32(
		                                      (ArcLength + Settings.PerimeterGapLength) / SectionWithGap));
	const float UsedLength = SectionCount * SectionWithGap - Settings.PerimeterGapLength;
	const float ThreatYaw = FMath::Atan2(ThreatDirection.Y, ThreatDirection.X);
	const float StartAngle = ThreatYaw - UsedLength * 0.5f / Base.PerimeterRadius;
	const FVector2D Centre(Base.Centre);
	// Dug from the middle of the arc outwards, so the side facing the player is dug first.
	TArray<int32> SectionOrder;
	for (int32 Offset = 0; SectionOrder.Num() < SectionCount; ++Offset)
	{
		const int32 Middle = SectionCount / 2;
		for (const int32 Candidate : {Middle + Offset, Middle - Offset - 1})
		{
			if (Candidate >= 0 && Candidate < SectionCount && not SectionOrder.Contains(Candidate))
			{
				SectionOrder.Add(Candidate);
			}
		}
	}
	for (const int32 SectionIndex : SectionOrder)
	{
		TArray<FVector2D>& Section = Base.PerimeterSections.AddDefaulted_GetRef();
		const float SectionStart = SectionIndex * SectionWithGap;
		const int32 PointCount = FMath::Max(2, FMath::CeilToInt32(Settings.PerimeterSectionLength / ArcPointSpacing) + 1);
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const float AlongArc = SectionStart + Settings.PerimeterSectionLength * PointIndex / (PointCount - 1);
			const float Angle = StartAngle + AlongArc / Base.PerimeterRadius;
			Section.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Base.PerimeterRadius);
		}
	}
	return true;
}

FVector2D UEnemyBaseTrenchComponent::GetThreatDirection(const FEnemyTrenchBase& Base) const
{
	if (not Base.ThreatDirection.IsNearlyZero())
	{
		return Base.ThreatDirection;
	}
	const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner());
	const UEnemyStrategicAIComponent* StrategicAI = IsValid(EnemyController)
		? EnemyController->GetEnemyStrategicAIComponent()
		: nullptr;
	if (not IsValid(StrategicAI))
	{
		return FVector2D::ZeroVector;
	}
	const FVector PlayerHQ = StrategicAI->GetStrategicAIBlackboard().CurrentPlayerUnitCounts.PlayerHQLocation;
	if (PlayerHQ.IsNearlyZero())
	{
		return FVector2D::ZeroVector;
	}
	return (FVector2D(PlayerHQ) - FVector2D(Base.Centre)).GetSafeNormal();
}

bool UEnemyBaseTrenchComponent::TryLayTrench(
	FEnemyTrenchBase& Base,
	const TArray<FVector2D>& Path,
	const FFieldTrenchAnchorHandle& Anchor)
{
	using namespace EnemyBaseTrenchPrivate;
	const UFieldTrenchAnchorComponent* AnchorComponent = Anchor.Get();
	const float RemainingLength = (Settings.MaxTrenchMetresPerBase - Base.DugMetres) * CentimetresPerMetre;
	if (Path.Num() < 2 || RemainingLength <= 0.0f)
	{
		return false;
	}
	FVector GroundStart(Path[0], Base.Centre.Z);
	if (IsValid(AnchorComponent))
	{
		GroundStart = AnchorComponent->GetAnchorLocation(Anchor.AnchorIndex);
	}
	else
	{
		FHitResult GroundHit;
		const FVector TraceStart(Path[0], Base.Centre.Z + GroundTraceHalfHeight);
		const FVector TraceEnd(Path[0], Base.Centre.Z - GroundTraceHalfHeight);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE))
		{
			GroundStart = GroundHit.ImpactPoint;
		}
	}
	ARTSFieldTrench* Trench = SpawnSovietTrench(GroundStart);
	if (not IsValid(Trench))
	{
		return false;
	}
	FFieldTrenchPlanOptions Options;
	if (IsValid(AnchorComponent))
	{
		// The anchor is also used to find the ground, which next to the pit lies under its walls.
		Trench->SetStartAnchor(Anchor);
		Options.StartPort = AnchorComponent->GetAnchorPort(Anchor.AnchorIndex);
	}
	const TArray<FFieldTrenchPiecePlacement> Pieces = PlanBuildablePieces(
		*Trench, Base, Path, Options, RemainingLength, GroundStart.Z,
		IsValid(AnchorComponent) ? AnchorComponent->GetOwner() : nullptr);
	if (Pieces.IsEmpty())
	{
		Trench->Destroy();
		return false;
	}
	float TrenchLength = 0.0f;
	for (const FFieldTrenchPiecePlacement& Piece : Pieces)
	{
		TrenchLength += Piece.LengthCm;
	}
	// The AI has no resources to pay with; the caps limit it instead.
	Trench->StartConstruction(Pieces, EnemyPlayerIndex, TMap<ERTSResourceType, int32>());
	M_Trenches.Add(Trench);
	++Base.TrenchCount;
	Base.DugMetres += TrenchLength / CentimetresPerMetre;
	return true;
}

TArray<FFieldTrenchPiecePlacement> UEnemyBaseTrenchComponent::PlanBuildablePieces(
	ARTSFieldTrench& Trench,
	const FEnemyTrenchBase& Base,
	const TArray<FVector2D>& Path,
	const FFieldTrenchPlanOptions& Options,
	const float MaximumLengthCm,
	const float GroundHeight,
	const AActor* IgnoredActor) const
{
	using namespace EnemyBaseTrenchPrivate;
	// Without a start cap the first piece is already trench worth keeping.
	const int32 PiecesBeforeTrench = Options.GetHasStartCap() ? 1 : 0;
	float MaximumLength = MaximumLengthCm;
	TArray<FFieldTrenchPiecePlacement> Pieces;
	for (int32 Attempt = 0; Attempt < MaximumShortenAttempts; ++Attempt)
	{
		Pieces = Trench.PlanOnGround(Path, MaximumLength, GroundHeight, Options);
		if (Pieces.IsEmpty())
		{
			return Pieces;
		}
		// Laid out as planned pieces, which tells how far each leans and where it lies on the ground.
		Trench.SetPlannedPieces(Pieces);
		const int32 FirstBadPiece = Trench.GetPieces().IndexOfByPredicate(
			[this, &Base, &Trench, IgnoredActor](const FFieldTrenchPieceRuntime& Piece)
			{
				return not GetIsPieceBuildable(Piece, Base, Trench, IgnoredActor);
			});
		if (FirstBadPiece == INDEX_NONE)
		{
			return Pieces;
		}
		if (FirstBadPiece <= PiecesBeforeTrench)
		{
			break;
		}
		// The trench is laid in the same order again, but ends before the first piece that does not fit.
		float KeptLength = 0.0f;
		for (int32 PieceIndex = 0; PieceIndex < FirstBadPiece; ++PieceIndex)
		{
			KeptLength += Pieces[PieceIndex].LengthCm;
		}
		MaximumLength = KeptLength + FieldTrenchLayout::GetPieceDefinition(EFieldTrenchPiece::End).LengthCm +
			LengthTolerance;
	}
	Pieces.Reset();
	return Pieces;
}

bool UEnemyBaseTrenchComponent::GetIsPieceBuildable(
	const FFieldTrenchPieceRuntime& Piece,
	const FEnemyTrenchBase& Base,
	const ARTSFieldTrench& Trench,
	const AActor* IgnoredActor) const
{
	const FVector PieceLocation = Piece.GroundTransform.GetLocation();
	if (Piece.SlopeDegrees > Settings.MaximumSlopeDegrees ||
		FVector::Dist2D(PieceLocation, Base.Centre) > GetBuildRadius(Base))
	{
		return false;
	}
	return not GetIsNearOtherTrench(PieceLocation, Trench) && not GetIsPieceBlocked(Piece, Trench, IgnoredActor);
}

bool UEnemyBaseTrenchComponent::GetIsPieceBlocked(
	const FFieldTrenchPieceRuntime& Piece,
	const ARTSFieldTrench& Trench,
	const AActor* IgnoredActor) const
{
	using namespace EnemyBaseTrenchPrivate;
	const FFieldTrenchPieceDefinition& Definition = FieldTrenchLayout::GetPieceDefinition(Piece.Placement.Piece);
	// Every piece runs from its origin along +X, except the End cap, which lies behind its port.
	const float PieceLength = FMath::Max(Piece.Placement.LengthCm, Definition.LengthCm);
	const float CentreX = Piece.Placement.Piece == EFieldTrenchPiece::End ? -PieceLength * 0.5f : PieceLength * 0.5f;
	const float CentreZ = (PieceCheckBottom + PieceCheckTop) * 0.5f;
	const FVector BoxCentre = Piece.GroundTransform.TransformPosition(FVector(CentreX, 0.0f, CentreZ));
	const FCollisionShape Box = FCollisionShape::MakeBox(
		FVector(PieceLength * 0.5f, PieceCheckHalfWidth, (PieceCheckTop - PieceCheckBottom) * 0.5f));
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	ObjectTypes.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	ObjectTypes.AddObjectTypesToQuery(COLLISION_OBJ_BUILDING_PLACEMENT);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBaseTrenchPiece), false, &Trench);
	if (IsValid(IgnoredActor))
	{
		QueryParams.AddIgnoredActor(IgnoredActor);
	}
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, BoxCentre, Piece.GroundTransform.GetRotation(), ObjectTypes, Box,
	                                     QueryParams);
	return Overlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
	{
		const AActor* OverlappedActor = Overlap.GetActor();
		// Units step aside; the ground is never in the way of a trench dug into it.
		return IsValid(OverlappedActor) && not OverlappedActor->IsA<APawn>() &&
			not OverlappedActor->IsA<ALandscapeProxy>();
	});
}

bool UEnemyBaseTrenchComponent::GetIsNearOtherTrench(const FVector& Location, const ARTSFieldTrench& Trench) const
{
	// Planned pieces do not collide yet, so the AI's other trenches are checked by distance.
	const float MinimumDistanceSquared = FMath::Square(Settings.MinimumTrenchSpacing);
	for (const TWeakObjectPtr<ARTSFieldTrench>& OtherTrench : M_Trenches)
	{
		if (not OtherTrench.IsValid() || OtherTrench.Get() == &Trench)
		{
			continue;
		}
		for (const FFieldTrenchPieceRuntime& OtherPiece : OtherTrench->GetPieces())
		{
			if (FVector::DistSquared2D(OtherPiece.GroundTransform.GetLocation(), Location) < MinimumDistanceSquared)
			{
				return true;
			}
		}
	}
	return false;
}

float UEnemyBaseTrenchComponent::GetBuildRadius(const FEnemyTrenchBase& Base) const
{
	const float Reach = Base.PerimeterRadius > 0.0f ? Base.PerimeterRadius : Settings.PitTrenchLength;
	return Reach + Settings.BuildRadiusMargin;
}

int32 UEnemyBaseTrenchComponent::CountLiveTrenches() const
{
	int32 LiveTrenches = 0;
	for (const TWeakObjectPtr<ARTSFieldTrench>& Trench : M_Trenches)
	{
		LiveTrenches += Trench.IsValid() ? 1 : 0;
	}
	return LiveTrenches;
}

bool UEnemyBaseTrenchComponent::GetIsAnchorFailed(const FFieldTrenchAnchorHandle& Anchor) const
{
	return M_FailedAnchors.Contains(Anchor);
}

ARTSFieldTrench* UEnemyBaseTrenchComponent::SpawnSovietTrench(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARTSFieldTrench* Trench = World->SpawnActor<ARTSFieldTrench>(ARTSFieldTrench::StaticClass(), FTransform(Location),
	                                                             SpawnParameters);
	if (not IsValid(Trench))
	{
		return nullptr;
	}
	// Before any piece exists, so every piece is made from the Soviet meshes.
	Trench->SetKit(EFieldTrenchKit::Soviet);
	Trench->SetHidePlannedPieces(Settings.bHidePlannedPieces);
	Trench->SetDigSecondsPerMetre(Settings.DigSecondsPerMetre);
	return Trench;
}
