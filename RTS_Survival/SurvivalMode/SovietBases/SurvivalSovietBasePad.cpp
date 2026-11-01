// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBasePad.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

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
