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

FStrategicAIRequestBatch::FStrategicAIRequestBatch()
{
}

FStrategicAIResultBatch::FStrategicAIResultBatch()
{
}
