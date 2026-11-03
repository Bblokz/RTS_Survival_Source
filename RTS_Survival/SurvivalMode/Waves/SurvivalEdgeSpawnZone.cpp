// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalEdgeSpawnZone.h"

#include "NavigationSystem.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#define LOCTEXT_NAMESPACE "SurvivalEdgeSpawnZone"

namespace SurvivalEdgeSpawnZoneConstants
{
	const FVector DefaultBoxExtent(500.f, 4000.f, 300.f);
	const FColor BoxColor(255, 90, 40);
	constexpr float BoxLineThickness = 25.f;
	constexpr float ArrowLength = 600.f;
	constexpr float ArrowSize = 6.f;
	constexpr float LabelWorldSize = 250.f;
	constexpr float LabelHeight = 600.f;
	// Fraction of the spacing used as random jitter along the edge, so fronts do not look like a ruler.
	constexpr float SpacingJitterFraction = 0.15f;
	// Fraction of the spacing used as the horizontal projection extent per point.
	constexpr float ProjectionExtentFraction = 0.45f;
	constexpr int32 InwardRetries = 3;
	constexpr float InwardRetryStep = 600.f;
	// Offset applied to repeated points when the zone produced fewer points than requested.
	constexpr float RepeatPointOffsetFraction = 0.35f;
	constexpr float DegreesPerCompassSector = 45.f;
	constexpr int32 CompassSectorCount = 8;
}

ASurvivalEdgeSpawnZone::ASurvivalEdgeSpawnZone()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	RootComponent = SpawnArea;
	SpawnArea->SetBoxExtent(SurvivalEdgeSpawnZoneConstants::DefaultBoxExtent);
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetGenerateOverlapEvents(false);
	SpawnArea->SetCanEverAffectNavigation(false);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->ShapeColor = SurvivalEdgeSpawnZoneConstants::BoxColor;
	SpawnArea->SetLineThickness(SurvivalEdgeSpawnZoneConstants::BoxLineThickness);

#if WITH_EDITORONLY_DATA
	M_InwardArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("InwardArrow"));
	if (M_InwardArrow)
	{
		M_InwardArrow->SetupAttachment(SpawnArea);
		M_InwardArrow->ArrowColor = SurvivalEdgeSpawnZoneConstants::BoxColor;
		M_InwardArrow->ArrowLength = SurvivalEdgeSpawnZoneConstants::ArrowLength;
		M_InwardArrow->ArrowSize = SurvivalEdgeSpawnZoneConstants::ArrowSize;
		M_InwardArrow->SetHiddenInGame(true);
	}

	M_Label = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (M_Label)
	{
		M_Label->SetupAttachment(SpawnArea);
		M_Label->SetRelativeLocation(FVector(0.f, 0.f, SurvivalEdgeSpawnZoneConstants::LabelHeight));
		M_Label->SetHorizontalAlignment(EHTA_Center);
		M_Label->SetWorldSize(SurvivalEdgeSpawnZoneConstants::LabelWorldSize);
		M_Label->SetTextRenderColor(SurvivalEdgeSpawnZoneConstants::BoxColor);
		M_Label->SetHiddenInGame(true);
		M_Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
#endif
}

FVector ASurvivalEdgeSpawnZone::GetInwardDirection() const
{
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}
	return Forward;
}

FVector ASurvivalEdgeSpawnZone::GetSpawnCenter() const
{
	if (not GetIsValidSpawnArea())
	{
		return GetActorLocation();
	}
	return SpawnArea->GetComponentLocation();
}

float ASurvivalEdgeSpawnZone::GetEdgeLength() const
{
	if (not GetIsValidSpawnArea())
	{
		return 0.f;
	}
	return SpawnArea->GetScaledBoxExtent().Y * 2.f;
}

bool ASurvivalEdgeSpawnZone::GetIsActiveAtMinute(const float SurvivalMinute) const
{
	return GetIsEnabled() and SurvivalMinute >= ActiveAfterMinute;
}

FText ASurvivalEdgeSpawnZone::GetEdgeDisplayName(const FVector& ReferenceLocation) const
{
	if (not DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return GetCompassName(GetSpawnCenter() - ReferenceLocation);
}

bool ASurvivalEdgeSpawnZone::GetSpawnPoints(const int32 PointCount, const float MinSpacing,
                                            TArray<FVector>& OutPoints) const
{
	using namespace SurvivalEdgeSpawnZoneConstants;
	OutPoints.Reset();
	if (PointCount <= 0)
	{
		return true;
	}

	const float Spacing = FMath::Max(MinSpacing, 100.f);
	const float HalfLength = FMath::Max(GetEdgeLength() * 0.5f, Spacing * 0.5f);
	const int32 PointsPerRow = FMath::Max(1, FMath::FloorToInt(HalfLength * 2.f / Spacing));
	const int32 PointsInFirstRow = FMath::Min(PointCount, PointsPerRow);
	const float FrontHalfWidth = (PointsInFirstRow - 1) * Spacing * 0.5f;
	const float MaxFrontCenterOffset = FMath::Max(0.f, HalfLength - FrontHalfWidth);
	const float FrontCenterOffset = FMath::FRandRange(-MaxFrontCenterOffset, MaxFrontCenterOffset);

	const FVector Center = GetSpawnCenter();
	const FVector Inward = GetInwardDirection();
	const FVector AlongEdge = FVector::CrossProduct(FVector::UpVector, Inward).GetSafeNormal();
	const float ProjectionExtent = Spacing * ProjectionExtentFraction;

	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const int32 Row = PointIndex / PointsPerRow;
		const int32 Column = PointIndex % PointsPerRow;
		const int32 PointsInRow = FMath::Min(PointCount - Row * PointsPerRow, PointsPerRow);
		const float ColumnOffset = (Column - (PointsInRow - 1) * 0.5f) * Spacing;
		const float Jitter = FMath::FRandRange(-Spacing, Spacing) * SpacingJitterFraction;
		const FVector Candidate = Center
			+ AlongEdge * (FrontCenterOffset + ColumnOffset + Jitter)
			+ Inward * (Row * Spacing);

		FVector ProjectedPoint;
		if (TryProjectWithInwardRetries(Candidate, ProjectionExtent, ProjectedPoint))
		{
			OutPoints.Add(ProjectedPoint);
		}
	}

	if (OutPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Survival edge spawn zone " + GetName() + " has no navmesh under its spawn box; no wave can spawn here."
			"\nAt ASurvivalEdgeSpawnZone::GetSpawnPoints");
		return false;
	}

	// Fill up with offset copies so the wave keeps its size on zones that are partly off-mesh.
	const int32 ProjectedCount = OutPoints.Num();
	for (int32 RepeatIndex = 0; OutPoints.Num() < PointCount; ++RepeatIndex)
	{
		const FVector SourcePoint = OutPoints[RepeatIndex % ProjectedCount];
		const FVector Offset = FMath::VRand().GetSafeNormal2D() * Spacing * RepeatPointOffsetFraction;
		FVector ProjectedPoint;
		if (not TryProjectToNavigation(SourcePoint + Offset, ProjectionExtent, ProjectedPoint))
		{
			ProjectedPoint = SourcePoint + Offset;
		}
		OutPoints.Add(ProjectedPoint);
	}
	return true;
}

bool ASurvivalEdgeSpawnZone::GetStagingLocation(const float StagingDistance, FVector& OutLocation) const
{
	const FVector Center = GetSpawnCenter();
	const float Extent = FMath::Max(StagingDistance * 0.5f, 500.f);
	if (TryProjectToNavigation(Center + GetInwardDirection() * StagingDistance, Extent, OutLocation))
	{
		return true;
	}
	return TryProjectToNavigation(Center, Extent, OutLocation);
}

void ASurvivalEdgeSpawnZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateEditorLabel();
}

bool ASurvivalEdgeSpawnZone::GetIsValidSpawnArea() const
{
	if (IsValid(SpawnArea))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "SpawnArea", "GetIsValidSpawnArea", this);
	return false;
}

bool ASurvivalEdgeSpawnZone::TryProjectToNavigation(const FVector& Location, const float HorizontalExtent,
                                                    FVector& OutLocation) const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return false;
	}
	FNavLocation ProjectedLocation;
	const FVector Extent(HorizontalExtent, HorizontalExtent, NavProjectionHalfHeight);
	if (not NavigationSystem->ProjectPointToNavigation(Location, ProjectedLocation, Extent))
	{
		return false;
	}
	OutLocation = ProjectedLocation.Location;
	return true;
}

bool ASurvivalEdgeSpawnZone::TryProjectWithInwardRetries(const FVector& Candidate, const float HorizontalExtent,
                                                         FVector& OutLocation) const
{
	using namespace SurvivalEdgeSpawnZoneConstants;
	const FVector Inward = GetInwardDirection();
	for (int32 Attempt = 0; Attempt <= InwardRetries; ++Attempt)
	{
		if (TryProjectToNavigation(Candidate + Inward * (Attempt * InwardRetryStep), HorizontalExtent, OutLocation))
		{
			return true;
		}
	}
	return false;
}

void ASurvivalEdgeSpawnZone::UpdateEditorLabel() const
{
#if WITH_EDITOR
	if (not IsValid(M_Label))
	{
		return;
	}
	const FText NameText = DisplayName.IsEmpty() ? FText::FromString(GetActorLabel()) : DisplayName;
	const FText LabelText = FText::Format(
		LOCTEXT("EditorLabel", "{0}\nweight {1}  from min {2}{3}"),
		NameText,
		FText::AsNumber(SelectionWeight),
		FText::AsNumber(ActiveAfterMinute),
		bEnabled ? FText::GetEmpty() : LOCTEXT("EditorLabelDisabled", "  (disabled)"));
	M_Label->SetText(LabelText);
#endif
}

FText ASurvivalEdgeSpawnZone::GetCompassName(const FVector& Direction)
{
	using namespace SurvivalEdgeSpawnZoneConstants;
	const FVector Flat = Direction.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		return LOCTEXT("CompassUnknown", "the edge");
	}
	// +X is north, +Y is east: atan2(Y, X) gives the bearing clockwise from north.
	const float BearingDegrees = FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(Flat.Y, Flat.X)));
	const float PositiveBearing = BearingDegrees < 0.f ? BearingDegrees + 360.f : BearingDegrees;
	const int32 Sector = FMath::RoundToInt(PositiveBearing / DegreesPerCompassSector) % CompassSectorCount;
	switch (Sector)
	{
	case 0:
		return LOCTEXT("CompassNorth", "North");
	case 1:
		return LOCTEXT("CompassNorthEast", "North-East");
	case 2:
		return LOCTEXT("CompassEast", "East");
	case 3:
		return LOCTEXT("CompassSouthEast", "South-East");
	case 4:
		return LOCTEXT("CompassSouth", "South");
	case 5:
		return LOCTEXT("CompassSouthWest", "South-West");
	case 6:
		return LOCTEXT("CompassWest", "West");
	default:
		return LOCTEXT("CompassNorthWest", "North-West");
	}
}

#undef LOCTEXT_NAMESPACE
