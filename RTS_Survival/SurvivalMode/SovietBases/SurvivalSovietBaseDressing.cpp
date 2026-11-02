// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseDressing.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBasePad.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

namespace SurvivalSovietBaseDressingConstants
{
	constexpr float GroundTraceUp = 3000.f;
	constexpr float GroundTraceDown = 6000.f;
	// The slab mesh: 100 x 100 cm, top at z = 0, 35 cm deep.
	constexpr double SlabMeshSize = 100.0;
	// The centre box is 80 deep: never scaled in height (rise 10 + relief up to 45 + sink still fits), so the edge chamfer keeps
	// its 5 cm.
	constexpr double SlabMeshDepth = 80.0;
	constexpr double SlabEdgeWidth = 6.0;
	// Spill lobes off a slab side (round 6): their reach out over the sand and their share of the side's length.
	constexpr double SpillReachMin = 100.0;
	constexpr double SpillReachMax = 200.0;
	constexpr double SpillLengthShareMin = 0.25;
	constexpr double SpillLengthShareMax = 0.45;
	constexpr double SpillEdge = 120.0;
	constexpr double SpillNoise = 110.0;
	constexpr int32 SpillSortOrder = -8;
	constexpr double SpillDecalDepth = 90.0;
	// Boards (round 6): separate boards, warm wood, with gaps; threshold boards when the road is close.
	constexpr double BoardWidth = 24.0;
	constexpr double BoardGap = 4.0;
	constexpr double BoardThickness = 5.0;
	constexpr int32 BoardsMin = 3;
	constexpr int32 BoardsMax = 5;
	constexpr double BoardHeightJitter = 1.0;
	constexpr double ThresholdBoardLength = 170.0;
	constexpr double ThresholdBoardGap = 6.0;
	constexpr double ThresholdStartGap = 30.0;
	// Clutter at least this tall casts a shadow; lower items do not (round 6 draw trim).
	constexpr double ClutterShadowMinHeight = 150.0;
	// One flat slab only where the ground under it varies less than this; steeper pads keep the decal alone.
	constexpr double MaxSlabRelief = 45.0;
	// Slabs of one yaw this close merge into one (no two trays at different heights side by side).
	constexpr double SlabMergeGap = 150.0;
	constexpr double SlabMergeYawTolerance = 1.0;
	// Decals per class, cm.
	constexpr double FoundationDecalInset = 30.0;
	constexpr double FoundationDecalCorner = 60.0;
	constexpr double FoundationDecalEdge = 150.0;
	constexpr double FoundationDecalNoise = 80.0;
	// Barracks and shelter halos (round 6): rounded blobs with a wide, strongly wobbling edge, no straight runs over ~6 m.
	constexpr double WalkDecalMargin = 170.0;
	constexpr double HaloCornerShare = 0.5;
	constexpr double WalkDecalEdge = 220.0;
	constexpr double WalkDecalNoise = 230.0;
	constexpr double TrampledScale = 1.35;
	constexpr double TrampledMargin = 60.0;
	constexpr double TrampledCornerShare = 0.85;
	constexpr double TrampledEdge = 220.0;
	constexpr double TrampledNoise = 150.0;
	constexpr double HardstandingMargin = 240.0;
	constexpr double HardstandingEdge = 220.0;
	constexpr double HardstandingNoise = 230.0;
	constexpr int32 DecalSortOrder = -10;
	// Aprons, forecourts and walks.
	constexpr double ApronMinLength = 150.0;
	constexpr double ApronMinWidth = 400.0;
	constexpr double ApronWallShare = 0.6;
	constexpr double ApronStartBelowSlab = 3.0;
	constexpr double ApronEndOverGround = 4.0;
	constexpr double ForecourtDepth = 450.0;
	constexpr double ForecourtWallShare = 0.8;
	constexpr double ForecourtGap = 20.0;
	constexpr double ForecourtOverGround = 8.0;
	constexpr double RutTrackWidth = 520.0;
	constexpr double DuckboardLength = 220.0;
	constexpr double DuckboardWidth = 100.0;
	constexpr int32 DuckboardMax = 6;
	constexpr double TentPlankLength = 200.0;
	constexpr double TentPlankWidth = 120.0;
	constexpr int32 TentPlankMax = 5;
	constexpr double PlankGap = 15.0;
	constexpr double PlankStartGap = 40.0;
	constexpr double PlankYawJitter = 4.0;
	constexpr double PlankSideJitter = 10.0;
	constexpr double PlankThickness = 8.0;
	// Clutter: tight work corners outside the slab or footprint.
	constexpr double ClutterKeepGap = 40.0;
	constexpr double ClusterOffset = 140.0;
	constexpr double ClusterRadius = 160.0;
	constexpr double ClusterSpanShare = 0.4;
	constexpr double ClutterSpacing = 15.0;
	constexpr int32 ClutterTries = 8;
	constexpr int32 ClusterMinItems = 2;
	constexpr int32 ClusterMaxItems = 4;
	constexpr double ClutterYawJitter = 15.0;
	constexpr double SandbagCornerYaw = 45.0;
}

namespace
{
	FVector2D GetAxis(const double YawDegrees)
	{
		const double Yaw = FMath::DegreesToRadians(YawDegrees);
		return FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
	}

	FVector2D GetSide(const double YawDegrees)
	{
		const FVector2D Axis = GetAxis(YawDegrees);
		return FVector2D(-Axis.Y, Axis.X);
	}

	/** @return Point in the rectangle's frame (along its yaw, then across). */
	FVector2D ToLocal(const FVector2D& Point, const FVector2D& Center, const double YawDegrees)
	{
		const FVector2D Delta = Point - Center;
		return FVector2D(FVector2D::DotProduct(Delta, GetAxis(YawDegrees)), FVector2D::DotProduct(Delta, GetSide(YawDegrees)));
	}

	/** @return Signed distance from Point to the rectangle's edge (negative inside). */
	double GetRectDistance(const FVector2D& Point, const FVector2D& Center, const double YawDegrees, const FVector2D& Half)
	{
		const FVector2D Local = ToLocal(Point, Center, YawDegrees);
		const FVector2D Outside(FMath::Abs(Local.X) - Half.X, FMath::Abs(Local.Y) - Half.Y);
		const FVector2D Positive(FMath::Max(Outside.X, 0.0), FMath::Max(Outside.Y, 0.0));
		return Positive.Size() + FMath::Min(FMath::Max(Outside.X, Outside.Y), 0.0);
	}

	bool GetGroundZ(const FSurvivalGroundProjector& Ground, const FVector2D& Point, double& OutZ)
	{
		FSurvivalGroundHit Hit;
		if (not Ground.TryProject(Point, Hit))
		{
			return false;
		}
		OutZ = Hit.Location.Z;
		return true;
	}

	const TCHAR* GetStyleName(const ESurvivalSovietFoundationStyle Style)
	{
		switch (Style)
		{
		case ESurvivalSovietFoundationStyle::Concrete: return TEXT("factory");
		case ESurvivalSovietFoundationStyle::Timber: return TEXT("tent");
		case ESurvivalSovietFoundationStyle::Gravel: return TEXT("barracks");
		case ESurvivalSovietFoundationStyle::Hardstanding: return TEXT("shelter");
		case ESurvivalSovietFoundationStyle::Forecourt: return TEXT("hq");
		default: return TEXT("none");
		}
	}
}

ASurvivalSovietBaseDressing::ASurvivalSovietBaseDressing()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
}

FVector2D ASurvivalSovietBaseDressing::GetDoorWallNormal(const double YawDegrees, const FVector2D& DoorDirection)
{
	const FVector2D Axis = GetAxis(YawDegrees);
	const FVector2D Side = GetSide(YawDegrees);
	const FVector2D Normals[4] = {Axis, -Axis, Side, -Side};
	FVector2D Best = Axis;
	double BestDot = -2.0;
	for (const FVector2D& Normal : Normals)
	{
		const double Dot = FVector2D::DotProduct(Normal, DoorDirection);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = Normal;
		}
	}
	return Best;
}

FSurvivalSovietFootprintRecord ASurvivalSovietBaseDressing::MakeRecordOnly(const FSurvivalSovietFootprintRequest& Request)
{
	FSurvivalSovietFootprintRecord Record;
	Record.Style = GetStyleName(Request.Style);
	Record.Center = Request.Center;
	Record.YawDegrees = Request.YawDegrees;
	Record.FootprintHalf = Request.FootprintHalf;
	Record.DoorNormal = Request.DoorDirection.IsNearlyZero() ? FVector2D::ZeroVector : GetDoorWallNormal(Request.YawDegrees, Request.DoorDirection);
	return Record;
}

int32 ASurvivalSovietBaseDressing::GetInstanceCount() const
{
	int32 Count = 0;
	for (const TPair<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& Entry : M_Instancers)
	{
		Count += IsValid(Entry.Value) ? Entry.Value->GetInstanceCount() : 0;
	}
	return Count;
}

void ASurvivalSovietBaseDressing::SetAllHidden(const UWorld* World, const bool bHidden)
{
	if (not IsValid(World))
	{
		return;
	}
	for (TActorIterator<ASurvivalSovietBaseDressing> It(World); It; ++It)
	{
		It->SetActorHiddenInGame(bHidden);
	}
}

UInstancedStaticMeshComponent* ASurvivalSovietBaseDressing::GetInstancer(UStaticMesh* Mesh, const EInstanceKind Kind,
                                                                         UMaterialInterface* Material, const float CullDistance)
{
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Existing = M_Instancers.Find(Mesh))
	{
		return Existing->Get();
	}
	UInstancedStaticMeshComponent* Instancer = NewObject<UInstancedStaticMeshComponent>(this);
	Instancer->SetStaticMesh(Mesh);
	// Visual only: units, paths and the navmesh ignore the dressing.
	Instancer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instancer->SetCanEverAffectNavigation(false);
	Instancer->SetMobility(EComponentMobility::Movable);
	// A slab rises 10 cm: its shadow adds nothing but shadow-pass draws. Planks are as flat.
	Instancer->SetCastShadow(Kind == EInstanceKind::Clutter
		and Mesh->GetBoundingBox().GetSize().Z >= SurvivalSovietBaseDressingConstants::ClutterShadowMinHeight);
	// The pad's foundation decal paints the slab's top; clutter and planks keep their own look.
	Instancer->bReceivesDecals = Kind == EInstanceKind::Slab;
	if (Kind != EInstanceKind::Slab)
	{
		Instancer->SetCullDistances(0, FMath::RoundToInt32(CullDistance));
		// Per-instance culling still issues the draw; past the distance the whole group is skipped (round 5 timing: the
		// map view paid 74 draws for clutter it did not show).
		Instancer->LDMaxDrawDistance = CullDistance;
		Instancer->SetCachedMaxDrawDistance(CullDistance);
	}
	if (Material != nullptr)
	{
		Instancer->SetMaterial(0, Material);
	}
	Instancer->SetupAttachment(GetRootComponent());
	Instancer->RegisterComponent();
	M_Instancers.Add(Mesh, Instancer);
	return Instancer;
}

double ASurvivalSovietBaseDressing::PlaceOnGround(UStaticMesh* Mesh, const EInstanceKind Kind, const FVector2D& WorldXY,
                                                 const double YawDegrees, const FVector& Scale, const double ExtraRise,
                                                 const FSurvivalSovietFootprintAssets& Assets, const FSurvivalGroundProjector& Ground)
{
	UInstancedStaticMeshComponent* Instancer = GetInstancer(Mesh, Kind, nullptr, Assets.ClutterCullDistance);
	double GroundZ = 0.0;
	if (Instancer == nullptr or not GetGroundZ(Ground, WorldXY, GroundZ))
	{
		return -1.0;
	}
	// The mesh's box: its bottom on the ground under its centre, its XY centre over WorldXY.
	const FBox Box = Mesh->GetBoundingBox();
	const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
	const FVector CentreOffset = Rotation.RotateVector(FVector(Box.GetCenter().X * Scale.X, Box.GetCenter().Y * Scale.Y, 0.0));
	const FVector Pivot(WorldXY.X - CentreOffset.X, WorldXY.Y - CentreOffset.Y, GroundZ + ExtraRise - Box.Min.Z * Scale.Z);
	Instancer->AddInstance(FTransform(Rotation, Pivot, Scale), true);
	// How far the bottom floats over the ground under the box's corners (a slope under a wide item).
	double MaxGap = 0.0;
	const FVector Extent = Box.GetExtent() * Scale;
	for (const FVector2D Corner : {FVector2D(1.0, 1.0), FVector2D(1.0, -1.0), FVector2D(-1.0, 1.0), FVector2D(-1.0, -1.0)})
	{
		const FVector Offset = Rotation.RotateVector(FVector(Corner.X * Extent.X, Corner.Y * Extent.Y, 0.0));
		double CornerZ = 0.0;
		if (GetGroundZ(Ground, WorldXY + FVector2D(Offset), CornerZ))
		{
			MaxGap = FMath::Max(MaxGap, FMath::Abs(GroundZ + ExtraRise - CornerZ));
		}
	}
	return MaxGap;
}

void ASurvivalSovietBaseDressing::UpdateSlabInstance(const FSlab& Slab, const FSurvivalSovietFootprintAssets& Assets)
{
	using namespace SurvivalSovietBaseDressingConstants;
	UInstancedStaticMeshComponent* Instancer = GetInstancer(Assets.SlabMesh, EInstanceKind::Slab, Assets.SlabMaterial, 0.f);
	UInstancedStaticMeshComponent* Edges = GetInstancer(Assets.SlabEdgeMesh, EInstanceKind::Slab, Assets.SlabMaterial, 0.f);
	if (Instancer == nullptr)
	{
		return;
	}
	const FVector Scale((2.0 * Slab.Half.X - 2.0 * SlabEdgeWidth) / SlabMeshSize, (2.0 * Slab.Half.Y - 2.0 * SlabEdgeWidth) / SlabMeshSize, 1.0);
	const FTransform Transform(FRotator(0.0, Slab.YawDegrees, 0.0), FVector(Slab.Center, Slab.TopZ), Scale);
	if (Instancer->IsValidInstance(Slab.Instance))
	{
		Instancer->UpdateInstanceTransform(Slab.Instance, Transform, true, true);
	}
	if (Edges == nullptr)
	{
		return;
	}
	// Each strip's outer (chamfered) side is its local -Y: turned so it faces +Y, -Y, +X and -X of the slab.
	const FVector2D Axis = GetAxis(Slab.YawDegrees);
	const FVector2D Side = GetSide(Slab.YawDegrees);
	const FVector2D Normals[4] = {Side, -Side, Axis, -Axis};
	const double Turns[4] = {180.0, 0.0, 90.0, -90.0};
	const double Reaches[4] = {Slab.Half.Y, Slab.Half.Y, Slab.Half.X, Slab.Half.X};
	const double Lengths[4] = {2.0 * Slab.Half.X, 2.0 * Slab.Half.X, 2.0 * Slab.Half.Y, 2.0 * Slab.Half.Y};
	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		const FVector2D Pivot = Slab.Center + Normals[Edge] * (Reaches[Edge] - SlabEdgeWidth);
		const FTransform EdgeTransform(FRotator(0.0, Slab.YawDegrees + Turns[Edge], 0.0), FVector(Pivot, Slab.TopZ),
		                               FVector(Lengths[Edge] / SlabMeshSize, 1.0, 1.0));
		if (Edges->IsValidInstance(Slab.EdgeInstances[Edge]))
		{
			Edges->UpdateInstanceTransform(Slab.EdgeInstances[Edge], EdgeTransform, true, true);
		}
	}
}

int32 ASurvivalSovietBaseDressing::AddSlab(const FVector2D& Center, const double YawDegrees, const FVector2D& Half,
                                          const FSurvivalSovietFootprintAssets& Assets, const FSurvivalGroundProjector& Ground,
                                          double& OutMinGround, double& OutMaxGround)
{
	using namespace SurvivalSovietBaseDressingConstants;
	const auto SampleGround = [&Ground](const FVector2D& SlabCenter, const double SlabYaw, const FVector2D& SlabHalf, double& OutMin,
	                                    double& OutMax)
	{
		OutMin = TNumericLimits<double>::Max();
		OutMax = TNumericLimits<double>::Lowest();
		const FVector2D Axis = GetAxis(SlabYaw);
		const FVector2D Side = GetSide(SlabYaw);
		for (int32 X = -1; X <= 1; ++X)
		{
			for (int32 Y = -1; Y <= 1; ++Y)
			{
				double Z = 0.0;
				if (GetGroundZ(Ground, SlabCenter + Axis * (X * SlabHalf.X) + Side * (Y * SlabHalf.Y), Z))
				{
					OutMin = FMath::Min(OutMin, Z);
					OutMax = FMath::Max(OutMax, Z);
				}
			}
		}
		return OutMin <= OutMax;
	};
	if (not SampleGround(Center, YawDegrees, Half, OutMinGround, OutMaxGround) or OutMaxGround - OutMinGround > MaxSlabRelief)
	{
		return INDEX_NONE;
	}
	// Merge into a slab of the same yaw (or turned by a quarter) that overlaps or nearly touches it.
	for (int32 Index = 0; Index < M_Slabs.Num(); ++Index)
	{
		FSlab& Other = M_Slabs[Index];
		const double Turn = FMath::Fmod(FMath::Abs(FRotator::NormalizeAxis(YawDegrees - Other.YawDegrees)), 90.0);
		if (FMath::Min(Turn, 90.0 - Turn) > SlabMergeYawTolerance)
		{
			continue;
		}
		const FVector2D LocalCenter = ToLocal(Center, Other.Center, Other.YawDegrees);
		const bool bQuarter = FMath::Abs(FVector2D::DotProduct(GetAxis(YawDegrees), GetAxis(Other.YawDegrees))) < 0.5;
		const FVector2D LocalHalf = bQuarter ? FVector2D(Half.Y, Half.X) : Half;
		const FVector2D NewMin = LocalCenter - LocalHalf;
		const FVector2D NewMax = LocalCenter + LocalHalf;
		const bool bNear = NewMin.X <= Other.Half.X + SlabMergeGap and NewMax.X >= -Other.Half.X - SlabMergeGap
			and NewMin.Y <= Other.Half.Y + SlabMergeGap and NewMax.Y >= -Other.Half.Y - SlabMergeGap;
		if (not bNear)
		{
			continue;
		}
		const FVector2D UnionMin(FMath::Min(NewMin.X, -Other.Half.X), FMath::Min(NewMin.Y, -Other.Half.Y));
		const FVector2D UnionMax(FMath::Max(NewMax.X, Other.Half.X), FMath::Max(NewMax.Y, Other.Half.Y));
		const FVector2D UnionLocalCenter = (UnionMin + UnionMax) * 0.5;
		const FVector2D UnionCenter = Other.Center + GetAxis(Other.YawDegrees) * UnionLocalCenter.X + GetSide(Other.YawDegrees) * UnionLocalCenter.Y;
		const FVector2D UnionHalf = (UnionMax - UnionMin) * 0.5;
		double MinGround = 0.0;
		double MaxGround = 0.0;
		if (not SampleGround(UnionCenter, Other.YawDegrees, UnionHalf, MinGround, MaxGround) or MaxGround - MinGround > MaxSlabRelief)
		{
			continue;
		}
		Other.Center = UnionCenter;
		Other.Half = UnionHalf;
		Other.TopZ = MaxGround + Assets.SlabRise;
		Other.BottomZ = MinGround - Assets.SlabSink;
		UpdateSlabInstance(Other, Assets);
		RaiseLifted(Other);
		OutMinGround = MinGround;
		OutMaxGround = MaxGround;
		return Index;
	}
	UInstancedStaticMeshComponent* Instancer = GetInstancer(Assets.SlabMesh, EInstanceKind::Slab, Assets.SlabMaterial, 0.f);
	if (Instancer == nullptr)
	{
		return INDEX_NONE;
	}
	FSlab& Slab = M_Slabs.AddDefaulted_GetRef();
	Slab.Center = Center;
	Slab.YawDegrees = YawDegrees;
	Slab.Half = Half;
	Slab.TopZ = OutMaxGround + Assets.SlabRise;
	Slab.BottomZ = OutMinGround - Assets.SlabSink;
	Slab.Instance = Instancer->AddInstance(FTransform::Identity, true);
	if (UInstancedStaticMeshComponent* Edges = GetInstancer(Assets.SlabEdgeMesh, EInstanceKind::Slab, Assets.SlabMaterial, 0.f))
	{
		for (int32 Edge = 0; Edge < 4; ++Edge)
		{
			Slab.EdgeInstances[Edge] = Edges->AddInstance(FTransform::Identity, true);
		}
	}
	UpdateSlabInstance(Slab, Assets);
	return M_Slabs.Num() - 1;
}

void ASurvivalSovietBaseDressing::LiftOnto(FSlab& Slab, AActor& Actor, const FSurvivalGroundProjector& Ground)
{
	double GroundZ = 0.0;
	if (not GetGroundZ(Ground, FVector2D(Actor.GetActorLocation()), GroundZ))
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<AActor>, double>& Entry : Slab.Lifted)
	{
		if (Entry.Key.Get() == &Actor)
		{
			return;
		}
	}
	// The actor keeps its pivot's height over the ground, now measured from the slab top.
	Slab.Lifted.Emplace(&Actor, Actor.GetActorLocation().Z - GroundZ);
	RaiseLifted(Slab);
}

void ASurvivalSovietBaseDressing::RaiseLifted(FSlab& Slab) const
{
	for (const TPair<TWeakObjectPtr<AActor>, double>& Entry : Slab.Lifted)
	{
		if (AActor* Actor = Entry.Key.Get())
		{
			FVector Location = Actor->GetActorLocation();
			Location.Z = Slab.TopZ + Entry.Value;
			Actor->SetActorLocation(Location);
		}
	}
}

void ASurvivalSovietBaseDressing::LiftIfOnSlab(AActor& Actor)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or Actor.IsA<ASurvivalSovietBasePad>())
	{
		return;
	}
	const FSurvivalGroundProjector Ground(*World, SurvivalSovietBaseDressingConstants::GroundTraceUp,
	                                      SurvivalSovietBaseDressingConstants::GroundTraceDown);
	for (FSlab& Slab : M_Slabs)
	{
		if (GetRectDistance(FVector2D(Actor.GetActorLocation()), Slab.Center, Slab.YawDegrees, Slab.Half) < 0.0)
		{
			LiftOnto(Slab, Actor, Ground);
			return;
		}
	}
}

bool ASurvivalSovietBaseDressing::LayStrip(const FVector2D& Start, const FVector2D& Direction, const double Length, const double Width,
                                          const double TopStartOverGround, const double TopEndOverGround, const TOptional<double> TopStartZ,
                                          const FSurvivalSovietFootprintAssets& Assets, const FSurvivalGroundProjector& Ground,
                                          FVector2D& OutEnd)
{
	using namespace SurvivalSovietBaseDressingConstants;
	UInstancedStaticMeshComponent* Instancer = GetInstancer(Assets.SlabMesh, EInstanceKind::Slab, Assets.SlabMaterial, 0.f);
	OutEnd = Start + Direction * Length;
	double StartZ = 0.0;
	double EndZ = 0.0;
	double MidZ = 0.0;
	if (Instancer == nullptr or not GetGroundZ(Ground, Start, StartZ) or not GetGroundZ(Ground, OutEnd, EndZ)
		or not GetGroundZ(Ground, (Start + OutEnd) * 0.5, MidZ))
	{
		return false;
	}
	const double TopStart = TopStartZ.IsSet() ? TopStartZ.GetValue() : StartZ + TopStartOverGround;
	const double TopEnd = EndZ + TopEndOverGround;
	const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(TopEnd - TopStart, Length));
	const double TopMid = (TopStart + TopEnd) * 0.5;
	// Deep enough to reach under the lowest of the three ground samples.
	const double Depth = TopMid - FMath::Min3(StartZ, EndZ, MidZ) + Assets.SlabSink;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	const FVector Scale(Length / SlabMeshSize, Width / SlabMeshSize, FMath::Max(1.0, Depth / SlabMeshDepth));
	Instancer->AddInstance(FTransform(FRotator(Pitch, Yaw, 0.0), FVector((Start + OutEnd) * 0.5, TopMid), Scale), true);
	return true;
}

int32 ASurvivalSovietBaseDressing::LayBoards(const FVector2D& DoorEdge, const FVector2D& Direction, const double Length,
                                            const int32 MaxSections, const double SectionLength, const FSurvivalSovietFootprintAssets& Assets,
                                            FRandomStream& Random, const FSurvivalGroundProjector& Ground)
{
	using namespace SurvivalSovietBaseDressingConstants;
	if (Assets.PlankMeshes.IsEmpty() or Direction.IsNearlyZero())
	{
		return 0;
	}
	const FVector2D Across(-Direction.Y, Direction.X);
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	// One board: the mesh stretched to Length x BoardWidth x BoardThickness, long along its longer axis.
	const auto Board = [&](const FVector2D& Spot, const double BoardYaw, const double BoardLength)
	{
		UStaticMesh* Mesh = Assets.PlankMeshes[Random.RandRange(0, Assets.PlankMeshes.Num() - 1)];
		if (Mesh == nullptr)
		{
			return 0;
		}
		const FVector Size = Mesh->GetBoundingBox().GetSize();
		const bool bLongX = Size.X >= Size.Y;
		const double LongScale = BoardLength / FMath::Max(bLongX ? Size.X : Size.Y, 1.0);
		const double WideScale = BoardWidth / FMath::Max(bLongX ? Size.Y : Size.X, 1.0);
		const FVector Scale = bLongX ? FVector(LongScale, WideScale, BoardThickness / FMath::Max(Size.Z, 1.0))
			                      : FVector(WideScale, LongScale, BoardThickness / FMath::Max(Size.Z, 1.0));
		const double Jitter = Random.FRandRange(-PlankYawJitter, PlankYawJitter);
		const double Lift = Random.FRandRange(0.0, BoardHeightJitter);
		return PlaceOnGround(Mesh, EInstanceKind::Plank, Spot, BoardYaw + (bLongX ? 0.0 : 90.0) + Jitter, Scale, Lift, Assets, Ground) >= 0.0 ? 1 : 0;
	};
	int32 Placed = 0;
	const int32 Sections = FMath::Min(MaxSections, FMath::FloorToInt32(Length / (SectionLength + PlankGap)));
	if (Sections <= 0)
	{
		// The road is closer than one section: two threshold boards across the door.
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector2D Spot = DoorEdge + Direction * (ThresholdStartGap + (Index + 0.5) * (BoardWidth + ThresholdBoardGap));
			Placed += Board(Spot, Yaw + 90.0, ThresholdBoardLength);
		}
		return Placed;
	}
	for (int32 Section = 0; Section < Sections; ++Section)
	{
		const int32 Boards = Random.RandRange(BoardsMin, BoardsMax);
		const FVector2D SectionCenter = DoorEdge + Direction * (PlankStartGap + (Section + 0.5) * (SectionLength + PlankGap));
		for (int32 Index = 0; Index < Boards; ++Index)
		{
			const double Offset = (Index - 0.5 * (Boards - 1)) * (BoardWidth + BoardGap);
			Placed += Board(SectionCenter + Across * Offset, Yaw, SectionLength * Random.FRandRange(0.9, 1.0));
		}
	}
	return Placed;
}

void ASurvivalSovietBaseDressing::AddSpills(const FSlab& Slab, const FVector2D& Door, const FSurvivalSovietFootprintAssets& Assets,
                                           FRandomStream& Random)
{
	using namespace SurvivalSovietBaseDressingConstants;
	if (Assets.SpillSurface == nullptr)
	{
		return;
	}
	const FVector2D Axis = GetAxis(Slab.YawDegrees);
	const FVector2D Side = GetSide(Slab.YawDegrees);
	const FVector2D Normals[4] = {Axis, -Axis, Side, -Side};
	// The door side always spills (traffic); a second, random side half of the time.
	int32 DoorSide = 0;
	double BestDot = -2.0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Dot = FVector2D::DotProduct(Normals[Index], Door);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			DoorSide = Index;
		}
	}
	TArray<int32> Sides = {DoorSide};
	if (Random.FRand() < 0.5f)
	{
		Sides.Add((DoorSide + 1 + Random.RandRange(0, 2)) % 4);
	}
	for (const int32 Index : Sides)
	{
		const bool bAlongX = Index < 2;
		const double SideLength = 2.0 * (bAlongX ? Slab.Half.Y : Slab.Half.X);
		const double Reach = bAlongX ? Slab.Half.X : Slab.Half.Y;
		const FVector2D Tangent = bAlongX ? Side : Axis;
		const double Out = Random.FRandRange(SpillReachMin, SpillReachMax);
		const double HalfLength = 0.5 * SideLength * Random.FRandRange(SpillLengthShareMin, SpillLengthShareMax);
		const FVector2D Center = Slab.Center + Normals[Index] * (Reach + Out * 0.4)
			+ Tangent * Random.FRandRange(-0.25, 0.25) * SideLength;
		const double Yaw = FMath::Atan2(Tangent.Y, Tangent.X);
		const FVector2D Half(HalfLength, Out * 0.6);
		UDecalComponent* Decal = NewObject<UDecalComponent>(this);
		Decal->SetupAttachment(GetRootComponent());
		Decal->RegisterComponent();
		Decal->SetWorldLocationAndRotation(FVector(Center, Slab.TopZ), FRotator(-90.0, FMath::RadiansToDegrees(Yaw), 0.0));
		Decal->DecalSize = FVector(SpillDecalDepth, Half.Y + SpillNoise, Half.X + SpillNoise);
		Decal->SortOrder = SpillSortOrder;
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Assets.SpillSurface, this);
		if (Instance == nullptr)
		{
			Decal->SetDecalMaterial(Assets.SpillSurface);
			continue;
		}
		Instance->SetVectorParameterValue(TEXT("ShapeCenter"), FLinearColor(Center.X, Center.Y, 0.f, 0.f));
		Instance->SetVectorParameterValue(TEXT("ShapeAxis"), FLinearColor(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f, 0.f));
		Instance->SetVectorParameterValue(TEXT("ShapeHalf"), FLinearColor(Half.X, Half.Y, FMath::Min(Half.X, Half.Y), 0.f));
		Instance->SetVectorParameterValue(TEXT("ShapeEdge"), FLinearColor(SpillEdge, SpillNoise, 0.f, 1.f));
		Instance->SetVectorParameterValue(TEXT("ShapeFade"), FLinearColor(0.f, 0.f, 0.f, 0.f));
		Decal->SetDecalMaterial(Instance);
	}
}

int32 ASurvivalSovietBaseDressing::LayPlanks(const FVector2D& Start, const FVector2D& Direction, const double Length, const int32 MaxPlanks,
                                            const double PlankLength, const double PlankWidth, const FSurvivalSovietFootprintAssets& Assets,
                                            FRandomStream& Random, const FSurvivalGroundProjector& Ground)
{
	using namespace SurvivalSovietBaseDressingConstants;
	if (Assets.PlankMeshes.IsEmpty() or Length < PlankLength)
	{
		return 0;
	}
	const int32 Count = FMath::Clamp(FMath::FloorToInt32(Length / (PlankLength + PlankGap)), 1, MaxPlanks);
	const FVector2D Across(-Direction.Y, Direction.X);
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	int32 Placed = 0;
	for (int32 Plank = 0; Plank < Count; ++Plank)
	{
		UStaticMesh* Mesh = Assets.PlankMeshes[Random.RandRange(0, Assets.PlankMeshes.Num() - 1)];
		if (Mesh == nullptr)
		{
			continue;
		}
		// Each plank stretched to its size: long along the walk, its own thickness kept.
		const FVector Size = Mesh->GetBoundingBox().GetSize();
		const bool bLongX = Size.X >= Size.Y;
		const FVector Scale = bLongX ? FVector(PlankLength / FMath::Max(Size.X, 1.0), PlankWidth / FMath::Max(Size.Y, 1.0), PlankThickness / FMath::Max(Size.Z, 1.0))
			                      : FVector(PlankWidth / FMath::Max(Size.X, 1.0), PlankLength / FMath::Max(Size.Y, 1.0), PlankThickness / FMath::Max(Size.Z, 1.0));
		const double PlankYaw = Yaw + (bLongX ? 0.0 : 90.0) + Random.FRandRange(-PlankYawJitter, PlankYawJitter);
		const FVector2D Spot = Start + Direction * ((Plank + 0.5) * (PlankLength + PlankGap)) + Across * Random.FRandRange(-PlankSideJitter, PlankSideJitter);
		Placed += PlaceOnGround(Mesh, EInstanceKind::Plank, Spot, PlankYaw, Scale, 0.0, Assets, Ground) >= 0.0 ? 1 : 0;
	}
	return Placed;
}

void ASurvivalSovietBaseDressing::LayClutterClusters(const FSurvivalSovietFootprintRequest& Request, const FVector2D& KeepHalf,
                                                    const TArray<UStaticMesh*>& Meshes, const TArrayView<const int32> PreferredWalls,
                                                    const int32 Clusters, const FSurvivalSovietFootprintAssets& Assets,
                                                    FRandomStream& Random, const FSurvivalGroundProjector& Ground,
                                                    FSurvivalSovietFootprintRecord& OutRecord)
{
	using namespace SurvivalSovietBaseDressingConstants;
	if (Meshes.IsEmpty() or PreferredWalls.IsEmpty())
	{
		return;
	}
	const FVector2D Axis = GetAxis(Request.YawDegrees);
	const FVector2D Side = GetSide(Request.YawDegrees);
	const FVector2D Normals[4] = {Axis, -Axis, Side, -Side};
	TArray<TPair<FVector2D, double>> Taken;
	for (int32 Cluster = 0; Cluster < Clusters; ++Cluster)
	{
		const int32 Wall = PreferredWalls[Cluster % PreferredWalls.Num()];
		const bool bAlongX = Wall < 2;
		const double Reach = bAlongX ? KeepHalf.X : KeepHalf.Y;
		const double Span = bAlongX ? KeepHalf.Y : KeepHalf.X;
		const FVector2D Tangent = bAlongX ? Side : Axis;
		const FVector2D ClusterCenter = Request.Center + Normals[Wall] * (Reach + ClutterKeepGap + ClusterOffset)
			+ Tangent * (Random.FRandRange(-ClusterSpanShare, ClusterSpanShare) * Span);
		const double WallYaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
		const int32 Items = Random.RandRange(ClusterMinItems, ClusterMaxItems);
		for (int32 Item = 0; Item < Items; ++Item)
		{
			UStaticMesh* Mesh = Meshes[Random.RandRange(0, Meshes.Num() - 1)];
			if (Mesh == nullptr)
			{
				continue;
			}
			const FVector Extent = Mesh->GetBoundingBox().GetExtent();
			const double Radius = FVector2D(Extent.X, Extent.Y).Size();
			for (int32 Try = 0; Try < ClutterTries; ++Try)
			{
				const FVector2D Spot = ClusterCenter + FVector2D(Random.FRandRange(-ClusterRadius, ClusterRadius), Random.FRandRange(-ClusterRadius, ClusterRadius));
				// Never on or across the slab (or the building): the whole box stays outside the keep-out rectangle.
				const double EdgeClearance = GetRectDistance(Spot, Request.Center, Request.YawDegrees, KeepHalf) - Radius;
				if (EdgeClearance < ClutterKeepGap)
				{
					continue;
				}
				bool bFree = true;
				for (const TPair<FVector2D, double>& Other : Taken)
				{
					bFree = bFree and FVector2D::Distance(Other.Key, Spot) >= Other.Value + Radius + ClutterSpacing;
				}
				if (not bFree)
				{
					continue;
				}
				const double Yaw = WallYaw + Random.FRandRange(-ClutterYawJitter, ClutterYawJitter) + (Random.FRand() < 0.5f ? 0.0 : 90.0);
				const double Gap = PlaceOnGround(Mesh, EInstanceKind::Clutter, Spot, Yaw, FVector::OneVector, 0.0, Assets, Ground);
				if (Gap < 0.0)
				{
					break;
				}
				Taken.Emplace(Spot, Radius);
				OutRecord.ClutterMaxGroundGap = FMath::Max(OutRecord.ClutterMaxGroundGap, Gap);
				OutRecord.ClutterMinEdgeClearance = OutRecord.Clutter == 0 ? EdgeClearance : FMath::Min(OutRecord.ClutterMinEdgeClearance, EdgeClearance);
				++OutRecord.Clutter;
				break;
			}
		}
	}
}

FSurvivalSovietFootprintRecord ASurvivalSovietBaseDressing::AddFootprint(const FSurvivalSovietFootprintRequest& Request,
                                                                        const FSurvivalSovietFootprintAssets& Assets,
                                                                        ASurvivalSovietBasePad& Pad,
                                                                        const TArrayView<const TWeakObjectPtr<AActor>> Structures)
{
	using namespace SurvivalSovietBaseDressingConstants;
	FSurvivalSovietFootprintRecord Record;
	Record.Style = GetStyleName(Request.Style);
	Record.Center = Request.Center;
	Record.YawDegrees = Request.YawDegrees;
	Record.FootprintHalf = Request.FootprintHalf;
	UWorld* World = GetWorld();
	if (not IsValid(World) or Request.Style == ESurvivalSovietFoundationStyle::None)
	{
		return Record;
	}
	const int32 InstancesBefore = GetInstanceCount();
	FRandomStream Random(Request.Seed);
	const FSurvivalGroundProjector Ground(*World, GroundTraceUp, GroundTraceDown);
	const FVector2D Axis = GetAxis(Request.YawDegrees);
	const FVector2D Side = GetSide(Request.YawDegrees);
	// The door wall: the footprint side facing the road (wall 0 +X, 1 -X, 2 +Y, 3 -Y).
	const FVector2D Normals[4] = {Axis, -Axis, Side, -Side};
	int32 DoorWall = 0;
	double BestDot = -2.0;
	for (int32 Wall = 0; Wall < 4; ++Wall)
	{
		const double Dot = FVector2D::DotProduct(Normals[Wall], Request.DoorDirection);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			DoorWall = Wall;
		}
	}
	const FVector2D Door = Normals[DoorWall];
	Record.DoorNormal = Door;
	const bool bDoorAlongX = DoorWall < 2;
	const double DoorReach = bDoorAlongX ? Request.FootprintHalf.X : Request.FootprintHalf.Y;
	const double WallHalf = bDoorAlongX ? Request.FootprintHalf.Y : Request.FootprintHalf.X;
	const int32 Back = DoorWall ^ 1;
	const int32 SideA = bDoorAlongX ? 2 : 0;
	const int32 SideB = SideA + 1;
	const int32 WorkWalls[3] = {Back, SideA, SideB};
	const bool bHasRoad = not Request.DoorDirection.IsNearlyZero() and Request.ApronLength >= ApronMinLength;

	FSurvivalSovietGroundDecalShape Shape;
	Shape.Center = Request.Center;
	Shape.YawRadians = FMath::DegreesToRadians(Request.YawDegrees);
	Shape.SortOrder = DecalSortOrder;
	switch (Request.Style)
	{
	case ESurvivalSovietFoundationStyle::Concrete:
	{
		const FVector2D SlabHalf = Request.FootprintHalf + FVector2D(Assets.SlabMargin, Assets.SlabMargin);
		double MinGround = 0.0;
		double MaxGround = 0.0;
		const int32 SlabIndex = AddSlab(Request.Center, Request.YawDegrees, SlabHalf, Assets, Ground, MinGround, MaxGround);
		if (SlabIndex != INDEX_NONE)
		{
			FSlab& Slab = M_Slabs[SlabIndex];
			Record.bSlab = true;
			Record.SlabIndex = SlabIndex;
			Record.SlabHalf = Slab.Half;
			Record.SlabTopOverGroundMin = Slab.TopZ - MaxGround;
			Record.SlabTopOverGroundMax = Slab.TopZ - MinGround;
			// The building stands on the slab: lift every structure standing inside the footprint.
			for (const TWeakObjectPtr<AActor>& Structure : Structures)
			{
				AActor* Actor = Structure.Get();
				if (Actor == nullptr or Actor->IsA<ASurvivalSovietBasePad>()
					or GetRectDistance(FVector2D(Actor->GetActorLocation()), Request.Center, Request.YawDegrees, Request.FootprintHalf) >= 0.0)
				{
					continue;
				}
				const double ZBefore = Actor->GetActorLocation().Z;
				LiftOnto(Slab, *Actor, Ground);
				Record.BuildingLift = Record.LiftedActors == 0 ? Actor->GetActorLocation().Z - ZBefore : Record.BuildingLift;
				++Record.LiftedActors;
			}
			// The concrete decal paints the slab's top and its soft edge straddles the slab edge onto the sand.
			Shape.HalfSize = SlabHalf - FVector2D(FoundationDecalInset, FoundationDecalInset);
			Shape.CornerRadius = FoundationDecalCorner;
			Shape.EdgeWidth = FoundationDecalEdge;
			Shape.EdgeNoise = FoundationDecalNoise;
			Pad.InitShape(Shape, Assets.FoundationSurface);
			AddSpills(Slab, Door, Assets, Random);
			if (bHasRoad)
			{
				const double Width = FMath::Min(FMath::Max(ApronMinWidth, 2.0 * WallHalf * ApronWallShare), 2.0 * WallHalf);
				const double Length = Request.ApronLength - Assets.SlabMargin;
				if (Length >= ApronMinLength)
				{
					const FVector2D Start = Request.Center + Door * (DoorReach + Assets.SlabMargin);
					if (LayStrip(Start, Door, Length, Width, 0.0, ApronEndOverGround, Slab.TopZ - ApronStartBelowSlab, Assets, Ground,
					             Record.ApronEnd))
					{
						Record.ApronLength = Length;
					}
				}
			}
		}
		const FVector2D Keep = Record.bSlab ? Record.SlabHalf : Request.FootprintHalf;
		LayClutterClusters(Request, Keep, Assets.FactoryClutter, MakeArrayView(WorkWalls), 2, Assets, Random, Ground, Record);
		break;
	}
	case ESurvivalSovietFoundationStyle::Gravel:
	{
		Shape.HalfSize = Request.FootprintHalf + FVector2D(WalkDecalMargin, WalkDecalMargin);
		Shape.CornerRadius = FMath::Min(Shape.HalfSize.X, Shape.HalfSize.Y) * HaloCornerShare;
		Shape.EdgeWidth = WalkDecalEdge;
		Shape.EdgeNoise = WalkDecalNoise;
		Pad.InitShape(Shape, Assets.GravelWalkSurface);
		if (bHasRoad)
		{
			const FVector2D DoorEdge = Request.Center + Door * DoorReach;
			Record.Planks = LayBoards(DoorEdge, Door, Request.ApronLength - PlankStartGap, DuckboardMax, DuckboardLength, Assets, Random, Ground);
			Record.ApronLength = Request.ApronLength;
			Record.ApronEnd = Request.Center + Door * (DoorReach + Request.ApronLength);
		}
		const int32 Walls[2] = {SideA, Back};
		LayClutterClusters(Request, Request.FootprintHalf + FVector2D(PlankStartGap, PlankStartGap), Assets.BarracksClutter, MakeArrayView(Walls),
		                   2, Assets, Random, Ground, Record);
		break;
	}
	case ESurvivalSovietFoundationStyle::Timber:
	{
		const FVector2D Half = Request.FootprintHalf * TrampledScale + FVector2D(TrampledMargin, TrampledMargin);
		Shape.HalfSize = Half;
		Shape.CornerRadius = FMath::Min(Half.X, Half.Y) * TrampledCornerShare;
		Shape.EdgeWidth = TrampledEdge;
		Shape.EdgeNoise = TrampledNoise;
		Pad.InitShape(Shape, Assets.TrampledSurface);
		{
			// Every tent shows its timber: board sections toward the road, or two threshold boards when the road is close.
			const FVector2D DoorEdge = Request.Center + Door * DoorReach;
			Record.Planks = LayBoards(DoorEdge, Door, FMath::Max(0.0, Request.ApronLength - PlankStartGap), TentPlankMax, TentPlankLength, Assets,
			                          Random, Ground);
			Record.ApronLength = Request.ApronLength;
			Record.ApronEnd = Request.Center + Door * (DoorReach + Request.ApronLength);
		}
		const int32 Walls[1] = {SideA};
		LayClutterClusters(Request, Request.FootprintHalf + FVector2D(PlankStartGap, PlankStartGap), Assets.TentClutter, MakeArrayView(Walls),
		                   1, Assets, Random, Ground, Record);
		break;
	}
	case ESurvivalSovietFoundationStyle::Hardstanding:
	{
		Shape.HalfSize = Request.FootprintHalf + FVector2D(HardstandingMargin, HardstandingMargin);
		Shape.CornerRadius = FMath::Min(Shape.HalfSize.X, Shape.HalfSize.Y) * HaloCornerShare;
		Shape.EdgeWidth = HardstandingEdge;
		Shape.EdgeNoise = HardstandingNoise;
		Pad.InitShape(Shape, Assets.GravelOilySurface);
		if (bHasRoad and Assets.RutTrackSurface != nullptr)
		{
			Pad.PaintApron(Door, DoorReach, Request.ApronLength, RutTrackWidth, Assets.RutTrackSurface);
			Record.ApronLength = Request.ApronLength;
			Record.ApronEnd = Request.Center + Door * (DoorReach + Request.ApronLength);
		}
		const int32 Walls[2] = {SideA, SideB};
		LayClutterClusters(Request, Request.FootprintHalf + FVector2D(ClutterKeepGap, ClutterKeepGap), Assets.ShelterClutter,
		                   MakeArrayView(Walls), 1 + Random.RandRange(0, 1), Assets, Random, Ground, Record);
		break;
	}
	case ESurvivalSovietFoundationStyle::Forecourt:
	{
		const double Width = 2.0 * WallHalf * ForecourtWallShare;
		const FVector2D Start = Request.Center + Door * (DoorReach + ForecourtGap);
		if (LayStrip(Start, Door, ForecourtDepth, Width, ForecourtOverGround, ForecourtOverGround, {}, Assets, Ground, Record.ApronEnd))
		{
			Record.ApronLength = ForecourtDepth;
			// A sandbag arc at the forecourt's two outer corners, angled outwards; the middle stays open to the road.
			const FVector2D Across(-Door.Y, Door.X);
			const double DoorYaw = FMath::RadiansToDegrees(FMath::Atan2(Door.Y, Door.X));
			for (const double Sign : {1.0, -1.0})
			{
				if (Assets.SandbagMeshes.IsEmpty())
				{
					break;
				}
				UStaticMesh* Mesh = Assets.SandbagMeshes[Random.RandRange(0, Assets.SandbagMeshes.Num() - 1)];
				const FVector2D Corner = Start + Door * ForecourtDepth + Across * (Sign * Width * 0.5);
				PlaceOnGround(Mesh, EInstanceKind::Clutter, Corner, DoorYaw + 90.0 + Sign * SandbagCornerYaw, FVector::OneVector, 0.0, Assets, Ground);
			}
		}
		const int32 Walls[2] = {SideA, SideB};
		LayClutterClusters(Request, Request.FootprintHalf + FVector2D(ClutterKeepGap, ClutterKeepGap), Assets.ForecourtClutter,
		                   MakeArrayView(Walls), 1, Assets, Random, Ground, Record);
		break;
	}
	default:
		break;
	}
	Record.Instances = GetInstanceCount() - InstancesBefore;
	return Record;
}
