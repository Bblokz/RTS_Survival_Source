// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrategicAIComponent.h"

#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UEnemyStrategicAIComponent::UEnemyStrategicAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CacheGenerationSeedFromGameInstance();
}

void UEnemyStrategicAIComponent::InitStrategicAIComponent(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
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

void UEnemyStrategicAIComponent::RequestRetreatDamagedTanks(const FFindAlliedTanksToRetreat& Request)
{
	QueueFindAlliedTanksToRetreatRequest(Request);
}

const FStrategicAIResultBatch& UEnemyStrategicAIComponent::GetLatestStrategicAIResults() const
{
	return M_LatestResults;
}

const FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetStrategicAIBlackboard() const
{
	return M_StrategicAIBlackboard;
}

FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetEditableStrategicAIBlackboard()
{
	return M_StrategicAIBlackboard;
}

void UEnemyStrategicAIComponent::BeginPlay()
{
	Super::BeginPlay();
	const float Now = GetWorld()->GetTimeSeconds();
	BeginPlay_PreThinKStep_InitThinkingTimers(Now);
	CacheGenerationSeedFromGameInstance();
	StartStrategicAIThinkingTimer();
}

void UEnemyStrategicAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStrategicAIThinkingTimer();
	Super::EndPlay(EndPlayReason);
}

void UEnemyStrategicAIComponent::BeginPlay_PreThinKStep_InitThinkingTimers(const float Now)
{
	M_AIBaseLocationThinkTimer.LastTimeThought = Now;
	M_AIBaseLocationThinkTimer.ThinkingInterval = EnemyAISettings::ThinkingTimers::UpdateAIBaseLocations_Interval;
	M_AIBaseLocationThinkTimer.ThinkStepDelegate.BindUObject(this, &UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep);
	M_AIThinkTimers.Add(&M_AIBaseLocationThinkTimer);
	
	M_PlayerUnitCountsBuildingCountsThinkTimer.LastTimeThought = Now;
	M_PlayerUnitCountsBuildingCountsThinkTimer.ThinkingInterval = EnemyAISettings::ThinkingTimers::UpdatePlayerCountsBaseLocations_Interval;
	M_AIBaseLocationThinkTimer.ThinkStepDelegate.BindUObject(this, &UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep);
	M_AIThinkTimers.Add(&M_PlayerUnitCountsBuildingCountsThinkTimer);

}

void UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep()
{
}

void UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep()
{
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
		&& M_PendingRequests.FindEnemyBaseClustersRequests.IsEmpty())
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
	ProcessAlliedTanksToRetreatResults(ResultBatch.AlliedTanksToRetreatResults);
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
		M_StrategicAIBlackboard.EnemyBasePoints = BaseClusterResult.BasePoints;
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
