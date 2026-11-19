// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrategicAIComponent.h"

#include "Components/SplineComponent.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyBaseTrenchComponent/EnemyBaseTrenchComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardQueries/BlackboardQueryHelpers.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticHelpers/StochasticHelpers.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyBuildingTrainer/EnemyBuildingTrainerComponent.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "NavigationData.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

// Where building-trained units are sent once spawned (AI_TRAIN defense_position).
DEFINE_LOG_CATEGORY_STATIC(LogEnemyTrainedUnitOrders, Log, All);

namespace EnemyStrategicAIMineLocationConstants
{
	constexpr float RoadSplineSampleSpacing = 750.f;
}

namespace EnemyStrategicAIBasesInCombatConstants
{
	// A base counts as in combat with less pressure than a designer attack location: one tank nearby is enough.
	constexpr float DefaultMaxInfluenceRadius = 6000.f;
	constexpr float DefaultMinimumThreatScoreToFlagBase = 1.5f;
	constexpr float DefaultMinimumEffectiveAttackerCount = 0.5f;
}

UEnemyStrategicAIComponent::UEnemyStrategicAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CacheGenerationSeedFromGameInstance();

	using namespace EnemyStrategicAIBasesInCombatConstants;
	FindBasesInCombat_TimerRequest.MaxInfluenceRadius = DefaultMaxInfluenceRadius;
	FindBasesInCombat_TimerRequest.MinimumThreatScoreToFlagLocation = DefaultMinimumThreatScoreToFlagBase;
	FindBasesInCombat_TimerRequest.MinimumEffectiveAttackerCount = DefaultMinimumEffectiveAttackerCount;
}

void UEnemyStrategicAIComponent::InitStrategicAIComponent(AEnemyController* EnemyController,
                                                          UStochasticDecisionTree* StochasticDecisionTree)
{
	M_StochasticDecisionTree = StochasticDecisionTree;
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	PreThinKStep_InitThinkingTimers(Now);
	CacheGenerationSeedFromGameInstance();

	const FEnemyAIMissionSettings& MissionSettings = M_Blackboard.StrategicAIMissionSettings;
	switch (MissionSettings.StrategicAIThinkingStartCondition)
	{
	case EEnemyStrategicAIThinkingStartCondition::StartImmediatelyAtGameStart:
		StartStrategicAIThinkingTimer();
		return;
	case EEnemyStrategicAIThinkingStartCondition::StartAfterSpecifiedSeconds:
		StartStrategicAIThinkingTimerAfterDelay(MissionSettings.StrategicAIThinkingStartDelaySeconds);
		return;
	case EEnemyStrategicAIThinkingStartCondition::StartAfterMissionTrigger:
		return;
	default:
		RTSFunctionLibrary::ReportError("Enemy strategic AI has an unsupported thinking start condition.");
	}
}


void UEnemyStrategicAIComponent::QueueFindClosestFlankableEnemyHeavyRequest(
	const FFindClosestFlankableEnemyHeavy& Request)
{
	M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueGetPlayerUnitCountsAndBaseRequest(
	const FGetPlayerUnitCountsAndBase& Request)
{
	M_PendingRequests.GetPlayerUnitCountsAndBaseRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindAlliedTanksToRetreatRequest(
	const FFindAlliedTanksToRetreat& Request)
{
	M_PendingRequests.FindAlliedTanksToRetreatRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindEnemyBaseClustersRequest(const FFindEnemyBaseClusters& Request)
{
	M_PendingRequests.FindEnemyBaseClustersRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindLocationsUnderPlayerAttackRequest(
	const FFindLocationsUnderPlayerAttack& Request)
{
	M_PendingRequests.FindLocationsUnderPlayerAttackRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindPlayerUnitBulkLocationsRequest(
	const FFindPlayerUnitBulkLocations& Request)
{
	M_PendingRequests.FindPlayerUnitBulkLocationsRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindConstructionLocationsRequest(
	const FFindConstructionLocations& Request)
{
	M_PendingRequests.FindConstructionLocationsRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindMineLocationsRequest(const FFindMineLocations& Request)
{
	M_PendingRequests.FindMineLocationsRequests.Add(Request);
}

void UEnemyStrategicAIComponent::QueueFindPlayerHeavyTankFlankLocationsRequest(
	const FFindClosestFlankableEnemyHeavy& Request)
{
	M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.Add(Request);
}

void UEnemyStrategicAIComponent::RequestRetreatDamagedTanks(const FFindAlliedTanksToRetreat& Request)
{
	FFindAlliedTanksToRetreat RequestToQueue = Request;
	FillRetreatRequestExcludedUnits(RequestToQueue);
	QueueFindAlliedTanksToRetreatRequest(RequestToQueue);
}

const FStrategicAIResultBatch& UEnemyStrategicAIComponent::GetLatestStrategicAIResults() const
{
	return M_LatestResults;
}

const FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetStrategicAIBlackboard() const
{
	return M_Blackboard;
}

FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetEditableStrategicAIBlackboard()
{
	return M_Blackboard;
}

FEnemyStrategicTrainingState& UEnemyStrategicAIComponent::GetEditableEnemyTrainingState()
{
	return M_TrainingState;
}

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
void UEnemyStrategicAIComponent::ShippingTest_RunTrainingPressureThinkStep()
{
	TrainingPressure_ThinkStep();
}

void UEnemyStrategicAIComponent::ShippingTest_AccumulateTrainingPointIncomeForElapsedSeconds(
	const float ElapsedSeconds)
{
	AccumulateTrainingPointIncomeForElapsedSeconds(ElapsedSeconds);
}
#endif

void UEnemyStrategicAIComponent::RegisterDroppedUnitWithBlackboardWhenReady(AActor* DroppedUnit)
{
	if (not IsValid(DroppedUnit))
	{
		return;
	}

	const URTSComponent* RTSComponent = DroppedUnit->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent) || FRTSPlayers::IsHumanPlayer(RTSComponent->GetOwningPlayer()))
	{
		return;
	}
	// A unit dropped for another AI player goes on that player's blackboard, never on this one.
	const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner());
	if (IsValid(EnemyController) && not EnemyController->GetCanControlUnit(DroppedUnit))
	{
		return;
	}

	ASquadController* SquadController = Cast<ASquadController>(DroppedUnit);
	if (not IsValid(SquadController) || SquadController->GetIsSquadFullyLoadedAndInitialized())
	{
		RegisterDroppedUnitWithBlackboard(DroppedUnit);
		return;
	}

	SquadController->OnSquadFullyLoaded.AddUObject(
		this,
		&UEnemyStrategicAIComponent::OnDroppedSquadFullyLoaded);
}


void UEnemyStrategicAIComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bind the owning controller at runtime so it is valid before InitStrategicAIComponent (called from the
	// controller's BeginPlay) runs, and is never left pointing at the class default object.
	M_EnemyController = Cast<AEnemyController>(GetOwner());
}

void UEnemyStrategicAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStrategicAIThinkingTimer();
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		for (FEnemyStrategicAITrainingSpawnBatch& SpawnBatch : M_ActiveTrainingSpawnBatches)
		{
			World->GetTimerManager().ClearTimer(SpawnBatch.TimerHandle);
		}
	}

	M_ActiveTrainingSpawnBatches.Empty();
	Super::EndPlay(EndPlayReason);
}

void UEnemyStrategicAIComponent::BeginPlay_CacheUnitManager()
{
	M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	(void)EnsureIsValidUnitManager();
}

bool UEnemyStrategicAIComponent::EnsureIsValidUnitManager()
{
	if (not M_GameUnitManager.IsValid())
	{
		M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
		if (not M_GameUnitManager.IsValid())
		{
			RTSFunctionLibrary::ReportError("Enemy strategic AI requires a valid game unit manager but "
				"cannot get it through helper function in FRTS_Statics.");
			return false;
		}
	}
	return true;
}


bool UEnemyStrategicAIComponent::GetIsAllowedDirectControlUnits() const
{
	return M_Blackboard.StrategicAIMissionSettings.bAllowDirectControlStochasticDecisionTree;
}

bool UEnemyStrategicAIComponent::GetIsAllowedUnitTraining() const
{
	return M_Blackboard.StrategicAIMissionSettings.bAllowUnitTraining;
}

bool UEnemyStrategicAIComponent::GetIsAllowedBuilding() const
{
	return M_Blackboard.StrategicAIMissionSettings.bAllowBuilding;
}

void UEnemyStrategicAIComponent::PreThinKStep_InitThinkingTimers(const float Now)
{
	M_AIThinkTimers.Reset();

	M_AIBaseLocationThinkTimer.LastTimeThought = Now;
	M_AIBaseLocationThinkTimer.ThinkingInterval = EnemyAISettings::ThinkingTimers::UpdateAIBaseLocations_Interval;
	M_AIBaseLocationThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep);
	M_AIThinkTimers.Add(&M_AIBaseLocationThinkTimer);

	M_PlayerUnitCountsBuildingCountsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitCountsBuildingCountsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerCountsBaseLocations_Interval;
	M_PlayerUnitCountsBuildingCountsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitCountsBuildingCountsThinkTimer);

	M_LocationsUnderAttackThinkTimer.LastTimeThought = Now;
	M_LocationsUnderAttackThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateLocationsUnderPlayerAttack_Interval;
	M_LocationsUnderAttackThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::LocationsUnderAttack_ThinkStep);
	M_AIThinkTimers.Add(&M_LocationsUnderAttackThinkTimer);

	M_BasesInCombatThinkTimer.LastTimeThought = Now;
	M_BasesInCombatThinkTimer.ThinkingInterval =
		EnemyAISettings::DamagedVehicleRetreat::UpdateBasesInCombat_Interval;
	M_BasesInCombatThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::BasesInCombat_ThinkStep);
	M_AIThinkTimers.Add(&M_BasesInCombatThinkTimer);

	M_PlayerUnitBulkLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerUnitBulkLocations_Interval;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitBulkLocationsThinkTimer);

	M_ConstructionLocationsThinkTimer.LastTimeThought = Now;
	M_ConstructionLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateConstructionLocations_Interval;
	M_ConstructionLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::ConstructionLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_ConstructionLocationsThinkTimer);

	M_MineLocationsThinkTimer.LastTimeThought = Now;
	M_MineLocationsThinkTimer.ThinkingInterval = EnemyAISettings::ThinkingTimers::UpdateMineLocations_Interval;
	M_MineLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::MineLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_MineLocationsThinkTimer);

	M_PlayerHeavyTankFlankLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerHeavyTankFlank_Interval;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerHeavyTankFlankLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerHeavyTankFlankLocationsThinkTimer);

	M_TrainingPressureThinkTimer.LastTimeThought = Now;
	M_TrainingPressureThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateEnemyTrainingPressure_Interval;
	M_TrainingPressureThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::TrainingPressure_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingPressureThinkTimer);

	M_TrainingRequirementsThinkTimer.LastTimeThought = Now;
	M_TrainingRequirementsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateAITechLevel_Interval;
	M_TrainingRequirementsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingRequirementsThinkTimer);

	M_TrainingPointsThinkTimer.LastTimeThought = Now;
	M_TrainingPointsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateEnemyTrainingPoints_Interval;
	M_TrainingPointsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::GetTrainingPoints_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingPointsThinkTimer);

	M_EnemyGlobalAbilityAIStartTimeSeconds = Now;
	M_EnemyGlobalAbilityThinkTimer.LastTimeThought = Now;
	M_EnemyGlobalAbilityThinkTimer.ThinkingInterval = GetEnemyGlobalAbilityThinkStepInterval();
	M_EnemyGlobalAbilityThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::EnemyGlobalAbility_ThinkStep);
	M_AIThinkTimers.Add(&M_EnemyGlobalAbilityThinkTimer);
}

void UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep()
{
	// Add player avg attackers locations and add hq, resource building locations for potential attack locs.
	AddPlayerUnitLocationsForDefensePositionsOfEnemyBases(FindEnemyBase_TimerRequest);
	QueueFindEnemyBaseClustersRequest(FindEnemyBase_TimerRequest);
}

void UEnemyStrategicAIComponent::AddPlayerUnitLocationsForDefensePositionsOfEnemyBases(
	FFindEnemyBaseClusters& OutFindBaseRequest)
{
	OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Reset();

	if (BlackboardQueries::HasValidPlayerAttackingLocations(M_Blackboard))
	{
		for (const auto& EachUnderAttLoc : M_Blackboard.CurrentLocationsUnderPlayerAttack.
		                                                LocationsUnderAttack)
		{
			// Always checked for near zero on async processor!
			OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(EachUnderAttLoc.AverageAttackerLocation);
		}
	}
	if (BlackboardQueries::HasValidPlayerUnitBulkLocations(M_Blackboard))
	{
		// Always checked for near zero on async processor!
		for (const auto& EachBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
		{
			OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(EachBulk.BulkLocation);
		}
	}
	if (BlackboardQueries::HasValidPlayerHQLocation(M_Blackboard))
	{
		OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Add(
			M_Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation);
	}
	if (BlackboardQueries::HasValidPlayerResourceBuildingLocations(M_Blackboard))
	{
		// Note that any locations in a locations array on the blackboard are always checked for near zero!
		OutFindBaseRequest.PlayerUnitLocationsToDefendAgainst.Append(
			M_Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings);
	}
}

void UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep()
{
	QueueGetPlayerUnitCountsAndBaseRequest(FindPlayerCountBase_TimerRequest);
}

void UEnemyStrategicAIComponent::LocationsUnderAttack_ThinkStep()
{
	QueueFindLocationsUnderPlayerAttackRequest(FindLocationsUnderAttack_TimerRequest);
}

void UEnemyStrategicAIComponent::BasesInCombat_ThinkStep()
{
	FFindLocationsUnderPlayerAttack RequestToQueue = FindBasesInCombat_TimerRequest;
	RequestToQueue.RequestID = EnemyAISettings::DamagedVehicleRetreat::BasesInCombatRequestID;
	RequestToQueue.LocationsToEvaluate.Reset();
	RequestToQueue.LocationsToEvaluate.Reserve(M_Blackboard.EnemyBasePoints.Num());
	for (const FEnemyBasePointCoreBuildings& BasePoint : M_Blackboard.EnemyBasePoints)
	{
		if (not BasePoint.BaseLocation.IsNearlyZero())
		{
			RequestToQueue.LocationsToEvaluate.Add(BasePoint.BaseLocation);
		}
	}

	if (RequestToQueue.LocationsToEvaluate.IsEmpty())
	{
		M_Blackboard.CurrentEnemyBasesInCombat.LocationsUnderAttack.Reset();
		return;
	}

	QueueFindLocationsUnderPlayerAttackRequest(RequestToQueue);
}

void UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep()
{
	QueueFindPlayerUnitBulkLocationsRequest(FindPlayerUnitBulkLocations_TimerRequest);
}

void UEnemyStrategicAIComponent::ConstructionLocations_ThinkStep()
{
	FFindConstructionLocations RequestToQueue = FindConstructionLocations_TimerRequest;
	FillConstructionLocationsTimerRequest(RequestToQueue);
	if (RequestToQueue.DefensePositions.IsEmpty() || RequestToQueue.PlayerBulkLocations.IsEmpty())
	{
		return;
	}

	QueueFindConstructionLocationsRequest(RequestToQueue);
}

void UEnemyStrategicAIComponent::FillConstructionLocationsTimerRequest(
	FFindConstructionLocations& RequestToFill) const
{
	RequestToFill.DefensePositions = M_Blackboard.CurrentBaseDefensePositions;
	RequestToFill.PlayerBulkLocations.Reset();
	RequestToFill.PlayerBulkLocations.Reserve(M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.Num());
	for (const FPlayerUnitBulkLocation& PlayerUnitBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		if (PlayerUnitBulk.BulkLocation.IsNearlyZero())
		{
			continue;
		}

		RequestToFill.PlayerBulkLocations.Add(PlayerUnitBulk.BulkLocation);
	}
}

void UEnemyStrategicAIComponent::MineLocations_ThinkStep()
{
	FFindMineLocations RequestToQueue = FindMineLocations_TimerRequest;
	FillMineLocationsTimerRequest(RequestToQueue);
	const bool bHasCandidateSources = not RequestToQueue.DefensePositions.IsEmpty()
		|| not RequestToQueue.RoadSplineSampleLocations.IsEmpty();
	if (RequestToQueue.EnemyBaseLocations.IsEmpty()
		|| RequestToQueue.PlayerBulkLocations.IsEmpty()
		|| not bHasCandidateSources)
	{
		return;
	}

	QueueFindMineLocationsRequest(RequestToQueue);
}

void UEnemyStrategicAIComponent::FillMineLocationsTimerRequest(FFindMineLocations& RequestToFill) const
{
	RequestToFill.DefensePositions = M_Blackboard.CurrentBaseDefensePositions;
	FillMineBaseLocationPayload(RequestToFill);
	FillMinePlayerBulkLocationPayload(RequestToFill);
	FillMineRoadSplineSamplePayload(RequestToFill);
}

void UEnemyStrategicAIComponent::FillMineBaseLocationPayload(FFindMineLocations& RequestToFill) const
{
	RequestToFill.EnemyBaseLocations.Reset();
	RequestToFill.EnemyBaseLocations.Reserve(M_Blackboard.EnemyBasePoints.Num());
	for (const FEnemyBasePointCoreBuildings& BasePoint : M_Blackboard.EnemyBasePoints)
	{
		if (not BasePoint.BaseLocation.IsNearlyZero())
		{
			RequestToFill.EnemyBaseLocations.Add(BasePoint.BaseLocation);
		}
	}
}

void UEnemyStrategicAIComponent::FillMinePlayerBulkLocationPayload(FFindMineLocations& RequestToFill) const
{
	RequestToFill.PlayerBulkLocations.Reset();
	RequestToFill.PlayerBulkLocations.Reserve(
		M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.Num());
	for (const FPlayerUnitBulkLocation& PlayerUnitBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		if (not PlayerUnitBulk.BulkLocation.IsNearlyZero())
		{
			RequestToFill.PlayerBulkLocations.Add(PlayerUnitBulk.BulkLocation);
		}
	}
}

void UEnemyStrategicAIComponent::FillMineRoadSplineSamplePayload(FFindMineLocations& RequestToFill) const
{
	RequestToFill.RoadSplineSampleLocations.Reset();
	for (const TWeakObjectPtr<ARoadSplineActor>& RoadSplineActor : M_Blackboard.RoadSplineActors)
	{
		if (not RoadSplineActor.IsValid() || not IsValid(RoadSplineActor->RoadSpline))
		{
			continue;
		}

		const USplineComponent* RoadSpline = RoadSplineActor->RoadSpline;
		const float SplineLength = RoadSpline->GetSplineLength();
		for (float Distance = 0.f;
			Distance < SplineLength;
			Distance += EnemyStrategicAIMineLocationConstants::RoadSplineSampleSpacing)
		{
			RequestToFill.RoadSplineSampleLocations.Add(RoadSpline->GetLocationAtDistanceAlongSpline(
				Distance,
				ESplineCoordinateSpace::World));
		}

		if (SplineLength > 0.f)
		{
			RequestToFill.RoadSplineSampleLocations.Add(RoadSpline->GetLocationAtDistanceAlongSpline(
				SplineLength,
				ESplineCoordinateSpace::World));
		}
	}
}

void UEnemyStrategicAIComponent::PlayerHeavyTankFlankLocations_ThinkStep()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	RemoveExpiredHeavyTankFlankingResults(World->GetTimeSeconds());
	FindPlayerHeavyTankFlankLocations_TimerRequest.RequestID = 0;
	QueueFindPlayerHeavyTankFlankLocationsRequest(FindPlayerHeavyTankFlankLocations_TimerRequest);
}

void UEnemyStrategicAIComponent::RemoveExpiredHeavyTankFlankingResults(const float CurrentTimeSeconds)
{
	const float ResultMaxAgeSeconds = FindPlayerHeavyTankFlankLocations_TimerRequest.ResultMaxAgeSeconds;
	if (ResultMaxAgeSeconds < 0.f)
	{
		return;
	}

	M_Blackboard.AgreggatedHeavyTankFlankingResults.RemoveAll(
		[CurrentTimeSeconds, ResultMaxAgeSeconds](const FResultClosestFlankableEnemyHeavy& Result)
		{
			return Result.ResultTimestampSeconds < 0.f
				|| CurrentTimeSeconds - Result.ResultTimestampSeconds > ResultMaxAgeSeconds;
		});
}

/**
* AI chooses a batch.

AI cannot afford it.

AI stores it as M_TrainingReservation.

Every later Training_ThinkStep() tries to pay for that exact batch first.

If still too expensive, the reservation blocks new random training choices.

Training points accumulate elsewhere over time.

Eventually the AI has enough points.

Training_TrySpendTrainingPoints() succeeds.

The reserved batch is created.

M_TrainingReservation.Reset() clears the reservation.

The next training think step can evaluate fresh pressure again.
 */
void UEnemyStrategicAIComponent::Training_ThinkStep()
{

	TRACE_CPUPROFILER_EVENT_SCOPE("StrategicAI_TrainingThinkStep");
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	if (bM_ShippingTest_PressureOnlyTraining)
	{
		return;
	}
#endif

#if RTS_WITH_MAP_TESTS
	if (bM_MapTest_IsTrainingThinkSuspended && not bM_MapTest_IsForcingTrainingThink)
	{
		return;
	}
#endif

	// Units train at buildings: know which ones stand before picking anything they would have to train.
	Training_RefreshBuildingTrainers();

	// Reserved batches are handled first so saved pressure cannot be replaced by cheaper random picks.
	// “Do I currently have at least ReservedTrainingBatch.TrainingPointCost training points?”
	if (Training_HandleReservedTrainingBatchIfAny())
	{
		return;
	}

	// Build the legal unit pool first, because pressure should only be spent when trainable options exist.
	TArray<ETankSubtype> AvailableTankSubtypes;
	TArray<ESquadSubtype> AvailableSquadSubtypes;
	Training_CollectAvailableTrainingOptions(AvailableTankSubtypes, AvailableSquadSubtypes);
	if (AvailableTankSubtypes.IsEmpty() && AvailableSquadSubtypes.IsEmpty())
	{
		return;
	}

	// Spending the selected buckets here prevents the same strategic need from dominating every think step.
	// As picked buckets get pressure removed.
	const FEnemyStrategicTrainingSelection Selection = M_TrainingState.PickAndSpendTrainingSelection();
	const bool bHasTrainingSelection = Selection.Focus != EAITrainingFocus::NoFocus
		|| Selection.Specialty != EAITrainingFocusSpecialty::NoTrainingPressure;
	if (not bHasTrainingSelection)
	{
		return;
	}

	// Apply the broad strategic selection after unlock checks so random batching only sees useful,
	// currently trainable and selection-compatible choices.
	TArray<ETankSubtype> FilteredTankSubtypes;
	TArray<ESquadSubtype> FilteredSquadSubtypes;
	Training_FilterTrainingOptionsForSelection(
		Selection,
		AvailableTankSubtypes,
		AvailableSquadSubtypes,
		FilteredTankSubtypes,
		FilteredSquadSubtypes);
	if (FilteredTankSubtypes.IsEmpty() && FilteredSquadSubtypes.IsEmpty())
	{
		return;
	}

	// Pick exact units last so availability, pressure, and cost all describe the same batch.
	const FEnemyStrategicAITrainingBatch TrainingBatch = Training_BuildRandomTrainingBatch(
		FilteredTankSubtypes,
		FilteredSquadSubtypes);
	if (TrainingBatch.IsEmpty())
	{
		return;
	}

	// Immediate spending keeps affordable responses fast; reservation preserves unaffordable intent for future income.
	if (Training_TrySpendTrainingPoints(TrainingBatch.TrainingPointCost))
	{
		CreateTrainingBatch(TrainingBatch.TankSubtypes, TrainingBatch.SquadSubtypes);
		return;
	}

	M_TrainingReservation.ReserveBatch(TrainingBatch);
}

bool UEnemyStrategicAIComponent::Training_HandleReservedTrainingBatchIfAny()
{
	// Returning false lets the caller continue with new pressure only when no previous choice is waiting.
	if (not M_TrainingReservation.HasReservation())
	{
		return false;
	}

	const FEnemyStrategicAITrainingBatch& ReservedTrainingBatch = M_TrainingReservation.ReservedTrainingBatch;
	// The building that would train part of the reservation is gone: drop it so the saved points go to what the AI
	// can still train, instead of blocking every later pick.
	if (not Training_GetCanTrainBatch(ReservedTrainingBatch))
	{
		M_TrainingReservation.Reset();
		return false;
	}
	// “Do I currently have at least ReservedTrainingBatch.TrainingPointCost training points?”
	if (not Training_TrySpendTrainingPoints(ReservedTrainingBatch.TrainingPointCost))
	{
		// The reservation still owns this think step, preventing new random batches from stealing saved points.
		return true;
	}

	// Once the exact reserved batch is paid for, clear the reservation so fresh pressure can be evaluated next time.
	CreateTrainingBatch(ReservedTrainingBatch.TankSubtypes, ReservedTrainingBatch.SquadSubtypes);
	M_TrainingReservation.Reset();
	return true;
}

void UEnemyStrategicAIComponent::Training_CollectAvailableTrainingOptions(
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	const FEnemyLevelTraining& EnemyLevelTraining = M_TrainingState.EnemyLevelTraining;
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.BasicInfantryOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.LightTankOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.MediumTankOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.Tier2Options, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.AdvancedInfantryOptions, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.Tier3Options, OutTankSubtypes, OutSquadSubtypes);
	Training_AddUnlockedTrainingOptions(EnemyLevelTraining.ExperimentalOptions, OutTankSubtypes, OutSquadSubtypes);

	// A unit the async spawner has no class for would keep its building busy for the whole training time and then be
	// dropped (seen in a survival match with Squad_Rus_HazmatBazooka): never pick it.
	if (const ARTSAsyncSpawner* const AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this))
	{
		OutTankSubtypes.RemoveAll([AsyncSpawner](const ETankSubtype TankSubtype)
		{
			return not AsyncSpawner->GetHasClassForTrainingOption(
				FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype)));
		});
		OutSquadSubtypes.RemoveAll([AsyncSpawner](const ESquadSubtype SquadSubtype)
		{
			return not AsyncSpawner->GetHasClassForTrainingOption(
				FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype)));
		});
	}

	// Unlocked is not producible: without a standing barracks/tent (squads) or vehicle building (tanks) the AI must
	// not spend pressure or points on that kind of unit.
	if (not Training_GetCanTrainUnitType(EAllUnitType::UNType_Squad))
	{
		OutSquadSubtypes.Empty();
	}
	if (not Training_GetCanTrainUnitType(EAllUnitType::UNType_Tank))
	{
		OutTankSubtypes.Empty();
	}
}

void UEnemyStrategicAIComponent::Training_AddUnlockedTrainingOptions(
	const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	const bool* const bIsTechLevelUnlocked = M_TrainingState.TechLevelUnlockedMap.Find(TrainingOptions.TechLevel);
	if (bIsTechLevelUnlocked == nullptr || not*bIsTechLevelUnlocked)
	{
		return;
	}

	OutTankSubtypes.Append(TrainingOptions.TrainableTankSubtypes);
	OutSquadSubtypes.Append(TrainingOptions.TrainableSquadSubtypes);
}

void UEnemyStrategicAIComponent::Training_FilterTrainingOptionsForSelection(
	const FEnemyStrategicTrainingSelection& Selection,
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes,
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes) const
{
	// Keep tanks and squads in separate pools because later batch construction must preserve their different spawn paths.
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		if (Training_GetDoesTankFitSelection(TankSubtype, Selection))
		{
			OutTankSubtypes.Add(TankSubtype);
		}
	}

	// Run the same selection over infantry so mixed focus/specialty pressure can still pick either type.
	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		if (Training_GetDoesSquadFitSelection(SquadSubtype, Selection))
		{
			OutSquadSubtypes.Add(SquadSubtype);
		}
	}
}

bool UEnemyStrategicAIComponent::Training_GetDoesTankFitSelection(
	const ETankSubtype TankSubtype,
	const FEnemyStrategicTrainingSelection& Selection) const
{
	if (Selection.Focus == EAITrainingFocus::Infantry)
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::LightTanks && not Global_GetIsLightTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::MediumTanks && not Global_GetIsMediumTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Focus == EAITrainingFocus::HeavyTanks && not Global_GetIsHeavyTank(TankSubtype))
	{
		return false;
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiTank)
	{
		return Global_GetIsTankDestroyer(TankSubtype);
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiInfantry)
	{
		return not Global_GetIsTankDestroyer(TankSubtype);
	}

	return true;
}

bool UEnemyStrategicAIComponent::Training_GetDoesSquadFitSelection(
	const ESquadSubtype SquadSubtype,
	const FEnemyStrategicTrainingSelection& Selection) const
{
	if (Selection.Focus != EAITrainingFocus::NoFocus && Selection.Focus != EAITrainingFocus::Infantry)
	{
		return false;
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiTank)
	{
		return Global_GetIsAntiTankSquad(SquadSubtype);
	}

	if (Selection.Specialty == EAITrainingFocusSpecialty::AntiInfantry)
	{
		return not Global_GetIsAntiTankSquad(SquadSubtype);
	}

	return true;
}

FEnemyStrategicAITrainingBatch UEnemyStrategicAIComponent::Training_BuildRandomTrainingBatch(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes) const
{
	FEnemyStrategicAITrainingBatch TrainingBatch;
	const int32 TotalOptionCount = TankSubtypes.Num() + SquadSubtypes.Num();
	if (TotalOptionCount <= 0)
	{
		return TrainingBatch;
	}

	const FEnemyAIMissionSettings& MissionSettings = M_Blackboard.StrategicAIMissionSettings;
	const int32 MinUnitsTrainedPerBatch = FMath::Max(1, MissionSettings.MinUnitsTrainedPerBatch);
	const int32 MaxUnitsTrainedPerBatch = FMath::Max(MinUnitsTrainedPerBatch, MissionSettings.MaxUnitsTrainedPerBatch);
	const int32 UnitPickRange = MaxUnitsTrainedPerBatch - MinUnitsTrainedPerBatch + 1;
	const int32 UnitsToPick = MinUnitsTrainedPerBatch + GetSeededIndex(UnitPickRange);
	for (int32 UnitPickIndex = 0; UnitPickIndex < UnitsToPick; ++UnitPickIndex)
	{
		const int32 OptionIndex = GetSeededIndex(TotalOptionCount, UnitPickIndex);
		if (TankSubtypes.IsValidIndex(OptionIndex))
		{
			TrainingBatch.TankSubtypes.Add(TankSubtypes[OptionIndex]);
			continue;
		}

		const int32 SquadOptionIndex = OptionIndex - TankSubtypes.Num();
		if (SquadSubtypes.IsValidIndex(SquadOptionIndex))
		{
			TrainingBatch.SquadSubtypes.Add(SquadSubtypes[SquadOptionIndex]);
		}
	}

	TrainingBatch.TrainingPointCost = Training_GetTrainingBatchCost(
		TrainingBatch.TankSubtypes,
		TrainingBatch.SquadSubtypes);
	return TrainingBatch;
}

int32 UEnemyStrategicAIComponent::Training_GetTrainingBatchCost(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes) const
{
	int32 TrainingPointCost = 0;
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		TrainingPointCost += EnemyTrainingHelpers::GetTankTrainingPointsCost(TankSubtype);
	}

	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		TrainingPointCost += EnemyTrainingHelpers::GetSquadTrainingPointCost(SquadSubtype);
	}

	return TrainingPointCost;
}

bool UEnemyStrategicAIComponent::Training_TrySpendTrainingPoints(const int32 TrainingPointCost)
{
	if (TrainingPointCost <= 0)
	{
		return false;
	}

	if (M_TrainingState.TrainingPoints < TrainingPointCost)
	{
		return false;
	}

	M_TrainingState.TrainingPoints -= TrainingPointCost;
	return true;
}

void UEnemyStrategicAIComponent::CreateTrainingBatch(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes)
{
	TArray<FTrainingOption> TrainingOptions;
	CreateTrainingOptionsToSpawn(TankSubtypes, SquadSubtypes, TrainingOptions);
	if (TrainingOptions.IsEmpty())
	{
		return;
	}

	TArray<FTrainingOption> OptionsWithoutBuilding;
	Training_QueueOptionsAtBuildingTrainers(TrainingOptions, OptionsWithoutBuilding);
	if (OptionsWithoutBuilding.IsEmpty())
	{
		return;
	}

	// Legacy levels: an enemy UTrainerComponent on the blackboard still spawns its units right there.
	FTransform LegacyTrainerTransform;
	FVector ProjectedSpawnLocation = FVector::ZeroVector;
	if (GetValidTrainerComponentLocationFromBlackboard(LegacyTrainerTransform)
		&& TryProjectTrainingLocationToNavmesh(LegacyTrainerTransform.GetLocation(), ProjectedSpawnLocation))
	{
		StartTrainingSpawnBatchTimer(OptionsWithoutBuilding, ProjectedSpawnLocation);
		return;
	}

	// No building can train these units (it fell between the pick and now): the AI gets the points back.
	int32 RefundedPoints = 0;
	for (const FTrainingOption& Option : OptionsWithoutBuilding)
	{
		RefundedPoints += Training_GetTrainingOptionCost(Option);
	}
	RefundTrainingPoints(RefundedPoints);
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::TrainingPressureDebugging)
	{
		RTSFunctionLibrary::PrintString(FString::Printf(
			TEXT("Enemy strategic AI: %d unit(s) had no standing building to train them; %d points refunded."),
			OptionsWithoutBuilding.Num(), RefundedPoints), FColor::Orange);
	}
}

void UEnemyStrategicAIComponent::Training_RefreshBuildingTrainers()
{
	M_BuildingTrainers.Reset();
	if (not EnsureIsValidUnitManager())
	{
		return;
	}
	const uint8 ControlledPlayer = static_cast<uint8>(GetControlledPlayer());
	for (ABuildingExpansion* const Bxp : M_GameUnitManager->GetPlayerBxps(ControlledPlayer))
	{
		if (not IsValid(Bxp))
		{
			continue;
		}
		UEnemyBuildingTrainerComponent* const Trainer = Bxp->FindComponentByClass<UEnemyBuildingTrainerComponent>();
		if (IsValid(Trainer))
		{
			M_BuildingTrainers.Add(Trainer);
		}
	}
}

bool UEnemyStrategicAIComponent::Training_GetHasLegacyEnemyTrainer() const
{
	for (const TWeakObjectPtr<UTrainerComponent>& LegacyTrainer : M_Blackboard.TrainerComponents)
	{
		if (LegacyTrainer.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool UEnemyStrategicAIComponent::Training_GetCanTrainUnitType(const EAllUnitType UnitType) const
{
	if (Training_GetHasLegacyEnemyTrainer())
	{
		return true;
	}
	const FTrainingOption ProbeOption(UnitType, 0);
	for (const TWeakObjectPtr<UEnemyBuildingTrainerComponent>& Trainer : M_BuildingTrainers)
	{
		if (Trainer.IsValid() && Trainer->GetCanTrainOption(ProbeOption))
		{
			return true;
		}
	}
	return false;
}

bool UEnemyStrategicAIComponent::Training_GetCanTrainBatch(const FEnemyStrategicAITrainingBatch& TrainingBatch) const
{
	if (not TrainingBatch.TankSubtypes.IsEmpty() && not Training_GetCanTrainUnitType(EAllUnitType::UNType_Tank))
	{
		return false;
	}
	if (not TrainingBatch.SquadSubtypes.IsEmpty() && not Training_GetCanTrainUnitType(EAllUnitType::UNType_Squad))
	{
		return false;
	}
	return true;
}

UEnemyBuildingTrainerComponent* UEnemyStrategicAIComponent::Training_PickBuildingTrainer(
	const FTrainingOption& TrainingOption) const
{
	UEnemyBuildingTrainerComponent* BestTrainer = nullptr;
	float BestQueuedSeconds = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UEnemyBuildingTrainerComponent>& WeakTrainer : M_BuildingTrainers)
	{
		UEnemyBuildingTrainerComponent* const Trainer = WeakTrainer.Get();
		if (not IsValid(Trainer) || not Trainer->GetCanTrainOption(TrainingOption))
		{
			continue;
		}
		const float QueuedSeconds = Trainer->GetQueuedSecondsRemaining();
		if (QueuedSeconds < BestQueuedSeconds)
		{
			BestQueuedSeconds = QueuedSeconds;
			BestTrainer = Trainer;
		}
	}
	return BestTrainer;
}

int32 UEnemyStrategicAIComponent::Training_GetTrainingOptionCost(const FTrainingOption& TrainingOption) const
{
	if (TrainingOption.UnitType == EAllUnitType::UNType_Squad)
	{
		return EnemyTrainingHelpers::GetSquadTrainingPointCost(static_cast<ESquadSubtype>(TrainingOption.SubtypeValue));
	}
	if (TrainingOption.UnitType == EAllUnitType::UNType_Tank)
	{
		return EnemyTrainingHelpers::GetTankTrainingPointsCost(static_cast<ETankSubtype>(TrainingOption.SubtypeValue));
	}
	return 0;
}

int32 UEnemyStrategicAIComponent::Training_QueueOptionsAtBuildingTrainers(
	const TArray<FTrainingOption>& TrainingOptions,
	TArray<FTrainingOption>& OutOptionsWithoutBuilding)
{
	int32 QueuedCount = 0;
	for (const FTrainingOption& Option : TrainingOptions)
	{
		// Picked one by one, so a batch of several units spreads over the buildings that can train them.
		UEnemyBuildingTrainerComponent* const Trainer = Training_PickBuildingTrainer(Option);
		if (IsValid(Trainer) && Trainer->EnqueueTraining(Option, Training_GetTrainingOptionCost(Option), this))
		{
			++QueuedCount;
			continue;
		}
		OutOptionsWithoutBuilding.Add(Option);
	}
	return QueuedCount;
}

void UEnemyStrategicAIComponent::RefundTrainingPoints(const int32 TrainingPoints)
{
	if (TrainingPoints <= 0)
	{
		return;
	}
	M_TrainingState.TrainingPoints += TrainingPoints;
#if RTS_WITH_MAP_TESTS
	M_MapTest_RefundedTrainingPoints += TrainingPoints;
#endif
}

void UEnemyStrategicAIComponent::OnBuildingTrainerUnitSpawned(
	const FTrainingOption& TrainingOption,
	AActor* SpawnedActor,
	const FVector& TrainingLocation,
	const UEnemyBuildingTrainerComponent* Trainer)
{
	if (IsValid(SpawnedActor))
	{
		FEnemyBuildingTrainedUnitExit Exit;
		Exit.ExitTransform = FTransform(TrainingLocation);
		Exit.BuildingCenter = TrainingLocation;
		if (IsValid(Trainer) && IsValid(Trainer->GetOwner()))
		{
			Exit.ExitTransform = Trainer->GetExitTransform();
			Exit.BuildingCenter = Trainer->GetOwner()->GetActorLocation();
		}
		for (auto It = M_BuildingTrainedUnitExits.CreateIterator(); It; ++It)
		{
			if (not It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
		M_BuildingTrainedUnitExits.Add(SpawnedActor, Exit);
		// Spawned in the open at the building: without this it shows through the fog until the fog's start delay
		// for the player trainer's hidden pre-spawn runs out.
		Training_StartFogNowHiddenUntilSeen(SpawnedActor);
	}
	OnBlackboardUnitSpawned(TrainingOption, SpawnedActor, INDEX_NONE, TrainingLocation);
}

void UEnemyStrategicAIComponent::Training_StartFogNowHiddenUntilSeen(AActor* Unit)
{
	if (not IsValid(Unit))
	{
		return;
	}
	TArray<AActor*> Actors = {Unit};
	if (ASquadController* const SquadController = Cast<ASquadController>(Unit))
	{
		for (ASquadUnit* const SquadUnit : SquadController->GetSquadUnitsChecked())
		{
			Actors.Add(SquadUnit);
		}
	}
	for (AActor* const Actor : Actors)
	{
		if (not IsValid(Actor))
		{
			continue;
		}
		if (UFowComp* const FowComponent = Actor->FindComponentByClass<UFowComp>())
		{
			FowComponent->StartFowNowHiddenUntilSeen();
		}
	}
}

bool UEnemyStrategicAIComponent::Training_MoveToStandSpot(AActor* Unit, ICommands& Commands,
                                                          const FEnemyBuildingTrainedUnitExit& Exit)
{
	using namespace EnemyAISettings::UnitTraining;
	UWorld* const World = GetWorld();
	if (not IsValid(World) || not IsValid(Unit))
	{
		return false;
	}
	FEnemyStandSpotQuery Query;
	Query.ExitLocation = Exit.ExitTransform.GetLocation();
	const FVector ExitForward = Exit.ExitTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	Query.ExitForward = ExitForward.IsNearlyZero() ? FVector::ForwardVector : ExitForward;
	Query.BuildingCenter = Exit.BuildingCenter;
	Query.UnitRadius = EnemyTrainedUnitStandSpots::GetUnitFootprintRadius(Unit);
	Query.NavData = EnemyTrainedUnitStandSpots::GetNavDataForUnit(World, Unit);
	Query.UnitNameForLog = Unit->GetName();
	// Every exit of this AI's buildings stays free for the units still to come out (read here, not from
	// M_BuildingTrainers, which only the training think refreshes).
	TArray<const UEnemyBuildingTrainerComponent*> Trainers;
	if (EnsureIsValidUnitManager())
	{
		for (const ABuildingExpansion* const Bxp : M_GameUnitManager->GetPlayerBxps(
			     static_cast<uint8>(GetControlledPlayer())))
		{
			if (IsValid(Bxp))
			{
				Trainers.Add(Bxp->FindComponentByClass<UEnemyBuildingTrainerComponent>());
			}
		}
	}
	for (const UEnemyBuildingTrainerComponent* const Trainer : Trainers)
	{
		if (not IsValid(Trainer))
		{
			continue;
		}
		const FTransform TrainerExit = Trainer->GetExitTransform();
		FEnemyStandSpotKeepClearLane& Lane = Query.KeepClearLanes.AddDefaulted_GetRef();
		Lane.Start = TrainerExit.GetLocation();
		Lane.Forward = TrainerExit.GetRotation().GetForwardVector().GetSafeNormal2D();
		Lane.Length = StandSpotExitLaneLengthCm;
		Lane.HalfWidth = Trainer->TrainerKind == EEnemyBuildingTrainerKind::Vehicles
			                 ? StandSpotVehicleExitLaneHalfWidthCm
			                 : StandSpotInfantryExitLaneHalfWidthCm;
	}
	Training_GatherStandSpotOccupied(Unit, Query.ExitLocation, Query.Occupied);
	FEnemyStandSpot Spot;
	if (not EnemyTrainedUnitStandSpots::FindStandSpot(World, Query, Spot))
	{
		return false;
	}
	FEnemyStandSpotReservation& Reservation = M_StandSpotReservations.AddDefaulted_GetRef();
	Reservation.Unit = Unit;
	Reservation.Location = Spot.Location;
	Reservation.Radius = Query.UnitRadius;
	Reservation.ReservedAtSeconds = World->GetTimeSeconds();
	constexpr bool bSetUnitToIdle = false;
	Commands.MoveToLocation(Spot.Location, bSetUnitToIdle, FRotator(0.f, Spot.Yaw, 0.f), true);
	return true;
}

void UEnemyStrategicAIComponent::Training_GatherStandSpotOccupied(const AActor* Unit, const FVector& ExitLocation,
                                                                  TArray<FEnemyStandSpotOccupied>& OutOccupied)
{
	using namespace EnemyAISettings::UnitTraining;
	const UWorld* const World = GetWorld();
	const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
	// A reservation lasts while its unit may still be walking there, or for as long as it stands on the spot.
	M_StandSpotReservations.RemoveAll([Unit, Now](const FEnemyStandSpotReservation& Reservation)
	{
		const AActor* const ReservedUnit = Reservation.Unit.Get();
		if (not IsValid(ReservedUnit) || ReservedUnit == Unit)
		{
			return true;
		}
		const bool bMayStillBeWalking = Now - Reservation.ReservedAtSeconds < StandSpotReservationWalkSeconds;
		const bool bStandsOnIt = FVector::Dist2D(ReservedUnit->GetActorLocation(), Reservation.Location)
			<= Reservation.Radius * 2.f;
		return not bMayStillBeWalking && not bStandsOnIt;
	});
	const float MaxDistanceSquared = FMath::Square(StandSpotOccupiedGatherRadiusCm);
	for (const FEnemyStandSpotReservation& Reservation : M_StandSpotReservations)
	{
		if (FVector::DistSquared2D(Reservation.Location, ExitLocation) <= MaxDistanceSquared)
		{
			OutOccupied.Add({Reservation.Location, Reservation.Radius});
		}
	}
	if (not EnsureIsValidUnitManager())
	{
		return;
	}
	const uint8 ControlledPlayer = static_cast<uint8>(GetControlledPlayer());
	TArray<const AActor*> OwnSoldiers;
	if (const ASquadController* const OwnSquad = Cast<ASquadController>(Unit))
	{
		for (ASquadUnit* const SquadUnit : const_cast<ASquadController*>(OwnSquad)->GetSquadUnitsChecked())
		{
			OwnSoldiers.Add(SquadUnit);
		}
	}
	const float SoldierRadius = EnemyTrainedUnitStandSpots::GetSoldierFootprintRadius();
	for (const ASquadUnit* const Soldier : M_GameUnitManager->GetSquadUnitsOfPlayerView(ControlledPlayer))
	{
		if (not IsValid(Soldier) || OwnSoldiers.Contains(Soldier)
			|| FVector::DistSquared2D(Soldier->GetActorLocation(), ExitLocation) > MaxDistanceSquared)
		{
			continue;
		}
		OutOccupied.Add({Soldier->GetActorLocation(), SoldierRadius});
	}
	for (const ATankMaster* const Tank : M_GameUnitManager->GetTanksOfPlayerView(ControlledPlayer))
	{
		if (not IsValid(Tank) || Tank == Unit
			|| FVector::DistSquared2D(Tank->GetActorLocation(), ExitLocation) > MaxDistanceSquared)
		{
			continue;
		}
		OutOccupied.Add({Tank->GetActorLocation(), EnemyTrainedUnitStandSpots::GetUnitFootprintRadius(Tank)});
	}
}

#if RTS_WITH_MAP_TESTS
int32 UEnemyStrategicAIComponent::MapTest_QueueTrainingAtBuildings(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes)
{
	Training_RefreshBuildingTrainers();
	TArray<FTrainingOption> TrainingOptions;
	CreateTrainingOptionsToSpawn(TankSubtypes, SquadSubtypes, TrainingOptions);
	TArray<FTrainingOption> OptionsWithoutBuilding;
	return Training_QueueOptionsAtBuildingTrainers(TrainingOptions, OptionsWithoutBuilding);
}

TArray<UEnemyBuildingTrainerComponent*> UEnemyStrategicAIComponent::MapTest_GetBuildingTrainers()
{
	Training_RefreshBuildingTrainers();
	TArray<UEnemyBuildingTrainerComponent*> Trainers;
	for (const TWeakObjectPtr<UEnemyBuildingTrainerComponent>& Trainer : M_BuildingTrainers)
	{
		if (Trainer.IsValid())
		{
			Trainers.Add(Trainer.Get());
		}
	}
	return Trainers;
}

void UEnemyStrategicAIComponent::MapTest_CollectAvailableTrainingOptions(
	TArray<ETankSubtype>& OutTankSubtypes,
	TArray<ESquadSubtype>& OutSquadSubtypes)
{
	Training_RefreshBuildingTrainers();
	Training_CollectAvailableTrainingOptions(OutTankSubtypes, OutSquadSubtypes);
}
#endif

void UEnemyStrategicAIComponent::CreateTrainingOptionsToSpawn(
	const TArray<ETankSubtype>& TankSubtypes,
	const TArray<ESquadSubtype>& SquadSubtypes,
	TArray<FTrainingOption>& OutTrainingOptions) const
{
	OutTrainingOptions.Reset();
	OutTrainingOptions.Reserve(TankSubtypes.Num() + SquadSubtypes.Num());
	for (const ETankSubtype TankSubtype : TankSubtypes)
	{
		if (TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		OutTrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype)));
	}

	for (const ESquadSubtype SquadSubtype : SquadSubtypes)
	{
		if (SquadSubtype == ESquadSubtype::Squad_None)
		{
			continue;
		}

		OutTrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype)));
	}
}

bool UEnemyStrategicAIComponent::TryProjectTrainingLocationToNavmesh(
	const FVector& TrainingLocation,
	FVector& OutProjectedTrainingLocation) const
{
	return StochasticHelpers::CanProjectNavigable_TrainingLocation(
		this,
		TrainingLocation,
		OutProjectedTrainingLocation);
}

void UEnemyStrategicAIComponent::StartTrainingSpawnBatchTimer(
	const TArray<FTrainingOption>& TrainingOptionsToSpawn,
	const FVector& SpawnLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FEnemyStrategicAITrainingSpawnBatch& SpawnBatch = M_ActiveTrainingSpawnBatches.AddDefaulted_GetRef();
	SpawnBatch.TrainingOptionsToSpawn = TrainingOptionsToSpawn;
	SpawnBatch.SpawnLocation = SpawnLocation;
	SpawnBatch.BatchID = M_NextTrainingSpawnBatchID++;

	const TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindLambda([WeakThis, BatchID = SpawnBatch.BatchID]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->TrainingSpawnBatchTimerTick(BatchID);
	});

	constexpr float MinimumTrainingSpawnInterval = 0.01f;
	const float TrainingSpawnInterval = FMath::Max(
		EnemyAISettings::UnitTraining::TrainingBatchSpawnInterval,
		MinimumTrainingSpawnInterval);
	World->GetTimerManager().SetTimer(
		SpawnBatch.TimerHandle,
		TimerDelegate,
		TrainingSpawnInterval,
		true,
		TrainingSpawnInterval);
}

void UEnemyStrategicAIComponent::TrainingSpawnBatchTimerTick(const int32 BatchID)
{
	FEnemyStrategicAITrainingSpawnBatch* SpawnBatch = M_ActiveTrainingSpawnBatches.FindByPredicate(
		[BatchID](const FEnemyStrategicAITrainingSpawnBatch& Batch)
		{
			return Batch.BatchID == BatchID;
		});
	if (SpawnBatch == nullptr || not SpawnBatch->HasMoreTrainingOptions())
	{
		StopTrainingSpawnBatchTimer(BatchID);
		return;
	}

	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(RTSAsyncSpawner))
	{
		StopTrainingSpawnBatchTimer(BatchID);
		return;
	}

	const FTrainingOption TrainingOption = SpawnBatch->TrainingOptionsToSpawn[SpawnBatch->NextTrainingOptionIndex];
	const int32 SpawnID = SpawnBatch->NextTrainingOptionIndex;
	++SpawnBatch->NextTrainingOptionIndex;

	const FVector TrainingLocation = SpawnBatch->SpawnLocation;
	const TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	// Trained units belong to the player of the owning controller.
	const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner());
	const int32 OwningPlayerOverride = IsValid(EnemyController) ? EnemyController->GetSpawnOwnerOverride() : INDEX_NONE;
	const bool bRequestStarted = RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption,
		TrainingLocation,
		this,
		SpawnID,
		[WeakThis, TrainingLocation](const FTrainingOption& SpawnedTrainingOption, AActor* SpawnedActor, const int32 ID)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->OnBlackboardUnitSpawned(SpawnedTrainingOption, SpawnedActor, ID, TrainingLocation);
		}, FRotator::ZeroRotator, OwningPlayerOverride);

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	M_ShippingTest_TrainingSpawnRequestCount += bRequestStarted ? 1 : 0;
#endif

	if (not bRequestStarted)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy strategic AI failed to start async spawn for training option: " + TrainingOption.GetTrainingName());
	}

	if (not SpawnBatch->HasMoreTrainingOptions())
	{
		StopTrainingSpawnBatchTimer(BatchID);
	}
}

void UEnemyStrategicAIComponent::StopTrainingSpawnBatchTimer(const int32 BatchID)
{
	const int32 BatchIndex = M_ActiveTrainingSpawnBatches.IndexOfByPredicate(
		[BatchID](const FEnemyStrategicAITrainingSpawnBatch& Batch)
		{
			return Batch.BatchID == BatchID;
		});
	if (not M_ActiveTrainingSpawnBatches.IsValidIndex(BatchIndex))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_ActiveTrainingSpawnBatches[BatchIndex].TimerHandle);
	}

	M_ActiveTrainingSpawnBatches.RemoveAtSwap(BatchIndex);
}

void UEnemyStrategicAIComponent::OnBlackboardUnitSpawned(
	const FTrainingOption& TrainingOption,
	AActor* SpawnedActor,
	const int32 ID,
	const FVector& TrainingLocation)
{
	(void)ID;
	if (not IsValid(SpawnedActor))
	{
		return;
	}
	if (const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner()))
	{
		EnemyController->LogSpawnedUnitOwnerMismatch(SpawnedActor);
	}

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	M_ShippingTest_TrainingSpawnSuccessCount++;
#endif

	DebugTrainedUnit(TrainingOption, TrainingLocation);
	if (TrainingOption.UnitType != EAllUnitType::UNType_Squad)
	{
		IssueOrdersToSpawnedBlackboardUnit(SpawnedActor);
		return;
	}

	ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(SquadController))
	{
		return;
	}

	if (SquadController->GetIsSquadFullyLoadedAndInitialized())
	{
		IssueOrdersToSpawnedBlackboardUnit(SquadController);
		return;
	}

	SquadController->OnSquadFullyLoaded.AddUObject(this, &UEnemyStrategicAIComponent::OnBlackboardSquadFullyLoaded);
}

void UEnemyStrategicAIComponent::OnBlackboardSquadFullyLoaded(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	SquadController->OnSquadFullyLoaded.RemoveAll(this);
	IssueOrdersToSpawnedBlackboardUnit(SquadController);
}

void UEnemyStrategicAIComponent::OnDroppedSquadFullyLoaded(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	SquadController->OnSquadFullyLoaded.RemoveAll(this);
	RegisterDroppedUnitWithBlackboard(SquadController);
}

void UEnemyStrategicAIComponent::RegisterDroppedUnitWithBlackboard(AActor* DroppedUnit)
{
	if (not IsValid(DroppedUnit) || M_Blackboard.IdleDirectControlUnits.Contains(DroppedUnit))
	{
		return;
	}

	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyDirectControlComponent* EnemyDirectControlComponent =
		M_EnemyController->GetEnemyDirectControlComponent();
	if (not GetIsValidEnemyDirectControlComponent(EnemyDirectControlComponent))
	{
		return;
	}

	EnemyDirectControlComponent->RegisterDirectControlUnit(DroppedUnit);
}

void UEnemyStrategicAIComponent::IssueOrdersToSpawnedBlackboardUnit(AActor* SpawnedActor)
{
	// Building-trained: read (and drop) where it came out.
	bool bIsBuildingTrained = false;
	FEnemyBuildingTrainedUnitExit BuildingExit;
	if (const FEnemyBuildingTrainedUnitExit* const FoundExit = M_BuildingTrainedUnitExits.Find(SpawnedActor))
	{
		bIsBuildingTrained = true;
		BuildingExit = *FoundExit;
		M_BuildingTrainedUnitExits.Remove(SpawnedActor);
		// Again once loaded: a squad's soldiers may not have existed when it spawned.
		Training_StartFogNowHiddenUntilSeen(SpawnedActor);
	}
	ICommands* Commands = Cast<ICommands>(SpawnedActor);
	if (Commands == nullptr)
	{
		return;
	}

	constexpr bool bResetOrderQueue = true;
	if (Commands->RegisterAsBlackboardIdle(bResetOrderQueue) != ECommandQueueError::NoError)
	{
		return;
	}

	if (bIsBuildingTrained)
	{
		// First wait by the building, clear of its exit, then go on to a defense position
		// (Training_ReleaseStandSpotWaiters). Left at the exit it would block the next unit.
		if (Training_MoveToStandSpot(SpawnedActor, *Commands, BuildingExit))
		{
			using namespace EnemyAISettings::UnitTraining;
			const UWorld* const World = GetWorld();
			const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
			FEnemyStandSpotWait& Wait = M_StandSpotWaits.AddDefaulted_GetRef();
			Wait.Unit = SpawnedActor;
			Wait.ReleaseAtSeconds = Now + StandSpotWaitSeconds
				+ static_cast<float>(GetSeededIndex(StandSpotWaitJitterSeconds + 1));
			UE_LOG(LogEnemyTrainedUnitOrders, Log, TEXT("AI_TRAIN stand_spot_wait %s t=%.1f release_at=%.1f"),
			       *SpawnedActor->GetName(), Now, Wait.ReleaseAtSeconds);
			return;
		}
	}
	MoveUnitToRandomDefensePosition(SpawnedActor, *Commands,
	                                bIsBuildingTrained ? TEXT("defense_position") : nullptr,
	                                BuildingExit.ExitTransform.GetLocation());
}

bool UEnemyStrategicAIComponent::MoveUnitToRandomDefensePosition(AActor* Unit, ICommands& Commands,
                                                                 const TCHAR* LogTag, const FVector& LogFromLocation)
{
	FDefensePositions DefensePosition;
	if (not IsValid(Unit) || not TryGetRandomBlackboardDefensePosition(DefensePosition))
	{
		return false;
	}
	if (LogTag != nullptr)
	{
		UE_LOG(LogEnemyTrainedUnitOrders, Log, TEXT("AI_TRAIN %s %s to %s (%.0f cm from its exit)"),
		       LogTag, *Unit->GetName(), *DefensePosition.Location.ToCompactString(),
		       FVector::Dist2D(DefensePosition.Location, LogFromLocation));
	}
	constexpr bool bQueueOrder = false;
	Commands.MoveToLocation(
		DefensePosition.Location,
		bQueueOrder,
		FRotator(0.f, DefensePosition.YawRotation, 0.f),
		true);
	return true;
}

void UEnemyStrategicAIComponent::Training_ReleaseStandSpotWaiters()
{
	if (M_StandSpotWaits.IsEmpty())
	{
		return;
	}
	const UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	for (int32 Index = M_StandSpotWaits.Num() - 1; Index >= 0; --Index)
	{
		const FEnemyStandSpotWait Wait = M_StandSpotWaits[Index];
		AActor* const Unit = Wait.Unit.Get();
		ICommands* const Commands = Cast<ICommands>(Unit);
		if (not IsValid(Unit) || Commands == nullptr)
		{
			M_StandSpotWaits.RemoveAtSwap(Index);
			continue;
		}
		if (Now < Wait.ReleaseAtSeconds)
		{
			continue;
		}
		// Picked from the idle pool for a mission meanwhile: that mission's orders win.
		if (not M_Blackboard.IdleDirectControlUnits.Contains(Unit))
		{
			UE_LOG(LogEnemyTrainedUnitOrders, Log, TEXT("AI_TRAIN stand_spot_picked %s t=%.1f"),
			       *Unit->GetName(), Now);
			M_StandSpotWaits.RemoveAtSwap(Index);
			continue;
		}
		// Still walking or busy (e.g. fighting): try again next think. Same without a defense position.
		if (not Commands->GetIsUnitIdle())
		{
			continue;
		}
		if (MoveUnitToRandomDefensePosition(Unit, *Commands, TEXT("stand_spot_release"), Unit->GetActorLocation()))
		{
			M_StandSpotWaits.RemoveAtSwap(Index);
		}
	}
}

bool UEnemyStrategicAIComponent::TryGetRandomBlackboardDefensePosition(FDefensePositions& OutDefensePosition) const
{
	if (M_Blackboard.CurrentBaseDefensePositions.IsEmpty())
	{
		return false;
	}

	const int32 DefensePositionIndex = GetSeededIndex(M_Blackboard.CurrentBaseDefensePositions.Num());
	if (not M_Blackboard.CurrentBaseDefensePositions.IsValidIndex(DefensePositionIndex))
	{
		return false;
	}

	OutDefensePosition = M_Blackboard.CurrentBaseDefensePositions[DefensePositionIndex];
	return true;
}

void UEnemyStrategicAIComponent::DebugTrainedUnit(
	const FTrainingOption& TrainingOption,
	const FVector& TrainingLocation) const
{
	constexpr float TrainedUnitDebugSphereRadius = 100.f;
	const FString TrainedUnitDebugText = FString::Printf(
		TEXT("trained : %s"),
		*TrainingOption.GetDisplayName());
	DebugPoint(
		TrainingLocation,
		TrainedUnitDebugSphereRadius,
		FColor::Green,
		EnemyAISettings::Debugging::TrainingPressureDebugDuration,
		TrainedUnitDebugText);
}

bool UEnemyStrategicAIComponent::GetValidTrainerComponentLocationFromBlackboard(FTransform& OutSpawnTransform) const
{
	OutSpawnTransform = FTransform::Identity;
	for (TWeakObjectPtr<UTrainerComponent> EachEnemyTrainer : M_Blackboard.TrainerComponents)
	{
		if (not EachEnemyTrainer.IsValid())
		{
			continue;
		}
		if (not EachEnemyTrainer->GetSpawnTransform(OutSpawnTransform))
		{
			continue;
		}
		return true;
	}
	return false;
}

void UEnemyStrategicAIComponent::TrainingPressure_ThinkStep()
{
	if (not GetIsAllowedUnitTraining() || not EnsureStochasticDecisionTreeIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float GameTimeSeconds = World->GetTimeSeconds();
	const TArray<FStrategicAIAction>& ActionDefinitions = M_StochasticDecisionTree->GetActionDefinitions();
	for (const FStrategicAIAction& ActionDefinition : ActionDefinitions)
	{
		for (const TObjectPtr<UStrategicAISubAction>& SubAction : ActionDefinition.GetSubActions())
		{
			AccumulateTrainingPressureFromSubAction(SubAction.Get(), GameTimeSeconds);
		}
	}
}

void UEnemyStrategicAIComponent::AccumulateTrainingPressureFromSubAction(
	const UStrategicAISubAction* SubAction,
	const float GameTimeSeconds)
{
	if (not IsValid(SubAction))
	{
		return;
	}

	TArray<FEnemyStrategicTrainingPressureContribution> PressureContributions;
	SubAction->BuildTrainingPressureContributions(M_Blackboard, GameTimeSeconds, PressureContributions);
	for (const FEnemyStrategicTrainingPressureContribution& PressureContribution : PressureContributions)
	{
		M_TrainingState.AddTrainingPressureContribution(PressureContribution);
	}
}

void UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep()
{
	if (not EnsureIsValidUnitManager())
	{
		return;
	}

	// Tech levels unlock from the buildings of the player this controller plays.
	const uint8 EnemyPlayerIndex = static_cast<uint8>(GetControlledPlayer());
	FEnemyLevelTraining& EnemyLevelTraining = M_TrainingState.EnemyLevelTraining;
	// This accumulates all the buidling types needed for the various tech levels;
	const TArray<EBuildingExpansionType> RequiredBxpTypes = EnemyLevelTraining.GetUniqueBuildingTypesForTechLevels();
	const TMap<EBuildingExpansionType, int32> BxpCountsByType = M_GameUnitManager->GetPlayerBxpCountsByType(
		EnemyPlayerIndex,
		RequiredBxpTypes);

	TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap = M_TrainingState.TechLevelUnlockedMap;
	TechLevelUnlockedMap.Empty();
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.BasicInfantryOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.LightTankOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.MediumTankOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier2Options,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.AdvancedInfantryOptions,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier3Options,
		BxpCountsByType);
	EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.ExperimentalOptions,
		BxpCountsByType);
}

void UEnemyStrategicAIComponent::GetTrainingPoints_ThinkStep()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float ElapsedSeconds = FMath::Max(
		0.f,
		World->GetTimeSeconds() - M_TrainingPointsThinkTimer.LastTimeThought);
	AccumulateTrainingPointIncomeForElapsedSeconds(ElapsedSeconds);
}

void UEnemyStrategicAIComponent::AccumulateTrainingPointIncomeForElapsedSeconds(const float ElapsedSeconds)
{
	constexpr float SecondsPerMinute = 60.f;
	const float SafeElapsedSeconds = FMath::Max(0.f, ElapsedSeconds);
	const float EarnedTrainingPoints = M_TrainingState.TrainingPointIncomeRemainder
		+ M_TrainingState.EnemyLevelTraining.TrainingPointsPerMinute * SafeElapsedSeconds / SecondsPerMinute;
	const int32 WholeTrainingPoints = FMath::FloorToInt(EarnedTrainingPoints);

	M_TrainingState.TrainingPoints += WholeTrainingPoints;
	M_TrainingState.TrainingPointIncomeRemainder = EarnedTrainingPoints - WholeTrainingPoints;
}

bool UEnemyStrategicAIComponent::EnsureEnemyControllerIsValid() const
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
		"M_EnemyController",
		"EnsureEnemyControllerIsValid",
		this);

	return false;
}

int32 UEnemyStrategicAIComponent::GetControlledPlayer() const
{
	const AEnemyController* EnemyController = Cast<AEnemyController>(GetOwner());
	if (not IsValid(EnemyController))
	{
		return RTSPlayers::FirstAIPlayer;
	}
	return EnemyController->GetControlledPlayer();
}


bool UEnemyStrategicAIComponent::EnsureStochasticDecisionTreeIsValid() const
{
	if (M_StochasticDecisionTree.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_StochasticDecisionTree",
		"EnsureStochasticDecisionTreeIsValid",
		this);

	return false;
}

void UEnemyStrategicAIComponent::CacheGenerationSeedFromGameInstance()
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
		RTSFunctionLibrary::ReportError("Enemy strategic AI component could not cache generation seed.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}

int32 UEnemyStrategicAIComponent::GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt) const
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

void UEnemyStrategicAIComponent::StartStrategicAIThinkingTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(M_StrategicAIThinkingTimerHandle))
	{
		return;
	}

	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	TimerManager.ClearTimer(M_StrategicAIStartDelayTimerHandle);
	TimerManager.SetTimer(
		M_StrategicAIThinkingTimerHandle,
		this,
		&UEnemyStrategicAIComponent::StrategicAiThinkingLoop,
		EnemyAISettings::EnemyStrategicAIThinkingSpeed,
		true);
	// Bases dig in from the moment the AI starts thinking; in survival that is the grace period.
	if (UEnemyBaseTrenchComponent* BaseTrenches = M_EnemyController->GetEnemyBaseTrenchComponent())
	{
		BaseTrenches->StartBaseTrenchDigging();
	}
}

void UEnemyStrategicAIComponent::StartStrategicAIThinkingTimerAfterDelay(const float DelaySeconds)
{
	if (DelaySeconds <= 0.f)
	{
		StartStrategicAIThinkingTimer();
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_StrategicAIStartDelayTimerHandle,
		this,
		&UEnemyStrategicAIComponent::StartStrategicAIThinkingTimer,
		DelaySeconds,
		false);
}


void UEnemyStrategicAIComponent::StrategicAiThinkingLoop()
{
	
	TRACE_CPUPROFILER_EVENT_SCOPE("StrategicAI_ThinkingLoop")
	const float Now = GetWorld()->GetTimeSeconds();
	ClearInvalidIdleUnitsFromBlackboard();
	Training_ReleaseStandSpotWaiters();
	for (auto EachThinkTimer : M_AIThinkTimers)
	{
		if (not EachThinkTimer || not EachThinkTimer->ThinkStepDelegate.IsBound())
		{
			continue;
		}
		// Sees if enough time has passed since last think step then calls delegate.
		EachThinkTimer->TryExecuteThinkStep(Now);
	}
	if (GetIsAllowedDirectControlUnits() && EnsureStochasticDecisionTreeIsValid())
	{
		M_StochasticDecisionTree->DecisionTree_ThinkStep(Now, M_Blackboard);
	}
	if (GetIsAllowedUnitTraining())
	{
		Training_ThinkStep();
	}
	ProcessStrategicAIRequests();
}

void UEnemyStrategicAIComponent::StopStrategicAIThinkingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_StrategicAIStartDelayTimerHandle);
		World->GetTimerManager().ClearTimer(M_StrategicAIThinkingTimerHandle);
	}
}

void UEnemyStrategicAIComponent::ProcessStrategicAIRequests()
{
	
	TRACE_CPUPROFILER_EVENT_SCOPE("StrategicAI_ProcessLoops")
	if (M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.IsEmpty()
		&& M_PendingRequests.GetPlayerUnitCountsAndBaseRequests.IsEmpty()
		&& M_PendingRequests.FindAlliedTanksToRetreatRequests.IsEmpty()
		&& M_PendingRequests.FindEnemyBaseClustersRequests.IsEmpty()
		&& M_PendingRequests.FindLocationsUnderPlayerAttackRequests.IsEmpty()
		&& M_PendingRequests.FindPlayerUnitBulkLocationsRequests.IsEmpty()
		&& M_PendingRequests.FindConstructionLocationsRequests.IsEmpty()
		&& M_PendingRequests.FindMineLocationsRequests.IsEmpty())
	{
		return;
	}

	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Enemy strategic AI requires a valid game unit manager.");
		return;
	}

	FStrategicAIRequestBatch RequestsToSend = MoveTemp(M_PendingRequests);
	M_PendingRequests = FStrategicAIRequestBatch();
	// Stamped at every send: the async thread cannot read player relations, and teams may change during a game.
	const int32 ControlledPlayer = GetControlledPlayer();
	RequestsToSend.Requester.RequestingPlayer = ControlledPlayer;
	RequestsToSend.Requester.HostileMask = FRTSPlayers::GetHostileMask(ControlledPlayer);
	RequestsToSend.Requester.FriendlyMask = FRTSPlayers::GetFriendlyMask(ControlledPlayer);

	TWeakObjectPtr<UEnemyStrategicAIComponent> WeakThis(this);
	GameUnitManager->RequestStrategicAIRequests(
		RequestsToSend,
		[WeakThis](const FStrategicAIResultBatch& ResultBatch)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnStrategicAIResultsReceived(ResultBatch);
			}
		});
}

void UEnemyStrategicAIComponent::OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch)
{
	M_LatestResults = ResultBatch;
	ProcessEnemyBaseClusterResults(ResultBatch.EnemyBaseClustersResults);
	ProcessPlayerUnitCountsAndBaseResults(ResultBatch.PlayerUnitCountsResults);
	// IMPORTANT; after unit counts to quickly invalidate old flanking data.
	ProcessClosestFlankableEnemyHeavyResults(ResultBatch.FindClosestFlankableEnemyHeavyResults);
	ProcessAlliedTanksToRetreatResults(ResultBatch.AlliedTanksToRetreatResults);
	ProcessLocationsUnderPlayerAttackResults(ResultBatch.LocationsUnderPlayerAttackResults);
	ProcessPlayerUnitBulkLocationsResults(ResultBatch.PlayerUnitBulkLocationsResults);
	ProcessConstructionLocationsResults(ResultBatch.ConstructionLocationsResults);
	ProcessMineLocationsResults(ResultBatch.MineLocationsResults);
	OnStrategicAIResultsApplied.Broadcast(ResultBatch);
}

void UEnemyStrategicAIComponent::ProcessClosestFlankableEnemyHeavyResults(
	const TArray<FResultClosestFlankableEnemyHeavy>& ClosestFlankableEnemyHeavyResults)
{
	if (M_Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks <= 0)
	{
		// Disregard data and clear all older flanking positions.
		M_Blackboard.AgreggatedHeavyTankFlankingResults.Empty();
		return;
	}

	if (ClosestFlankableEnemyHeavyResults.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	M_Blackboard.AgreggatedHeavyTankFlankingResults.Reset(ClosestFlankableEnemyHeavyResults.Num());
	for (const FResultClosestFlankableEnemyHeavy& Result : ClosestFlankableEnemyHeavyResults)
	{
		FResultClosestFlankableEnemyHeavy ResultWithTimestamp = Result;
		ResultWithTimestamp.ResultTimestampSeconds = CurrentTimeSeconds;
		M_Blackboard.AgreggatedHeavyTankFlankingResults.Add(MoveTemp(ResultWithTimestamp));
	}
	if (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::FlankPositionsDebugging)
	{
		DebugFlankingPositiions();
	}
}

void UEnemyStrategicAIComponent::ProcessEnemyBaseClusterResults(
	const TArray<FResultEnemyBaseClusters>& EnemyBaseClusterResults)
{
	if (EnemyBaseClusterResults.IsEmpty())
	{
		return;
	}

	for (const FResultEnemyBaseClusters& BaseClusterResult : EnemyBaseClusterResults)
	{
		M_Blackboard.EnemyBasePoints = BaseClusterResult.BasePoints;
		M_Blackboard.CurrentBaseDefensePositions = BaseClusterResult.DefensePointCandidates;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::BaseLocationDebugging)
	{
		DebugBlackboardBasePoints();
	}
}

void UEnemyStrategicAIComponent::ProcessAlliedTanksToRetreatResults(
	const TArray<FResultAlliedTanksToRetreat>& AlliedTanksToRetreatResults)
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	if (AlliedTanksToRetreatResults.IsEmpty())
	{
		return;
	}

	UEnemyDirectControlComponent* EnemyDirectControlComponent = M_EnemyController->GetEnemyDirectControlComponent();
	if (not GetIsValidEnemyDirectControlComponent(EnemyDirectControlComponent))
	{
		return;
	}

	for (const FResultAlliedTanksToRetreat& RetreatResult : AlliedTanksToRetreatResults)
	{
		EnemyDirectControlComponent->HandleAsyncRetreatGroupResult(RetreatResult);
	}
}

void UEnemyStrategicAIComponent::ProcessPlayerUnitCountsAndBaseResults(
	const TArray<FResultPlayerUnitCounts>& PlayerUnitCountsAndBaseResults)
{
	if (PlayerUnitCountsAndBaseResults.IsEmpty())
	{
		return;
	}
	// Always get the last entry; most up to date.
	M_Blackboard.CurrentPlayerUnitCounts = PlayerUnitCountsAndBaseResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::PlayerCountsDebugging)
	{
		DebugBlackboardUnitCounts();
	}
}

void UEnemyStrategicAIComponent::ProcessLocationsUnderPlayerAttackResults(
	const TArray<FResultLocationsUnderPlayerAttack>& LocationsUnderPlayerAttackResults)
{
	// Walk back to front so the most recent result of each kind wins, like taking Last() before base-combat routing.
	const FResultLocationsUnderPlayerAttack* LatestDesignerResult = nullptr;
	const FResultLocationsUnderPlayerAttack* LatestBasesInCombatResult = nullptr;
	for (int32 ResultIndex = LocationsUnderPlayerAttackResults.Num() - 1; ResultIndex >= 0; --ResultIndex)
	{
		const FResultLocationsUnderPlayerAttack& Result = LocationsUnderPlayerAttackResults[ResultIndex];
		const FResultLocationsUnderPlayerAttack*& LatestOfKind = GetIsBasesInCombatResult(Result)
			                                                         ? LatestBasesInCombatResult
			                                                         : LatestDesignerResult;
		if (LatestOfKind == nullptr)
		{
			LatestOfKind = &Result;
		}
	}

	if (LatestBasesInCombatResult != nullptr)
	{
		M_Blackboard.CurrentEnemyBasesInCombat = *LatestBasesInCombatResult;
	}
	if (LatestDesignerResult == nullptr)
	{
		return;
	}
	M_Blackboard.CurrentLocationsUnderPlayerAttack = *LatestDesignerResult;
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::LocationsUnderPlayerAttackDebugging)
	{
		DebugBlackboardLocationsUnderAttack();
	}
}

bool UEnemyStrategicAIComponent::GetIsBasesInCombatResult(const FResultLocationsUnderPlayerAttack& Result) const
{
	return Result.RequestID == EnemyAISettings::DamagedVehicleRetreat::BasesInCombatRequestID;
}

void UEnemyStrategicAIComponent::ProcessPlayerUnitBulkLocationsResults(
	const TArray<FResultPlayerUnitBulkLocations>& PlayerUnitBulkLocationsResults)
{
	if (PlayerUnitBulkLocationsResults.IsEmpty())
	{
		return;
	}
	M_Blackboard.CurrentPlayerUnitBulkLocations = PlayerUnitBulkLocationsResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::PlayerUnitBulkDebugging)
	{
		DebugBlackboardBulkPlayerUnits();
	}
}

void UEnemyStrategicAIComponent::ProcessConstructionLocationsResults(
	const TArray<FResultConstructionLocations>& ConstructionLocationsResults)
{
	if (ConstructionLocationsResults.IsEmpty())
	{
		return;
	}

	M_Blackboard.CurrentConstructionLocations = ConstructionLocationsResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::ConstructionLocationsDebugging)
	{
		DebugConstructionLocations();
	}
}

void UEnemyStrategicAIComponent::ProcessMineLocationsResults(
	const TArray<FResultMineLocations>& MineLocationsResults)
{
	if (MineLocationsResults.IsEmpty())
	{
		return;
	}

	M_Blackboard.CurrentMineLocations = MineLocationsResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols
		&& EnemyAISettings::Debugging::MineLocationsDebugging)
	{
		DebugMineLocations();
	}
}

bool UEnemyStrategicAIComponent::GetIsValidEnemyDirectControlComponent(
	UEnemyDirectControlComponent* EnemyDirectControlComponent) const
{
	if (IsValid(EnemyDirectControlComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"EnemyDirectControlComponent",
		"GetIsValidEnemyDirectControlComponent",
		this);

	return false;
}

void UEnemyStrategicAIComponent::FillRetreatRequestExcludedUnits(FFindAlliedTanksToRetreat& RequestToFill) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyDirectControlComponent* EnemyDirectControlComponent = M_EnemyController->GetEnemyDirectControlComponent();
	if (not GetIsValidEnemyDirectControlComponent(EnemyDirectControlComponent))
	{
		return;
	}

	RequestToFill.ExcludedRetreatUnitActors = EnemyDirectControlComponent->GetRetreatGroupUnitsToExclude();
}

void UEnemyStrategicAIComponent::ClearInvalidIdleUnitsFromBlackboard()
{
	TArray<FBlackboardIdleUnitEntry> ValidIdleUnits;
	bool bWasAnyUnitInvalid = false;
	for (auto EachIdleUnit : M_Blackboard.IdleDirectControlUnits)
	{
		if (RTSFunctionLibrary::RTSIsValidWeak(EachIdleUnit.IdleUnit))
		{
			ValidIdleUnits.Add(EachIdleUnit);
		}
		else
		{
			bWasAnyUnitInvalid = true;
		}
	}
	if (bWasAnyUnitInvalid)
	{
		M_Blackboard.IdleDirectControlUnits = ValidIdleUnits;
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardBasePoints() const
{
	using namespace EnemyAISettings::Debugging;
	for (const FEnemyBasePointCoreBuildings& BasePoint : M_Blackboard.EnemyBasePoints)
	{
		DebugPoint(BasePoint.BaseLocation, BaseLocationDebuggingRadius, EnemyLocationColor, BaseLocationDebugDuration,
		           "Enemy Base Point");
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardLocationsUnderAttack() const
{
	using namespace EnemyAISettings::Debugging;
	for (const auto& EachUnderAttackLocation : M_Blackboard.CurrentLocationsUnderPlayerAttack.
	                                                        LocationsUnderAttack)
	{
		DebugPoint(EachUnderAttackLocation.LocationUnderAttack, LocationUnderAttackDebuggingRadius, AttackLocationColor,
		           LocationsUnderAttackDuration, "Location Under Player Attack");
		DebugPoint(EachUnderAttackLocation.AverageAttackerLocation, LocationUnderAttackDebuggingRadius,
		           PlayerLocationColor, LocationsUnderAttackDuration, "avg Loc: attack location Units");
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardBulkPlayerUnits() const
{
	using namespace EnemyAISettings::Debugging;
	for (const auto& EachBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		FString DebugText = FString::Printf(TEXT("Player Unit Bulk: %d units"), EachBulk.UnitsInBulk.Num());
		DebugPoint(EachBulk.BulkLocation, PlayerUnitBulkLocationDebuggingRadius, PlayerLocationColor,
		           PlayerUnitBulkLocationDebugDuration, DebugText);
	}
}

void UEnemyStrategicAIComponent::DebugConstructionLocations() const
{
	using namespace EnemyAISettings::Debugging;
	for (const FVector& ConstructionLocation : M_Blackboard.CurrentConstructionLocations.ConstructionLocations)
	{
		DebugPoint(
			ConstructionLocation,
			ConstructionLocationDebuggingRadius,
			ConstructionLocationColor,
			ConstructionLocationDebugDuration,
			TEXT("Construction Location"));
	}
}

void UEnemyStrategicAIComponent::DebugMineLocations() const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols
		&& EnemyAISettings::Debugging::MineLocationsDebugging)
	{
		using namespace EnemyAISettings::Debugging;
		const auto DrawMineLocation = [this](const FVector& MineLocation)
		{
			DrawDebugSphere(
				GetWorld(),
				MineLocation,
				MineLocationDebuggingRadius,
				12,
				MineLocationColor,
				false,
				MineLocationDebugDuration,
				0,
				2.f);
		};

		for (const FVector& RoadMineLocation : M_Blackboard.CurrentMineLocations.RoadMineLocations)
		{
			DrawMineLocation(RoadMineLocation);
		}
		for (const FVector& DefenseArcMineLocation : M_Blackboard.CurrentMineLocations.DefenseArcMineLocations)
		{
			DrawMineLocation(DefenseArcMineLocation);
		}
	}
}

void UEnemyStrategicAIComponent::DebugPoint(const FVector& Point, const float Radius,
                                            const FColor& Color, const float Duration, const FString& Text) const
{
	DrawDebugSphere(GetWorld(), Point, Radius, 20,
	                Color, false, Duration, 0, 5.f);
	DrawDebugString(
		GetWorld(), Point + FVector(0, 0, Radius),
		Text, nullptr, Color, Duration,
		false);
}

void UEnemyStrategicAIComponent::DebugBlackboardUnitCounts() const
{
	FString DebugString = "\n-------- Player Counts --------";
	DebugString += FString::Printf(
		TEXT("\n Armored Cars: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerArmoredCars);
	DebugString += FString::Printf(
		TEXT("\n Light Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerLightTanks);
	DebugString += FString::Printf(
		TEXT("\n Medium Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerMediumTanks);
	DebugString += FString::Printf(
		TEXT("\n Heavy Tanks: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks);
	DebugString += FString::Printf(
		TEXT("\n Squads: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerSquads);
	DebugString += FString::Printf(
		TEXT("\n Nomads: %d"), M_Blackboard.CurrentPlayerUnitCounts.PlayerNomadicVehicles);
	const bool bFoundHQ = M_Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation != FVector::ZeroVector;
	DebugString += FString::Printf(
		TEXT("\n Found Player HQ: %d"), bFoundHQ ? 1 : 0);
	DebugString += FString::Printf(
		TEXT("\n Resource Buildings: %d"),
		M_Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings.Num());
	RTSFunctionLibrary::PrintString(
		DebugString, FColor::Green, EnemyAISettings::Debugging::PlayerCountsDuration);
}

void UEnemyStrategicAIComponent::DebugFlankingPositiions() const
{
	for (auto EachFlank : M_Blackboard.AgreggatedHeavyTankFlankingResults)
	{
		for (const auto& EachHeavyLocations : EachFlank.FlankLocationsAroundHeavyTank)
			for (const auto& EachLocation : EachHeavyLocations.Locations)
			{
				DrawDebugSphere(GetWorld(), EachLocation, 33.f, 20,
				                EnemyAISettings::Debugging::FlankLocationColor, false, 2.f, 0, 5.f);
			}
	}
}

float UEnemyStrategicAIComponent::GetEnemyGlobalAbilityThinkStepInterval() const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return EnemyAISettings::ThinkingTimers::UpdateEnemyGlobalAbility_Interval;
	}

	const FEnemyGlobalAbilityAISettings& Settings = M_EnemyController.Get()->GetEnemyGlobalAbilityAISettings();
	if (Settings.ThinkStepInterval <= 0.0f)
	{
		return EnemyAISettings::ThinkingTimers::UpdateEnemyGlobalAbility_Interval;
	}

	return FMath::Max(
		Settings.ThinkStepInterval,
		EnemyAISettings::ThinkingTimers::MinimumEnemyGlobalAbility_Interval);
}

void UEnemyStrategicAIComponent::EnemyGlobalAbility_ThinkStep()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UGlobalAbilitiesManager* GlobalAbilitiesManager = M_EnemyController.Get()->GetEnemyGlobalAbilitiesManager();
	if (not IsValid(GlobalAbilitiesManager))
	{
		return;
	}

	UGlobalAbility* AbilityToExecute = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	if (not TryPickEnemyGlobalAbilityAndTarget(AbilityToExecute, TargetLocation))
	{
		return;
	}

	GlobalAbilitiesManager->TryExecuteAbilityAtLocation(AbilityToExecute, TargetLocation);
}

bool UEnemyStrategicAIComponent::TryPickEnemyGlobalAbilityAndTarget(
	UGlobalAbility*& OutAbility,
	FVector& OutTargetLocation) const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	const UGlobalAbilitiesManager* GlobalAbilitiesManager = M_EnemyController.Get()->GetEnemyGlobalAbilitiesManager();
	if (not IsValid(GlobalAbilitiesManager))
	{
		return false;
	}

	const TArray<UGlobalAbility*> AvailableAbilities = GlobalAbilitiesManager->GetAvailableEnemyGlobalAbilities();
	TArray<TTuple<UGlobalAbility*, FVector>> AbilityTargetPairs;
	const auto AddBucketPairs = [this, &AvailableAbilities, &AbilityTargetPairs](
		const TArray<FVector>& TargetLocations,
		bool (UEnemyStrategicAIComponent::*Predicate)(EGlobalAbility) const)
	{
		TArray<UGlobalAbility*> BucketAbilities;
		AddAbilityBucketOptions(AvailableAbilities, BucketAbilities, Predicate);
		for (UGlobalAbility* BucketAbility : BucketAbilities)
		{
			for (const FVector& TargetLocation : TargetLocations)
			{
				AbilityTargetPairs.Emplace(BucketAbility, TargetLocation);
			}
		}
	};

	if (GetCanUseReinforcementAbilities())
	{
		AddBucketPairs(
			GetReinforcementAbilityTargetLocations(),
			&UEnemyStrategicAIComponent::IsEnemyReinforcementAbility);
	}
	if (GetCanUseBarrages())
	{
		AddBucketPairs(GetBarrageTargetLocations(), &UEnemyStrategicAIComponent::IsEnemyBarrageAbility);
	}
	if (GetCanUseLightMediumAirStrikes())
	{
		AddBucketPairs(
			GetLightMediumAirStrikeTargetLocations(),
			&UEnemyStrategicAIComponent::IsEnemyLightMediumAirStrikeAbility);
	}
	if (GetCanUseHeavyCarpetBombing())
	{
		AddBucketPairs(
			GetHeavyCarpetBombingTargetLocations(),
			&UEnemyStrategicAIComponent::IsEnemyHeavyCarpetBombingAbility);
	}

	if (AbilityTargetPairs.IsEmpty())
	{
		return false;
	}

	const TTuple<UGlobalAbility*, FVector>& PickedPair = AbilityTargetPairs[GetSeededIndex(AbilityTargetPairs.Num())];
	OutAbility = PickedPair.Get<0>();
	OutTargetLocation = PickedPair.Get<1>();
	return IsValid(OutAbility) && not OutTargetLocation.IsNearlyZero();
}

void UEnemyStrategicAIComponent::AddAbilityBucketOptions(
	const TArray<UGlobalAbility*>& AvailableAbilities,
	TArray<UGlobalAbility*>& OutBucket,
	bool (UEnemyStrategicAIComponent::*Predicate)(EGlobalAbility) const) const
{
	for (UGlobalAbility* Ability : AvailableAbilities)
	{
		if (IsValid(Ability) && (this->*Predicate)(Ability->GetAbilityType()))
		{
			OutBucket.Add(Ability);
		}
	}
}

bool UEnemyStrategicAIComponent::IsEnemyReinforcementAbility(const EGlobalAbility AbilityType) const
{
	return AbilityType == EGlobalAbility::GA_SovietT34Drop
		|| AbilityType == EGlobalAbility::GA_SovietT34DropVariation
		|| AbilityType == EGlobalAbility::GA_SovietInfantryDrop
		|| AbilityType == EGlobalAbility::GA_SovietParatroopers
		|| AbilityType == EGlobalAbility::GA_SovietHeavyTankDrop
		|| AbilityType == EGlobalAbility::GA_SovietHeavyTankDropVariation
		// Reinforcement targets are its defended positions, which is where a minefield belongs.
		|| AbilityType == EGlobalAbility::GA_SovietAirMinefield;
}

bool UEnemyStrategicAIComponent::IsEnemyBarrageAbility(const EGlobalAbility AbilityType) const
{
	return AbilityType == EGlobalAbility::GA_SovietSmallBarrage
		|| AbilityType == EGlobalAbility::GA_SovietLargeBarrage
		|| AbilityType == EGlobalAbility::GA_SovietLargeBarrageVariation;
}

bool UEnemyStrategicAIComponent::IsEnemyLightMediumAirStrikeAbility(const EGlobalAbility AbilityType) const
{
	return AbilityType == EGlobalAbility::GA_2xPE2Strafing
		|| AbilityType == EGlobalAbility::GA_4xPE2Strafing
		|| AbilityType == EGlobalAbility::GA_SovietReconFlight;
}

bool UEnemyStrategicAIComponent::IsEnemyHeavyCarpetBombingAbility(const EGlobalAbility AbilityType) const
{
	return AbilityType == EGlobalAbility::GA_HeavyCarpet_PE8_500Gr
		|| AbilityType == EGlobalAbility::GA_HeavyCarpet_PE8_1000Gr
		|| AbilityType == EGlobalAbility::GA_HeavyCarpet_PE8_3000Gr
		|| AbilityType == EGlobalAbility::GA_SovietRadixiteNuke;
}

bool UEnemyStrategicAIComponent::GetCanUseReinforcementAbilities() const
{
	return not M_Blackboard.CurrentBaseDefensePositions.IsEmpty();
}

bool UEnemyStrategicAIComponent::GetCanUseBarrages() const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FEnemyGlobalAbilityAISettings& Settings = M_EnemyController.Get()->GetEnemyGlobalAbilityAISettings();
	return World->GetTimeSeconds() - M_EnemyGlobalAbilityAIStartTimeSeconds >= Settings.TimeTillAllowBarrages
		&& not GetBarrageTargetLocations().IsEmpty();
}

bool UEnemyStrategicAIComponent::GetCanUseLightMediumAirStrikes() const
{
	return not M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.IsEmpty();
}

bool UEnemyStrategicAIComponent::GetCanUseHeavyCarpetBombing() const
{
	if (not EnsureEnemyControllerIsValid())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FEnemyGlobalAbilityAISettings& Settings = M_EnemyController.Get()->GetEnemyGlobalAbilityAISettings();
	return World->GetTimeSeconds() - M_EnemyGlobalAbilityAIStartTimeSeconds >= Settings.TimeTillAllowHeavyCarpetBombing
		&& not M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks.IsEmpty();
}

TArray<FVector> UEnemyStrategicAIComponent::GetReinforcementAbilityTargetLocations() const
{
	TArray<FVector> Locations;
	Locations.Reserve(M_Blackboard.CurrentBaseDefensePositions.Num());
	for (const FDefensePositions& DefensePosition : M_Blackboard.CurrentBaseDefensePositions)
	{
		if (not DefensePosition.Location.IsNearlyZero())
		{
			Locations.Add(DefensePosition.Location);
		}
	}
	return Locations;
}

TArray<FVector> UEnemyStrategicAIComponent::GetBarrageTargetLocations() const
{
	TArray<FVector> Locations;
	for (const FPlayerAttackLocationEvaluation& AttackLocation : M_Blackboard.CurrentLocationsUnderPlayerAttack.LocationsUnderAttack)
	{
		if (not AttackLocation.LocationUnderAttack.IsNearlyZero())
		{
			Locations.Add(AttackLocation.LocationUnderAttack);
		}
	}
	for (const FResultClosestFlankableEnemyHeavy& FlankingResult : M_Blackboard.AgreggatedHeavyTankFlankingResults)
	{
		for (const FWeakActorLocations& HeavyTankLocations : FlankingResult.FlankLocationsAroundHeavyTank)
		{
			if (HeavyTankLocations.Actor.IsValid())
			{
				Locations.Add(HeavyTankLocations.Actor.Get()->GetActorLocation());
			}
		}
	}
	return Locations;
}

TArray<FVector> UEnemyStrategicAIComponent::GetLightMediumAirStrikeTargetLocations() const
{
	TArray<FVector> Locations;
	for (const FPlayerUnitBulkLocation& PlayerUnitBulk : M_Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		if (not PlayerUnitBulk.BulkLocation.IsNearlyZero())
		{
			Locations.Add(PlayerUnitBulk.BulkLocation);
		}
	}
	return Locations;
}

TArray<FVector> UEnemyStrategicAIComponent::GetHeavyCarpetBombingTargetLocations() const
{
	return GetLightMediumAirStrikeTargetLocations();
}

void UEnemyStrategicAIComponent::SetStochasticDecisionTree(UStochasticDecisionTree* NewTree)
{
	M_StochasticDecisionTree = NewTree;
}
