// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"

#include "Algo/Sort.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "EngineUtils.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"

namespace
{
	constexpr int32 MaxStepAttempts = 25;
	constexpr int32 MaxTotalAttempts = 200;
	constexpr int32 AttemptSeedMultiplier = 13;
	constexpr float DebugAnchorDurationSeconds = 10.f;

	struct FAnchorCandidate
	{
		TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
		float DistanceSquared = 0.f;
		FGuid AnchorKey;
	};

	struct FClosestConnectionCandidate
	{
		TWeakObjectPtr<AConnection> Connection;
		FVector JunctionLocation = FVector::ZeroVector;
		float DistanceSquared = TNumericLimits<float>::Max();
	};

	constexpr float SegmentIntersectionTolerance = 0.01f;
	constexpr int32 NoRequiredItems = 0;

	TArray<FAnchorCandidate> BuildAndSortCandidates(AAnchorPoint* AnchorPoint,
		const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints, const int32 MaxConnections,
		const float MaxPreferredDistance, const bool bIgnoreDistance)
	{
		TArray<FAnchorCandidate> Candidates;
		if (not IsValid(AnchorPoint))
		{
			return Candidates;
		}

		const FVector AnchorLocation = AnchorPoint->GetActorLocation();
		const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
		const float MaxDistanceSquared = FMath::Square(MaxPreferredDistance);

		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : AnchorPoints)
		{
			if (not IsValid(CandidateAnchor) || CandidateAnchor == AnchorPoint)
			{
				continue;
			}

			if (AnchorPoint->GetNeighborAnchors().Contains(CandidateAnchor))
			{
				continue;
			}

			if (CandidateAnchor->GetConnectionCount() >= MaxConnections)
			{
				continue;
			}

			const FVector CandidateLocation = CandidateAnchor->GetActorLocation();
			const FVector2D CandidateLocation2D(CandidateLocation.X, CandidateLocation.Y);
			const float DistanceSquared = FVector2D::DistSquared(AnchorLocation2D, CandidateLocation2D);
			if (not bIgnoreDistance && DistanceSquared > MaxDistanceSquared)
			{
				continue;
			}

			FAnchorCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.DistanceSquared = DistanceSquared;
			Candidate.AnchorKey = CandidateAnchor->GetAnchorKey();
			Candidates.Add(Candidate);
		}

		Algo::Sort(Candidates, [](const FAnchorCandidate& Left, const FAnchorCandidate& Right)
		{
			if (Left.DistanceSquared != Right.DistanceSquared)
			{
				return Left.DistanceSquared < Right.DistanceSquared;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		return Candidates;
	}

	FClosestConnectionCandidate FindClosestConnectionCandidate(AAnchorPoint* AnchorPoint,
		const TArray<TObjectPtr<AConnection>>& Connections)
	{
		FClosestConnectionCandidate Result;
		if (not IsValid(AnchorPoint))
		{
			return Result;
		}

		const FVector AnchorLocation = AnchorPoint->GetActorLocation();
		const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);

		for (const TObjectPtr<AConnection>& Connection : Connections)
		{
			if (not IsValid(Connection))
			{
				continue;
			}

			if (Connection->GetIsThreeWayConnection())
			{
				continue;
			}

			const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
			if (ConnectedAnchors.Num() < 2)
			{
				continue;
			}

			AAnchorPoint* FirstAnchor = ConnectedAnchors[0];
			AAnchorPoint* SecondAnchor = ConnectedAnchors[1];
			if (not IsValid(FirstAnchor) || not IsValid(SecondAnchor))
			{
				continue;
			}

			if (ConnectedAnchors.Contains(AnchorPoint))
			{
				continue;
			}

			const FVector AnchorLocationA = FirstAnchor->GetActorLocation();
			const FVector AnchorLocationB = SecondAnchor->GetActorLocation();
			const FVector2D AnchorLocationA2D(AnchorLocationA.X, AnchorLocationA.Y);
			const FVector2D AnchorLocationB2D(AnchorLocationB.X, AnchorLocationB.Y);

			const FVector2D SegmentVector = AnchorLocationB2D - AnchorLocationA2D;
			const FVector2D AnchorVector = AnchorLocation2D - AnchorLocationA2D;
			const float SegmentLengthSquared = SegmentVector.SizeSquared();
			if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float Projection = FVector2D::DotProduct(AnchorVector, SegmentVector) / SegmentLengthSquared;
			const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
			const FVector2D ProjectedPoint2D = AnchorLocationA2D + SegmentVector * ClampedProjection;
			const float CandidateDistanceSquared = FVector2D::DistSquared(AnchorLocation2D, ProjectedPoint2D);

			if (CandidateDistanceSquared >= Result.DistanceSquared)
			{
				continue;
			}

			constexpr float HalfValue = 0.5f;
			const float JunctionZ = (AnchorLocationA.Z + AnchorLocationB.Z) * HalfValue;
			Result.DistanceSquared = CandidateDistanceSquared;
			Result.JunctionLocation = FVector(ProjectedPoint2D.X, ProjectedPoint2D.Y, JunctionZ);
			Result.Connection = Connection;
		}

		return Result;
	}

	const FDifficultyEnumItemOverrides& GetDifficultyOverrides(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		switch (Tuning.DifficultyLevel)
		{
		case ERTSGameDifficulty::NewToRTS:
			return Tuning.DifficultyEnumOverrides.NewToRTS;
		case ERTSGameDifficulty::Hard:
			return Tuning.DifficultyEnumOverrides.Hard;
		case ERTSGameDifficulty::Brutal:
			return Tuning.DifficultyEnumOverrides.Brutal;
		case ERTSGameDifficulty::Ironman:
			return Tuning.DifficultyEnumOverrides.Ironman;
		case ERTSGameDifficulty::Normal:
		default:
			return Tuning.DifficultyEnumOverrides.Normal;
		}
	}

	int32 GetRequiredEnemyItemCount(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		int32 TotalRequired = 0;
		for (const TPair<EMapEnemyItem, int32>& Pair : Tuning.BaseEnemyItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapEnemyItem, int32>& Pair : Overrides.ExtraEnemyItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		return TotalRequired;
	}

	int32 GetRequiredNeutralItemCount(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		int32 TotalRequired = 0;
		for (const TPair<EMapNeutralObjectType, int32>& Pair : Tuning.BaseNeutralItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapNeutralObjectType, int32>& Pair : Overrides.ExtraNeutralItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		return TotalRequired;
	}

	int32 GetRequiredMissionCount(const FMissionPlacement& MissionRules)
	{
		return MissionRules.RulesByMission.Num();
	}

	void BuildBacktrackEscalationSteps(ECampaignGenerationStep FailedStep,
		TArray<ECampaignGenerationStep>& OutSteps)
	{
		OutSteps.Reset();
		if (FailedStep == ECampaignGenerationStep::MissionsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::NeutralObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::NeutralObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::PlayerHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
		}
	}

	bool IsPointOnSegment(const FVector2D& SegmentStart, const FVector2D& SegmentEnd, const FVector2D& Point)
	{
		const float MinX = FMath::Min(SegmentStart.X, SegmentEnd.X) - SegmentIntersectionTolerance;
		const float MaxX = FMath::Max(SegmentStart.X, SegmentEnd.X) + SegmentIntersectionTolerance;
		const float MinY = FMath::Min(SegmentStart.Y, SegmentEnd.Y) - SegmentIntersectionTolerance;
		const float MaxY = FMath::Max(SegmentStart.Y, SegmentEnd.Y) + SegmentIntersectionTolerance;
		return Point.X >= MinX && Point.X <= MaxX && Point.Y >= MinY && Point.Y <= MaxY;
	}

	float CalculateOrientation(const FVector2D& PointA, const FVector2D& PointB, const FVector2D& PointC)
	{
		return (PointB.Y - PointA.Y) * (PointC.X - PointB.X) - (PointB.X - PointA.X) * (PointC.Y - PointB.Y);
	}

	bool DoSegmentsIntersect(const FVector2D& SegmentAStart, const FVector2D& SegmentAEnd,
		const FVector2D& SegmentBStart, const FVector2D& SegmentBEnd)
	{
		const float Orientation1 = CalculateOrientation(SegmentAStart, SegmentAEnd, SegmentBStart);
		const float Orientation2 = CalculateOrientation(SegmentAStart, SegmentAEnd, SegmentBEnd);
		const float Orientation3 = CalculateOrientation(SegmentBStart, SegmentBEnd, SegmentAStart);
		const float Orientation4 = CalculateOrientation(SegmentBStart, SegmentBEnd, SegmentAEnd);

		if (Orientation1 * Orientation2 < 0.f && Orientation3 * Orientation4 < 0.f)
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation1) && IsPointOnSegment(SegmentAStart, SegmentAEnd, SegmentBStart))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation2) && IsPointOnSegment(SegmentAStart, SegmentAEnd, SegmentBEnd))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation3) && IsPointOnSegment(SegmentBStart, SegmentBEnd, SegmentAStart))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation4) && IsPointOnSegment(SegmentBStart, SegmentBEnd, SegmentAEnd))
		{
			return true;
		}

		return false;
	}
}

AGeneratorWorldCampaign::AGeneratorWorldCampaign()
{
	PrimaryActorTick.bCanEverTick = false;
	M_ConnectionClass = AConnection::StaticClass();
}

void AGeneratorWorldCampaign::CreateConnectionsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::ConnectionsCreated))
	{
		RTSFunctionLibrary::ReportError(TEXT("CreateConnectionsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::ConnectionsCreated,
		&AGeneratorWorldCampaign::ExecuteCreateConnections);
}

void AGeneratorWorldCampaign::PlaceHQStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::PlayerHQPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceHQStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::PlayerHQPlaced, &AGeneratorWorldCampaign::ExecutePlaceHQ);
}

void AGeneratorWorldCampaign::PlaceEnemyHQStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::EnemyHQPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceEnemyHQStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::EnemyHQPlaced, &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ);
}

void AGeneratorWorldCampaign::PlaceEnemyObjectsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::EnemyObjectsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceEnemyObjectsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::EnemyObjectsPlaced,
		&AGeneratorWorldCampaign::ExecutePlaceEnemyObjects);
}

void AGeneratorWorldCampaign::PlaceNeutralObjectsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::NeutralObjectsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceNeutralObjectsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::NeutralObjectsPlaced,
		&AGeneratorWorldCampaign::ExecutePlaceNeutralObjects);
}

void AGeneratorWorldCampaign::PlaceMissionsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::MissionsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceMissionsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::MissionsPlaced, &AGeneratorWorldCampaign::ExecutePlaceMissions);
}

void AGeneratorWorldCampaign::ExecuteAllSteps()
{
	if (M_GenerationStep != ECampaignGenerationStep::NotStarted)
	{
		EraseAllGeneration();
	}

	ExecuteAllStepsWithBacktracking();
}

void AGeneratorWorldCampaign::EraseAllGeneration()
{
	for (const FCampaignGenerationStepTransaction& Transaction : M_StepTransactions)
	{
		for (const TObjectPtr<AActor>& SpawnedActor : Transaction.SpawnedActors)
		{
			if (not IsValid(SpawnedActor))
			{
				continue;
			}

			SpawnedActor->Destroy();
		}

		for (const TObjectPtr<AConnection>& SpawnedConnection : Transaction.SpawnedConnections)
		{
			if (not IsValid(SpawnedConnection))
			{
				continue;
			}

			SpawnedConnection->Destroy();
		}
	}

	ClearExistingConnections();
	ClearPlacementState();
	ClearDerivedData();
	M_StepTransactions.Reset();
	M_StepAttemptIndices.Reset();
	M_TotalAttemptCount = 0;
	M_GenerationStep = ECampaignGenerationStep::NotStarted;

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	GatherAnchorPoints(AnchorPoints);
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->ClearConnections();
	}
}

bool AGeneratorWorldCampaign::GetIsValidPlayerHQAnchor() const
{
	if (M_PlacementState.PlayerHQAnchor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlacementState.PlayerHQAnchor"),
		TEXT("AGeneratorWorldCampaign::GetIsValidPlayerHQAnchor"), this);
	return false;
}

bool AGeneratorWorldCampaign::GetIsValidEnemyHQAnchor() const
{
	if (M_PlacementState.EnemyHQAnchor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlacementState.EnemyHQAnchor"),
		TEXT("AGeneratorWorldCampaign::GetIsValidEnemyHQAnchor"), this);
	return false;
}

bool AGeneratorWorldCampaign::ExecuteStepWithTransaction(ECampaignGenerationStep CompletedStep,
	bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&))
{
	if (not CanExecuteStep(CompletedStep))
	{
		return false;
	}

	FCampaignGenerationStepTransaction Transaction;
	Transaction.CompletedStep = CompletedStep;
	Transaction.PreviousPlacementState = M_PlacementState;
	Transaction.PreviousDerivedData = M_DerivedData;

	if (not (this->*StepFunction)(Transaction))
	{
		return false;
	}

	M_StepTransactions.Add(Transaction);
	M_GenerationStep = CompletedStep;
	ResetStepAttemptsFrom(CompletedStep);
	return true;
}

bool AGeneratorWorldCampaign::ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction)
{
	if (not ValidateGenerationRules())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	if (not PrepareAnchorsForConnectionGeneration(AnchorPoints))
	{
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::ConnectionsCreated);
	const int32 SeedOffset = AttemptIndex * AttemptSeedMultiplier;
	FRandomStream RandomStream(M_CountAndDifficultyTuning.Seed + SeedOffset);
	TMap<TObjectPtr<AAnchorPoint>, int32> DesiredConnections;
	GenerateConnectionsForAnchors(AnchorPoints, RandomStream, DesiredConnections);
	CacheGeneratedState(AnchorPoints);
	OutTransaction.SpawnedConnections = M_GeneratedConnections;
	return true;
}

bool AGeneratorWorldCampaign::PrepareAnchorsForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints)
{
	ClearExistingConnections();
	ClearPlacementState();
	ClearDerivedData();

	GatherAnchorPoints(OutAnchorPoints);
	if (OutAnchorPoints.Num() < 2)
	{
		return false;
	}

	Algo::Sort(OutAnchorPoints, [](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : OutAnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->ClearConnections();
	}

	return true;
}

void AGeneratorWorldCampaign::GenerateConnectionsForAnchors(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	FRandomStream& RandomStream, TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections)
{
	AssignDesiredConnections(AnchorPoints, RandomStream, OutDesiredConnections);

	TArray<TObjectPtr<AAnchorPoint>> ShuffledAnchors = AnchorPoints;
	RandomStream.Shuffle(ShuffledAnchors);

	TArray<FConnectionSegment> ExistingSegments;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : ShuffledAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
		{
			DebugNotifyAnchorProcessing(AnchorPoint, TEXT("Processing"), FColor::Cyan);
		}

		GeneratePhasePreferredConnections(AnchorPoint, AnchorPoints, OutDesiredConnections, ExistingSegments);
		GeneratePhaseExtendedConnections(AnchorPoint, AnchorPoints, ExistingSegments);
		GeneratePhaseThreeWayConnections(AnchorPoint, ExistingSegments);
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->SortNeighborsByKey();
	}
}

void AGeneratorWorldCampaign::CacheGeneratedState(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints)
{
	M_PlacementState.SeedUsed = M_CountAndDifficultyTuning.Seed;
	M_PlacementState.CachedAnchors.Reset();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_PlacementState.CachedAnchors.Add(AnchorPoint);
	}

	M_PlacementState.CachedConnections.Reset();
	for (const TObjectPtr<AConnection>& Connection : M_GeneratedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		M_PlacementState.CachedConnections.Add(Connection);
	}

	CacheAnchorConnectionDegrees();
}

bool AGeneratorWorldCampaign::ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction)
{
	(void)OutTransaction;

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no cached anchors available."));
		return false;
	}

	constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = M_PlayerHQPlacementRules.AnchorCandidates.Num() > 0
		? M_PlayerHQPlacementRules.AnchorCandidates
		: M_PlacementState.CachedAnchors;

	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	if (not BuildHQAnchorCandidates(CandidateSource, M_PlayerHQPlacementRules.MinAnchorDegreeForHQ,
		MaxAnchorDegreeUnlimited, nullptr, Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no valid anchor candidates found."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::PlayerHQPlaced);
	AAnchorPoint* AnchorPoint = SelectAnchorCandidateByAttempt(Candidates, AttemptIndex);
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: selected anchor was invalid."));
		return false;
	}

	M_PlacementState.PlayerHQAnchor = AnchorPoint;
	M_PlacementState.PlayerHQAnchorKey = AnchorPoint->GetAnchorKey();

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(AnchorPoint, TEXT("Player HQ"), FColor::Green);
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction)
{
	(void)OutTransaction;

	if (not GetIsValidPlayerHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: no cached anchors available."));
		return false;
	}

	constexpr int32 MinAnchorDegreeFloor = 0;
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = M_EnemyHQPlacementRules.AnchorCandidates.Num() > 0
		? M_EnemyHQPlacementRules.AnchorCandidates
		: M_PlacementState.CachedAnchors;

	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	const int32 MinAnchorDegree = FMath::Max(MinAnchorDegreeFloor, M_EnemyHQPlacementRules.MinAnchorDegree);
	const int32 MaxAnchorDegree = FMath::Max(MinAnchorDegree, M_EnemyHQPlacementRules.MaxAnchorDegree);
	if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegree, MaxAnchorDegree,
		M_PlacementState.PlayerHQAnchor.Get(), Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: no valid anchor candidates found."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyHQPlaced);
	AAnchorPoint* AnchorPoint = SelectAnchorCandidateByAttempt(Candidates, AttemptIndex);
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: selected anchor was invalid."));
		return false;
	}

	M_PlacementState.EnemyHQAnchor = AnchorPoint;
	M_PlacementState.EnemyHQAnchorKey = AnchorPoint->GetAnchorKey();

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(AnchorPoint, TEXT("Enemy HQ"), FColor::Red);
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction)
{
	(void)OutTransaction;

	const int32 RequiredEnemyItems = GetRequiredEnemyItemCount(M_CountAndDifficultyTuning);
	if (RequiredEnemyItems <= NoRequiredItems)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Enemy object placement is not implemented yet."));
	return false;
}

bool AGeneratorWorldCampaign::ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction)
{
	(void)OutTransaction;

	const int32 RequiredNeutralItems = GetRequiredNeutralItemCount(M_CountAndDifficultyTuning);
	if (RequiredNeutralItems <= NoRequiredItems)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Neutral object placement is not implemented yet."));
	return false;
}

bool AGeneratorWorldCampaign::ExecutePlaceMissions(FCampaignGenerationStepTransaction& OutTransaction)
{
	(void)OutTransaction;

	const int32 RequiredMissions = GetRequiredMissionCount(M_MissionPlacementRules);
	if (RequiredMissions <= NoRequiredItems)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(TEXT("Mission placement is not implemented yet."));
	return false;
}

bool AGeneratorWorldCampaign::ExecuteAllStepsWithBacktracking()
{
	TArray<ECampaignGenerationStep> StepOrder;
	StepOrder.Add(ECampaignGenerationStep::ConnectionsCreated);
	StepOrder.Add(ECampaignGenerationStep::PlayerHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::NeutralObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::MissionsPlaced);

	int32 StepIndex = 0;
	while (StepIndex < StepOrder.Num())
	{
		const ECampaignGenerationStep StepToExecute = StepOrder[StepIndex];
		bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&) = nullptr;
		switch (StepToExecute)
		{
		case ECampaignGenerationStep::ConnectionsCreated:
			StepFunction = &AGeneratorWorldCampaign::ExecuteCreateConnections;
			break;
		case ECampaignGenerationStep::PlayerHQPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceHQ;
			break;
		case ECampaignGenerationStep::EnemyHQPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ;
			break;
		case ECampaignGenerationStep::EnemyObjectsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyObjects;
			break;
		case ECampaignGenerationStep::NeutralObjectsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceNeutralObjects;
			break;
		case ECampaignGenerationStep::MissionsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceMissions;
			break;
		default:
			return false;
		}

		const bool bDidExecute = ExecuteStepWithTransaction(StepToExecute, StepFunction);
		if (bDidExecute)
		{
			StepIndex++;
			continue;
		}

		if (not HandleStepFailure(StepToExecute, StepIndex, StepOrder))
		{
			return false;
		}
	}

	M_GenerationStep = ECampaignGenerationStep::Finished;
	return true;
}

bool AGeneratorWorldCampaign::CanExecuteStep(ECampaignGenerationStep CompletedStep) const
{
	return M_GenerationStep == GetPrerequisiteStep(CompletedStep);
}

ECampaignGenerationStep AGeneratorWorldCampaign::GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const
{
	switch (CompletedStep)
	{
	case ECampaignGenerationStep::ConnectionsCreated:
		return ECampaignGenerationStep::NotStarted;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return ECampaignGenerationStep::ConnectionsCreated;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return ECampaignGenerationStep::PlayerHQPlaced;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return ECampaignGenerationStep::EnemyHQPlaced;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return ECampaignGenerationStep::EnemyObjectsPlaced;
	case ECampaignGenerationStep::MissionsPlaced:
		return ECampaignGenerationStep::NeutralObjectsPlaced;
	case ECampaignGenerationStep::Finished:
		return ECampaignGenerationStep::MissionsPlaced;
	case ECampaignGenerationStep::NotStarted:
	default:
		return ECampaignGenerationStep::NotStarted;
	}
}

ECampaignGenerationStep AGeneratorWorldCampaign::GetNextStep(ECampaignGenerationStep CurrentStep) const
{
	switch (CurrentStep)
	{
	case ECampaignGenerationStep::NotStarted:
		return ECampaignGenerationStep::ConnectionsCreated;
	case ECampaignGenerationStep::ConnectionsCreated:
		return ECampaignGenerationStep::PlayerHQPlaced;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return ECampaignGenerationStep::EnemyHQPlaced;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return ECampaignGenerationStep::EnemyObjectsPlaced;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return ECampaignGenerationStep::NeutralObjectsPlaced;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return ECampaignGenerationStep::MissionsPlaced;
	case ECampaignGenerationStep::MissionsPlaced:
		return ECampaignGenerationStep::Finished;
	default:
		return ECampaignGenerationStep::Finished;
	}
}

int32 AGeneratorWorldCampaign::GetStepAttemptIndex(ECampaignGenerationStep CompletedStep) const
{
	const int32* AttemptIndex = M_StepAttemptIndices.Find(CompletedStep);
	return AttemptIndex ? *AttemptIndex : 0;
}

void AGeneratorWorldCampaign::IncrementStepAttempt(ECampaignGenerationStep CompletedStep)
{
	const int32 CurrentAttempt = GetStepAttemptIndex(CompletedStep);
	M_StepAttemptIndices.Add(CompletedStep, CurrentAttempt + 1);
}

void AGeneratorWorldCampaign::ResetStepAttemptsFrom(ECampaignGenerationStep CompletedStep)
{
	ECampaignGenerationStep StepToReset = GetNextStep(CompletedStep);
	while (StepToReset != ECampaignGenerationStep::Finished)
	{
		M_StepAttemptIndices.Remove(StepToReset);
		StepToReset = GetNextStep(StepToReset);
	}
}

EPlacementFailurePolicy AGeneratorWorldCampaign::GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const
{
	const EPlacementFailurePolicy GlobalPolicy = M_PlacementFailurePolicy.GlobalPolicy;
	switch (FailedStep)
	{
	case ECampaignGenerationStep::ConnectionsCreated:
		return M_PlacementFailurePolicy.CreateConnectionsPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.CreateConnectionsPolicy
			: GlobalPolicy;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return M_PlacementFailurePolicy.PlaceHQPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.PlaceHQPolicy
			: GlobalPolicy;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return M_PlacementFailurePolicy.PlaceEnemyHQPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.PlaceEnemyHQPolicy
			: GlobalPolicy;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return M_PlacementFailurePolicy.PlaceEnemyObjectsPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.PlaceEnemyObjectsPolicy
			: GlobalPolicy;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return M_PlacementFailurePolicy.PlaceNeutralObjectsPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.PlaceNeutralObjectsPolicy
			: GlobalPolicy;
	case ECampaignGenerationStep::MissionsPlaced:
		return M_PlacementFailurePolicy.PlaceMissionsPolicy != EPlacementFailurePolicy::NotSet
			? M_PlacementFailurePolicy.PlaceMissionsPolicy
			: GlobalPolicy;
	default:
		return GlobalPolicy;
	}
}

bool AGeneratorWorldCampaign::HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
	const TArray<ECampaignGenerationStep>& StepOrder)
{
	IncrementStepAttempt(FailedStep);
	M_TotalAttemptCount++;

	if (GetStepAttemptIndex(FailedStep) > MaxStepAttempts)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: maximum step attempts exceeded."));
		return false;
	}

	if (M_TotalAttemptCount > MaxTotalAttempts)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: maximum total attempts exceeded."));
		return false;
	}

	const EPlacementFailurePolicy FailurePolicy = GetFailurePolicyForStep(FailedStep);
	if (FailurePolicy == EPlacementFailurePolicy::NotSet)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: no failure policy set for step."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);

	TArray<ECampaignGenerationStep> EscalationSteps;
	BuildBacktrackEscalationSteps(FailedStep, EscalationSteps);
	const int32 MaxBacktrackSteps = EscalationSteps.Num();
	const int32 TransactionsToUndo = FMath::Clamp(AttemptIndex - 1, 0, MaxBacktrackSteps);
	const int32 AvailableTransactions = M_StepTransactions.Num();
	const int32 UndoCount = FMath::Min(TransactionsToUndo, AvailableTransactions);

	for (int32 StepCount = 0; StepCount < UndoCount; StepCount++)
	{
		UndoLastTransaction();
	}

	InOutStepIndex = StepOrder.IndexOfByKey(GetNextStep(M_GenerationStep));
	if (InOutStepIndex == INDEX_NONE)
	{
		InOutStepIndex = 0;
	}

	return true;
}

void AGeneratorWorldCampaign::UndoLastTransaction()
{
	if (M_StepTransactions.Num() == 0)
	{
		M_GenerationStep = ECampaignGenerationStep::NotStarted;
		return;
	}

	const FCampaignGenerationStepTransaction Transaction = M_StepTransactions.Pop();
	for (const TObjectPtr<AActor>& SpawnedActor : Transaction.SpawnedActors)
	{
		if (not IsValid(SpawnedActor))
		{
			continue;
		}

		SpawnedActor->Destroy();
	}

	switch (Transaction.CompletedStep)
	{
	case ECampaignGenerationStep::ConnectionsCreated:
		UndoConnections(Transaction);
		break;
	default:
		break;
	}

	M_PlacementState = Transaction.PreviousPlacementState;
	M_DerivedData = Transaction.PreviousDerivedData;
	M_GenerationStep = GetPrerequisiteStep(Transaction.CompletedStep);
}

void AGeneratorWorldCampaign::UndoConnections(const FCampaignGenerationStepTransaction& Transaction)
{
	for (const TObjectPtr<AConnection>& Connection : Transaction.SpawnedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		Connection->Destroy();
	}

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	GatherAnchorPoints(AnchorPoints);
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->ClearConnections();
	}

	M_GeneratedConnections.Reset();
}

void AGeneratorWorldCampaign::ClearPlacementState()
{
	M_PlacementState = FWorldCampaignPlacementState();
}

void AGeneratorWorldCampaign::ClearDerivedData()
{
	M_DerivedData = FWorldCampaignDerivedData();
}

void AGeneratorWorldCampaign::CacheAnchorConnectionDegrees()
{
	M_DerivedData.AnchorConnectionDegreesByAnchorKey.Reset();
	for (const TWeakObjectPtr<AAnchorPoint>& AnchorPointWeak : M_PlacementState.CachedAnchors)
	{
		AAnchorPoint* AnchorPoint = AnchorPointWeak.Get();
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_DerivedData.AnchorConnectionDegreesByAnchorKey.Add(AnchorPoint->GetAnchorKey(),
			AnchorPoint->GetConnectionCount());
	}
}

void AGeneratorWorldCampaign::DebugNotifyAnchorPicked(const AAnchorPoint* AnchorPoint, const FString& Label,
	const FColor& Color) const
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	AnchorPoint->DebugDrawAnchorState(Label, Color, DebugAnchorDurationSeconds);
}

bool AGeneratorWorldCampaign::ValidateGenerationRules() const
{
	if (ConnectionGenerationRules.MinConnections < 0)
	{
		return false;
	}

	if (ConnectionGenerationRules.MaxConnections < ConnectionGenerationRules.MinConnections)
	{
		return false;
	}

	if (ConnectionGenerationRules.MaxPreferredDistance < 0.f)
	{
		return false;
	}

	return ConnectionGenerationRules.MaxConnections >= 0;
}

int32 AGeneratorWorldCampaign::GetAnchorConnectionDegree(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return 0;
	}

	const int32* CachedDegree = M_DerivedData.AnchorConnectionDegreesByAnchorKey.Find(AnchorPoint->GetAnchorKey());
	return CachedDegree ? *CachedDegree : AnchorPoint->GetConnectionCount();
}

bool AGeneratorWorldCampaign::IsAnchorCached(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	for (const TWeakObjectPtr<AAnchorPoint>& CachedAnchor : M_PlacementState.CachedAnchors)
	{
		if (CachedAnchor.Get() == AnchorPoint)
		{
			return true;
		}
	}

	return false;
}

bool AGeneratorWorldCampaign::BuildHQAnchorCandidates(const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	int32 MinDegree, int32 MaxDegree, const AAnchorPoint* AnchorToExclude,
	TArray<TObjectPtr<AAnchorPoint>>& OutCandidates) const
{
	OutCandidates.Reset();
	OutCandidates.Reserve(CandidateSource.Num());

	for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
	{
		if (not IsValid(CandidateAnchor))
		{
			continue;
		}

		if (not IsAnchorCached(CandidateAnchor))
		{
			continue;
		}

		if (AnchorToExclude != nullptr && CandidateAnchor == AnchorToExclude)
		{
			continue;
		}

		const int32 ConnectionDegree = GetAnchorConnectionDegree(CandidateAnchor);
		if (ConnectionDegree < MinDegree || ConnectionDegree > MaxDegree)
		{
			continue;
		}

		OutCandidates.Add(CandidateAnchor);
	}

	if (OutCandidates.Num() == 0)
	{
		return false;
	}

	Algo::Sort(OutCandidates, [](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});

	return true;
}

AAnchorPoint* AGeneratorWorldCampaign::SelectAnchorCandidateByAttempt(const TArray<TObjectPtr<AAnchorPoint>>& Candidates,
	int32 AttemptIndex) const
{
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	const int32 CandidateIndex = AttemptIndex % Candidates.Num();
	return Candidates[CandidateIndex].Get();
}

bool AGeneratorWorldCampaign::HasSharedEndpoint(const FConnectionSegment& Segment, const AAnchorPoint* AnchorA,
	const AAnchorPoint* AnchorB) const
{
	return Segment.StartAnchor.Get() == AnchorA || Segment.EndAnchor.Get() == AnchorA
		|| Segment.StartAnchor.Get() == AnchorB || Segment.EndAnchor.Get() == AnchorB;
}

bool AGeneratorWorldCampaign::IsSegmentIntersectingExisting(const FVector2D& StartPoint, const FVector2D& EndPoint,
	const AAnchorPoint* StartAnchor, const AAnchorPoint* EndAnchor,
	const TArray<FConnectionSegment>& ExistingSegments, const AConnection* ConnectionToIgnore) const
{
	for (const FConnectionSegment& Segment : ExistingSegments)
	{
		if (Segment.OwningConnection.Get() == ConnectionToIgnore)
		{
			continue;
		}

		if (HasSharedEndpoint(Segment, StartAnchor, EndAnchor))
		{
			continue;
		}

		if (DoSegmentsIntersect(StartPoint, EndPoint, Segment.StartPoint, Segment.EndPoint))
		{
			return true;
		}
	}

	return false;
}

void AGeneratorWorldCampaign::ClearExistingConnections()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (TActorIterator<AConnection> ConnectionIterator(World); ConnectionIterator; ++ConnectionIterator)
	{
		AConnection* Connection = *ConnectionIterator;
		if (not IsValid(Connection))
		{
			continue;
		}

		Connection->Destroy();
	}

	M_GeneratedConnections.Reset();
}

void AGeneratorWorldCampaign::GatherAnchorPoints(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	OutAnchorPoints.Reset();
	for (TActorIterator<AAnchorPoint> AnchorIterator(World); AnchorIterator; ++AnchorIterator)
	{
		AAnchorPoint* AnchorPoint = *AnchorIterator;
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		OutAnchorPoints.Add(AnchorPoint);
	}
}

void AGeneratorWorldCampaign::AssignDesiredConnections(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	FRandomStream& RandomStream, TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections) const
{
	OutDesiredConnections.Reset();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const int32 DesiredConnections = RandomStream.RandRange(ConnectionGenerationRules.MinConnections,
			ConnectionGenerationRules.MaxConnections);
		OutDesiredConnections.Add(AnchorPoint, DesiredConnections);
	}
}

void AGeneratorWorldCampaign::GeneratePhasePreferredConnections(AAnchorPoint* AnchorPoint,
	const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	const TMap<TObjectPtr<AAnchorPoint>, int32>& DesiredConnections,
	TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	const int32* DesiredConnectionsPtr = DesiredConnections.Find(AnchorPoint);
	const int32 DesiredConnectionCount = DesiredConnectionsPtr ? *DesiredConnectionsPtr : ConnectionGenerationRules.MinConnections;
	if (AnchorPoint->GetConnectionCount() >= DesiredConnectionCount)
	{
		return;
	}

	TArray<FAnchorCandidate> Candidates = BuildAndSortCandidates(AnchorPoint, AnchorPoints,
		ConnectionGenerationRules.MaxConnections, ConnectionGenerationRules.MaxPreferredDistance, false);

	const FColor RegularConnectionColor = FColor::Green;
	for (const FAnchorCandidate& Candidate : Candidates)
	{
		if (AnchorPoint->GetConnectionCount() >= DesiredConnectionCount)
		{
			break;
		}

		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
		{
			break;
		}

		if (not IsValid(Candidate.AnchorPoint))
		{
			continue;
		}

		TryCreateConnection(AnchorPoint, Candidate.AnchorPoint, ExistingSegments, RegularConnectionColor);
	}
}

void AGeneratorWorldCampaign::GeneratePhaseExtendedConnections(AAnchorPoint* AnchorPoint,
	const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MinConnections)
	{
		return;
	}

	TArray<FAnchorCandidate> Candidates = BuildAndSortCandidates(AnchorPoint, AnchorPoints,
		ConnectionGenerationRules.MaxConnections, ConnectionGenerationRules.MaxPreferredDistance, true);

	const FColor ExtendedConnectionColor = FColor::Yellow;
	for (const FAnchorCandidate& Candidate : Candidates)
	{
		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MinConnections)
		{
			break;
		}

		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
		{
			break;
		}

		if (not IsValid(Candidate.AnchorPoint))
		{
			continue;
		}

		TryCreateConnection(AnchorPoint, Candidate.AnchorPoint, ExistingSegments, ExtendedConnectionColor);
	}
}

void AGeneratorWorldCampaign::GeneratePhaseThreeWayConnections(AAnchorPoint* AnchorPoint,
	TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	while (AnchorPoint->GetConnectionCount() < ConnectionGenerationRules.MinConnections)
	{
		if (not TryAddThreeWayConnection(AnchorPoint, ExistingSegments))
		{
			break;
		}
	}
}

bool AGeneratorWorldCampaign::TryCreateConnection(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
	TArray<FConnectionSegment>& ExistingSegments, const FColor& DebugColor)
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	if (not IsValid(CandidateAnchor))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	const FVector CandidateLocation = CandidateAnchor->GetActorLocation();
	const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
	const FVector2D CandidateLocation2D(CandidateLocation.X, CandidateLocation.Y);

	if (not IsConnectionAllowed(AnchorPoint, CandidateAnchor, AnchorLocation2D, CandidateLocation2D, ExistingSegments, nullptr))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	constexpr float HalfValue = 0.5f;
	const FVector SpawnLocation = (AnchorLocation + CandidateLocation) * HalfValue;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;

	const TSubclassOf<AConnection> ConnectionClassToSpawn = M_ConnectionClass ? M_ConnectionClass : AConnection::StaticClass();
	AConnection* NewConnection = World->SpawnActor<AConnection>(ConnectionClassToSpawn, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(NewConnection))
	{
		return false;
	}

	NewConnection->InitializeConnection(AnchorPoint, CandidateAnchor);
	RegisterConnectionOnAnchors(NewConnection, AnchorPoint, CandidateAnchor);
	AddConnectionSegment(NewConnection, AnchorPoint, CandidateAnchor, ExistingSegments);
	M_GeneratedConnections.Add(NewConnection);

	if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
	{
		DebugDrawConnection(NewConnection, DebugColor);
	}

	return true;
}

bool AGeneratorWorldCampaign::IsConnectionAllowed(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
	const FVector2D& StartPoint, const FVector2D& EndPoint, const TArray<FConnectionSegment>& ExistingSegments,
	const AConnection* ConnectionToIgnore) const
{
	if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
	{
		return false;
	}

	if (CandidateAnchor->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
	{
		return false;
	}

	return not IsSegmentIntersectingExisting(StartPoint, EndPoint, AnchorPoint, CandidateAnchor,
		ExistingSegments, ConnectionToIgnore);
}

bool AGeneratorWorldCampaign::TryAddThreeWayConnection(AAnchorPoint* AnchorPoint,
	TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	if (M_GeneratedConnections.Num() == 0)
	{
		return false;
	}

	const FClosestConnectionCandidate ClosestCandidate = FindClosestConnectionCandidate(AnchorPoint, M_GeneratedConnections);
	AConnection* ClosestConnection = ClosestCandidate.Connection.Get();
	if (not IsValid(ClosestConnection))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
	const FVector2D JunctionLocation2D(ClosestCandidate.JunctionLocation.X, ClosestCandidate.JunctionLocation.Y);
	if (IsSegmentIntersectingExisting(AnchorLocation2D, JunctionLocation2D, AnchorPoint, nullptr,
		ExistingSegments, ClosestConnection))
	{
		return false;
	}

	if (not ClosestConnection->TryAddThirdAnchor(AnchorPoint))
	{
		return false;
	}

	RegisterThirdAnchorOnConnection(ClosestConnection, AnchorPoint);
	AddThirdConnectionSegment(ClosestConnection, AnchorPoint, ClosestCandidate.JunctionLocation, ExistingSegments);

	if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
	{
		const FColor ThreeWayConnectionColor = FColor::Red;
		DebugDrawThreeWay(ClosestConnection, ThreeWayConnectionColor);
	}

	return true;
}

void AGeneratorWorldCampaign::RegisterConnectionOnAnchors(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(AnchorA))
	{
		return;
	}

	if (not IsValid(AnchorB))
	{
		return;
	}

	AnchorA->AddConnection(Connection, AnchorB);
	AnchorB->AddConnection(Connection, AnchorA);
}

void AGeneratorWorldCampaign::RegisterThirdAnchorOnConnection(AConnection* Connection, AAnchorPoint* ThirdAnchor) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(ThirdAnchor))
	{
		return;
	}

	const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
	if (ConnectedAnchors.Num() < 2)
	{
		return;
	}

	AAnchorPoint* FirstAnchor = ConnectedAnchors[0];
	AAnchorPoint* SecondAnchor = ConnectedAnchors[1];
	if (not IsValid(FirstAnchor) || not IsValid(SecondAnchor))
	{
		return;
	}

	ThirdAnchor->AddConnection(Connection, FirstAnchor);
	ThirdAnchor->AddConnection(Connection, SecondAnchor);

	FirstAnchor->AddConnection(Connection, ThirdAnchor);
	SecondAnchor->AddConnection(Connection, ThirdAnchor);
}

void AGeneratorWorldCampaign::AddConnectionSegment(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB,
	TArray<FConnectionSegment>& ExistingSegments) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(AnchorA) || not IsValid(AnchorB))
	{
		return;
	}

	const FVector LocationA = AnchorA->GetActorLocation();
	const FVector LocationB = AnchorB->GetActorLocation();

	FConnectionSegment Segment;
	Segment.StartPoint = FVector2D(LocationA.X, LocationA.Y);
	Segment.EndPoint = FVector2D(LocationB.X, LocationB.Y);
	Segment.StartAnchor = AnchorA;
	Segment.EndAnchor = AnchorB;
	Segment.OwningConnection = Connection;
	ExistingSegments.Add(Segment);
}

void AGeneratorWorldCampaign::AddThirdConnectionSegment(AConnection* Connection, AAnchorPoint* ThirdAnchor,
	const FVector& JunctionLocation, TArray<FConnectionSegment>& ExistingSegments) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(ThirdAnchor))
	{
		return;
	}

	const FVector ThirdAnchorLocation = ThirdAnchor->GetActorLocation();
	FConnectionSegment Segment;
	Segment.StartPoint = FVector2D(ThirdAnchorLocation.X, ThirdAnchorLocation.Y);
	Segment.EndPoint = FVector2D(JunctionLocation.X, JunctionLocation.Y);
	Segment.StartAnchor = ThirdAnchor;
	Segment.EndAnchor = nullptr;
	Segment.OwningConnection = Connection;
	ExistingSegments.Add(Segment);
}

void AGeneratorWorldCampaign::DebugNotifyAnchorProcessing(const AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	constexpr float DebugDuration = 10.f;
	AnchorPoint->DebugDrawAnchorState(Label, Color, DebugDuration);
}

void AGeneratorWorldCampaign::DebugDrawConnection(const AConnection* Connection, const FColor& Color) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	constexpr float DebugDuration = 10.f;
	Connection->DebugDrawBaseConnection(Color, DebugDuration);
}

void AGeneratorWorldCampaign::DebugDrawThreeWay(const AConnection* Connection, const FColor& Color) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	constexpr float DebugDuration = 10.f;
	Connection->DebugDrawThirdConnection(Color, DebugDuration);
}
