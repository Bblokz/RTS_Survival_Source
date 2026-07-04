// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CoreMinimal.h"

#include "Algo/Sort.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
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
	constexpr double MaxValidationSeconds = 130.0;

	struct FDivisionPathingValidationRequest
	{
		int32 Seed = DefaultValidationSeed;
		int32 DivisionCount = DefaultValidationDivisionCount;
		bool bShouldRequestExit = false;
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
		for (TActorIterator<AWorldSplineBoundary> BoundaryIterator(&World); BoundaryIterator; ++BoundaryIterator)
		{
			const AWorldSplineBoundary* Boundary = *BoundaryIterator;
			if (not IsValid(Boundary))
			{
				continue;
			}

			Boundary->GetSampledPolygon2D(ValidationBoundarySampleSpacing, OutBoundaryPolygon);
			return;
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

	bool RunDivisionPathingValidation(UWorld& World, const FDivisionPathingValidationRequest& Request)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
		const float AvoidanceRadius = IsValid(Settings) ? Settings->WorldDivisionAnchorAvoidanceRadius : 0.f;

		TArray<AAnchorPoint*> Anchors;
		FString FailureReason;
		TArray<FVector2D> BoundaryPolygon;
		BuildBoundaryPolygonForValidation(World, BoundaryPolygon);
		if (not BuildAnchorsForValidation(World, Request.Seed, Anchors, FailureReason))
		{
			UE_LOG(LogTemp, Error, TEXT("World division pathing validation failed: %s"), *FailureReason);
			return false;
		}

		TArray<AAnchorPoint*> AnchorsWithClearance;
		BuildAnchorsWithAvoidanceClearance(Anchors, AvoidanceRadius, AnchorsWithClearance);
		if (AnchorsWithClearance.Num() < 2)
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
			TEXT("World division pathing validation started: seed=%d divisions=%d anchors=%d validationAnchors=%d avoidanceRadius=%.2f hardCapSeconds=%.0f."),
			Request.Seed,
			Request.DivisionCount,
			Anchors.Num(),
			AnchorsWithClearance.Num(),
			AvoidanceRadius,
			MaxValidationSeconds);

		bool bAllCasesPassed = true;
		TArray<TWeakObjectPtr<AWorldDivisionBase>> SpawnedDivisions;
		SpawnedDivisions.Reserve(Request.DivisionCount);
		for (int32 DivisionIndex = 0; DivisionIndex < Request.DivisionCount; DivisionIndex++)
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
				TEXT("World division pathing validation passed: divisions=%d anchors=%d validationAnchors=%d duration=%.2fs."),
				Request.DivisionCount,
				Anchors.Num(),
				AnchorsWithClearance.Num(),
				DurationSeconds);
			return true;
		}

		UE_LOG(
			LogTemp,
			Error,
			TEXT("World division pathing validation failed: divisions=%d anchors=%d validationAnchors=%d duration=%.2fs."),
			Request.DivisionCount,
			Anchors.Num(),
			AnchorsWithClearance.Num(),
			DurationSeconds);
		return false;
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
		TEXT("Generates a pruned campaign and validates temporary division paths against anchor avoidance. Args: [Seed] [DivisionCount] [Quit]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ValidateWorldDivisionPathingCommand));
}
