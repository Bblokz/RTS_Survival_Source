// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyNavigationAIComponent.h"

#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "NavAreas/NavArea_Default.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
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
	constexpr float MinimumSampleSpacing = 100.f;

	FVector GetProjectionExtent(const float ProjectionScale)
	{
		return DeveloperSettings::GamePlay::Navigation::RTSToNavProjectionExtent * FMath::Abs(ProjectionScale);
	}

	float GetSampleSpacing(const FVector& ProjectionExtent, const float SampleDensityScalar)
	{
		const float SafeDensityScalar = FMath::Max(SampleDensityScalar, MinSampleDensityScalar);
		const FVector AbsoluteProjectionExtent = ProjectionExtent.GetAbs();
		const float BaseSpacing = FMath::Max(AbsoluteProjectionExtent.X, AbsoluteProjectionExtent.Y);
		return FMath::Max(MinimumSampleSpacing, BaseSpacing / SafeDensityScalar);
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


	TArray<FVector> ProjectDefaultCostPoints(
		ARecastNavMesh* RecastNavMesh,
		const TArray<FVector>& SamplePoints,
		const FVector& ProjectionExtent)
	{
		TArray<FVector> ProjectedPoints;
		if (not IsInGameThread())
		{
			UE_LOG(LogTemp, Error,
				TEXT("Enemy navigation default-cost batch projection attempted to access Recast off the game thread."));
			return ProjectedPoints;
		}

		if (not IsValid(RecastNavMesh) || SamplePoints.IsEmpty())
		{
			return ProjectedPoints;
		}

		TArray<FNavigationProjectionWork> ProjectionWork;
		ProjectionWork.Reserve(SamplePoints.Num());

		for (const FVector& SamplePoint : SamplePoints)
		{
			// 2nd ctor param is FBox (custom limits), NOT FVector extent.
			// Rely on the shared ProjectionExtent passed to BatchProjectPoints.
			ProjectionWork.Emplace(SamplePoint);
		}

		RecastNavMesh->BatchProjectPoints(ProjectionWork, ProjectionExtent);

		ProjectedPoints.Reserve(ProjectionWork.Num());

		for (const FNavigationProjectionWork& WorkItem : ProjectionWork)
		{
			if (not WorkItem.bIsValid || not WorkItem.bResult)
			{
				continue;
			}

			const FNavLocation& ProjectedNavLocation = WorkItem.OutLocation;

			if (not GetIsDefaultNavAreaAtLocation(RecastNavMesh, ProjectedNavLocation))
			{
				continue;
			}

			ProjectedPoints.Add(ProjectedNavLocation.Location);
		}

		return ProjectedPoints;
	}
}

UEnemyNavigationAIComponent::UEnemyNavigationAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CacheGenerationSeedFromGameInstance();
}

void UEnemyNavigationAIComponent::InitNavigationAIComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
}

void UEnemyNavigationAIComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bind the owning controller at runtime, before any of this component's logic uses it. The
	// constructor-time InitNavigationAIComponent(this) captures the class default object for a placed
	// controller (the owner reference deserializes back to the CDO), so it must be re-established here.
	M_EnemyController = Cast<AEnemyController>(GetOwner());

	CacheGenerationSeedFromGameInstance();
	CacheRecastNavMesh();
	EnsureRoadSplineActorsCachedAndPropagated();
}

void UEnemyNavigationAIComponent::CacheGenerationSeedFromGameInstance()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UGameInstance* const GameInstance = World->GetGameInstance();
	URTSGameInstance* const RTSGameInstance = Cast<URTSGameInstance>(GameInstance);
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError("Enemy navigation AI component could not cache generation seed.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}

int32 UEnemyNavigationAIComponent::GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt) const
{
	if (OptionCount <= 0)
	{
		return INDEX_NONE;
	}

	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.RandRange(0, OptionCount - 1);
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
	if (TryProjectDefaultCostPointWithStrategy(OriginalLocation, ProjectionScale, ProjectionFailedStrategy,
	                                           NavLocation))
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
	TFunction<void(const TArray<FVector>&)> OnPointsFound)
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
	TFunction<void(const TArray<FVector>&)> OnPointsFound)
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
	// The cached controller must be this component's actual owner; a mismatch means a stale reference
	// (e.g. the class default object captured at construction) and is treated as invalid.
	const AEnemyController* EnemyController = M_EnemyController.Get();
	if (IsValid(EnemyController) && EnemyController == GetOwner())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
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

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
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

void UEnemyNavigationAIComponent::EnsureRoadSplineActorsCachedAndPropagated()
{
	constexpr int32 MaxRoadSplineCacheRetryAttempts = 60;
	constexpr float RoadSplineCacheRetryIntervalSeconds = 1.f;

	CacheRoadSplineActors();
	PropagateRoadSplineActorsToEnemyAIBlackBoard();

	// Success is measured by the strategic blackboard actually holding the splines, not by the local cache:
	// caching can return zero before the level's placed road spline actors register, and propagation can
	// no-op if the strategic component is not ready yet. Retry until the blackboard is populated.
	if (GetBlackboardHasRoadSplines() || M_RoadSplineCacheRetryAttempts >= MaxRoadSplineCacheRetryAttempts)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_RoadSplineCacheRetryAttempts++;
	World->GetTimerManager().SetTimer(
		M_RoadSplineCacheRetryTimerHandle,
		this,
		&UEnemyNavigationAIComponent::EnsureRoadSplineActorsCachedAndPropagated,
		RoadSplineCacheRetryIntervalSeconds,
		false);
}

void UEnemyNavigationAIComponent::PropagateRoadSplineActorsToEnemyAIBlackBoard() const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}
	AEnemyController* EnemyController = M_EnemyController.Get();
	UEnemyStrategicAIComponent* StrategicAIComponent = EnemyController->GetEnemyStrategicAIComponent();
	if(not IsValid(StrategicAIComponent))
	{
		RTSFunctionLibrary::ReportError("Could not get valid enemy strategic ai componenet to cache road spline actors"
								  "at begin play in UEnemyNavigationAIComponent!");
		return;
	}
	// Rebuild the blackboard's spline list so repeated propagation attempts stay idempotent.
	FStrategicAIBlackboard& Blackboard = StrategicAIComponent->GetEditableStrategicAIBlackboard();
	Blackboard.RoadSplineActors.Reset();
	for(auto EachSpline :M_RoadSplineActors)
	{
		if(not EachSpline.IsValid())
		{
			continue;
		}

	Blackboard.RoadSplineActors.Add(EachSpline.Get());

	}
}

bool UEnemyNavigationAIComponent::GetBlackboardHasRoadSplines() const
{
	if (not M_EnemyController.IsValid())
	{
		return false;
	}

	const UEnemyStrategicAIComponent* StrategicAIComponent = M_EnemyController->GetEnemyStrategicAIComponent();
	return IsValid(StrategicAIComponent)
		&& StrategicAIComponent->GetStrategicAIBlackboard().RoadSplineActors.Num() > 0;
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
	const FString& DebugContext)
{
	const TWeakObjectPtr<UEnemyNavigationAIComponent> WeakThis(this);
	TFunction<void(const TArray<FVector>&)> PointsFoundCallback = MoveTemp(OnPointsFound);

	AsyncTask(
		ENamedThreads::GameThread,
		[WeakThis,
		 ProjectionExtent,
		 SamplePoints,
		 PointsFoundCallback = MoveTemp(PointsFoundCallback),
		 DebugContext]() mutable
		{
			UEnemyNavigationAIComponent* NavigationComponent = WeakThis.Get();
			UWorld* World = IsValid(NavigationComponent) ? NavigationComponent->GetWorld() : nullptr;
			if (not IsValid(NavigationComponent)
				|| not NavigationComponent->HasBegunPlay()
				|| not IsValid(World)
				|| World->bIsTearingDown
				|| not NavigationComponent->GetIsValidRecastNavMesh())
			{
				if (PointsFoundCallback)
				{
					PointsFoundCallback({});
				}
				return;
			}

			const TArray<FVector> ProjectedPoints = FEnemyNavigationAIHelpers::ProjectDefaultCostPoints(
				NavigationComponent->M_RecastNavMesh.Get(),
				SamplePoints,
				ProjectionExtent);
			if (PointsFoundCallback)
			{
				PointsFoundCallback(ProjectedPoints);
			}

			NavigationComponent = WeakThis.Get();
			World = IsValid(NavigationComponent) ? NavigationComponent->GetWorld() : nullptr;
			if (not IsValid(NavigationComponent) || not IsValid(World) || World->bIsTearingDown)
			{
				return;
			}

			NavigationComponent->DebugBatchResult(ProjectedPoints, DebugContext);
		});
}

void UEnemyNavigationAIComponent::DebugProjectionAttempt(
	const FVector& Location,
	const FVector& Extent,
	const FString& Context) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_NavDetector_DebugSymbols)
	{
		const FVector TextLocation = Location + FVector(
			0.f, 0.f, Extent.Z + FEnemyNavigationAIHelpers::DebugTextOffsetZ);
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
