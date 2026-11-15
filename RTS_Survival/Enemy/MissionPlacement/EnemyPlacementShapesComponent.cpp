// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyPlacementShapesComponent.h"

#include "HAL/IConsoleManager.h"
#include "SceneView.h"

namespace EnemyPlacementShapesConstants
{
	constexpr int32 CircleSegments = 48;
	constexpr float MinArcSegmentDegrees = 7.5f;
	constexpr float BoundsPadding = 1500.f;

	constexpr int32 GizmosHidden = 0;
	constexpr int32 GizmosDetailWhenSelected = 1;
	constexpr int32 GizmosAllDetail = 2;
}

static TAutoConsoleVariable<int32> CVarEnemyPlacementGizmos(
	TEXT("RTS.EnemyPlacement.Gizmos"),
	EnemyPlacementShapesConstants::GizmosDetailWhenSelected,
	TEXT("Editor drawing of placed enemy wave spawners, convoy routes and strongpoints.\n")
	TEXT("0: hidden. 1: summary always, detail for the selected actor (default). 2: all detail always."),
	ECVF_RenderThreadSafe);

namespace
{
	/** Draws only in views with the Editor show flag (level viewports), never in game or PIE views. */
	class FEnemyPlacementSceneProxy final : public FDebugRenderSceneProxy
	{
	public:
		FEnemyPlacementSceneProxy(const UPrimitiveComponent* InComponent, const EEnemyPlacementShapesLayer InLayer)
			: FDebugRenderSceneProxy(InComponent)
			, M_Layer(InLayer)
		{
			ViewFlagName = TEXT("Editor");
			ViewFlagIndex = static_cast<uint32>(FEngineShowFlags::FindIndexByName(*ViewFlagName));
			DrawType = SolidAndWireMeshes;
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			const bool bIsEditorView = ViewFlagIndex != static_cast<uint32>(INDEX_NONE)
				&& View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex) && IsShown(View);
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = bIsEditorView && GetIsLayerShown();
			Result.bDynamicRelevance = true;
			Result.bSeparateTranslucency = Result.bNormalTranslucency = Result.bDrawRelevance;
			return Result;
		}

	private:
		EEnemyPlacementShapesLayer M_Layer;

		bool GetIsLayerShown() const
		{
			using namespace EnemyPlacementShapesConstants;
			const int32 Mode = CVarEnemyPlacementGizmos.GetValueOnRenderThread();
			if (Mode <= GizmosHidden)
			{
				return false;
			}
			if (M_Layer == EEnemyPlacementShapesLayer::Summary || Mode >= GizmosAllDetail)
			{
				return true;
			}
			return IsSelected();
		}
	};

	FVector GetFlatDirection(const FVector& Direction)
	{
		const FVector Flat(Direction.X, Direction.Y, 0.f);
		return Flat.IsNearlyZero() ? FVector::ForwardVector : Flat.GetSafeNormal();
	}
}

void FEnemyPlacementShapes::AddCircle(const FVector& Center, const float Radius, const FColor& Color,
                                      const float Thickness)
{
	AddArc(Center, Radius, FVector::ForwardVector, 360.f, Color, Thickness);
}

void FEnemyPlacementShapes::AddArc(const FVector& Center, const float Radius, const FVector& CentreDirection,
                                   const float ArcDegrees, const FColor& Color, const float Thickness)
{
	using namespace EnemyPlacementShapesConstants;
	if (Radius <= 0.f || ArcDegrees <= 0.f)
	{
		return;
	}
	const float ClampedArc = FMath::Min(ArcDegrees, 360.f);
	const int32 Segments = FMath::Max(2, FMath::CeilToInt(ClampedArc / FMath::Max(360.f / CircleSegments,
		MinArcSegmentDegrees)));
	const float StartYaw = GetFlatDirection(CentreDirection).Rotation().Yaw - ClampedArc * 0.5f;
	const float StepDegrees = ClampedArc / Segments;
	FVector Previous = Center + FRotator(0.f, StartYaw, 0.f).Vector() * Radius;
	for (int32 Segment = 1; Segment <= Segments; ++Segment)
	{
		const FVector Next = Center + FRotator(0.f, StartYaw + Segment * StepDegrees, 0.f).Vector() * Radius;
		Lines.Emplace(Previous, Next, Color, Thickness);
		Previous = Next;
	}
}

void FEnemyPlacementShapes::AddFlatBox(const FVector& Center, const FVector& HalfExtent, const float Yaw,
                                       const FColor& Color, const bool bSolid)
{
	const FTransform BoxTransform(FRotator(0.f, Yaw, 0.f), Center);
	Boxes.Emplace(FBox(-HalfExtent, HalfExtent), Color, BoxTransform,
	              bSolid ? FDebugRenderSceneProxy::EDrawType::SolidAndWireMeshes
	                     : FDebugRenderSceneProxy::EDrawType::WireMesh);
}

FBox FEnemyPlacementShapes::GetBounds() const
{
	FBox Bounds(ForceInit);
	for (const FDebugRenderSceneProxy::FDebugLine& Line : Lines)
	{
		Bounds += Line.Start;
		Bounds += Line.End;
	}
	for (const FDebugRenderSceneProxy::FDashedLine& Line : DashedLines)
	{
		Bounds += Line.Start;
		Bounds += Line.End;
	}
	for (const FDebugRenderSceneProxy::FArrowLine& Line : ArrowLines)
	{
		Bounds += Line.Start;
		Bounds += Line.End;
	}
	for (const FDebugRenderSceneProxy::FSphere& Sphere : Spheres)
	{
		Bounds += FBox::BuildAABB(Sphere.Location, FVector(Sphere.Radius));
	}
	for (const FDebugRenderSceneProxy::FWireCylinder& Cylinder : Cylinders)
	{
		Bounds += FBox::BuildAABB(Cylinder.Base, FVector(Cylinder.Radius, Cylinder.Radius, Cylinder.HalfHeight));
	}
	for (const FDebugRenderSceneProxy::FDebugBox& Box : Boxes)
	{
		Bounds += Box.Box.TransformBy(Box.Transform);
	}
	return Bounds;
}

UEnemyPlacementShapesComponent::UEnemyPlacementShapesComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetHiddenInGame(true);
	SetCastShadow(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bIsEditorOnly = true;
	// Needed for the proxy to know its actor is selected (detail layer); clicking a shape selects the actor.
	bSelectable = true;
}

void UEnemyPlacementShapesComponent::SetShapes(FEnemyPlacementShapes&& Shapes)
{
	M_Shapes = MoveTemp(Shapes);
	UpdateBounds();
	MarkRenderStateDirty();
}

FDebugRenderSceneProxy* UEnemyPlacementShapesComponent::CreateDebugSceneProxy()
{
	FEnemyPlacementSceneProxy* Proxy = new FEnemyPlacementSceneProxy(this, M_Layer);
	Proxy->Lines = M_Shapes.Lines;
	Proxy->DashedLines = M_Shapes.DashedLines;
	Proxy->ArrowLines = M_Shapes.ArrowLines;
	Proxy->Spheres = M_Shapes.Spheres;
	Proxy->Cylinders = M_Shapes.Cylinders;
	Proxy->Boxes = M_Shapes.Boxes;
	return Proxy;
}

FBoxSphereBounds UEnemyPlacementShapesComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox ShapeBounds = M_Shapes.GetBounds();
	ShapeBounds += LocalToWorld.GetLocation();
	return FBoxSphereBounds(ShapeBounds.ExpandBy(EnemyPlacementShapesConstants::BoundsPadding));
}
