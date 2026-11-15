// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyWaveSpawnerVisualComponent.h"

#include "SceneView.h"

namespace EnemyWaveSpawnerVisualConstants
{
	constexpr float SpawnPointRadius = 120.f;
	constexpr float SpawnPointStemHeight = 400.f;
	constexpr float WaypointRadius = 70.f;
	constexpr float PathLiftZ = 60.f;
	constexpr float PathThickness = 6.f;
	constexpr float OriginLinkThickness = 1.5f;
	constexpr float PatrolDashSize = 120.f;
	constexpr float GuardRingHalfHeight = 2.f;
	constexpr float FinalDirectionLength = 900.f;
	constexpr float BoundsPadding = 1500.f;
}

namespace
{
	/** Draws only in views with the Editor show flag (level viewports), never in game or PIE views. */
	class FEnemyWaveSpawnerSceneProxy final : public FDebugRenderSceneProxy
	{
	public:
		explicit FEnemyWaveSpawnerSceneProxy(const UPrimitiveComponent* InComponent)
			: FDebugRenderSceneProxy(InComponent)
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
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = ViewFlagIndex != static_cast<uint32>(INDEX_NONE)
				&& View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex) && IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View);
			return Result;
		}
	};

	FVector Lift(const FVector& Point)
	{
		return Point + FVector(0.f, 0.f, EnemyWaveSpawnerVisualConstants::PathLiftZ);
	}

	FVector GetPointsCentre(const TArray<FVector>& Points, const FVector& Fallback)
	{
		if (Points.IsEmpty())
		{
			return Fallback;
		}
		FVector Sum = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Sum += Point;
		}
		return Sum / Points.Num();
	}

	void AddSpawnPoints(FDebugRenderSceneProxy& Proxy, const FEnemyWaveSpawnerVisualData& Data)
	{
		using namespace EnemyWaveSpawnerVisualConstants;
		for (const FVector& SpawnPoint : Data.SpawnPoints)
		{
			Proxy.Spheres.Emplace(SpawnPointRadius, SpawnPoint, FLinearColor(Data.Color));
			Proxy.Lines.Emplace(SpawnPoint, SpawnPoint + FVector(0.f, 0.f, SpawnPointStemHeight), Data.Color,
			                    PathThickness);
			Proxy.Lines.Emplace(Data.Origin, SpawnPoint, Data.Color, OriginLinkThickness);
		}
	}

	void AddAttackPath(FDebugRenderSceneProxy& Proxy, const FEnemyWaveSpawnerVisualData& Data)
	{
		using namespace EnemyWaveSpawnerVisualConstants;
		FVector Previous = GetPointsCentre(Data.SpawnPoints, Data.Origin);
		for (const FVector& Waypoint : Data.Waypoints)
		{
			Proxy.ArrowLines.Emplace(Lift(Previous), Lift(Waypoint), Data.Color);
			Proxy.Lines.Emplace(Lift(Previous), Lift(Waypoint), Data.Color, PathThickness);
			Proxy.Spheres.Emplace(WaypointRadius, Waypoint, FLinearColor(Data.Color),
			                      FDebugRenderSceneProxy::EDrawType::WireMesh);
			Previous = Waypoint;
		}
		if (Data.bHasFinalDirection && not Data.Waypoints.IsEmpty())
		{
			const FVector Final = Lift(Data.Waypoints.Last());
			Proxy.ArrowLines.Emplace(Final, Final + Data.FinalDirection * FinalDirectionLength, FColor::White);
		}
	}

	void AddPatrolLoop(FDebugRenderSceneProxy& Proxy, const FEnemyWaveSpawnerVisualData& Data)
	{
		using namespace EnemyWaveSpawnerVisualConstants;
		if (not Data.Waypoints.IsEmpty())
		{
			// The patrol walks from its spawn to its first point; the points are then visited at random.
			const FVector SpawnCentre = GetPointsCentre(Data.SpawnPoints, Data.Origin);
			Proxy.ArrowLines.Emplace(Lift(SpawnCentre), Lift(Data.Waypoints[0]), Data.Color);
		}
		for (int32 PointIndex = 0; PointIndex < Data.Waypoints.Num(); ++PointIndex)
		{
			const FVector& Point = Data.Waypoints[PointIndex];
			const FVector& Next = Data.Waypoints[(PointIndex + 1) % Data.Waypoints.Num()];
			if (Data.Waypoints.Num() > 1)
			{
				Proxy.DashedLines.Emplace(Lift(Point), Lift(Next), Data.Color, PatrolDashSize);
			}
			Proxy.Spheres.Emplace(WaypointRadius, Point, FLinearColor(Data.Color),
			                      FDebugRenderSceneProxy::EDrawType::WireMesh);
			if (Data.GuardRadius > 0.f)
			{
				Proxy.Cylinders.Emplace(Lift(Point), FVector::UpVector, Data.GuardRadius, GuardRingHalfHeight,
				                        Data.Color, FDebugRenderSceneProxy::EDrawType::WireMesh);
			}
		}
	}
}

UEnemyWaveSpawnerVisualComponent::UEnemyWaveSpawnerVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetHiddenInGame(true);
	SetCastShadow(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bIsEditorOnly = true;
	bSelectable = false;
}

void UEnemyWaveSpawnerVisualComponent::SetVisualData(const FEnemyWaveSpawnerVisualData& VisualData)
{
	M_VisualData = VisualData;
	UpdateBounds();
	MarkRenderStateDirty();
}

FDebugRenderSceneProxy* UEnemyWaveSpawnerVisualComponent::CreateDebugSceneProxy()
{
	FEnemyWaveSpawnerSceneProxy* Proxy = new FEnemyWaveSpawnerSceneProxy(this);
	AddSpawnPoints(*Proxy, M_VisualData);
	if (M_VisualData.bIsPatrol)
	{
		AddPatrolLoop(*Proxy, M_VisualData);
	}
	else
	{
		AddAttackPath(*Proxy, M_VisualData);
	}
	return Proxy;
}

FBoxSphereBounds UEnemyWaveSpawnerVisualComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox Box(ForceInit);
	Box += LocalToWorld.GetLocation();
	Box += M_VisualData.Origin;
	for (const FVector& SpawnPoint : M_VisualData.SpawnPoints)
	{
		Box += SpawnPoint;
	}
	for (const FVector& Waypoint : M_VisualData.Waypoints)
	{
		Box += Waypoint;
	}
	return FBoxSphereBounds(Box.ExpandBy(EnemyWaveSpawnerVisualConstants::BoundsPadding));
}
