// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnPlanning.h"

#include "Math/RandomStream.h"
#include "Templates/TypeHash.h"

namespace WorldEnemyTurnPlanningConstants
{
	// Last part of every reaction piece key; keeps the key valid and apart from other generated keys.
	constexpr uint32 ReactionPieceKeySalt = 0x45545552;
	constexpr float SpawnFanStepDegrees = 25.f;
	constexpr float MinDirectionSizeSquared = 1.f;

	constexpr EWorldFortificationStrength DeliveryModifierOrder[] = {
		EWorldFortificationStrength::Bunkers,
		EWorldFortificationStrength::Minefields,
		EWorldFortificationStrength::AABattery
	};
}

namespace WorldEnemyTurnPlanningPrivate
{
	struct FRankedPlace
	{
		int32 Index = INDEX_NONE;
		double DistanceSquared = 0.0;
		FGuid Key;
	};

	double GetDistanceSquaredToNearest(const FVector2D& Location, const TArray<FVector2D>& Others)
	{
		double NearestDistanceSquared = TNumericLimits<double>::Max();
		for (const FVector2D& Other : Others)
		{
			NearestDistanceSquared = FMath::Min(NearestDistanceSquared, FVector2D::DistSquared(Location, Other));
		}
		return NearestDistanceSquared;
	}

	bool GetIsRankedBefore(const FRankedPlace& Left, const FRankedPlace& Right)
	{
		if (Left.DistanceSquared != Right.DistanceSquared)
		{
			return Left.DistanceSquared < Right.DistanceSquared;
		}
		return Left.Key < Right.Key;
	}
}

bool WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(const FWorldEnemyTurnState& State,
                                                                  const int32 MissionsCompleted)
{
	return State.RecentlyStruckAnchorKeys.Num() > 0
		|| State.RecentlyStruckLocations.Num() > 0
		|| MissionsCompleted > State.MissionsCompletedAtLastEnemyTurn;
}

FWorldEnemyReactionPurchase WorldEnemyTurnPlanning::PlanReactionPurchase(const int32 AvailablePoints,
                                                                         const int32 ConvoyCost,
                                                                         const int32 DivisionCost,
                                                                         const int32 MaxPieces)
{
	FWorldEnemyReactionPurchase Purchase;
	const int32 SafeConvoyCost = FMath::Max(1, ConvoyCost);
	const int32 SafeDivisionCost = FMath::Max(1, DivisionCost);
	const int32 SafeMaxPieces = FMath::Max(0, MaxPieces);
	int32 RemainingPoints = FMath::Max(0, AvailablePoints);

	while (Purchase.GetPieceCount() < SafeMaxPieces && RemainingPoints >= SafeDivisionCost)
	{
		Purchase.DivisionCount++;
		RemainingPoints -= SafeDivisionCost;
	}

	while (Purchase.GetPieceCount() < SafeMaxPieces && RemainingPoints >= SafeConvoyCost)
	{
		Purchase.ConvoyCount++;
		RemainingPoints -= SafeConvoyCost;
	}

	Purchase.PointsLeft = RemainingPoints;
	Purchase.PointsSpent = FMath::Max(0, AvailablePoints) - RemainingPoints;
	return Purchase;
}

int32 WorldEnemyTurnPlanning::GetBankedPointsAfterPurchase(const FWorldEnemyReactionPurchase& Purchase,
                                                           const int32 MaxBankedPoints)
{
	return FMath::Clamp(Purchase.PointsLeft, 0, FMath::Max(0, MaxBankedPoints));
}

TArray<int32> WorldEnemyTurnPlanning::SelectReactionTargetIndices(const TArray<FVector2D>& StruckLocations,
                                                                  const TArray<FWorldEnemyTurnPlace>& Candidates,
                                                                  const int32 MaxTargets)
{
	TArray<int32> TargetIndices;
	if (StruckLocations.Num() == 0 || Candidates.Num() == 0 || MaxTargets <= 0)
	{
		return TargetIndices;
	}

	using WorldEnemyTurnPlanningPrivate::FRankedPlace;
	TArray<FRankedPlace> RankedPlaces;
	RankedPlaces.Reserve(Candidates.Num());
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); CandidateIndex++)
	{
		FRankedPlace RankedPlace;
		RankedPlace.Index = CandidateIndex;
		RankedPlace.Key = Candidates[CandidateIndex].Key;
		RankedPlace.DistanceSquared = WorldEnemyTurnPlanningPrivate::GetDistanceSquaredToNearest(
			Candidates[CandidateIndex].Location, StruckLocations);
		RankedPlaces.Add(RankedPlace);
	}

	RankedPlaces.Sort([](const FRankedPlace& Left, const FRankedPlace& Right)
	{
		return WorldEnemyTurnPlanningPrivate::GetIsRankedBefore(Left, Right);
	});
	const int32 TargetCount = FMath::Min(MaxTargets, RankedPlaces.Num());
	for (int32 RankIndex = 0; RankIndex < TargetCount; RankIndex++)
	{
		TargetIndices.Add(RankedPlaces[RankIndex].Index);
	}
	return TargetIndices;
}

int32 WorldEnemyTurnPlanning::SelectNearestPlaceIndex(const FVector2D& Location,
                                                      const TArray<FWorldEnemyTurnPlace>& Places)
{
	const TArray<FVector2D> SingleLocation = {Location};
	const TArray<int32> NearestIndices = SelectReactionTargetIndices(SingleLocation, Places, 1);
	return NearestIndices.Num() > 0 ? NearestIndices[0] : INDEX_NONE;
}

int32 WorldEnemyTurnPlanning::GetTargetIndexForPiece(const int32 PieceIndex, const int32 TargetCount)
{
	if (TargetCount <= 0)
	{
		return INDEX_NONE;
	}
	return FMath::Max(0, PieceIndex) % TargetCount;
}

TArray<EWorldFieldDivisions> WorldEnemyTurnPlanning::PlanPieceTypes(const FWorldEnemyReactionPurchase& Purchase,
                                                                    const TArray<EWorldFieldDivisions>& DivisionTypes,
                                                                    const int32 TurnSeed)
{
	TArray<EWorldFieldDivisions> PieceTypes;
	const FRandomStream RandomStream(TurnSeed);
	if (DivisionTypes.Num() > 0)
	{
		for (int32 DivisionIndex = 0; DivisionIndex < Purchase.DivisionCount; DivisionIndex++)
		{
			PieceTypes.Add(DivisionTypes[RandomStream.RandRange(0, DivisionTypes.Num() - 1)]);
		}
	}

	for (int32 ConvoyIndex = 0; ConvoyIndex < Purchase.ConvoyCount; ConvoyIndex++)
	{
		PieceTypes.Add(EWorldFieldDivisions::SovietSupplyConvoy);
	}
	return PieceTypes;
}

int32 WorldEnemyTurnPlanning::BuildEnemyTurnSeed(const int32 CampaignSeed, const int32 EnemyTurnsPlayed)
{
	return static_cast<int32>(HashCombine(GetTypeHash(CampaignSeed), GetTypeHash(EnemyTurnsPlayed)));
}

FGuid WorldEnemyTurnPlanning::BuildReactionPieceKey(const int32 CampaignSeed, const int32 EnemyTurnsPlayed,
                                                    const int32 PieceIndex)
{
	return FGuid(GetTypeHash(CampaignSeed),
	             static_cast<uint32>(EnemyTurnsPlayed),
	             static_cast<uint32>(PieceIndex),
	             WorldEnemyTurnPlanningConstants::ReactionPieceKeySalt);
}

EWorldFortificationStrength WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(
	const TArray<EWorldFortificationStrength>& ExistingModifiers, const int32 TurnSeed)
{
	const EWorldFortificationStrength* DeliveryModifierOrder = WorldEnemyTurnPlanningConstants::DeliveryModifierOrder;
	constexpr int32 ModifierCount = UE_ARRAY_COUNT(WorldEnemyTurnPlanningConstants::DeliveryModifierOrder);
	const int32 StartOffset = static_cast<int32>(static_cast<uint32>(TurnSeed) % ModifierCount);
	for (int32 StepIndex = 0; StepIndex < ModifierCount; StepIndex++)
	{
		const EWorldFortificationStrength Candidate = DeliveryModifierOrder[(StartOffset + StepIndex) % ModifierCount];
		if (not ExistingModifiers.Contains(Candidate))
		{
			return Candidate;
		}
	}
	return EWorldFortificationStrength::None;
}

FVector2D WorldEnemyTurnPlanning::BuildSpawnDirection(const FVector2D& SourceLocation,
                                                      const FVector2D& TargetLocation,
                                                      const int32 PieceIndexAtSource)
{
	FVector2D BaseDirection = TargetLocation - SourceLocation;
	BaseDirection = BaseDirection.SizeSquared() > WorldEnemyTurnPlanningConstants::MinDirectionSizeSquared
		                ? BaseDirection.GetSafeNormal()
		                : FVector2D(1.f, 0.f);

	// 0 -> straight, 1 -> +step, 2 -> -step, 3 -> +2 steps, ...
	const int32 SafePieceIndex = FMath::Max(0, PieceIndexAtSource);
	const int32 FanStepCount = (SafePieceIndex + 1) / 2;
	const float FanSign = SafePieceIndex % 2 == 1 ? 1.f : -1.f;
	const float FanAngleRadians = FMath::DegreesToRadians(
		WorldEnemyTurnPlanningConstants::SpawnFanStepDegrees * FanStepCount * FanSign);
	const float Cosine = FMath::Cos(FanAngleRadians);
	const float Sine = FMath::Sin(FanAngleRadians);
	return FVector2D(BaseDirection.X * Cosine - BaseDirection.Y * Sine,
	                 BaseDirection.X * Sine + BaseDirection.Y * Cosine);
}
