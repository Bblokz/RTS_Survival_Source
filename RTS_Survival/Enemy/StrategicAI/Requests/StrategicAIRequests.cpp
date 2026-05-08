#include "StrategicAIRequests.h"

FWeakActorLocations::FWeakActorLocations()
	: Actor(nullptr)
{
}

FFindClosestFlankableEnemyHeavy::FFindClosestFlankableEnemyHeavy()
	: RequestID(0)
	, StartSearchLocation(FVector::ZeroVector)
	, MaxHeavyTanksToFlank(0)
	, MaxSuggestedFlankPositionsPerTank(0)
	, DeltaYawFromLeftRight(0.f)
	, MinDistanceToTank(0.f)
	, MaxDistanceToTank(0.f)
	, FlankingPositionsSpreadScaler(1.f)
{
}

FResultClosestFlankableEnemyHeavy::FResultClosestFlankableEnemyHeavy()
	: RequestID(0)
{
}

FGetPlayerUnitCountsAndBase::FGetPlayerUnitCountsAndBase()
	: RequestID(0)
{
}

FResultPlayerUnitCounts::FResultPlayerUnitCounts()
	: RequestID(0)
	, PlayerArmoredCars(0)
	, PlayerLightTanks(0)
	, PlayerMediumTanks(0)
	, PlayerHeavyTanks(0)
	, PlayerSquads(0)
	, PlayerNomadicVehicles(0)
	, PlayerHQLocation(FVector::ZeroVector)
{
}

FFindAlliedTanksToRetreat::FFindAlliedTanksToRetreat()
	: RequestID(0)
	, MaxTanksToFind(12)
	, MaxDistanceFromOtherGroupMembers(4000.f)
	, MaxIdleHazmatsToConsider(8)
	, HealthRatioThresholdConsiderUnitToRetreat(0.3f)
	, RetreatFormationDistance(0.f)
	, MaxFormationWidth(0)
{
}

FDamagedTanksRetreatGroup::FDamagedTanksRetreatGroup()
{
}

FResultAlliedTanksToRetreat::FResultAlliedTanksToRetreat()
	: RequestID(0)
{
}

FEnemyBaseDefenseArcCandidateParams::FEnemyBaseDefenseArcCandidateParams()
	: MaxThreatLocationsPerBase(3)
	, ThreatLocationMergeDistanceXY(3000.f)
	, ThreatDirectionMergeAngleDegrees(25.f)
	, MaxAnchorBuildingsPerThreatDirection(2)
	, CoreBuildingAnchorWeight(1.25f)
	, SatelliteBuildingAnchorWeight(1.f)
	, MinOffsetFromDefendedBuildingXY(700.f)
	, PreferredOffsetFromDefendedBuildingXY(1500.f)
	, MaxOffsetFromDefendedBuildingXY(3500.f)
	, MinOffsetFromAnyBaseBuildingXY(600.f)
	, MinDistanceDefPositionFromAlliedUnits(500.f)
	, PointsPerArc(5)
	, ArcAngleDegrees(90.f)
	, ArcRowsPerAnchor(2)
	, ArcRowOffsetXY(900.f)
	, ArcRowAngleScale(1.15f)
	, MinPointSpacingXY(650.f)
	, PositionJitterXY(150.f)
	, ArcDistanceJitterRatio(0.06f)
	, MaxYawOffsetFromThreatDegrees(25.f)
	, OuterArcYawScale(1.1f)
	, YawJitterDegrees(4.f)
	, DuplicateCandidateMergeDistanceXY(500.f)
	, MaxDefensePointCandidatesPerBase(24)
	, MaxDefensePointCandidatesTotal(48)
{
}

FDefensePositions::FDefensePositions()
	: Location(FVector::ZeroVector)
	, YawRotation(0.f)
{
}

FFindEnemyBaseClusters::FFindEnemyBaseClusters()
	: RequestID(0)
	, CoreClusterDistanceXY(1500.f)
	, MinCoreNeighbors(2)
	, MinTotalBuildingsPerBase(4)
	, MaxBasesToReturn(2)
	, MinBaseScoreToReturn(100.f)
{
}

FResultEnemyBaseClusters::FResultEnemyBaseClusters()
	: RequestID(0)
{
}

FFindLocationsUnderPlayerAttack::FFindLocationsUnderPlayerAttack()
	: RequestID(0)
	, MaxInfluenceRadius(9000.f)
	, DistanceExponent(1.6f)
	, MinimumThreatScoreToFlagLocation(5.f)
	, MinimumEffectiveAttackerCount(1.5f)
	, GroupAmplifierPerExtraEffectiveAttacker(0.33f)
	, SquadThreatScore(1.f)
	, LightTankThreatScore(2.f)
	, MediumTankThreatScore(3.5f)
	, HeavyTankThreatScore(5.f)
	, ArmoredCarThreatScore(1.5f)
{
}

FResultLocationsUnderPlayerAttack::FResultLocationsUnderPlayerAttack()
	: RequestID(0)
{
}

FFindPlayerUnitBulkLocations::FFindPlayerUnitBulkLocations()
	: RequestID(0)
	, ClusterRadiusXY(4500.f)
	, MinUnitsPerBulk(3)
	, MinWeightedBulkScore(3.f)
	, MaxAverageBulkRadiusXY(3000.f)
	, MaxBulksToReturn(8)
	, SquadBulkScore(1.f)
	, LightTankBulkScore(2.f)
	, MediumTankBulkScore(3.5f)
	, HeavyTankBulkScore(5.f)
	, ArmoredCarBulkScore(1.5f)
{
}

FResultPlayerUnitBulkLocations::FResultPlayerUnitBulkLocations()
	: RequestID(0)
{
}

FStrategicAIRequestBatch::FStrategicAIRequestBatch()
{
}

FStrategicAIResultBatch::FStrategicAIResultBatch()
{
}
