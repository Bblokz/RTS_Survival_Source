// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBasePad.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

namespace SurvivalSovietBasePadConstants
{
	// Half the height of the decal box: base sites are flat, so a shallow box reaches the ground without painting up walls,
	// buildings and vehicles.
	constexpr double DecalHalfDepth = 90.0;
	// Pads paint under tracks and craters.
	constexpr int32 DecalSortOrder = -10;
	constexpr double PadEdgeShare = 0.4;
	constexpr double PadMinEdge = 90.0;
	constexpr double PadMaxEdge = 240.0;
	constexpr double PadEdgeNoiseShare = 0.55;
	constexpr double PadCornerShare = 0.3;
	constexpr double PadMaxCorner = 220.0;
	// Foundation dressing.
	constexpr float GroundTraceUp = 3000.f;
	constexpr float GroundTraceDown = 6000.f;
	constexpr int32 MinClutter = 2;
	constexpr int32 MaxClutter = 4;
	constexpr double ClutterWallGap = 35.0;
	constexpr double ClutterSpanShare = 0.7;
	constexpr double ClutterYawJitter = 15.0;
	constexpr double ApronWidthShare = 0.7;
	constexpr double ApronMaxWidth = 650.0;
	constexpr double ApronMinWidth = 300.0;
	constexpr double ApronMinLength = 150.0;
	constexpr double ApronEdgeWidth = 160.0;
	constexpr double ApronEdgeNoise = 90.0;
	constexpr double ApronFadeShare = 0.5;
	constexpr int32 ApronSortOrder = -9;
	constexpr int32 TentBoards = 2;
	constexpr double TentBoardGap = 25.0;
	constexpr double BoardRise = 4.0;
}

ASurvivalSovietBasePad::ASurvivalSovietBasePad()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
	M_Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	M_Decal->SetupAttachment(Root);
	// Projects straight down: the decal's X is the depth, its Y the actor's Y, its Z the actor's X.
	M_Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	M_Decal->SortOrder = SurvivalSovietBasePadConstants::DecalSortOrder;
}

void ASurvivalSovietBasePad::InitPad(const FVector2D& HalfSize, UMaterialInterface* Material)
{
	using namespace SurvivalSovietBasePadConstants;
	FSurvivalSovietGroundDecalShape Shape;
	Shape.Center = FVector2D(GetActorLocation());
	Shape.YawRadians = FMath::DegreesToRadians(GetActorRotation().Yaw);
	// The soft edge is centred on the pad's outline: half of it fades out beyond, so the building never stands on its fade.
	const double Smaller = FMath::Min(HalfSize.X, HalfSize.Y);
	Shape.EdgeWidth = FMath::Clamp(Smaller * PadEdgeShare, PadMinEdge, PadMaxEdge);
	Shape.EdgeNoise = Shape.EdgeWidth * PadEdgeNoiseShare;
	Shape.CornerRadius = FMath::Min(Smaller * PadCornerShare, PadMaxCorner);
	Shape.HalfSize = HalfSize + FVector2D(Shape.EdgeWidth * 0.5, Shape.EdgeWidth * 0.5);
	Shape.SortOrder = DecalSortOrder;
	InitShape(Shape, Material);
}

void ASurvivalSovietBasePad::InitShape(const FSurvivalSovietGroundDecalShape& Shape, UMaterialInterface* Material)
{
	SetActorScale3D(FVector::OneVector);
	const bool bPiece = Shape.BoxHalfAlong > 0.0;
	SetActorLocationAndRotation(FVector(bPiece ? Shape.BoxCenter : Shape.Center, GetActorLocation().Z),
	                            FRotator(0.0, FMath::RadiansToDegrees(Shape.YawRadians), 0.0));
	const double Reach = FMath::Max(Shape.EdgeNoise, 0.0);
	const double HalfAlong = bPiece ? Shape.BoxHalfAlong : Shape.HalfSize.X + Reach;
	M_Decal->DecalSize = FVector(SurvivalSovietBasePadConstants::DecalHalfDepth, Shape.HalfSize.Y + Reach, HalfAlong);
	M_Decal->SortOrder = Shape.SortOrder;
	UMaterialInstanceDynamic* Instance = Material != nullptr ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	if (Instance == nullptr)
	{
		M_Decal->SetDecalMaterial(Material);
		M_Decal->MarkRenderStateDirty();
		return;
	}
	const bool bRoad = Shape.Mode == ESurvivalSovietGroundDecalMode::Road;
	Instance->SetVectorParameterValue(TEXT("ShapeCenter"), FLinearColor(Shape.Center.X, Shape.Center.Y, 0.f, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeAxis"), FLinearColor(FMath::Cos(Shape.YawRadians), FMath::Sin(Shape.YawRadians), 0.f, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeHalf"), FLinearColor(Shape.HalfSize.X, Shape.HalfSize.Y, Shape.CornerRadius, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeEdge"), FLinearColor(Shape.EdgeWidth, Shape.EdgeNoise, bRoad ? 1.f : 0.f,
	                                                                  Shape.VergeStrength));
	Instance->SetVectorParameterValue(TEXT("ShapeFade"), FLinearColor(Shape.bFadeStart ? 1.f : 0.f, Shape.bFadeEnd ? 1.f : 0.f,
	                                                                  Shape.FadeLength, 0.f));
	M_Decal->SetDecalMaterial(Instance);
	M_Decal->MarkRenderStateDirty();
}

int32 ASurvivalSovietBasePad::InitDressing(const FSurvivalSovietPadDressing& Dressing)
{
	using namespace SurvivalSovietBasePadConstants;
	UWorld* World = GetWorld();
	if (Dressing.Style == 0 or not IsValid(World))
	{
		return 0;
	}
	FRandomStream Random(Dressing.Seed);
	const FSurvivalGroundProjector Ground(*World, GroundTraceUp, GroundTraceDown);
	int32 Instances = 0;
	if (Dressing.Style == 1)
	{
		Instances += LayFoundationTiles(Dressing, Random, Ground);
		Instances += LayClutter(Dressing, Random, Ground);
	}
	else
	{
		Instances += LayTentBoards(Dressing, Random, Ground);
	}
	LayApron(Dressing);
	return Instances;
}

UInstancedStaticMeshComponent* ASurvivalSovietBasePad::GetOrAddInstancer(UStaticMesh* Mesh)
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
	Instancer->SetupAttachment(GetRootComponent());
	Instancer->RegisterComponent();
	M_Instancers.Add(Mesh, Instancer);
	return Instancer;
}

bool ASurvivalSovietBasePad::PlaceOnGround(UStaticMesh* Mesh, const FVector2D& WorldXY, const double YawDegrees, const FVector& Scale,
                                           const double Rise, const FSurvivalGroundProjector& Ground)
{
	UInstancedStaticMeshComponent* Instancer = GetOrAddInstancer(Mesh);
	FSurvivalGroundHit Hit;
	if (Instancer == nullptr or not Ground.TryProject(WorldXY, Hit))
	{
		return false;
	}
	// The mesh's own box: its top ends Rise above the ground, its XY centre over WorldXY.
	const FBox Box = Mesh->GetBoundingBox();
	const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
	const FVector CentreOffset = Rotation.RotateVector(FVector(Box.GetCenter().X * Scale.X, Box.GetCenter().Y * Scale.Y, 0.0));
	const FVector Pivot(WorldXY.X - CentreOffset.X, WorldXY.Y - CentreOffset.Y, Hit.Location.Z + Rise - Box.Max.Z * Scale.Z);
	Instancer->AddInstance(FTransform(Rotation, Pivot, Scale), true);
	return true;
}

double ASurvivalSovietBasePad::GetFootprintReach(const FVector2D& FootprintHalf, const FVector2D& Direction) const
{
	const double Yaw = FMath::DegreesToRadians(GetActorRotation().Yaw);
	const FVector2D Axis(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Side(-Axis.Y, Axis.X);
	return FMath::Abs(FVector2D::DotProduct(Direction, Axis)) * FootprintHalf.X + FMath::Abs(FVector2D::DotProduct(Direction, Side)) * FootprintHalf.Y;
}

int32 ASurvivalSovietBasePad::LayFoundationTiles(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random,
                                                const FSurvivalGroundProjector& Ground)
{
	if (Dressing.TileMeshes.IsEmpty() or Dressing.TileMeshes[0] == nullptr)
	{
		return 0;
	}
	// One cell size for the slab (the first tile's footprint), stretched so a whole number of tiles covers it exactly.
	const FVector TileSize = Dressing.TileMeshes[0]->GetBoundingBox().GetSize();
	const FVector2D SlabHalf = Dressing.FootprintHalf + FVector2D(Dressing.SlabMargin, Dressing.SlabMargin);
	const int32 CountX = FMath::Max(1, FMath::CeilToInt32(2.0 * SlabHalf.X / FMath::Max(TileSize.X, 1.0)));
	const int32 CountY = FMath::Max(1, FMath::CeilToInt32(2.0 * SlabHalf.Y / FMath::Max(TileSize.Y, 1.0)));
	const FTransform Frame(GetActorRotation(), GetActorLocation());
	int32 Placed = 0;
	for (int32 X = 0; X < CountX; ++X)
	{
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			UStaticMesh* Mesh = Dressing.TileMeshes[Random.RandRange(0, Dressing.TileMeshes.Num() - 1)];
			if (Mesh == nullptr)
			{
				continue;
			}
			const FVector Size = Mesh->GetBoundingBox().GetSize();
			const FVector Scale(2.0 * SlabHalf.X / (CountX * FMath::Max(Size.X, 1.0)), 2.0 * SlabHalf.Y / (CountY * FMath::Max(Size.Y, 1.0)), 1.0);
			const FVector Local(-SlabHalf.X + (X + 0.5) * 2.0 * SlabHalf.X / CountX, -SlabHalf.Y + (Y + 0.5) * 2.0 * SlabHalf.Y / CountY, 0.0);
			const FVector World = Frame.TransformPosition(Local);
			// Half the tiles turned round, so the joints and stains do not repeat in one direction.
			const double Yaw = GetActorRotation().Yaw + (Random.FRand() < 0.5f ? 0.0 : 180.0);
			Placed += PlaceOnGround(Mesh, FVector2D(World), Yaw, Scale, Dressing.SlabRise, Ground) ? 1 : 0;
		}
	}
	return Placed;
}

int32 ASurvivalSovietBasePad::LayClutter(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random,
                                        const FSurvivalGroundProjector& Ground)
{
	using namespace SurvivalSovietBasePadConstants;
	if (Dressing.ClutterMeshes.IsEmpty())
	{
		return 0;
	}
	const double Yaw = FMath::DegreesToRadians(GetActorRotation().Yaw);
	const FVector2D Axis(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Side(-Axis.Y, Axis.X);
	// The four walls as outward normals and half lengths; the door wall (facing the road) stays clear.
	const FVector2D Normals[4] = {Axis, -Axis, Side, -Side};
	const double Spans[4] = {Dressing.FootprintHalf.Y, Dressing.FootprintHalf.Y, Dressing.FootprintHalf.X, Dressing.FootprintHalf.X};
	const double Reaches[4] = {Dressing.FootprintHalf.X, Dressing.FootprintHalf.X, Dressing.FootprintHalf.Y, Dressing.FootprintHalf.Y};
	int32 DoorWall = INDEX_NONE;
	double BestDot = -2.0;
	for (int32 Wall = 0; Wall < 4; ++Wall)
	{
		const double Dot = FVector2D::DotProduct(Normals[Wall], Dressing.DoorDirection);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			DoorWall = Wall;
		}
	}
	const int32 Count = Random.RandRange(MinClutter, MaxClutter);
	int32 Placed = 0;
	for (int32 Item = 0; Item < Count; ++Item)
	{
		int32 Wall = Random.RandRange(0, 3);
		Wall = Wall == DoorWall ? (Wall + 1 + Random.RandRange(0, 2)) % 4 : Wall;
		UStaticMesh* Mesh = Dressing.ClutterMeshes[Random.RandRange(0, Dressing.ClutterMeshes.Num() - 1)];
		if (Mesh == nullptr)
		{
			continue;
		}
		const double HalfDepth = FMath::Min(Mesh->GetBoundingBox().GetExtent().X, Mesh->GetBoundingBox().GetExtent().Y);
		const FVector2D Along = Wall < 2 ? Side : Axis;
		const FVector2D Spot = FVector2D(GetActorLocation()) + Normals[Wall] * (Reaches[Wall] + ClutterWallGap + HalfDepth)
			+ Along * (Random.FRandRange(-ClutterSpanShare, ClutterSpanShare) * Spans[Wall]);
		const double ItemYaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)) + Random.FRandRange(-ClutterYawJitter, ClutterYawJitter);
		// Clutter stands on the ground (its bottom at the ground): rise = its own height.
		Placed += PlaceOnGround(Mesh, Spot, ItemYaw, FVector::OneVector, Mesh->GetBoundingBox().GetSize().Z, Ground) ? 1 : 0;
	}
	return Placed;
}

int32 ASurvivalSovietBasePad::LayTentBoards(const FSurvivalSovietPadDressing& Dressing, FRandomStream& Random,
                                           const FSurvivalGroundProjector& Ground)
{
	using namespace SurvivalSovietBasePadConstants;
	if (Dressing.BoardMeshes.IsEmpty() or Dressing.DoorDirection.IsNearlyZero())
	{
		return 0;
	}
	const FVector2D Across(-Dressing.DoorDirection.Y, Dressing.DoorDirection.X);
	const double Reach = GetFootprintReach(Dressing.FootprintHalf, Dressing.DoorDirection);
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dressing.DoorDirection.Y, Dressing.DoorDirection.X));
	int32 Placed = 0;
	for (int32 Board = 0; Board < TentBoards; ++Board)
	{
		UStaticMesh* Mesh = Dressing.BoardMeshes[Random.RandRange(0, Dressing.BoardMeshes.Num() - 1)];
		if (Mesh == nullptr)
		{
			continue;
		}
		const FVector Extent = Mesh->GetBoundingBox().GetExtent();
		const double Offset = (Board - 0.5 * (TentBoards - 1)) * (2.0 * Extent.Y + TentBoardGap);
		const FVector2D Spot = FVector2D(GetActorLocation()) + Dressing.DoorDirection * (Reach + Extent.X + TentBoardGap) + Across * Offset;
		Placed += PlaceOnGround(Mesh, Spot, Yaw + Random.FRandRange(-6.0, 6.0), FVector::OneVector, BoardRise, Ground) ? 1 : 0;
	}
	return Placed;
}

void ASurvivalSovietBasePad::LayApron(const FSurvivalSovietPadDressing& Dressing)
{
	using namespace SurvivalSovietBasePadConstants;
	if (Dressing.ApronMaterial == nullptr or Dressing.DoorDirection.IsNearlyZero() or Dressing.ApronLength < ApronMinLength)
	{
		return;
	}
	const FVector2D Across(-Dressing.DoorDirection.Y, Dressing.DoorDirection.X);
	const double Reach = GetFootprintReach(Dressing.FootprintHalf, Dressing.DoorDirection);
	const double Width = FMath::Clamp(2.0 * GetFootprintReach(Dressing.FootprintHalf, Across) * ApronWidthShare, ApronMinWidth, ApronMaxWidth);
	FSurvivalSovietGroundDecalShape Shape;
	Shape.Mode = ESurvivalSovietGroundDecalMode::Road;
	// From just under the building's door edge to the road's verge.
	Shape.Center = FVector2D(GetActorLocation()) + Dressing.DoorDirection * (Reach + Dressing.ApronLength * 0.5 - Dressing.SlabMargin);
	Shape.YawRadians = FMath::Atan2(Dressing.DoorDirection.Y, Dressing.DoorDirection.X);
	Shape.HalfSize = FVector2D(Dressing.ApronLength * 0.5 + Dressing.SlabMargin, Width * 0.5);
	Shape.EdgeWidth = ApronEdgeWidth;
	Shape.EdgeNoise = ApronEdgeNoise;
	Shape.bFadeStart = false;
	Shape.bFadeEnd = true;
	Shape.FadeLength = FMath::Max(ApronMinLength, Dressing.ApronLength * ApronFadeShare);
	Shape.SortOrder = ApronSortOrder;
	M_ApronDecal = NewObject<UDecalComponent>(this);
	M_ApronDecal->SetupAttachment(GetRootComponent());
	M_ApronDecal->RegisterComponent();
	M_ApronDecal->SetWorldLocationAndRotation(FVector(Shape.Center, GetActorLocation().Z),
	                                          FRotator(-90.0, FMath::RadiansToDegrees(Shape.YawRadians), 0.0));
	M_ApronDecal->DecalSize = FVector(DecalHalfDepth, Shape.HalfSize.Y + Shape.EdgeNoise, Shape.HalfSize.X + Shape.EdgeNoise);
	M_ApronDecal->SortOrder = Shape.SortOrder;
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Dressing.ApronMaterial, this);
	if (Instance == nullptr)
	{
		M_ApronDecal->SetDecalMaterial(Dressing.ApronMaterial);
		return;
	}
	Instance->SetVectorParameterValue(TEXT("ShapeCenter"), FLinearColor(Shape.Center.X, Shape.Center.Y, 0.f, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeAxis"), FLinearColor(FMath::Cos(Shape.YawRadians), FMath::Sin(Shape.YawRadians), 0.f, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeHalf"), FLinearColor(Shape.HalfSize.X, Shape.HalfSize.Y, Shape.CornerRadius, 0.f));
	Instance->SetVectorParameterValue(TEXT("ShapeEdge"), FLinearColor(Shape.EdgeWidth, Shape.EdgeNoise, 1.f, Shape.VergeStrength));
	Instance->SetVectorParameterValue(TEXT("ShapeFade"), FLinearColor(0.f, 1.f, Shape.FadeLength, 0.f));
	M_ApronDecal->SetDecalMaterial(Instance);
}
