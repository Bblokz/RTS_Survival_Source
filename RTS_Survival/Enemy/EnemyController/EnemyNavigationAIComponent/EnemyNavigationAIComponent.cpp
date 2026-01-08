// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyNavigationAIComponent.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FEnemyNavigationAIHelpers
{
	constexpr int32 MaxProjectedPoints = 128;
	constexpr float MinSampleDensityScalar = 0.1f;
	constexpr float DoubleExtentMultiplier = 2.f;
	constexpr float DebugDrawDurationSeconds = 6.f;
	constexpr float DebugTextOffsetZ = 60.f;
	constexpr float DebugLineThickness = 2.f;
	constexpr int32 DebugSphereSegments = 12;
	constexpr float DebugPointSize = 12.f;

	FVector GetProjectionExtent(const float ProjectionScale)
	{
		return DeveloperSettings::GamePlay::Navigation::RTSToNavProjectionExtent * ProjectionScale;
	}

	float GetSampleSpacing(const FVector& ProjectionExtent, const float SampleDensityScalar)
	{
		const float SafeDensityScalar = FMath::Max(SampleDensityScalar, MinSampleDensityScalar);
		const float BaseSpacing = FMath::Max(ProjectionExtent.X, ProjectionExtent.Y);
		return BaseSpacing / SafeDensityScalar;
	}

	TArray<FVector> BuildProjectionOffsets(
		const FVector& ProjectionExtent,
		const EOnProjectionFailedStrategy ProjectionFailedStrategy)
	{
		TArray<FVector> Offsets;
		if (ProjectionFailedStrategy == EOnProjectionFailedStrategy::None)
		{
			return Offsets;
		}

		const float ExtentMultiplier = ProjectionFailedStrategy == EOnProjectionFailedStrategy::LookAtDoubleExtent
			? DoubleExtentMultiplier
			: 1.f;
		const FVector OffsetExtent = ProjectionExtent * ExtentMultiplier;
		Offsets.Reserve(4);
		Offsets.Add(FVector(OffsetExtent.X, 0.f, 0.f));
		Offsets.Add(FVector(-OffsetExtent.X, 0.f, 0.f));
		Offsets.Add(FVector(0.f, OffsetExtent.Y, 0.f));
		Offsets.Add(FVector(0.f, -OffsetExtent.Y, 0.f));
		return Offsets;
	}

	TArray<FVector> SamplePointsInBox(
		const FVector& CenterPoint,
		const FVector& BoxExtent,
		const float SampleSpacing,
		const int32 MaxPointsToReturn)
	{
		TArray<FVector> SamplePoints;
		if (MaxPointsToReturn <= 0)
		{
			return SamplePoints;
		}

		const FVector AbsExtent = BoxExtent.GetAbs();
		SamplePoints.Reserve(MaxPointsToReturn);

		for (float XOffset = -AbsExtent.X; XOffset <= AbsExtent.X && SamplePoints.Num() < MaxPointsToReturn;
			XOffset += SampleSpacing)
		{
			for (float YOffset = -AbsExtent.Y; YOffset <= AbsExtent.Y && SamplePoints.Num() < MaxPointsToReturn;
				YOffset += SampleSpacing)
			{
				SamplePoints.Add(CenterPoint + FVector(XOffset, YOffset, 0.f));
			}
		}

		return SamplePoints;
	}

	TArray<FVector> SamplePointsAlongSpline(
		const USplineComponent* SplineComponent,
		const float SampleSpacing)
	{
		TArray<FVector> SamplePoints;
		if (not IsValid(SplineComponent))
		{
			return SamplePoints;
		}

		const float SplineLength = SplineComponent->GetSplineLength();
		for (float Distance = 0.f; Distance <= SplineLength; Distance += SampleSpacing)
		{
			SamplePoints.Add(SplineComponent->GetLocationAtDistanceAlongSpline(
				Distance, ESplineCoordinateSpace::World));
		}

		return SamplePoints;
	}

	TArray<FVector> ProjectDefaultCostPoints(
		ARecastNavMesh* RecastNavMesh,
		const TArray<FVector>& SamplePoints,
		const FVector& ProjectionExtent)
	{
		TArray<FVector> ProjectedPoints;
		if (RecastNavMesh == nullptr)
		{
			return ProjectedPoints;
		}

		TArray<FNavigationProjectionWork> ProjectionWork;
		ProjectionWork.Reserve(SamplePoints.Num());
		for (const FVector& SamplePoint : SamplePoints)
		{
			ProjectionWork.Emplace(SamplePoint, ProjectionExtent);
		}

		RecastNavMesh->BatchProjectPoints(ProjectionWork, ProjectionExtent);

		ProjectedPoints.Reserve(ProjectionWork.Num());
		for (const FNavigationProjectionWork& WorkItem : ProjectionWork)
		{
			if (not WorkItem.bResult)
			{
				continue;
			}

			if (not GetIsDefaultNavAreaAtLocation(RecastNavMesh, WorkItem.Result))
			{
				continue;
			}

			ProjectedPoints.Add(WorkItem.Result.Location);
		}

		return ProjectedPoints;
	}

	bool GetIsDefaultNavAreaAtLocation(
		const ARecastNavMesh* const RecastNavMesh,
		const FNavLocation& NavLocation)
	{
		if (RecastNavMesh == nullptr)
		{
			return false;
		}

		if (not NavLocation.HasNodeRef())
		{
			return false;
		}

		FNavMeshNodeFlags PolyFlags;
		if (not RecastNavMesh->GetPolyFlags(NavLocation.NodeRef, PolyFlags))
		{
			return false;
		}

		const UClass* const AreaClass = RecastNavMesh->GetAreaClass(PolyFlags.Area);
		if (AreaClass == nullptr)
		{
			return false;
		}

		return AreaClass->IsChildOf(UNavArea_Default::StaticClass());
	}
}

UEnemyNavigationAIComponent::UEnemyNavigationAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyNavigationAIComponent::InitNavigationAIComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}

void UEnemyNavigationAIComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheRecastNavMesh();
	CacheRoadSplineActors();
}

bool UEnemyNavigationAIComponent::GetNavigablePoint(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	FVector& OutProjectedLocation,
	const EOnProjectionFailedStrategy ProjectionFailedStrategy) const
{
	OutProjectedLocation = OriginalLocation;

	FNavLocation NavLocation;
	if (TryProjectPointToNavigation(OriginalLocation, ProjectionScale, NavLocation))
	{
		OutProjectedLocation = NavLocation.Location;
		return true;
	}

	if (ProjectionFailedStrategy == EOnProjectionFailedStrategy::None)
	{
		return false;
	}

	if (TryProjectPointWithStrategy(OriginalLocation, ProjectionScale, ProjectionFailedStrategy, NavLocation))
	{
		OutProjectedLocation = NavLocation.Location;
		return true;
	}

	return false;
}

bool UEnemyNavigationAIComponent::GetNavPointDefaultCosts(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	FVector& OutProjectedLocation,
	const EOnProjectionFailedStrategy ProjectionFailedStrategy) const
{
	OutProjectedLocation = OriginalLocation;

	if (not GetIsValidRecastNavMesh())
	{
		return false;
	}

	FNavLocation NavLocation;
	if (TryProjectDefaultCostPointWithStrategy(OriginalLocation, ProjectionScale, ProjectionFailedStrategy, NavLocation))
	{
		OutProjectedLocation = NavLocation.Location;
		return true;
	}

	return false;
}

void UEnemyNavigationAIComponent::FindDefaultNavCostPointsInAreaBetweenTwoPointsAsync(
	const FVector& StartPoint,
	const FVector& EndPoint,
	const FVector& BoxExtent,
	const float SampleDensityScalar,
	const float ProjectionScale,
	const int32 MaxPointsToReturn,
	TFunction<void(const TArray<FVector>&)> OnPointsFound) const
{
	if (not OnPointsFound)
	{
		return;
	}

	if (not GetIsValidRecastNavMesh())
	{
		OnPointsFound({});
		return;
	}

	const FVector CenterPoint = (StartPoint + EndPoint) * 0.5f;
	FVector ProjectedCenter;
	if (not GetNavPointDefaultCosts(
		CenterPoint,
		ProjectionScale,
		ProjectedCenter,
		EOnProjectionFailedStrategy::None))
	{
		OnPointsFound({});
		return;
	}

	const int32 ClampedMaxPoints = FMath::Clamp(MaxPointsToReturn, 0, FEnemyNavigationAIHelpers::MaxProjectedPoints);
	if (ClampedMaxPoints == 0)
	{
		OnPointsFound({});
		return;
	}

	const FVector ProjectionExtent = FEnemyNavigationAIHelpers::GetProjectionExtent(ProjectionScale);
	const float SampleSpacing = FEnemyNavigationAIHelpers::GetSampleSpacing(ProjectionExtent, SampleDensityScalar);
	const TArray<FVector> SamplePoints = FEnemyNavigationAIHelpers::SamplePointsInBox(
		ProjectedCenter,
		BoxExtent,
		SampleSpacing,
		ClampedMaxPoints);

	if (SamplePoints.IsEmpty())
	{
		OnPointsFound({});
		return;
	}

	QueueDefaultCostBatchProjectionAsync(
		SamplePoints,
		ProjectionExtent,
		MoveTemp(OnPointsFound),
		TEXT("FindDefaultNavCostPointsInAreaBetweenTwoPointsAsync"));
}

void UEnemyNavigationAIComponent::FindDefaultNavCostPointsAlongClosestRoadSplineAsync(
	const FVector& StartSearchPoint,
	const float SampleDensityScalar,
	const float ProjectionScale,
	TFunction<void(const TArray<FVector>&)> OnPointsFound) const
{
	if (not OnPointsFound)
	{
		return;
	}

	if (not GetIsValidRecastNavMesh())
	{
		OnPointsFound({});
		return;
	}

	ARoadSplineActor* ClosestRoadSplineActor = GetClosestRoadSplineActor(StartSearchPoint);
	if (not IsValid(ClosestRoadSplineActor))
	{
		OnPointsFound({});
		return;
	}

	const USplineComponent* SplineComponent = ClosestRoadSplineActor->RoadSpline;
	if (not IsValid(SplineComponent))
	{
		OnPointsFound({});
		return;
	}

	const FVector ProjectionExtent = FEnemyNavigationAIHelpers::GetProjectionExtent(ProjectionScale);
	const float SampleSpacing = FEnemyNavigationAIHelpers::GetSampleSpacing(ProjectionExtent, SampleDensityScalar);
	const TArray<FVector> SamplePoints = FEnemyNavigationAIHelpers::SamplePointsAlongSpline(
		SplineComponent,
		SampleSpacing);

	const FBox SplineBounds = ClosestRoadSplineActor->GetComponentsBoundingBox(true);
	DebugProjectionAttempt(SplineBounds.GetCenter(), SplineBounds.GetExtent(), TEXT("RoadSplineBounds"));

	QueueDefaultCostBatchProjectionAsync(
		SamplePoints,
		ProjectionExtent,
		MoveTemp(OnPointsFound),
		TEXT("FindDefaultNavCostPointsAlongClosestRoadSplineAsync"));
}

bool UEnemyNavigationAIComponent::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_EnemyController"),
		TEXT("EnsureEnemyControllerIsValid"),
		this);
	return false;
}

bool UEnemyNavigationAIComponent::GetIsValidRecastNavMesh() const
{
	if (M_RecastNavMesh.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_RecastNavMesh"),
		TEXT("GetIsValidRecastNavMesh"),
		this);
	return false;
}

void UEnemyNavigationAIComponent::CacheRecastNavMesh()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (not IsValid(NavSys))
	{
		return;
	}

	M_RecastNavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
}

void UEnemyNavigationAIComponent::CacheRoadSplineActors()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, ARoadSplineActor::StaticClass(), FoundActors);
	M_RoadSplineActors.Reset(FoundActors.Num());

	for (AActor* FoundActor : FoundActors)
	{
		ARoadSplineActor* RoadSplineActor = Cast<ARoadSplineActor>(FoundActor);
		if (not IsValid(RoadSplineActor))
		{
			continue;
		}
		M_RoadSplineActors.Add(RoadSplineActor);
	}
}

bool UEnemyNavigationAIComponent::TryProjectPointToNavigation(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	FNavLocation& OutNavLocation) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (not IsValid(NavSys))
	{
		return false;
	}

	const FVector ProjectionExtent = FEnemyNavigationAIHelpers::GetProjectionExtent(ProjectionScale);
	DebugProjectionAttempt(OriginalLocation, ProjectionExtent, TEXT("TryProjectPointToNavigation"));

	if (NavSys->ProjectPointToNavigation(OriginalLocation, OutNavLocation, ProjectionExtent))
	{
		DebugProjectionResult(OutNavLocation.Location, true, TEXT("TryProjectPointToNavigation"));
		return true;
	}

	DebugProjectionResult(OriginalLocation, false, TEXT("TryProjectPointToNavigation"));
	return false;
}

bool UEnemyNavigationAIComponent::TryProjectPointWithStrategy(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	const EOnProjectionFailedStrategy ProjectionFailedStrategy,
	FNavLocation& OutNavLocation) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (not IsValid(NavSys))
	{
		return false;
	}

	const FVector ProjectionExtent = FEnemyNavigationAIHelpers::GetProjectionExtent(ProjectionScale);
	const TArray<FVector> ProjectionOffsets = FEnemyNavigationAIHelpers::BuildProjectionOffsets(
		ProjectionExtent,
		ProjectionFailedStrategy);

	for (const FVector& Offset : ProjectionOffsets)
	{
		const FVector OffsetLocation = OriginalLocation + Offset;
		DebugOffsetAttempt(OriginalLocation, OffsetLocation, TEXT("TryProjectPointWithStrategy"));
		if (NavSys->ProjectPointToNavigation(OffsetLocation, OutNavLocation, ProjectionExtent))
		{
			DebugProjectionResult(OutNavLocation.Location, true, TEXT("TryProjectPointWithStrategy"));
			return true;
		}
		DebugProjectionResult(OffsetLocation, false, TEXT("TryProjectPointWithStrategy"));
	}

	return false;
}

bool UEnemyNavigationAIComponent::TryProjectDefaultCostPoint(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	const FString& DebugContext,
	FNavLocation& OutNavLocation) const
{
	if (not GetIsValidRecastNavMesh())
	{
		return false;
	}

	if (not TryProjectPointToNavigation(OriginalLocation, ProjectionScale, OutNavLocation))
	{
		return false;
	}

	const bool bIsDefaultArea = FEnemyNavigationAIHelpers::GetIsDefaultNavAreaAtLocation(
		M_RecastNavMesh.Get(),
		OutNavLocation);
	DebugDefaultAreaResult(OutNavLocation.Location, bIsDefaultArea, DebugContext);
	return bIsDefaultArea;
}

bool UEnemyNavigationAIComponent::TryProjectDefaultCostPointWithStrategy(
	const FVector& OriginalLocation,
	const float ProjectionScale,
	const EOnProjectionFailedStrategy ProjectionFailedStrategy,
	FNavLocation& OutNavLocation) const
{
	if (TryProjectDefaultCostPoint(
		OriginalLocation,
		ProjectionScale,
		TEXT("TryProjectDefaultCostPointWithStrategy"),
		OutNavLocation))
	{
		return true;
	}

	if (ProjectionFailedStrategy == EOnProjectionFailedStrategy::None)
	{
		return false;
	}

	const FVector ProjectionExtent = FEnemyNavigationAIHelpers::GetProjectionExtent(ProjectionScale);
	const TArray<FVector> ProjectionOffsets = FEnemyNavigationAIHelpers::BuildProjectionOffsets(
		ProjectionExtent,
		ProjectionFailedStrategy);

	for (const FVector& Offset : ProjectionOffsets)
	{
		const FVector OffsetLocation = OriginalLocation + Offset;
		DebugOffsetAttempt(OriginalLocation, OffsetLocation, TEXT("TryProjectDefaultCostPointWithStrategy"));
		if (TryProjectDefaultCostPoint(
			OffsetLocation,
			ProjectionScale,
			TEXT("TryProjectDefaultCostPointWithStrategy"),
			OutNavLocation))
		{
			DebugProjectionResult(OutNavLocation.Location, true, TEXT("TryProjectDefaultCostPointWithStrategy"));
			return true;
		}
	}

	return false;
}

ARoadSplineActor* UEnemyNavigationAIComponent::GetClosestRoadSplineActor(const FVector& SearchPoint) const
{
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	ARoadSplineActor* ClosestRoadSpline = nullptr;

	for (const TWeakObjectPtr<ARoadSplineActor>& RoadSplineActor : M_RoadSplineActors)
	{
		if (not RoadSplineActor.IsValid())
		{
			continue;
		}

		const USplineComponent* SplineComponent = RoadSplineActor->RoadSpline;
		if (not IsValid(SplineComponent))
		{
			continue;
		}

		const float InputKey = SplineComponent->FindInputKeyClosestToWorldLocation(SearchPoint);
		const FVector ClosestPoint = SplineComponent->GetLocationAtSplineInputKey(
			InputKey,
			ESplineCoordinateSpace::World);
		const float DistanceSquared = FVector::DistSquared(SearchPoint, ClosestPoint);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestRoadSpline = RoadSplineActor.Get();
		}
	}

	return ClosestRoadSpline;
}

void UEnemyNavigationAIComponent::QueueDefaultCostBatchProjectionAsync(
	const TArray<FVector>& SamplePoints,
	const FVector& ProjectionExtent,
	TFunction<void(const TArray<FVector>&)> OnPointsFound,
	const FString& DebugContext) const
{
	TWeakObjectPtr<UEnemyNavigationAIComponent> WeakThis = this;
	TWeakObjectPtr<ARecastNavMesh> WeakRecastNavMesh = M_RecastNavMesh;
	TFunction<void(const TArray<FVector>&)> PointsFoundCallback = MoveTemp(OnPointsFound);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, WeakRecastNavMesh, ProjectionExtent, SamplePoints, PointsFoundCallback, DebugContext]() mutable
		{
			if (not WeakRecastNavMesh.IsValid())
			{
				AsyncTask(ENamedThreads::GameThread, [PointsFoundCallback]()
				{
					PointsFoundCallback({});
				});
				return;
			}

			const TArray<FVector> ProjectedPoints = FEnemyNavigationAIHelpers::ProjectDefaultCostPoints(
				WeakRecastNavMesh.Get(),
				SamplePoints,
				ProjectionExtent);

			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, PointsFoundCallback, ProjectedPoints, DebugContext]() mutable
				{
					PointsFoundCallback(ProjectedPoints);
					if (WeakThis.IsValid())
					{
						WeakThis->DebugBatchResult(ProjectedPoints, DebugContext);
					}
				});
		});
}

void UEnemyNavigationAIComponent::DebugProjectionAttempt(
	const FVector& Location,
	const FVector& Extent,
	const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		const FVector TextLocation = Location + FVector(0.f, 0.f, Extent.Z + FEnemyNavigationAIHelpers::DebugTextOffsetZ);
		DrawDebugSphere(
			GetWorld(),
			Location,
			Extent.GetMax(),
			FEnemyNavigationAIHelpers::DebugSphereSegments,
			FColor::Yellow,
			false,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds);
		DrawDebugString(
			GetWorld(),
			TextLocation,
			Context,
			nullptr,
			FColor::Yellow,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
			false);
	}
}

void UEnemyNavigationAIComponent::DebugProjectionResult(
	const FVector& Location,
	const bool bSuccess,
	const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		const FColor ResultColor = bSuccess ? FColor::Green : FColor::Red;
		DrawDebugPoint(
			GetWorld(),
			Location,
			FEnemyNavigationAIHelpers::DebugPointSize,
			ResultColor,
			false,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds);
		DrawDebugString(
			GetWorld(),
			Location + FVector(0.f, 0.f, FEnemyNavigationAIHelpers::DebugTextOffsetZ),
			Context,
			nullptr,
			ResultColor,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
			false);
	}
}

void UEnemyNavigationAIComponent::DebugOffsetAttempt(
	const FVector& OriginalLocation,
	const FVector& OffsetLocation,
	const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		DrawDebugLine(
			GetWorld(),
			OriginalLocation,
			OffsetLocation,
			FColor::Cyan,
			false,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
			0,
			FEnemyNavigationAIHelpers::DebugLineThickness);
		DrawDebugString(
			GetWorld(),
			OffsetLocation + FVector(0.f, 0.f, FEnemyNavigationAIHelpers::DebugTextOffsetZ),
			Context,
			nullptr,
			FColor::Cyan,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
			false);
	}
}

void UEnemyNavigationAIComponent::DebugDefaultAreaResult(
	const FVector& Location,
	const bool bIsDefaultArea,
	const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		const FColor ResultColor = bIsDefaultArea ? FColor::Emerald : FColor::Orange;
		DrawDebugPoint(
			GetWorld(),
			Location,
			FEnemyNavigationAIHelpers::DebugPointSize,
			ResultColor,
			false,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds);
		DrawDebugString(
			GetWorld(),
			Location + FVector(0.f, 0.f, FEnemyNavigationAIHelpers::DebugTextOffsetZ),
			Context,
			nullptr,
			ResultColor,
			FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
			false);
	}
}

void UEnemyNavigationAIComponent::DebugBatchResult(const TArray<FVector>& Points, const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		for (const FVector& Point : Points)
		{
			DrawDebugPoint(
				GetWorld(),
				Point,
				FEnemyNavigationAIHelpers::DebugPointSize,
				FColor::Purple,
				false,
				FEnemyNavigationAIHelpers::DebugDrawDurationSeconds);
			DrawDebugString(
				GetWorld(),
				Point + FVector(0.f, 0.f, FEnemyNavigationAIHelpers::DebugTextOffsetZ),
				Context,
				nullptr,
				FColor::Purple,
				FEnemyNavigationAIHelpers::DebugDrawDurationSeconds,
				false);
		}
	}
}
