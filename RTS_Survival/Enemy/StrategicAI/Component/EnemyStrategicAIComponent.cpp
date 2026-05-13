// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrategicAIComponent.h"

#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardQueries/BlackboardQueryHelpers.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace EnemyStrategicAITrainingRequirements
{
	bool GetIsTechLevelUnlockedByBxpCounts(
		const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
		const TMap<EBuildingExpansionType, int32>& BxpCountsByType)
	{
		for (const EBuildingExpansionType RequiredBxpType : TrainingOptions.TypesUnlockingThisLevel)
		{
			const int32* const BxpCount = BxpCountsByType.Find(RequiredBxpType);
			if (BxpCount != nullptr && *BxpCount > 0)
			{
				return true;
			}
		}

		return false;
	}

	void UpdateTechLevelUnlockedMapForOptions(
		TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap,
		const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
		const TMap<EBuildingExpansionType, int32>& BxpCountsByType)
	{
		TechLevelUnlockedMap.Add(
			TrainingOptions.TechLevel,
			GetIsTechLevelUnlockedByBxpCounts(TrainingOptions, BxpCountsByType));
	}
}

UEnemyStrategicAIComponent::UEnemyStrategicAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CacheGenerationSeedFromGameInstance();
}

void UEnemyStrategicAIComponent::InitStrategicAIComponent(AEnemyController* EnemyController,
                                                          UStochasticDecisionTree* StochasticDecisionTree)
{
	M_StochasticDecisionTree = StochasticDecisionTree;
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
	const float Now = GetWorld()->GetTimeSeconds();
	PreThinKStep_InitThinkingTimers(Now);
	CacheGenerationSeedFromGameInstance();
	StartStrategicAIThinkingTimer();
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


void UEnemyStrategicAIComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyStrategicAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStrategicAIThinkingTimer();
	Super::EndPlay(EndPlayReason);
}

void UEnemyStrategicAIComponent::BeginPlay_CacheUnitManager()
{
	M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	(void)EnsureIsValidUnitManager();
}

bool UEnemyStrategicAIComponent::EnsureIsValidUnitManager()
{
	if(not M_GameUnitManager.IsValid())
	{
		M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
		if(not M_GameUnitManager.IsValid())
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

void UEnemyStrategicAIComponent::PreThinKStep_InitThinkingTimers(const float Now)
{
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

	M_PlayerUnitBulkLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerUnitBulkLocations_Interval;
	M_PlayerUnitBulkLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitBulkLocationsThinkTimer);

	M_PlayerHeavyTankFlankLocationsThinkTimer.LastTimeThought = Now;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdatePlayerHeavyTankFlank_Interval;
	M_PlayerHeavyTankFlankLocationsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::PlayerHeavyTankFlankLocations_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerHeavyTankFlankLocationsThinkTimer);

	M_TrainingRequirementsThinkTimer.LastTimeThought = Now;
	M_TrainingRequirementsThinkTimer.ThinkingInterval =
		EnemyAISettings::ThinkingTimers::UpdateAITechLevel_Interval;
	M_TrainingRequirementsThinkTimer.ThinkStepDelegate.BindUObject(
		this, &UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep);
	M_AIThinkTimers.Add(&M_TrainingRequirementsThinkTimer);
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

void UEnemyStrategicAIComponent::PlayerUnitBulkLocations_ThinkStep()
{
	QueueFindPlayerUnitBulkLocationsRequest(FindPlayerUnitBulkLocations_TimerRequest);
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

void UEnemyStrategicAIComponent::Training_ThinkStep()
{
	
}

void UEnemyStrategicAIComponent::TrainingRequirements_ThinkStep()
{
	if (not EnsureIsValidUnitManager())
	{
		return;
	}

	constexpr uint8 EnemyPlayerIndex = 2;
	FEnemyLevelTraining& EnemyLevelTraining = M_TrainingState.EnemyLevelTraining;
	const TArray<EBuildingExpansionType> RequiredBxpTypes = EnemyLevelTraining.GetUniqueBuildingTypesForTechLevels();
	const TMap<EBuildingExpansionType, int32> BxpCountsByType = M_GameUnitManager->GetPlayerBxpCountsByType(
		EnemyPlayerIndex,
		RequiredBxpTypes);

	TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap = M_TrainingState.TechLevelUnlockedMap;
	TechLevelUnlockedMap.Empty();
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.BasicInfantryOptions,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.LightTankOptions,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.MediumTankOptions,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier2Options,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.AdvancedInfantryOptions,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.Tier3Options,
		BxpCountsByType);
	EnemyStrategicAITrainingRequirements::UpdateTechLevelUnlockedMapForOptions(
		TechLevelUnlockedMap,
		EnemyLevelTraining.ExperimentalOptions,
		BxpCountsByType);
}

bool UEnemyStrategicAIComponent::EnsureEnemyControllerIsValid() const
{
	if (M_EnemyController.IsValid())
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
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_StrategicAIThinkingTimerHandle,
		this,
		&UEnemyStrategicAIComponent::StrategicAiThinkStep,
		EnemyAISettings::EnemyStrategicAIThinkingSpeed,
		true);
}


void UEnemyStrategicAIComponent::StrategicAiThinkStep()
{
	const float Now = GetWorld()->GetTimeSeconds();
	ClearInvalidIdleUnitsFromBlackboard();
	for (auto EachThinkTimer : M_AIThinkTimers)
	{
		if (not EachThinkTimer || not EachThinkTimer->ThinkStepDelegate.IsBound())
		{
			continue;
		}
		// Sees if enough time has passed since last think step then calls delegate.
		EachThinkTimer->TryExecuteThinkStep(Now);
	}
	if (M_StochasticDecisionTree.IsValid() && GetIsAllowedDirectControlUnits())
	{
		M_StochasticDecisionTree->DecisionTree_ThinkStep(Now, M_Blackboard);
	}
	ProcessStrategicAIRequests();
}

void UEnemyStrategicAIComponent::StopStrategicAIThinkingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_StrategicAIThinkingTimerHandle);
	}
}

void UEnemyStrategicAIComponent::ProcessStrategicAIRequests()
{
	if (M_PendingRequests.FindClosestFlankableEnemyHeavyRequests.IsEmpty()
		&& M_PendingRequests.GetPlayerUnitCountsAndBaseRequests.IsEmpty()
		&& M_PendingRequests.FindAlliedTanksToRetreatRequests.IsEmpty()
		&& M_PendingRequests.FindEnemyBaseClustersRequests.IsEmpty()
		&& M_PendingRequests.FindLocationsUnderPlayerAttackRequests.IsEmpty()
		&& M_PendingRequests.FindPlayerUnitBulkLocationsRequests.IsEmpty())
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
	if(DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
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
	if (LocationsUnderPlayerAttackResults.IsEmpty())
	{
		return;
	}
	M_Blackboard.CurrentLocationsUnderPlayerAttack = LocationsUnderPlayerAttackResults.Last();
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::LocationsUnderPlayerAttackDebugging)
	{
		DebugBlackboardLocationsUnderAttack();
	}
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
	for (const FEnemyBasePointCoreBuildings& BasePoint : M_StrategicAIBlackboard.EnemyBasePoints)
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
for(auto EachFlank : M_Blackboard.AgreggatedHeavyTankFlankingResults)
{
	for(const auto& EachHeavyLocations : EachFlank.FlankLocationsAroundHeavyTank)
		for(const auto& EachLocation : EachHeavyLocations.Locations)
		{
			DrawDebugSphere(GetWorld(), EachLocation, 33.f, 20,
			                EnemyAISettings::Debugging::FlankLocationColor, false, 2.f, 0, 5.f);
		}
}
}
