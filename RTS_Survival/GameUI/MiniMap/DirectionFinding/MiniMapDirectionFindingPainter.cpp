// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MiniMapDirectionFindingPainter.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingManager.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingSubsystem.h"

namespace MiniMapDirectionFindingConstants
{
	// Wedge fills, wedge edges, overlap fill and overlap edge each take a layer.
	constexpr int32 LayerCount = 5;
}

namespace MiniMapDirectionFindingPrivate
{
	const ADirectionFindingManager* FindStartedManager(const UWorld* World)
	{
		const UDirectionFindingSubsystem* const Subsystem = IsValid(World)
			                                                    ? World->GetSubsystem<UDirectionFindingSubsystem>()
			                                                    : nullptr;
		const ADirectionFindingManager* const Manager = IsValid(Subsystem)
			                                                ? Subsystem->GetDirectionFindingManager()
			                                                : nullptr;
		return IsValid(Manager) && Manager->GetHasStarted() ? Manager : nullptr;
	}
}

bool FMiniMapDirectionFindingPainter::GetHasWedges(const UWorld* World)
{
	return MiniMapDirectionFindingPrivate::FindStartedManager(World) != nullptr;
}

int32 FMiniMapDirectionFindingPainter::Paint(const UWorld* World, const AFowManager& FowManager,
                                              const FGeometry& MiniMapGeometry, const FGeometry& AllottedGeometry,
                                              const bool bIsTacticalMap, FSlateWindowElementList& OutDrawElements,
                                              const int32 LayerId)
{
	const ADirectionFindingManager* const Manager = MiniMapDirectionFindingPrivate::FindStartedManager(World);
	if (Manager == nullptr)
	{
		return LayerId;
	}
	const double MapSize = FowManager.GetMapExtent() * 2.0;
	if (MapSize <= 0.0 || MiniMapGeometry.GetLocalSize().X <= 0.f)
	{
		return LayerId;
	}
	const FVector2D MapOrigin(FowManager.GetActorLocation());
	const FBox2D MapBounds(MapOrigin - FVector2D(MapSize * 0.5), MapOrigin + FVector2D(MapSize * 0.5));
	FDirectionFindingDrawData DrawData;
	Manager->GetDrawData(MapBounds, DrawData);
	if (DrawData.Wedges.IsEmpty())
	{
		return LayerId;
	}

	const FDirectionFindingLook& Look = Manager->GetLook();
	const float Thickness = bIsTacticalMap ? Look.TacticalMapEdgeThickness : Look.EdgeThickness;
	for (const FDirectionFindingWedgeDraw& Wedge : DrawData.Wedges)
	{
		const TArray<FVector2f> LocalPolygon = ToLocal(Wedge.Polygon, MapOrigin, MapSize, MiniMapGeometry,
		                                               AllottedGeometry);
		FLinearColor Fill = Wedge.bHeld ? Look.HeldFill : Look.LostFill;
		if (Wedge.bHeld)
		{
			Fill.A = FMath::Min(1.f, Fill.A + Look.PulseFillAlpha * Wedge.Pulse);
		}
		PaintFill(LocalPolygon, Fill, AllottedGeometry, OutDrawElements, LayerId + 1);
		PaintEdge(LocalPolygon, Wedge.bHeld ? Look.HeldEdge : Look.LostEdge, Thickness, AllottedGeometry,
		          OutDrawElements, LayerId + 2);
	}
	// The decoy's overlap is shaded exactly like the real one: the player has to find out which one lies.
	for (const TArray<FVector2D>* const OverlapPolygon : {&DrawData.Overlap, &DrawData.DecoyOverlap})
	{
		if (OverlapPolygon->IsEmpty())
		{
			continue;
		}
		const TArray<FVector2f> LocalOverlap = ToLocal(*OverlapPolygon, MapOrigin, MapSize, MiniMapGeometry,
		                                               AllottedGeometry);
		PaintFill(LocalOverlap, Look.OverlapFill, AllottedGeometry, OutDrawElements, LayerId + 3);
		PaintEdge(LocalOverlap, Look.OverlapEdge, Thickness, AllottedGeometry, OutDrawElements, LayerId + 4);
	}
	return LayerId + MiniMapDirectionFindingConstants::LayerCount;
}

void FMiniMapDirectionFindingPainter::PaintFill(const TArray<FVector2f>& LocalPolygon, const FLinearColor& Color,
                                                const FGeometry& AllottedGeometry,
                                                FSlateWindowElementList& OutDrawElements, const int32 LayerId)
{
	if (LocalPolygon.Num() < 3 || not FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateRenderer* const Renderer = FSlateApplication::Get().GetRenderer();
	const FSlateBrush* const WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (Renderer == nullptr || WhiteBrush == nullptr)
	{
		return;
	}
	// Custom vertices are in window space: the local points go through the accumulated render transform. The
	// polygon is convex, so a fan from its first point fills it.
	const FSlateRenderTransform& RenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
	const FColor VertexColor = Color.ToFColorSRGB();
	TArray<FSlateVertex> Vertices;
	Vertices.Reserve(LocalPolygon.Num());
	for (const FVector2f& Point : LocalPolygon)
	{
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			RenderTransform, Point, FVector2f::ZeroVector, VertexColor));
	}
	TArray<SlateIndex> Indices;
	for (int32 PointIndex = 1; PointIndex + 1 < LocalPolygon.Num(); ++PointIndex)
	{
		Indices.Add(0);
		Indices.Add(static_cast<SlateIndex>(PointIndex));
		Indices.Add(static_cast<SlateIndex>(PointIndex + 1));
	}
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Renderer->GetResourceHandle(*WhiteBrush), Vertices,
	                                   Indices, nullptr, 0, 0);
}

void FMiniMapDirectionFindingPainter::PaintEdge(const TArray<FVector2f>& LocalPolygon, const FLinearColor& Color,
                                                const float Thickness, const FGeometry& AllottedGeometry,
                                                FSlateWindowElementList& OutDrawElements, const int32 LayerId)
{
	if (LocalPolygon.Num() < 2)
	{
		return;
	}
	TArray<FVector2f> Loop = LocalPolygon;
	Loop.Add(LocalPolygon[0]);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Loop),
	                             ESlateDrawEffect::None, Color, true, Thickness);
}

TArray<FVector2f> FMiniMapDirectionFindingPainter::ToLocal(const TArray<FVector2D>& WorldPolygon,
                                                           const FVector2D& MapOrigin, const double MapSize,
                                                           const FGeometry& MiniMapGeometry,
                                                           const FGeometry& AllottedGeometry)
{
	// The same mapping as AFowManager::GetMiniMapUVFromWorldLocation, without its rejection of off-map points.
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	TArray<FVector2f> LocalPolygon;
	LocalPolygon.Reserve(WorldPolygon.Num());
	for (const FVector2D& WorldPoint : WorldPolygon)
	{
		const FVector2D UV = (WorldPoint - MapOrigin) / MapSize + FVector2D(0.5, 0.5);
		const FVector2D Absolute = MiniMapGeometry.LocalToAbsolute(UV * MiniMapSize);
		LocalPolygon.Add(FVector2f(AllottedGeometry.AbsoluteToLocal(Absolute)));
	}
	return LocalPolygon;
}
