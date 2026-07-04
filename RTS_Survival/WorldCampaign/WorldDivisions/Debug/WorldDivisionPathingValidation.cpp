// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CoreMinimal.h"

#include "Algo/Sort.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldSquadDivision.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	constexpr float ValidationBoundarySampleSpacing = 250.f;
	constexpr int32 DefaultValidationSeed = 0;
	constexpr int32 DefaultValidationDivisionCount = 8;
	constexpr int32 MinimumValidationDivisionCount = 2;
	constexpr int32 AlternatingDivisionClassCount = 2;
	constexpr int32 TargetAnchorStride = 3;
	constexpr float SpawnOffsetRadiusScale = 2.5f;
	constexpr float MinSpawnOffset = 250.f;
	constexpr float FallbackAngleStepRadians = UE_PI / 3.f;
	constexpr float ValidationDistanceTolerance = 0.5f;
	constexpr float ValidationPathSampleSpacing = 100.f;
	constexpr int32 ValidationPathMinSampleCount = 8;
	constexpr int32 ValidationPathMaxSampleCount = 256;
	constexpr int32 DefaultBenchmarkPreviewCount = 120;
	constexpr int32 MinimumBenchmarkPreviewCount = 1;
	constexpr int32 MaximumBenchmarkPreviewCount = 1000;
	constexpr int32 BenchmarkTargetAnchorStride = 7;
	constexpr double MillisecondsPerSecond = 1000.0;
	constexpr float BenchmarkP95Percentile = 0.95f;
	constexpr double MaxValidationSeconds = 130.0;

	struct FDivisionPathingValidationRequest
	{
		int32 Seed = DefaultValidationSeed;
		int32 DivisionCount = DefaultValidationDivisionCount;
		bool bShouldRequestExit = false;
		bool bRunExactRegressionCases = true;
		bool bRunGeneratedCases = true;
	};

	struct FDivisionPathingBenchmarkRequest
	{
		int32 Seed = DefaultValidationSeed;
		int32 PreviewCount = DefaultBenchmarkPreviewCount;
		bool bShouldRequestExit = false;
	};

	struct FDivisionPathingBenchmarkStats
	{
		TArray<double> PathSeconds;
		double TotalPathSeconds = 0.0;
		double MinPathSeconds = TNumericLimits<double>::Max();
		double MaxPathSeconds = 0.0;
		int32 TotalPathPointCount = 0;
		int32 MaxPathPreviewIndex = INDEX_NONE;
		int32 MaxPathPointCount = 0;
		FString MaxPathTargetName = TEXT("None");
	};

	struct FExactDivisionPathingRegressionCase
	{
		FString Name;
		FVector StartLocation = FVector::ZeroVector;
		FVector TargetLocation = FVector::ZeroVector;
	};

	struct FClosestAnchorPathDistance
	{
		float Distance = TNumericLimits<float>::Max();
		FString AnchorName = TEXT("None");
		FVector AnchorLocation = FVector::ZeroVector;
		FVector SegmentStart = FVector::ZeroVector;
		FVector SegmentEnd = FVector::ZeroVector;
		float AnchorBoundaryDistance = TNumericLimits<float>::Max();
		int32 SegmentIndex = INDEX_NONE;
	};

	struct FPathBoundaryViolation
	{
		FVector OutsidePoint = FVector::ZeroVector;
		FVector SegmentStart = FVector::ZeroVector;
		FVector SegmentEnd = FVector::ZeroVector;
		int32 SegmentIndex = INDEX_NONE;
		int32 SampleIndex = INDEX_NONE;
	};

	FVector2D GetValidationXY(const FVector& Location)
	{
		return FVector2D(Location.X, Location.Y);
	}

	FVector2D GetValidationClosestPointOnSegment(const FVector2D& Point,
	                                             const FVector2D& SegmentStart,
	                                             const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentSizeSquared = Segment.SizeSquared();
		if (SegmentSizeSquared <= KINDA_SMALL_NUMBER)
		{
			return SegmentStart;
		}

		const float Alpha = FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentSizeSquared;
		return SegmentStart + Segment * FMath::Clamp(Alpha, 0.f, 1.f);
	}

	bool GetValidationIsPointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return false;
		}

		bool bIsInside = false;
		int32 PreviousIndex = Polygon.Num() - 1;
		for (int32 CurrentIndex = 0; CurrentIndex < Polygon.Num(); CurrentIndex++)
		{
			const FVector2D& CurrentPoint = Polygon[CurrentIndex];
			const FVector2D& PreviousPoint = Polygon[PreviousIndex];
			const bool bDoesEdgeCrossY = (CurrentPoint.Y > Point.Y) != (PreviousPoint.Y > Point.Y);
			if (bDoesEdgeCrossY)
			{
				const float EdgeX = (PreviousPoint.X - CurrentPoint.X)
					* (Point.Y - CurrentPoint.Y)
					/ (PreviousPoint.Y - CurrentPoint.Y)
					+ CurrentPoint.X;
				if (Point.X < EdgeX)
				{
					bIsInside = not bIsInside;
				}
			}

			PreviousIndex = CurrentIndex;
		}

		return bIsInside;
	}

	float GetPointDistanceToBoundary(const FVector2D& Point, const TArray<FVector2D>& BoundaryPolygon)
	{
		float ClosestDistance = TNumericLimits<float>::Max();
		for (int32 BoundaryIndex = 0; BoundaryIndex < BoundaryPolygon.Num(); BoundaryIndex++)
		{
			const FVector2D& SegmentStart = BoundaryPolygon[BoundaryIndex];
			const FVector2D& SegmentEnd = BoundaryPolygon[(BoundaryIndex + 1) % BoundaryPolygon.Num()];
			const FVector2D ClosestPoint = GetValidationClosestPointOnSegment(Point, SegmentStart, SegmentEnd);
			ClosestDistance = FMath::Min(ClosestDistance, FVector2D::Distance(Point, ClosestPoint));
		}

		return ClosestDistance;
	}

	bool TryFindBoundaryViolationOnSegment(const FVector& SegmentStart,
	                                       const FVector& SegmentEnd,
	                                       const TArray<FVector2D>& BoundaryPolygon,
	                                       const int32 SegmentIndex,
	                                       FPathBoundaryViolation& OutViolation)
	{
		const float SegmentDistance = FVector2D::Distance(GetValidationXY(SegmentStart), GetValidationXY(SegmentEnd));
		const int32 SampleCount = FMath::Clamp(
			FMath::CeilToInt(SegmentDistance / ValidationPathSampleSpacing),
			ValidationPathMinSampleCount,
			ValidationPathMaxSampleCount);
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; SampleIndex++)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const FVector Candidate = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			if (GetValidationIsPointInsidePolygon(GetValidationXY(Candidate), BoundaryPolygon))
			{
				continue;
			}

			if (GetPointDistanceToBoundary(GetValidationXY(Candidate), BoundaryPolygon)
				<= ValidationDistanceTolerance)
			{
				continue;
			}

			OutViolation.OutsidePoint = Candidate;
			OutViolation.SegmentStart = SegmentStart;
			OutViolation.SegmentEnd = SegmentEnd;
			OutViolation.SegmentIndex = SegmentIndex;
			OutViolation.SampleIndex = SampleIndex;
			return true;
		}

		return false;
	}

	bool TryFindPathBoundaryViolation(const TArray<FVector>& PathPoints,
	                                  const TArray<FVector2D>& BoundaryPolygon,
	                                  FPathBoundaryViolation& OutViolation)
	{
		if (PathPoints.Num() < 2 || BoundaryPolygon.Num() < 3)
		{
			return true;
		}

		for (int32 PathPointIndex = 0; PathPointIndex < PathPoints.Num() - 1; PathPointIndex++)
		{
			if (TryFindBoundaryViolationOnSegment(
				PathPoints[PathPointIndex],
				PathPoints[PathPointIndex + 1],
				BoundaryPolygon,
				PathPointIndex,
				OutViolation))
			{
				return true;
			}
		}

		return false;
	}

	float GetAnchorDistanceToSegment(const AAnchorPoint& AnchorPoint,
	                                 const FVector& SegmentStart,
	                                 const FVector& SegmentEnd)
	{
		const FVector2D AnchorXY = GetValidationXY(AnchorPoint.GetActorLocation());
		const FVector2D ClosestPoint = GetValidationClosestPointOnSegment(
			AnchorXY,
			GetValidationXY(SegmentStart),
			GetValidationXY(SegmentEnd));
		return FVector2D::Distance(AnchorXY, ClosestPoint);
	}

	float GetAnchorDistanceToBoundary(const AAnchorPoint& AnchorPoint, const TArray<FVector2D>& BoundaryPolygon)
	{
		if (BoundaryPolygon.Num() < 3)
		{
			return TNumericLimits<float>::Max();
		}

		float ClosestDistance = TNumericLimits<float>::Max();
		const FVector2D AnchorXY = GetValidationXY(AnchorPoint.GetActorLocation());
		for (int32 BoundaryIndex = 0; BoundaryIndex < BoundaryPolygon.Num(); BoundaryIndex++)
		{
			const FVector2D& SegmentStart = BoundaryPolygon[BoundaryIndex];
			const FVector2D& SegmentEnd = BoundaryPolygon[(BoundaryIndex + 1) % BoundaryPolygon.Num()];
			const FVector2D ClosestPoint =
				GetValidationClosestPointOnSegment(AnchorXY, SegmentStart, SegmentEnd);
			ClosestDistance = FMath::Min(ClosestDistance, FVector2D::Distance(AnchorXY, ClosestPoint));
		}

		return ClosestDistance;
	}

	FClosestAnchorPathDistance GetClosestAnchorPathDistance(const TArray<AAnchorPoint*>& Anchors,
	                                                       const TArray<FVector>& PathPoints,
	                                                       const TArray<FVector2D>& BoundaryPolygon)
	{
		FClosestAnchorPathDistance ClosestDistance;
		if (PathPoints.Num() < 2)
		{
			return ClosestDistance;
		}

		for (int32 PathPointIndex = 0; PathPointIndex < PathPoints.Num() - 1; PathPointIndex++)
		{
			for (const AAnchorPoint* AnchorPoint : Anchors)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				const float Distance = GetAnchorDistanceToSegment(
					*AnchorPoint,
					PathPoints[PathPointIndex],
					PathPoints[PathPointIndex + 1]);
				if (Distance >= ClosestDistance.Distance)
				{
					continue;
				}

				ClosestDistance.Distance = Distance;
				ClosestDistance.AnchorName = AnchorPoint->GetName();
				ClosestDistance.AnchorLocation = AnchorPoint->GetActorLocation();
				ClosestDistance.SegmentStart = PathPoints[PathPointIndex];
				ClosestDistance.SegmentEnd = PathPoints[PathPointIndex + 1];
				ClosestDistance.AnchorBoundaryDistance =
					GetAnchorDistanceToBoundary(*AnchorPoint, BoundaryPolygon);
				ClosestDistance.SegmentIndex = PathPointIndex;
			}
		}

		return ClosestDistance;
	}

	void BuildBoundaryPolygonForValidation(UWorld& World, TArray<FVector2D>& OutBoundaryPolygon)
	{
		OutBoundaryPolygon.Reset();
		TArray<AActor*> BoundaryActors;
		UGameplayStatics::GetAllActorsOfClass(&World, AWorldSplineBoundary::StaticClass(), BoundaryActors);
		for (AActor* BoundaryActor : BoundaryActors)
		{
			const AWorldSplineBoundary* Boundary = Cast<AWorldSplineBoundary>(BoundaryActor);
			if (not IsValid(Boundary))
			{
				continue;
			}

			Boundary->GetSampledPolygon2D(ValidationBoundarySampleSpacing, OutBoundaryPolygon);
			if (OutBoundaryPolygon.Num() >= 3)
			{
				return;
			}
		}
	}

	void BuildAnchorsWithAvoidanceClearance(const TArray<AAnchorPoint*>& Anchors,
	                                        const float AvoidanceRadius,
	                                        TArray<AAnchorPoint*>& OutAnchorsWithClearance)
	{
		OutAnchorsWithClearance.Reset();
		const float RequiredDistanceSquared = FMath::Square(AvoidanceRadius * 2.f + ValidationDistanceTolerance);
		for (AAnchorPoint* AnchorPoint : Anchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			bool bHasOverlappingNeighbor = false;
			for (const AAnchorPoint* OtherAnchorPoint : Anchors)
			{
				if (AnchorPoint == OtherAnchorPoint || not IsValid(OtherAnchorPoint))
				{
					continue;
				}

				const float DistanceSquared = FVector2D::DistSquared(
					GetValidationXY(AnchorPoint->GetActorLocation()),
					GetValidationXY(OtherAnchorPoint->GetActorLocation()));
				if (DistanceSquared >= RequiredDistanceSquared)
				{
					continue;
				}

				bHasOverlappingNeighbor = true;
				break;
			}

			if (not bHasOverlappingNeighbor)
			{
				OutAnchorsWithClearance.Add(AnchorPoint);
			}
		}
	}

	bool GetShouldRequestExitAfterValidation(const TArray<FString>& Args)
	{
		for (const FString& Arg : Args)
		{
			FString NormalizedArg = Arg;
			NormalizedArg.ReplaceInline(TEXT(";"), TEXT(""));
			if (NormalizedArg.Equals(TEXT("Quit"), ESearchCase::IgnoreCase)
				|| NormalizedArg.Equals(TEXT("Exit"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	FDivisionPathingValidationRequest BuildValidationRequest(const TArray<FString>& Args)
	{
		FDivisionPathingValidationRequest Request;
		Request.bShouldRequestExit = GetShouldRequestExitAfterValidation(Args);
		for (const FString& Arg : Args)
		{
			FString NormalizedArg = Arg;
			NormalizedArg.ReplaceInline(TEXT(";"), TEXT(""));
			if (NormalizedArg.Equals(TEXT("ExactOnly"), ESearchCase::IgnoreCase))
			{
				Request.bRunExactRegressionCases = true;
				Request.bRunGeneratedCases = false;
			}
			else if (NormalizedArg.Equals(TEXT("GeneratedOnly"), ESearchCase::IgnoreCase))
			{
				Request.bRunExactRegressionCases = false;
				Request.bRunGeneratedCases = true;
			}
		}

		if (Args.Num() >= 1)
		{
			Request.Seed = FCString::Atoi(*Args[0]);
		}

		if (Args.Num() >= 2)
		{
			Request.DivisionCount = FMath::Max(MinimumValidationDivisionCount, FCString::Atoi(*Args[1]));
		}

		return Request;
	}

	void RequestExitAfterValidationIfNeeded(const bool bShouldRequestExit)
	{
		if (not bShouldRequestExit)
		{
			return;
		}

		FPlatformMisc::RequestExit(false);
	}

	template <typename TActorType>
	TActorType* FindFirstActor(UWorld& World)
	{
		for (TActorIterator<TActorType> ActorIterator(&World); ActorIterator; ++ActorIterator)
		{
			return *ActorIterator;
		}

		return nullptr;
	}

	void InitializeGeneratorForPathingValidation(AGeneratorWorldCampaign& Generator,
	                                             UWorld& World,
	                                             const int32 Seed)
	{
		AWorldPlayerController* WorldPlayerController = FindFirstActor<AWorldPlayerController>(World);
		URTSGameInstance* GameInstance = World.GetGameInstance<URTSGameInstance>();
		if (not IsValid(WorldPlayerController) || not IsValid(GameInstance))
		{
			return;
		}

		FCampaignGenerationSettings CampaignSettings = GameInstance->GetCampaignGenerationSettings();
		CampaignSettings.GenerationSeed = Seed;
		Generator.InitializeWorldGenerator(
			WorldPlayerController,
			CampaignSettings,
			GameInstance->GetSelectedGameDifficulty());
	}

	void GatherValidAnchors(const FWorldCampaignPlacementState& PlacementState, TArray<AAnchorPoint*>& OutAnchors)
	{
		OutAnchors.Reset();
		OutAnchors.Reserve(PlacementState.CachedAnchors.Num());
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			if (IsValid(AnchorPoint))
			{
				OutAnchors.Add(AnchorPoint.Get());
			}
		}

		Algo::Sort(OutAnchors, [](const AAnchorPoint* Left, const AAnchorPoint* Right)
		{
			return AAnchorPoint::IsDeterministicAnchorOrderLess(Left, Right);
		});
	}

	void GatherExistingAnchors(UWorld& World, TArray<AAnchorPoint*>& OutAnchors)
	{
		OutAnchors.Reset();
		TArray<AActor*> AnchorActors;
		UGameplayStatics::GetAllActorsOfClass(&World, AAnchorPoint::StaticClass(), AnchorActors);
		OutAnchors.Reserve(AnchorActors.Num());
		for (AActor* AnchorActor : AnchorActors)
		{
			AAnchorPoint* AnchorPoint = Cast<AAnchorPoint>(AnchorActor);
			if (IsValid(AnchorPoint))
			{
				OutAnchors.Add(AnchorPoint);
			}
		}

		Algo::Sort(OutAnchors, [](const AAnchorPoint* Left, const AAnchorPoint* Right)
		{
			return AAnchorPoint::IsDeterministicAnchorOrderLess(Left, Right);
		});
	}

	bool BuildAnchorsForValidation(UWorld& World,
	                               int32 Seed,
	                               TArray<AAnchorPoint*>& OutAnchors,
	                               FString& OutFailureReason);

	bool PrepareAnchorsForValidation(UWorld& World,
	                                 const FDivisionPathingValidationRequest& Request,
	                                 TArray<AAnchorPoint*>& OutAnchors,
	                                 FString& OutFailureReason)
	{
		if (Request.bRunGeneratedCases)
		{
			return BuildAnchorsForValidation(World, Request.Seed, OutAnchors, OutFailureReason);
		}

		GatherExistingAnchors(World, OutAnchors);
		return true;
	}

	bool BuildAnchorsForValidation(UWorld& World,
	                               const int32 Seed,
	                               TArray<AAnchorPoint*>& OutAnchors,
	                               FString& OutFailureReason)
	{
		AGeneratorWorldCampaign* Generator = FindFirstActor<AGeneratorWorldCampaign>(World);
		if (not IsValid(Generator))
		{
			OutFailureReason = TEXT("no world campaign generator found");
			return false;
		}

		InitializeGeneratorForPathingValidation(*Generator, World, Seed);

		FString GenerationFailureReason;
		if (not Generator->GenerateAndValidatePrunedWorldForSeed(Seed, GenerationFailureReason))
		{
			OutFailureReason = FString::Printf(
				TEXT("could not generate pruned world for seed %d: %s"),
				Seed,
				*GenerationFailureReason);
			return false;
		}

		GatherValidAnchors(Generator->GetPlacementState(), OutAnchors);
		if (OutAnchors.Num() < 2)
		{
			OutFailureReason = FString::Printf(
				TEXT("need at least 2 anchors, found %d"),
				OutAnchors.Num());
			return false;
		}

		return true;
	}

	FVector BuildDivisionStartLocation(const AAnchorPoint& StartAnchor,
	                                   const AAnchorPoint& TargetAnchor,
	                                   const int32 DivisionIndex,
	                                   const float AvoidanceRadius)
	{
		FVector2D Direction =
			GetValidationXY(StartAnchor.GetActorLocation()) - GetValidationXY(TargetAnchor.GetActorLocation());
		if (Direction.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			const float Angle = static_cast<float>(DivisionIndex) * FallbackAngleStepRadians;
			Direction = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
		}

		const float SpawnOffset = FMath::Max(MinSpawnOffset, AvoidanceRadius * SpawnOffsetRadiusScale);
		const FVector2D StartXY =
			GetValidationXY(StartAnchor.GetActorLocation()) + Direction.GetSafeNormal() * SpawnOffset;
		return FVector(StartXY.X, StartXY.Y, StartAnchor.GetActorLocation().Z);
	}

	int32 GetTargetAnchorIndex(const int32 DivisionIndex, const int32 AnchorCount)
	{
		const int32 StartAnchorIndex = DivisionIndex % AnchorCount;
		int32 TargetAnchorIndex = (DivisionIndex * TargetAnchorStride + AnchorCount / 2) % AnchorCount;
		if (TargetAnchorIndex == StartAnchorIndex)
		{
			TargetAnchorIndex = (TargetAnchorIndex + 1) % AnchorCount;
		}

		return TargetAnchorIndex;
	}

	AWorldDivisionBase* SpawnValidationDivision(UWorld& World,
	                                            const int32 DivisionIndex,
	                                            const FVector& StartLocation)
	{
		const TSubclassOf<AWorldDivisionBase> DivisionClass = DivisionIndex % AlternatingDivisionClassCount == 0
			                                                     ? AWorldTankDivision::StaticClass()
			                                                     : AWorldSquadDivision::StaticClass();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;

		FTransform SpawnTransform = FTransform::Identity;
		SpawnTransform.SetLocation(StartLocation);
		return World.SpawnActor<AWorldDivisionBase>(DivisionClass, SpawnTransform, SpawnParameters);
	}

	void DestroyValidationDivisions(const TArray<TWeakObjectPtr<AWorldDivisionBase>>& SpawnedDivisions)
	{
		for (int32 DivisionIndex = SpawnedDivisions.Num() - 1; DivisionIndex >= 0; DivisionIndex--)
		{
			AWorldDivisionBase* WorldDivision = SpawnedDivisions[DivisionIndex].Get();
			if (IsValid(WorldDivision))
			{
				WorldDivision->Destroy();
			}
		}
	}

	TArray<FExactDivisionPathingRegressionCase> BuildExactRegressionCases()
	{
		TArray<FExactDivisionPathingRegressionCase> RegressionCases;
		RegressionCases.Add({
			TEXT("Image1_BorderConcavity"),
			FVector(1530.593711, -5147.434470, 0.000046),
			FVector(1375.027801, -6665.369551, 0.000010)
		});
		RegressionCases.Add({
			TEXT("Image2_LongBorderConcavity"),
			FVector(3151.956549, -7886.137827, 0.000047),
			FVector(1123.281896, -6736.084855, 0.000005)
		});
		RegressionCases.Add({
			TEXT("Image3_PositiveBorderExample"),
			FVector(4014.155085, -10413.672700, 0.000047),
			FVector(1993.829431, -6669.245776, 0.000098)
		});
		return RegressionCases;
	}

	void LogValidationPath(const FString& CaseName, const TArray<FVector>& PathPoints)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("World division pathing validation %s adjusted path point count=%d."),
			*CaseName,
			PathPoints.Num());
		for (int32 PathPointIndex = 0; PathPointIndex < PathPoints.Num(); PathPointIndex++)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("World division pathing validation %s path[%d]=%s."),
				*CaseName,
				PathPointIndex,
				*PathPoints[PathPointIndex].ToCompactString());
		}
	}

	bool ValidatePathInsideBoundary(const FString& CaseName,
	                                const TArray<FVector>& PathPoints,
	                                const TArray<FVector2D>& BoundaryPolygon)
	{
		FPathBoundaryViolation BoundaryViolation;
		if (not TryFindPathBoundaryViolation(PathPoints, BoundaryPolygon, BoundaryViolation))
		{
			return true;
		}

		LogValidationPath(CaseName, PathPoints);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing validation failed: %s left world boundary. Segment=%d Sample=%d OutsidePoint=%s SegmentStart=%s SegmentEnd=%s."),
			*CaseName,
			BoundaryViolation.SegmentIndex,
			BoundaryViolation.SampleIndex,
			*BoundaryViolation.OutsidePoint.ToCompactString(),
			*BoundaryViolation.SegmentStart.ToCompactString(),
			*BoundaryViolation.SegmentEnd.ToCompactString());
		return false;
	}

	bool RunSingleDivisionPathingValidationCase(UWorld& World,
	                                            const TArray<AAnchorPoint*>& Anchors,
	                                            const TArray<FVector2D>& BoundaryPolygon,
	                                            const int32 DivisionIndex,
	                                            const float AvoidanceRadius,
	                                            TArray<TWeakObjectPtr<AWorldDivisionBase>>& SpawnedDivisions)
	{
		const int32 StartAnchorIndex = DivisionIndex % Anchors.Num();
		const int32 TargetAnchorIndex = GetTargetAnchorIndex(DivisionIndex, Anchors.Num());
		const AAnchorPoint* StartAnchor = Anchors[StartAnchorIndex];
		const AAnchorPoint* TargetAnchor = Anchors[TargetAnchorIndex];
		if (not IsValid(StartAnchor) || not IsValid(TargetAnchor))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: invalid selected anchors."));
			return false;
		}

		const FVector StartLocation = BuildDivisionStartLocation(
			*StartAnchor,
			*TargetAnchor,
			DivisionIndex,
			AvoidanceRadius);
		AWorldDivisionBase* WorldDivision = SpawnValidationDivision(World, DivisionIndex, StartLocation);
		if (not IsValid(WorldDivision))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: could not spawn division %d."),
			       DivisionIndex);
			return false;
		}

		SpawnedDivisions.Add(WorldDivision);
		if (not WorldDivision->IssueMoveOrderToPoint(TargetAnchor->GetActorLocation()))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: division %d rejected move order."),
			       DivisionIndex);
			return false;
		}

		const FWorldDivisionSaveData DivisionSaveData = WorldDivision->BuildWorldDivisionSaveData();
		const FString CaseName = FString::Printf(TEXT("Generated_%d"), DivisionIndex);
		if (not ValidatePathInsideBoundary(CaseName, DivisionSaveData.PathPoints, BoundaryPolygon))
		{
			return false;
		}

		const FClosestAnchorPathDistance ClosestDistance =
			GetClosestAnchorPathDistance(Anchors, DivisionSaveData.PathPoints, BoundaryPolygon);
		const bool bAvoidanceRadiusHeld =
			ClosestDistance.Distance + ValidationDistanceTolerance >= AvoidanceRadius;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("World division pathing validation case %d: target=%s pathPoints=%d minAnchorDistance=%.2f radius=%.2f closestAnchor=%s segment=%d."),
			DivisionIndex,
			*TargetAnchor->GetName(),
			DivisionSaveData.PathPoints.Num(),
			ClosestDistance.Distance,
			AvoidanceRadius,
			*ClosestDistance.AnchorName,
			ClosestDistance.SegmentIndex);
		if (bAvoidanceRadiusHeld)
		{
			return true;
		}

		LogValidationPath(CaseName, DivisionSaveData.PathPoints);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing validation failed: division %d path entered anchor avoidance radius by %.2f units. Anchor=%s AnchorLocation=%s AnchorBoundaryDistance=%.2f SegmentStart=%s SegmentEnd=%s."),
			DivisionIndex,
			AvoidanceRadius - ClosestDistance.Distance,
			*ClosestDistance.AnchorName,
			*ClosestDistance.AnchorLocation.ToCompactString(),
			ClosestDistance.AnchorBoundaryDistance,
			*ClosestDistance.SegmentStart.ToCompactString(),
			*ClosestDistance.SegmentEnd.ToCompactString());
		return false;
	}

	bool RunExactDivisionPathingRegressionCase(UWorld& World,
	                                           const FExactDivisionPathingRegressionCase& RegressionCase,
	                                           const int32 CaseIndex,
	                                           const TArray<FVector2D>& BoundaryPolygon,
	                                           TArray<TWeakObjectPtr<AWorldDivisionBase>>& SpawnedDivisions)
	{
		AWorldDivisionBase* WorldDivision = SpawnValidationDivision(World, CaseIndex, RegressionCase.StartLocation);
		if (not IsValid(WorldDivision))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("World division pathing validation failed: %s could not spawn division."),
				*RegressionCase.Name);
			return false;
		}

		SpawnedDivisions.Add(WorldDivision);
		if (not WorldDivision->IssueMoveOrderToPoint(RegressionCase.TargetLocation))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("World division pathing validation failed: %s rejected move order Start=%s Target=%s."),
				*RegressionCase.Name,
				*RegressionCase.StartLocation.ToCompactString(),
				*RegressionCase.TargetLocation.ToCompactString());
			return false;
		}

		const FWorldDivisionSaveData DivisionSaveData = WorldDivision->BuildWorldDivisionSaveData();
		LogValidationPath(RegressionCase.Name, DivisionSaveData.PathPoints);
		const bool bPathInsideBoundary =
			ValidatePathInsideBoundary(RegressionCase.Name, DivisionSaveData.PathPoints, BoundaryPolygon);
		if (bPathInsideBoundary)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("World division pathing validation exact case passed: %s Start=%s Target=%s."),
				*RegressionCase.Name,
				*RegressionCase.StartLocation.ToCompactString(),
				*RegressionCase.TargetLocation.ToCompactString());
		}

		return bPathInsideBoundary;
	}

	bool RunExactDivisionPathingRegressionCases(UWorld& World, const TArray<FVector2D>& BoundaryPolygon)
	{
		bool bAllCasesPassed = true;
		const TArray<FExactDivisionPathingRegressionCase> RegressionCases = BuildExactRegressionCases();
		TArray<TWeakObjectPtr<AWorldDivisionBase>> SpawnedDivisions;
		SpawnedDivisions.Reserve(RegressionCases.Num());
		for (int32 CaseIndex = 0; CaseIndex < RegressionCases.Num(); CaseIndex++)
		{
			if (RunExactDivisionPathingRegressionCase(
				World,
				RegressionCases[CaseIndex],
				CaseIndex,
				BoundaryPolygon,
				SpawnedDivisions))
			{
				continue;
			}

			bAllCasesPassed = false;
		}

		DestroyValidationDivisions(SpawnedDivisions);
		return bAllCasesPassed;
	}

	bool RunDivisionPathingValidation(UWorld& World, const FDivisionPathingValidationRequest& Request)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
		const float AvoidanceRadius = IsValid(Settings) ? Settings->WorldDivisionAnchorAvoidanceRadius : 0.f;

		TArray<AAnchorPoint*> Anchors;
		FString FailureReason;
		if (not PrepareAnchorsForValidation(World, Request, Anchors, FailureReason))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: %s"), *FailureReason);
			return false;
		}

		TArray<FVector2D> BoundaryPolygon;
		BuildBoundaryPolygonForValidation(World, BoundaryPolygon);
		if (BoundaryPolygon.Num() < 3)
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: boundary polygon is invalid."));
			return false;
		}

		TArray<AAnchorPoint*> AnchorsWithClearance;
		BuildAnchorsWithAvoidanceClearance(Anchors, AvoidanceRadius, AnchorsWithClearance);
		if (Request.bRunGeneratedCases && AnchorsWithClearance.Num() < 2)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("World division pathing validation failed: need at least 2 anchors with %.2f avoidance clearance, found %d of %d."),
				AvoidanceRadius,
				AnchorsWithClearance.Num(),
				Anchors.Num());
			return false;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("World division pathing validation started: seed=%d divisions=%d anchors=%d validationAnchors=%d avoidanceRadius=%.2f exact=%s generated=%s hardCapSeconds=%.0f."),
			Request.Seed,
			Request.DivisionCount,
			Anchors.Num(),
			AnchorsWithClearance.Num(),
			AvoidanceRadius,
			Request.bRunExactRegressionCases ? TEXT("true") : TEXT("false"),
			Request.bRunGeneratedCases ? TEXT("true") : TEXT("false"),
			MaxValidationSeconds);

		bool bAllCasesPassed = true;
		if (Request.bRunExactRegressionCases)
		{
			bAllCasesPassed = RunExactDivisionPathingRegressionCases(World, BoundaryPolygon) && bAllCasesPassed;
		}

		TArray<TWeakObjectPtr<AWorldDivisionBase>> SpawnedDivisions;
		SpawnedDivisions.Reserve(Request.DivisionCount);
		for (int32 DivisionIndex = 0; Request.bRunGeneratedCases && DivisionIndex < Request.DivisionCount;
		     DivisionIndex++)
		{
			if (FPlatformTime::Seconds() - StartSeconds > MaxValidationSeconds)
			{
				UE_LOG(LogTemp, Error,
				       TEXT("World division pathing validation failed: exceeded %.0f second hard cap."),
				       MaxValidationSeconds);
				bAllCasesPassed = false;
				break;
			}

			if (not RunSingleDivisionPathingValidationCase(
				World,
				AnchorsWithClearance,
				BoundaryPolygon,
				DivisionIndex,
				AvoidanceRadius,
				SpawnedDivisions))
			{
				bAllCasesPassed = false;
			}
		}

		DestroyValidationDivisions(SpawnedDivisions);

		const double DurationSeconds = FPlatformTime::Seconds() - StartSeconds;
		if (bAllCasesPassed)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("World division pathing validation passed: divisions=%d anchors=%d validationAnchors=%d exact=%s generated=%s duration=%.2fs."),
				Request.DivisionCount,
				Anchors.Num(),
				AnchorsWithClearance.Num(),
				Request.bRunExactRegressionCases ? TEXT("true") : TEXT("false"),
				Request.bRunGeneratedCases ? TEXT("true") : TEXT("false"),
				DurationSeconds);
			return true;
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing validation failed: divisions=%d anchors=%d validationAnchors=%d exact=%s generated=%s duration=%.2fs."),
			Request.DivisionCount,
			Anchors.Num(),
			AnchorsWithClearance.Num(),
			Request.bRunExactRegressionCases ? TEXT("true") : TEXT("false"),
			Request.bRunGeneratedCases ? TEXT("true") : TEXT("false"),
			DurationSeconds);
		return false;
	}

	FDivisionPathingBenchmarkRequest BuildBenchmarkRequest(const TArray<FString>& Args)
	{
		FDivisionPathingBenchmarkRequest Request;
		Request.bShouldRequestExit = GetShouldRequestExitAfterValidation(Args);
		if (Args.Num() >= 1)
		{
			Request.Seed = FCString::Atoi(*Args[0]);
		}

		if (Args.Num() >= 2)
		{
			Request.PreviewCount = FMath::Clamp(
				FCString::Atoi(*Args[1]),
				MinimumBenchmarkPreviewCount,
				MaximumBenchmarkPreviewCount);
		}

		return Request;
	}

	void RecordBenchmarkSample(const int32 PreviewIndex,
	                           const FString& TargetAnchorName,
	                           const double PathSeconds,
	                           const int32 PathPointCount,
	                           FDivisionPathingBenchmarkStats& InOutStats)
	{
		InOutStats.PathSeconds.Add(PathSeconds);
		InOutStats.TotalPathSeconds += PathSeconds;
		InOutStats.MinPathSeconds = FMath::Min(InOutStats.MinPathSeconds, PathSeconds);
		InOutStats.TotalPathPointCount += PathPointCount;
		if (PathSeconds <= InOutStats.MaxPathSeconds)
		{
			return;
		}

		InOutStats.MaxPathSeconds = PathSeconds;
		InOutStats.MaxPathPreviewIndex = PreviewIndex;
		InOutStats.MaxPathPointCount = PathPointCount;
		InOutStats.MaxPathTargetName = TargetAnchorName;
	}

	double GetBenchmarkPercentileMilliseconds(TArray<double> PathSeconds, const float Percentile)
	{
		if (PathSeconds.Num() <= 0)
		{
			return 0.0;
		}

		Algo::Sort(PathSeconds);
		const int32 PercentileIndex = FMath::Clamp(
			FMath::CeilToInt(static_cast<float>(PathSeconds.Num()) * Percentile) - 1,
			0,
			PathSeconds.Num() - 1);
		return PathSeconds[PercentileIndex] * MillisecondsPerSecond;
	}

	int32 GetBenchmarkTargetAnchorIndex(const int32 PreviewIndex, const int32 AnchorCount)
	{
		if (AnchorCount <= 1)
		{
			return 0;
		}

		int32 TargetAnchorIndex = (PreviewIndex * BenchmarkTargetAnchorStride + AnchorCount / 2) % AnchorCount;
		if (TargetAnchorIndex == 0)
		{
			TargetAnchorIndex = (TargetAnchorIndex + 1) % AnchorCount;
		}

		return TargetAnchorIndex;
	}

	bool ValidateBenchmarkPath(const int32 PreviewIndex,
	                           const TArray<AAnchorPoint*>& Anchors,
	                           const TArray<FVector2D>& BoundaryPolygon,
	                           const float AvoidanceRadius,
	                           const FWorldDivisionSaveData& DivisionSaveData)
	{
		const FString CaseName = FString::Printf(TEXT("Benchmark_%d"), PreviewIndex);
		if (not ValidatePathInsideBoundary(CaseName, DivisionSaveData.PathPoints, BoundaryPolygon))
		{
			return false;
		}

		const FClosestAnchorPathDistance ClosestDistance =
			GetClosestAnchorPathDistance(Anchors, DivisionSaveData.PathPoints, BoundaryPolygon);
		if (ClosestDistance.Distance + ValidationDistanceTolerance >= AvoidanceRadius)
		{
			return true;
		}

		LogValidationPath(CaseName, DivisionSaveData.PathPoints);
		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing benchmark failed: preview %d entered anchor avoidance radius by %.2f units. Anchor=%s Segment=%d."),
			PreviewIndex,
			AvoidanceRadius - ClosestDistance.Distance,
			*ClosestDistance.AnchorName,
			ClosestDistance.SegmentIndex);
		return false;
	}

	void LogDivisionPathingBenchmarkSummary(const FDivisionPathingBenchmarkRequest& Request,
	                                        const FDivisionPathingBenchmarkStats& Stats,
	                                        const int32 AnchorCount,
	                                        const bool bAllPathsPassed)
	{
		const int32 CompletedPreviewCount = Stats.PathSeconds.Num();
		const double AveragePathMilliseconds = CompletedPreviewCount > 0
			                                      ? Stats.TotalPathSeconds
			                                      / static_cast<double>(CompletedPreviewCount)
			                                      * MillisecondsPerSecond
			                                      : 0.0;
		const double AveragePathPointCount = CompletedPreviewCount > 0
			                                     ? static_cast<double>(Stats.TotalPathPointCount)
			                                     / static_cast<double>(CompletedPreviewCount)
			                                     : 0.0;
		const double P95PathMilliseconds =
			GetBenchmarkPercentileMilliseconds(Stats.PathSeconds, BenchmarkP95Percentile);
		const TCHAR* ResultText = bAllPathsPassed ? TEXT("passed") : TEXT("failed");
		if (bAllPathsPassed)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("World division pathing benchmark %s: seed=%d previews=%d anchors=%d avg=%.3fms p95=%.3fms min=%.3fms max=%.3fms avgPathPoints=%.2f maxPreview=%d maxTarget=%s maxPathPoints=%d."),
				ResultText,
				Request.Seed,
				CompletedPreviewCount,
				AnchorCount,
				AveragePathMilliseconds,
				P95PathMilliseconds,
				Stats.MinPathSeconds * MillisecondsPerSecond,
				Stats.MaxPathSeconds * MillisecondsPerSecond,
				AveragePathPointCount,
				Stats.MaxPathPreviewIndex,
				*Stats.MaxPathTargetName,
				Stats.MaxPathPointCount);
			return;
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing benchmark %s: seed=%d previews=%d anchors=%d avg=%.3fms p95=%.3fms min=%.3fms max=%.3fms avgPathPoints=%.2f maxPreview=%d maxTarget=%s maxPathPoints=%d."),
			ResultText,
			Request.Seed,
			CompletedPreviewCount,
			AnchorCount,
			AveragePathMilliseconds,
			P95PathMilliseconds,
			Stats.MinPathSeconds * MillisecondsPerSecond,
			Stats.MaxPathSeconds * MillisecondsPerSecond,
			AveragePathPointCount,
			Stats.MaxPathPreviewIndex,
			*Stats.MaxPathTargetName,
			Stats.MaxPathPointCount);
	}

	bool RunDivisionPathingBenchmarkRequests(AWorldDivisionBase& WorldDivision,
	                                         const FDivisionPathingBenchmarkRequest& Request,
	                                         const TArray<AAnchorPoint*>& AnchorsWithClearance,
	                                         const TArray<FVector2D>& BoundaryPolygon,
	                                         const float AvoidanceRadius,
	                                         FDivisionPathingBenchmarkStats& OutStats)
	{
		bool bAllPathsPassed = true;
		OutStats.PathSeconds.Reserve(Request.PreviewCount);
		for (int32 PreviewIndex = 0; PreviewIndex < Request.PreviewCount; PreviewIndex++)
		{
			const AAnchorPoint* TargetAnchor =
				AnchorsWithClearance[GetBenchmarkTargetAnchorIndex(PreviewIndex, AnchorsWithClearance.Num())];
			const double PathStartSeconds = FPlatformTime::Seconds();
			const bool bIssuedMoveOrder = WorldDivision.IssueMoveOrderToPoint(TargetAnchor->GetActorLocation());
			const double PathSeconds = FPlatformTime::Seconds() - PathStartSeconds;
			const FWorldDivisionSaveData DivisionSaveData = WorldDivision.BuildWorldDivisionSaveData();
			RecordBenchmarkSample(
				PreviewIndex,
				TargetAnchor->GetName(),
				PathSeconds,
				DivisionSaveData.PathPoints.Num(),
				OutStats);

			if (not bIssuedMoveOrder
				|| not ValidateBenchmarkPath(
					PreviewIndex,
					AnchorsWithClearance,
					BoundaryPolygon,
					AvoidanceRadius,
					DivisionSaveData))
			{
				bAllPathsPassed = false;
			}
		}

		return bAllPathsPassed;
	}

	bool RunDivisionPathingBenchmark(UWorld& World, const FDivisionPathingBenchmarkRequest& Request)
	{
		const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
		const float AvoidanceRadius = IsValid(Settings) ? Settings->WorldDivisionAnchorAvoidanceRadius : 0.f;

		TArray<AAnchorPoint*> Anchors;
		FString FailureReason;
		if (not BuildAnchorsForValidation(World, Request.Seed, Anchors, FailureReason))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing benchmark failed: %s"), *FailureReason);
			return false;
		}

		TArray<FVector2D> BoundaryPolygon;
		BuildBoundaryPolygonForValidation(World, BoundaryPolygon);
		if (BoundaryPolygon.Num() < 3)
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing benchmark failed: boundary polygon is invalid."));
			return false;
		}

		TArray<AAnchorPoint*> AnchorsWithClearance;
		BuildAnchorsWithAvoidanceClearance(Anchors, AvoidanceRadius, AnchorsWithClearance);
		if (AnchorsWithClearance.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing benchmark failed: not enough anchors with clearance."));
			return false;
		}

		const FVector StartLocation = BuildDivisionStartLocation(
			*AnchorsWithClearance[0],
			*AnchorsWithClearance[1],
			0,
			AvoidanceRadius);
		TArray<TWeakObjectPtr<AWorldDivisionBase>> SpawnedDivisions;
		AWorldDivisionBase* WorldDivision = SpawnValidationDivision(World, 0, StartLocation);
		SpawnedDivisions.Add(WorldDivision);
		if (not IsValid(WorldDivision))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing benchmark failed: could not spawn division."));
			return false;
		}

		FDivisionPathingBenchmarkStats Stats;
		const bool bAllPathsPassed = RunDivisionPathingBenchmarkRequests(
			*WorldDivision,
			Request,
			AnchorsWithClearance,
			BoundaryPolygon,
			AvoidanceRadius,
			Stats);
		DestroyValidationDivisions(SpawnedDivisions);
		LogDivisionPathingBenchmarkSummary(Request, Stats, AnchorsWithClearance.Num(), bAllPathsPassed);
		return bAllPathsPassed;
	}

	void ValidateWorldDivisionPathingCommand(const TArray<FString>& Args, UWorld* World)
	{
		const FDivisionPathingValidationRequest Request = BuildValidationRequest(Args);
		if constexpr (DeveloperSettings::Debugging::GWorldCampaign_DivisionPathing_Compile_DebugSymbols)
		{
			if (not IsValid(World))
			{
				UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: invalid world."));
				RequestExitAfterValidationIfNeeded(Request.bShouldRequestExit);
				return;
			}

			(void)RunDivisionPathingValidation(*World, Request);
		}

		RequestExitAfterValidationIfNeeded(Request.bShouldRequestExit);
	}

	static FAutoConsoleCommandWithWorldAndArgs GValidateWorldDivisionPathingCommand(
		TEXT("RTS.WorldCampaign.ValidateDivisionPathing"),
		TEXT("Generates a pruned campaign and validates temporary division paths against boundary and anchor constraints. Args: [Seed] [DivisionCount] [ExactOnly|GeneratedOnly] [Quit]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ValidateWorldDivisionPathingCommand));

	void BenchmarkWorldDivisionPathingCommand(const TArray<FString>& Args, UWorld* World)
	{
		const FDivisionPathingBenchmarkRequest Request = BuildBenchmarkRequest(Args);
		if constexpr (DeveloperSettings::Debugging::GWorldCampaign_DivisionPathing_Compile_DebugSymbols)
		{
			if (not IsValid(World))
			{
				UE_LOG(LogTemp, Error, TEXT("World division pathing benchmark failed: invalid world."));
				RequestExitAfterValidationIfNeeded(Request.bShouldRequestExit);
				return;
			}

			(void)RunDivisionPathingBenchmark(*World, Request);
		}

		RequestExitAfterValidationIfNeeded(Request.bShouldRequestExit);
	}

	static FAutoConsoleCommandWithWorldAndArgs GBenchmarkWorldDivisionPathingCommand(
		TEXT("RTS.WorldCampaign.BenchmarkDivisionPathing"),
		TEXT("Generates a pruned campaign and times repeated preview-style division path requests. Args: [Seed] [PreviewCount] [Quit]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BenchmarkWorldDivisionPathingCommand));
}
