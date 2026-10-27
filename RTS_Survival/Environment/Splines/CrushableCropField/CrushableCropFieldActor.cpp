// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CrushableCropFieldActor.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Environment/DestructableEnvActor/ApartmentDestruction/ApartmentFootprintNavModifier.h"
#include "RTS_Survival/Environment/DestructableEnvActor/CrushDestructionTypes/FRTSCrushGate.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace CrushableCropField
{
	// Distance between outline samples when the spline has curved segments.
	constexpr float OutlineSampleSpacing = 100.f;
	// Ground traces start/end this far above/below the actor.
	constexpr float GroundTraceHalfHeight = 3000.f;
	// The scanned volume reaches this far past the outermost plants so vehicles are tracked before they touch any.
	constexpr float DetectionMargin = 450.f;
	constexpr float DetectionHalfHeight = 500.f;
	// Footprints of oversized components (sensors, attached decals) are clamped to this half extent.
	constexpr float MaxFootprintHalfExtent = 900.f;
	// Plants further than this above/below the footprint centre are not under the vehicle (bridges, slopes).
	constexpr float MaxCrushHeightDifference = 400.f;
	// Standing-plant nav cells reach this far past their spacing so neighbouring cells always touch.
	constexpr float NavCellOverlap = 15.f;
	constexpr float NavAreaHalfHeight = 600.f;
	constexpr float NavRefreshDelay = 0.5f;
	// Crushed remains lie in the vehicle's heading with this much random spread.
	constexpr float CrushedYawSpread = 18.f;
	constexpr float CrushedMaxLift = 1.5f;
	// Layout spacings below this would place thousands of plants per square metre.
	constexpr float MinLayoutSpacing = 20.f;
	// A jittered plant may move a little into the edge inset before it is dropped.
	constexpr float JitteredInsetFraction = 0.75f;
	// Hidden standing instances are shrunk and moved below the ground (instances are never removed so indices stay put).
	constexpr float HiddenInstanceScale = 0.001f;
	constexpr float HiddenInstanceDrop = 500.f;
	constexpr int32 TintCustomDataIndex = 0;
}

ACrushableCropFieldActor::ACrushableCropFieldActor()
{
	PrimaryActorTick.bCanEverTick = false;

	FieldOutline = CreateDefaultSubobject<USplineComponent>(TEXT("FieldOutline"));
	RootComponent = FieldOutline;
	FieldOutline->SetClosedLoop(true, false);
	FieldOutline->ClearSplinePoints(false);
	FieldOutline->AddSplinePoint(FVector(-1000.f, -750.f, 0.f), ESplineCoordinateSpace::Local, false);
	FieldOutline->AddSplinePoint(FVector(1000.f, -750.f, 0.f), ESplineCoordinateSpace::Local, false);
	FieldOutline->AddSplinePoint(FVector(1000.f, 750.f, 0.f), ESplineCoordinateSpace::Local, false);
	FieldOutline->AddSplinePoint(FVector(-1000.f, 750.f, 0.f), ESplineCoordinateSpace::Local, false);
	for (int32 PointIndex = 0; PointIndex < FieldOutline->GetNumberOfSplinePoints(); ++PointIndex)
	{
		FieldOutline->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
	}
	FieldOutline->UpdateSpline();

	M_VehicleDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleDetectionBox"));
	M_VehicleDetectionBox->SetupAttachment(FieldOutline);
	M_VehicleDetectionBox->SetCanEverAffectNavigation(false);
	// A field-sized primitive would be found by cover, guard-surface and AOE object queries; the scan queries instead.
	M_VehicleDetectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_VehicleDetectionBox->SetGenerateOverlapEvents(false);
	M_VehicleDetectionBox->SetHiddenInGame(true);

	M_StandingPlantsNav = CreateDefaultSubobject<UApartmentFootprintNavModifier>(TEXT("StandingPlantsNav"));
}

void ACrushableCropFieldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		// Spawned at runtime: BeginPlay builds (and binds the crush logic).
		return;
	}
	BuildField();
}

void ACrushableCropFieldActor::RebuildField()
{
	BuildField();
}

void ACrushableCropFieldActor::BeginPlay()
{
	Super::BeginPlay();
	BuildField();
	BeginPlay_StartVehicleScan();
}

void ACrushableCropFieldActor::BeginPlay_StartVehicleScan()
{
	UWorld* World = GetWorld();
	if (not World || M_NumStandingPlants <= 0)
	{
		return;
	}
	// Random first delay so the fields of a map do not all query on the same frame.
	World->GetTimerManager().SetTimer(M_VehicleScanTimer, this, &ACrushableCropFieldActor::ScanForVehicles,
	                                  VehicleScanInterval, true, FMath::FRandRange(0.f, VehicleScanInterval));
}

void ACrushableCropFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_VehicleScanTimer);
		World->GetTimerManager().ClearTimer(M_CrushSampleTimer);
		World->GetTimerManager().ClearTimer(M_NavRefreshTimer);
	}
	M_CrushingVehicles.Empty();
	Super::EndPlay(EndPlayReason);
}

bool ACrushableCropFieldActor::GetIsValidVehicleDetectionBox() const
{
	if (IsValid(M_VehicleDetectionBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_VehicleDetectionBox", "GetIsValidVehicleDetectionBox", this);
	return false;
}

bool ACrushableCropFieldActor::GetIsValidStandingPlantsNav() const
{
	if (IsValid(M_StandingPlantsNav))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_StandingPlantsNav", "GetIsValidStandingPlantsNav", this);
	return false;
}

// ------------------------- BUILD ----------------------------------

void ACrushableCropFieldActor::ClearField()
{
	for (UInstancedStaticMeshComponent* Component : M_IntactComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	for (UInstancedStaticMeshComponent* Component : M_CrushedComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	M_IntactComponents.Empty();
	M_CrushedComponents.Empty();
	M_Plants.Empty();
	M_GridCells.Empty();
	M_GridDimensions = FIntPoint::ZeroValue;
	M_NumStandingPlants = 0;
}

void ACrushableCropFieldActor::BuildField()
{
	ClearField();
	if (not IsValid(FieldOutline))
	{
		return;
	}
	if (not FieldOutline->IsClosedLoop())
	{
		FieldOutline->SetClosedLoop(true, true);
	}
	const TArray<FVector2D> Polygon = GetLocalOutlinePolygon();
	const float TotalWeight = GetTotalVariantWeight();
	if (Polygon.Num() < 3 || TotalWeight <= 0.f)
	{
		UpdateStandingPlantsNav();
		return;
	}
	CreateVariantComponents();
	PlacePlants(Polygon, TotalWeight);
	AddPlantInstances();
	M_NumStandingPlants = M_Plants.Num();

	BuildPlantGrid();
	UpdateDetectionBox();
	UpdateStandingPlantsNav();
}

float ACrushableCropFieldActor::GetTotalVariantWeight() const
{
	float TotalWeight = 0.f;
	for (const FCrushableCropVariant& Variant : Variants)
	{
		if (IsValid(Variant.IntactMesh) && Variant.Weight > 0.f)
		{
			TotalWeight += Variant.Weight;
		}
	}
	return TotalWeight;
}

void ACrushableCropFieldActor::CreateVariantComponents()
{
	M_IntactComponents.SetNum(Variants.Num());
	M_CrushedComponents.SetNum(Variants.Num());
	// Variants usually share one crushed mesh: one component (one draw) per distinct crushed mesh.
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> CrushedComponentByMesh;
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FCrushableCropVariant& Variant = Variants[VariantIndex];
		if (not IsValid(Variant.IntactMesh) || Variant.Weight <= 0.f)
		{
			continue;
		}
		M_IntactComponents[VariantIndex] = CreatePlantComponent(
			Variant.IntactMesh, bStandingPlantsCastShadow, TEXT("StandingPlants"), VariantIndex);
		if (not IsValid(Variant.CrushedMesh))
		{
			continue;
		}
		if (UInstancedStaticMeshComponent* const* Shared = CrushedComponentByMesh.Find(Variant.CrushedMesh))
		{
			M_CrushedComponents[VariantIndex] = *Shared;
			continue;
		}
		UInstancedStaticMeshComponent* Crushed = CreatePlantComponent(
			Variant.CrushedMesh, bCrushedPlantsCastShadow, TEXT("CrushedPlants"), VariantIndex);
		CrushedComponentByMesh.Add(Variant.CrushedMesh, Crushed);
		M_CrushedComponents[VariantIndex] = Crushed;
	}
}

void ACrushableCropFieldActor::PlacePlants(const TArray<FVector2D>& Polygon, const float TotalWeight)
{
	const FRandomStream Stream(Layout.RandomSeed);
	const FBox2D PolygonBounds(Polygon);
	const float RowSpacing = FMath::Max(Layout.RowSpacing, CrushableCropField::MinLayoutSpacing);
	const float PlantSpacing = FMath::Max(Layout.PlantSpacing, CrushableCropField::MinLayoutSpacing);
	TArray<FVector2D> RowIntervals;
	for (float RowY = PolygonBounds.Min.Y + RowSpacing * 0.5f; RowY < PolygonBounds.Max.Y; RowY += RowSpacing)
	{
		GetRowIntervals(Polygon, RowY, Layout.EdgeInset, RowIntervals);
		for (const FVector2D& Interval : RowIntervals)
		{
			const float IntervalLength = Interval.Y - Interval.X;
			const int32 NumSpots = FMath::FloorToInt(IntervalLength / PlantSpacing) + 1;
			// Centre the spots in the interval so both row ends keep the same gap to the outline.
			const float FirstX = Interval.X + (IntervalLength - (NumSpots - 1) * PlantSpacing) * 0.5f;
			for (int32 SpotIndex = 0; SpotIndex < NumSpots; ++SpotIndex)
			{
				TryAddPlant(Stream, TotalWeight, Polygon, FVector2D(FirstX + SpotIndex * PlantSpacing, RowY));
			}
		}
	}
}

void ACrushableCropFieldActor::TryAddPlant(const FRandomStream& Stream, const float TotalWeight,
                                           const TArray<FVector2D>& Polygon, const FVector2D& GridSpot)
{
	// Draw every random number up front so the layout does not shift when one test fails.
	const float SkipRoll = Stream.FRand();
	const FVector2D Jitter(Stream.FRandRange(-1.f, 1.f) * Layout.PositionJitter * Layout.PlantSpacing,
	                       Stream.FRandRange(-1.f, 1.f) * Layout.PositionJitter * Layout.RowSpacing);
	const int32 VariantIndex = PickVariant(Stream, TotalWeight);
	const float RandomYaw = Stream.FRand() * 360.f;
	const bool bFlip = Stream.FRand() < 0.5f;
	const float YawJitter = Stream.FRandRange(-Layout.YawJitterDegrees, Layout.YawJitterDegrees);
	const float Scale = Stream.FRandRange(Layout.MinScale, FMath::Max(Layout.MinScale, Layout.MaxScale));
	const float TintSeed = Stream.FRand();

	if (SkipRoll < Layout.MissingPlantChance || VariantIndex == INDEX_NONE)
	{
		return;
	}
	const FVector2D Spot = GridSpot + Jitter;
	if (not GetIsInsidePolygon(Polygon, Spot) ||
		GetDistanceToOutline(Polygon, Spot) < Layout.EdgeInset * CrushableCropField::JitteredInsetFraction)
	{
		return;
	}
	float Yaw = YawJitter;
	if (Layout.bRandomYaw)
	{
		Yaw += RandomYaw;
	}
	else if (Layout.bFlipHalfOfRowPlants && bFlip)
	{
		Yaw += 180.f;
	}
	FCrushableCropPlant Plant;
	Plant.VariantIndex = VariantIndex;
	Plant.TintSeed = TintSeed;
	Plant.LocalTransform = FTransform(FRotator(0.f, Yaw, 0.f), FVector(Spot, TraceLocalGroundZ(Spot)), FVector(Scale));
	M_Plants.Add(Plant);
}

void ACrushableCropFieldActor::AddPlantInstances()
{
	TArray<TArray<FTransform>> TransformsPerVariant;
	TArray<TArray<int32>> PlantsPerVariant;
	TransformsPerVariant.SetNum(Variants.Num());
	PlantsPerVariant.SetNum(Variants.Num());
	for (int32 PlantIndex = 0; PlantIndex < M_Plants.Num(); ++PlantIndex)
	{
		const int32 VariantIndex = M_Plants[PlantIndex].VariantIndex;
		TransformsPerVariant[VariantIndex].Add(M_Plants[PlantIndex].LocalTransform);
		PlantsPerVariant[VariantIndex].Add(PlantIndex);
	}
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		UInstancedStaticMeshComponent* Component = M_IntactComponents[VariantIndex];
		if (not IsValid(Component) || TransformsPerVariant[VariantIndex].IsEmpty())
		{
			continue;
		}
		const TArray<int32> InstanceIndices = Component->AddInstances(
			TransformsPerVariant[VariantIndex], true, false, false);
		for (int32 Entry = 0; Entry < InstanceIndices.Num(); ++Entry)
		{
			FCrushableCropPlant& Plant = M_Plants[PlantsPerVariant[VariantIndex][Entry]];
			Plant.IntactInstanceIndex = InstanceIndices[Entry];
			Component->SetCustomDataValue(Plant.IntactInstanceIndex, CrushableCropField::TintCustomDataIndex,
			                              Plant.TintSeed, false);
		}
		Component->MarkRenderStateDirty();
	}
}

TArray<FVector2D> ACrushableCropFieldActor::GetLocalOutlinePolygon() const
{
	TArray<FVector2D> Polygon;
	const int32 NumPoints = FieldOutline->GetNumberOfSplinePoints();
	if (NumPoints < 3)
	{
		return Polygon;
	}
	const float SplineLength = FieldOutline->GetSplineLength();
	const int32 NumSamples = FMath::Max(NumPoints,
	                                    FMath::CeilToInt(SplineLength / CrushableCropField::OutlineSampleSpacing));
	Polygon.Reserve(NumSamples);
	for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		const float Distance = SplineLength * SampleIndex / NumSamples;
		const FVector Location = FieldOutline->GetLocationAtDistanceAlongSpline(
			Distance, ESplineCoordinateSpace::Local);
		Polygon.Add(FVector2D(Location));
	}
	// Linear outlines: make sure every corner is in, sampling may step over it.
	for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
	{
		if (FieldOutline->GetSplinePointType(PointIndex) != ESplinePointType::Linear)
		{
			return Polygon;
		}
	}
	Polygon.Reset();
	for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
	{
		Polygon.Add(FVector2D(FieldOutline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local)));
	}
	return Polygon;
}

void ACrushableCropFieldActor::GetRowIntervals(const TArray<FVector2D>& Polygon, const float LocalY,
                                               const float Inset, TArray<FVector2D>& OutIntervals)
{
	OutIntervals.Reset();
	TArray<float> Crossings;
	for (int32 EdgeIndex = 0; EdgeIndex < Polygon.Num(); ++EdgeIndex)
	{
		const FVector2D& EdgeStart = Polygon[EdgeIndex];
		const FVector2D& EdgeEnd = Polygon[(EdgeIndex + 1) % Polygon.Num()];
		const bool bCrosses = (EdgeStart.Y <= LocalY && EdgeEnd.Y > LocalY) ||
			(EdgeEnd.Y <= LocalY && EdgeStart.Y > LocalY);
		if (not bCrosses)
		{
			continue;
		}
		const float Alpha = (LocalY - EdgeStart.Y) / (EdgeEnd.Y - EdgeStart.Y);
		Crossings.Add(FMath::Lerp(EdgeStart.X, EdgeEnd.X, Alpha));
	}
	Crossings.Sort();
	for (int32 CrossingIndex = 0; CrossingIndex + 1 < Crossings.Num(); CrossingIndex += 2)
	{
		const float MinX = Crossings[CrossingIndex] + Inset;
		const float MaxX = Crossings[CrossingIndex + 1] - Inset;
		if (MaxX > MinX)
		{
			OutIntervals.Add(FVector2D(MinX, MaxX));
		}
	}
}

bool ACrushableCropFieldActor::GetIsInsidePolygon(const TArray<FVector2D>& Polygon, const FVector2D& Point)
{
	// Even-odd rule: count the edges a ray towards +X crosses.
	bool bInside = false;
	for (int32 EdgeIndex = 0, PreviousIndex = Polygon.Num() - 1; EdgeIndex < Polygon.Num(); PreviousIndex = EdgeIndex++)
	{
		const FVector2D& EdgeStart = Polygon[PreviousIndex];
		const FVector2D& EdgeEnd = Polygon[EdgeIndex];
		if ((EdgeEnd.Y > Point.Y) == (EdgeStart.Y > Point.Y))
		{
			continue;
		}
		const float CrossingX = EdgeEnd.X + (Point.Y - EdgeEnd.Y) * (EdgeStart.X - EdgeEnd.X) / (EdgeStart.Y - EdgeEnd.Y);
		if (Point.X < CrossingX)
		{
			bInside = not bInside;
		}
	}
	return bInside;
}

float ACrushableCropFieldActor::GetDistanceToOutline(const TArray<FVector2D>& Polygon, const FVector2D& Point)
{
	float MinDistanceSquared = TNumericLimits<float>::Max();
	for (int32 EdgeIndex = 0; EdgeIndex < Polygon.Num(); ++EdgeIndex)
	{
		const FVector2D& EdgeStart = Polygon[EdgeIndex];
		const FVector2D& EdgeEnd = Polygon[(EdgeIndex + 1) % Polygon.Num()];
		const FVector Closest = FMath::ClosestPointOnSegment(
			FVector(Point, 0.f), FVector(EdgeStart, 0.f), FVector(EdgeEnd, 0.f));
		MinDistanceSquared = FMath::Min(MinDistanceSquared, FVector2D::DistSquared(FVector2D(Closest), Point));
	}
	return FMath::Sqrt(MinDistanceSquared);
}

int32 ACrushableCropFieldActor::PickVariant(const FRandomStream& Stream, const float TotalWeight) const
{
	float Roll = Stream.FRand() * TotalWeight;
	int32 LastUsable = INDEX_NONE;
	for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
	{
		const FCrushableCropVariant& Variant = Variants[VariantIndex];
		if (not IsValid(Variant.IntactMesh) || Variant.Weight <= 0.f)
		{
			continue;
		}
		LastUsable = VariantIndex;
		Roll -= Variant.Weight;
		if (Roll <= 0.f)
		{
			return VariantIndex;
		}
	}
	return LastUsable;
}

float ACrushableCropFieldActor::TraceLocalGroundZ(const FVector2D& LocalPoint) const
{
	const UWorld* World = GetWorld();
	if (not World)
	{
		return 0.f;
	}
	const FTransform& ActorTransform = GetActorTransform();
	const FVector TraceStart = ActorTransform.TransformPosition(
		FVector(LocalPoint, CrushableCropField::GroundTraceHalfHeight));
	const FVector TraceEnd = ActorTransform.TransformPosition(
		FVector(LocalPoint, -CrushableCropField::GroundTraceHalfHeight));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CropFieldGround), false, this);
	FHitResult Hit;
	if (not World->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd,
	                                           FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return 0.f;
	}
	return ActorTransform.InverseTransformPosition(Hit.ImpactPoint).Z;
}

UInstancedStaticMeshComponent* ACrushableCropFieldActor::CreatePlantComponent(
	UStaticMesh* Mesh, const bool bCastShadow, const TCHAR* NamePrefix, const int32 Index)
{
	const FName ComponentName = MakeUniqueObjectName(
		this, UInstancedStaticMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("%s_%d"), NamePrefix, Index)));
	// Transient: rebuilt from the layout on construction and BeginPlay, never saved with the level.
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(
		this, ComponentName, RF_Transient);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetupAttachment(FieldOutline);
	Component->SetStaticMesh(Mesh);
	if (IsValid(MaterialOverride))
	{
		for (int32 SlotIndex = 0; SlotIndex < Mesh->GetStaticMaterials().Num(); ++SlotIndex)
		{
			Component->SetMaterial(SlotIndex, MaterialOverride);
		}
	}
	Component->SetNumCustomDataFloats(1);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(bCastShadow);
	Component->RegisterComponent();
	return Component;
}

void ACrushableCropFieldActor::BuildPlantGrid()
{
	M_GridCells.Empty();
	M_GridDimensions = FIntPoint::ZeroValue;
	if (M_Plants.IsEmpty())
	{
		return;
	}
	FBox2D PlantBounds(ForceInit);
	for (const FCrushableCropPlant& Plant : M_Plants)
	{
		PlantBounds += FVector2D(Plant.LocalTransform.GetLocation());
	}
	M_GridCellSize = FMath::Clamp(FMath::Max(Layout.PlantSpacing, Layout.RowSpacing) * 2.f, 100.f, 600.f);
	M_GridOrigin = PlantBounds.Min - FVector2D(1.f);
	const FVector2D GridSize = PlantBounds.GetSize() + FVector2D(2.f);
	M_GridDimensions = FIntPoint(FMath::FloorToInt(GridSize.X / M_GridCellSize) + 1,
	                             FMath::FloorToInt(GridSize.Y / M_GridCellSize) + 1);
	M_GridCells.SetNum(M_GridDimensions.X * M_GridDimensions.Y);
	for (int32 PlantIndex = 0; PlantIndex < M_Plants.Num(); ++PlantIndex)
	{
		const FVector2D Cell = (FVector2D(M_Plants[PlantIndex].LocalTransform.GetLocation()) - M_GridOrigin) /
			M_GridCellSize;
		const int32 CellX = FMath::Clamp(FMath::FloorToInt(Cell.X), 0, M_GridDimensions.X - 1);
		const int32 CellY = FMath::Clamp(FMath::FloorToInt(Cell.Y), 0, M_GridDimensions.Y - 1);
		M_GridCells[CellX + CellY * M_GridDimensions.X].Add(PlantIndex);
	}
}

void ACrushableCropFieldActor::UpdateDetectionBox()
{
	if (not GetIsValidVehicleDetectionBox())
	{
		return;
	}
	if (M_Plants.IsEmpty())
	{
		M_VehicleDetectionBox->SetBoxExtent(FVector(1.f), false);
		return;
	}
	FBox PlantBounds(ForceInit);
	for (const FCrushableCropPlant& Plant : M_Plants)
	{
		PlantBounds += Plant.LocalTransform.GetLocation();
	}
	const FVector Extent(PlantBounds.GetExtent().X + CrushableCropField::DetectionMargin,
	                     PlantBounds.GetExtent().Y + CrushableCropField::DetectionMargin,
	                     PlantBounds.GetExtent().Z + CrushableCropField::DetectionHalfHeight);
	M_VehicleDetectionBox->SetRelativeLocation(PlantBounds.GetCenter());
	M_VehicleDetectionBox->SetBoxExtent(Extent, false);
}

// ------------------------- CRUSH ----------------------------------

void ACrushableCropFieldActor::ScanForVehicles()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	if (M_NumStandingPlants <= 0)
	{
		World->GetTimerManager().ClearTimer(M_VehicleScanTimer);
		return;
	}
	if (not GetIsValidVehicleDetectionBox())
	{
		return;
	}
	FCollisionObjectQueryParams VehicleObjects;
	// Same vehicle object channels the crushable spline pieces overlap.
	VehicleObjects.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	VehicleObjects.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CropFieldVehicleScan), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, M_VehicleDetectionBox->GetComponentLocation(),
	                                M_VehicleDetectionBox->GetComponentQuat(), VehicleObjects,
	                                FCollisionShape::MakeBox(M_VehicleDetectionBox->GetScaledBoxExtent()),
	                                QueryParams);

	TArray<FCrushableCropVehicle> VehiclesInField;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* VehicleActor = Overlap.GetActor();
		if (not IsValid(VehicleActor) || not FRTSCrushGate::GetPassesCrushGate(VehicleActor, CrushDestructionType))
		{
			continue;
		}
		// The root primitive is the hull / track mesh: one footprint per vehicle, whatever else overlapped.
		UPrimitiveComponent* VehicleRoot = Cast<UPrimitiveComponent>(VehicleActor->GetRootComponent());
		if (not IsValid(VehicleRoot))
		{
			VehicleRoot = Overlap.GetComponent();
		}
		const bool bAlreadyFound = VehiclesInField.ContainsByPredicate([VehicleRoot](const FCrushableCropVehicle& Found)
		{
			return Found.Component.Get() == VehicleRoot;
		});
		if (IsValid(VehicleRoot) && not bAlreadyFound)
		{
			VehiclesInField.Add(MakeTrackedVehicle(VehicleRoot));
		}
	}
	M_CrushingVehicles = MoveTemp(VehiclesInField);
	if (M_CrushingVehicles.IsEmpty() || World->GetTimerManager().IsTimerActive(M_CrushSampleTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_CrushSampleTimer, this, &ACrushableCropFieldActor::SampleCrushingVehicles,
	                                  CrushSampleInterval, true, 0.f);
}

FCrushableCropVehicle ACrushableCropFieldActor::MakeTrackedVehicle(UPrimitiveComponent* VehicleRoot) const
{
	for (const FCrushableCropVehicle& Tracked : M_CrushingVehicles)
	{
		if (Tracked.Component.Get() == VehicleRoot)
		{
			return Tracked;
		}
	}
	FCrushableCropVehicle Vehicle;
	Vehicle.Component = VehicleRoot;
	const FBox LocalBounds = VehicleRoot->CalcBounds(FTransform::Identity).GetBox();
	const FVector ClampedExtent = LocalBounds.GetExtent().ComponentMin(
		FVector(CrushableCropField::MaxFootprintHalfExtent));
	Vehicle.LocalFootprint = FBox::BuildAABB(LocalBounds.GetCenter(), ClampedExtent);
	return Vehicle;
}

void ACrushableCropFieldActor::SampleCrushingVehicles()
{
	M_CrushingVehicles.RemoveAllSwap([](const FCrushableCropVehicle& Tracked)
	{
		return not Tracked.Component.IsValid();
	});
	if (M_CrushingVehicles.IsEmpty() || M_NumStandingPlants <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_CrushSampleTimer);
		}
		return;
	}
	int32 NumCrushed = 0;
	TArray<FVector> CrushedWorldLocations;
	for (const FCrushableCropVehicle& Vehicle : M_CrushingVehicles)
	{
		CrushedWorldLocations.Reset();
		CrushUnderVehicle(Vehicle, CrushedWorldLocations);
		NumCrushed += CrushedWorldLocations.Num();
		PlayCrushFX(CrushedWorldLocations);
	}
	FinishCrushBatch(NumCrushed);
}

void ACrushableCropFieldActor::CrushUnderVehicle(const FCrushableCropVehicle& Vehicle,
                                                 TArray<FVector>& OutCrushedWorldLocations)
{
	const UPrimitiveComponent* Component = Vehicle.Component.Get();
	if (not IsValid(Component) || not Vehicle.LocalFootprint.IsValid)
	{
		return;
	}
	const FTransform& ComponentTransform = Component->GetComponentTransform();
	const FTransform& ActorTransform = GetActorTransform();

	// The footprint as an oriented rectangle in the field's local XY plane.
	const FVector WorldCentre = ComponentTransform.TransformPosition(Vehicle.LocalFootprint.GetCenter());
	const FVector HalfExtent = Vehicle.LocalFootprint.GetExtent() * ComponentTransform.GetScale3D().GetAbs();
	const FVector LocalCentre = ActorTransform.InverseTransformPosition(WorldCentre);
	FVector2D AxisX(ActorTransform.InverseTransformVectorNoScale(ComponentTransform.GetUnitAxis(EAxis::X)));
	if (not AxisX.Normalize())
	{
		AxisX = FVector2D(1.f, 0.f);
	}
	const FVector2D AxisY(-AxisX.Y, AxisX.X);
	const float HalfLength = HalfExtent.X + CrushFootprintPadding;
	const float HalfWidth = HalfExtent.Y + CrushFootprintPadding;
	const FVector2D Centre2D(LocalCentre);
	const FVector2D Reach = FVector2D(FMath::Abs(AxisX.X), FMath::Abs(AxisX.Y)) * HalfLength +
		FVector2D(FMath::Abs(AxisY.X), FMath::Abs(AxisY.Y)) * HalfWidth;

	TArray<int32> Candidates;
	GatherPlantsInLocalBox(FBox2D(Centre2D - Reach, Centre2D + Reach), Candidates);
	const float HeadingYaw = FMath::RadiansToDegrees(FMath::Atan2(AxisX.Y, AxisX.X));
	for (const int32 PlantIndex : Candidates)
	{
		const FCrushableCropPlant& Plant = M_Plants[PlantIndex];
		if (Plant.bCrushed)
		{
			continue;
		}
		const FVector PlantLocation = Plant.LocalTransform.GetLocation();
		const FVector2D Offset = FVector2D(PlantLocation) - Centre2D;
		if (FMath::Abs(FVector2D::DotProduct(Offset, AxisX)) > HalfLength ||
			FMath::Abs(FVector2D::DotProduct(Offset, AxisY)) > HalfWidth ||
			FMath::Abs(PlantLocation.Z - LocalCentre.Z) > HalfExtent.Z + CrushableCropField::MaxCrushHeightDifference)
		{
			continue;
		}
		CrushPlant(PlantIndex, HeadingYaw);
		OutCrushedWorldLocations.Add(ActorTransform.TransformPosition(PlantLocation));
	}
}

void ACrushableCropFieldActor::CrushPlant(const int32 PlantIndex, const float LocalHeadingYaw)
{
	FCrushableCropPlant& Plant = M_Plants[PlantIndex];
	if (Plant.bCrushed)
	{
		return;
	}
	Plant.bCrushed = true;
	--M_NumStandingPlants;
	bM_InstancesDirty = true;

	UInstancedStaticMeshComponent* IntactComponent = M_IntactComponents.IsValidIndex(Plant.VariantIndex)
		                                                             ? M_IntactComponents[Plant.VariantIndex].Get()
		                                                             : nullptr;
	if (IsValid(IntactComponent) && Plant.IntactInstanceIndex != INDEX_NONE)
	{
		FTransform HiddenTransform = Plant.LocalTransform;
		HiddenTransform.SetScale3D(FVector(CrushableCropField::HiddenInstanceScale));
		HiddenTransform.AddToTranslation(FVector(0.f, 0.f, -CrushableCropField::HiddenInstanceDrop));
		IntactComponent->UpdateInstanceTransform(Plant.IntactInstanceIndex, HiddenTransform, false, false, true);
	}

	UInstancedStaticMeshComponent* CrushedComponent =
		M_CrushedComponents.IsValidIndex(Plant.VariantIndex) ? M_CrushedComponents[Plant.VariantIndex].Get() : nullptr;
	if (IsValid(CrushedComponent))
	{
		const float Yaw = LocalHeadingYaw + FMath::FRandRange(-CrushableCropField::CrushedYawSpread,
		                                                      CrushableCropField::CrushedYawSpread);
		// A small random lift keeps overlapping flat remains of neighbouring plants from z-fighting.
		const FVector Lift(0.f, 0.f, FMath::FRandRange(0.f, CrushableCropField::CrushedMaxLift));
		const FTransform CrushedTransform(FRotator(0.f, Yaw, 0.f), Plant.LocalTransform.GetLocation() + Lift,
		                                  Plant.LocalTransform.GetScale3D());
		const int32 CrushedIndex = CrushedComponent->AddInstance(CrushedTransform, false);
		CrushedComponent->SetCustomDataValue(CrushedIndex, CrushableCropField::TintCustomDataIndex,
		                                     Plant.TintSeed, false);
	}
	if (StandingPlantsNavArea)
	{
		bM_NavDirty = true;
	}
}

void ACrushableCropFieldActor::FinishCrushBatch(const int32 NumCrushed)
{
	if (not bM_InstancesDirty)
	{
		return;
	}
	bM_InstancesDirty = false;
	for (UInstancedStaticMeshComponent* Component : M_IntactComponents)
	{
		if (IsValid(Component))
		{
			Component->MarkRenderStateDirty();
		}
	}
	// Crushed components are shared between variants; dirty each only once.
	TSet<UInstancedStaticMeshComponent*> DirtiedCrushedComponents;
	for (UInstancedStaticMeshComponent* Component : M_CrushedComponents)
	{
		if (IsValid(Component) && not DirtiedCrushedComponents.Contains(Component))
		{
			DirtiedCrushedComponents.Add(Component);
			Component->MarkRenderStateDirty();
		}
	}
	if (bM_NavDirty)
	{
		RequestStandingPlantsNavRefresh();
	}
	if (NumCrushed > 0)
	{
		BP_OnPlantsCrushed(NumCrushed);
	}
}

void ACrushableCropFieldActor::PlayCrushFX(const TArray<FVector>& CrushedWorldLocations)
{
	if (CrushedWorldLocations.IsEmpty())
	{
		return;
	}
	const int32 NumVfx = FMath::Min(MaxCrushVfxPerSample, CrushedWorldLocations.Num());
	for (int32 VfxIndex = 0; VfxIndex < NumVfx; ++VfxIndex)
	{
		// Spread the effects over the crushed plants instead of stacking them on the first ones.
		const int32 LocationIndex = (VfxIndex * CrushedWorldLocations.Num()) / NumVfx;
		CrushFX.SpawnVfxAtLocation(this, CrushedWorldLocations[LocationIndex] + CrushFX.FxLocationOffset);
	}
	const UWorld* World = GetWorld();
	if (not World || World->GetTimeSeconds() - M_LastCrushSoundTime < CrushSoundCooldown)
	{
		return;
	}
	M_LastCrushSoundTime = World->GetTimeSeconds();
	CrushFX.PlaySfxAtLocation(this, CrushedWorldLocations[CrushedWorldLocations.Num() / 2] + CrushFX.FxLocationOffset);
}

void ACrushableCropFieldActor::UpdateStandingPlantsNav()
{
	bM_NavDirty = false;
	if (not GetIsValidStandingPlantsNav())
	{
		return;
	}
	if (not StandingPlantsNavArea)
	{
		M_StandingPlantsNav->SetNavigationRelevancy(false);
		M_StandingPlantsNav->SetFootprintBoxes({}, 0.f, 0.f);
		return;
	}
	const FVector2D CellHalfSize(Layout.PlantSpacing * 0.5f + CrushableCropField::NavCellOverlap,
	                             Layout.RowSpacing * 0.5f + CrushableCropField::NavCellOverlap);
	TArray<FBox2D> StandingCells;
	StandingCells.Reserve(M_NumStandingPlants);
	float MinZ = 0.f;
	float MaxZ = 0.f;
	for (const FCrushableCropPlant& Plant : M_Plants)
	{
		if (Plant.bCrushed)
		{
			continue;
		}
		const FVector Location = Plant.LocalTransform.GetLocation();
		StandingCells.Add(FBox2D(FVector2D(Location) - CellHalfSize, FVector2D(Location) + CellHalfSize));
		MinZ = FMath::Min(MinZ, Location.Z);
		MaxZ = FMath::Max(MaxZ, Location.Z);
	}
	M_StandingPlantsNav->SetAreaClass(StandingPlantsNavArea);
	M_StandingPlantsNav->SetNavigationRelevancy(true);
	M_StandingPlantsNav->SetFootprintBoxes(StandingCells, MinZ - CrushableCropField::NavAreaHalfHeight,
	                                       MaxZ + CrushableCropField::NavAreaHalfHeight);
}

void ACrushableCropFieldActor::RequestStandingPlantsNavRefresh()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		UpdateStandingPlantsNav();
		return;
	}
	if (World->GetTimerManager().IsTimerActive(M_NavRefreshTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_NavRefreshTimer, this, &ACrushableCropFieldActor::UpdateStandingPlantsNav,
	                                  CrushableCropField::NavRefreshDelay, false);
}

void ACrushableCropFieldActor::GatherPlantsInLocalBox(const FBox2D& LocalBox, TArray<int32>& OutPlantIndices) const
{
	OutPlantIndices.Reset();
	if (M_GridCells.IsEmpty())
	{
		return;
	}
	const FVector2D MinCell = (LocalBox.Min - M_GridOrigin) / M_GridCellSize;
	const FVector2D MaxCell = (LocalBox.Max - M_GridOrigin) / M_GridCellSize;
	const int32 MinX = FMath::Max(FMath::FloorToInt(MinCell.X), 0);
	const int32 MinY = FMath::Max(FMath::FloorToInt(MinCell.Y), 0);
	const int32 MaxX = FMath::Min(FMath::FloorToInt(MaxCell.X), M_GridDimensions.X - 1);
	const int32 MaxY = FMath::Min(FMath::FloorToInt(MaxCell.Y), M_GridDimensions.Y - 1);
	for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
	{
		for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
		{
			OutPlantIndices.Append(M_GridCells[CellX + CellY * M_GridDimensions.X]);
		}
	}
}

int32 ACrushableCropFieldActor::CrushPlantsInRadius(const FVector WorldLocation, const float Radius)
{
	if (Radius <= 0.f || M_NumStandingPlants <= 0)
	{
		return 0;
	}
	const FTransform& ActorTransform = GetActorTransform();
	const FVector2D LocalCentre(ActorTransform.InverseTransformPosition(WorldLocation));
	TArray<int32> Candidates;
	GatherPlantsInLocalBox(FBox2D(LocalCentre - FVector2D(Radius), LocalCentre + FVector2D(Radius)), Candidates);
	TArray<FVector> CrushedWorldLocations;
	for (const int32 PlantIndex : Candidates)
	{
		const FCrushableCropPlant& Plant = M_Plants[PlantIndex];
		const FVector2D Offset = FVector2D(Plant.LocalTransform.GetLocation()) - LocalCentre;
		if (Plant.bCrushed || Offset.SizeSquared() > FMath::Square(Radius))
		{
			continue;
		}
		// Blown outwards: the remains point away from the blast.
		CrushPlant(PlantIndex, FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X)));
		CrushedWorldLocations.Add(ActorTransform.TransformPosition(Plant.LocalTransform.GetLocation()));
	}
	PlayCrushFX(CrushedWorldLocations);
	FinishCrushBatch(CrushedWorldLocations.Num());
	return CrushedWorldLocations.Num();
}

float ACrushableCropFieldActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                           AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ExplosionCrushRadiusFraction > 0.f)
	{
		const FRadialDamageEvent* RadialEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
		CrushPlantsInRadius(RadialEvent->Origin, RadialEvent->Params.OuterRadius * ExplosionCrushRadiusFraction);
	}
	// Never return 0: weapons treat 0 as a kill (see UWeaponState::FluxDamageHitActor_DidActorDie).
	return 1.f;
}
