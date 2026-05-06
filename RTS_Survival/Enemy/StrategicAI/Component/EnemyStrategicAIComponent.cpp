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

void UEnemyStrategicAIComponent::InitStrategicAIComponent(AEnemyController* EnemyController,
	UStochasticDecisionTree* StochasticDecisionTree)
{
M_StochasticDecisionTree = StochasticDecisionTree;	
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

void UEnemyStrategicAIComponent::QueueFindLocationsUnderPlayerAttackRequest(
	const FFindLocationsUnderPlayerAttack& Request)
{
	M_PendingRequests.FindLocationsUnderPlayerAttackRequests.Add(Request);
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
	return M_StrategicAIBlackboard;
}

FStrategicAIBlackboard& UEnemyStrategicAIComponent::GetEditableStrategicAIBlackboard()
{
	return M_StrategicAIBlackboard;
}

FStrategicAIBlackboard* UEnemyStrategicAIComponent::GetEditableStrategicAIBlackboardPointer()
{
	return &M_StrategicAIBlackboard;
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


bool UEnemyStrategicAIComponent::GetIsAllowedDirectControlUnits() const
{
	return M_StrategicAIBlackboard.StrategicAIMissionSettings.bAllowDirectControlStochasticDecisionTree;
}

void UEnemyStrategicAIComponent::BeginPlay_PreThinKStep_InitThinkingTimers(const float Now)
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
}

void UEnemyStrategicAIComponent::AIBaseLocation_ThinkStep()
{
	QueueFindEnemyBaseClustersRequest(FindEnemyBase_TimerRequest);
}

void UEnemyStrategicAIComponent::PlayerUnitCountsBuildingCounts_ThinkStep()
{
	QueueGetPlayerUnitCountsAndBaseRequest(FindPlayerCountBase_TimerRequest);
}

void UEnemyStrategicAIComponent::LocationsUnderAttack_ThinkStep()
{
	QueueFindLocationsUnderPlayerAttackRequest(FindLocationsUnderAttack_TimerRequest);
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
	for (auto EachThinkTimer : M_AIThinkTimers)
	{
		if (not EachThinkTimer || not EachThinkTimer->ThinkStepDelegate.IsBound())
		{
			continue;
		}
		// Sees if enough time has passed since last think step then calls delegate.
		EachThinkTimer->TryExecuteThinkStep(Now);
	}
	if(M_StochasticDecisionTree.IsValid() && GetIsAllowedDirectControlUnits())
	{
		M_StochasticDecisionTree->DecisionTree_ThinkStep(Now, M_StrategicAIBlackboard);
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
		&& M_PendingRequests.FindLocationsUnderPlayerAttackRequests.IsEmpty())
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
	ProcessAlliedTanksToRetreatResults(ResultBatch.AlliedTanksToRetreatResults);
	ProcessLocationsUnderPlayerAttackResults(ResultBatch.LocationsUnderPlayerAttackResults);
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
	M_StrategicAIBlackboard.CurrentPlayerUnitCounts = PlayerUnitCountsAndBaseResults.Last();
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
	M_StrategicAIBlackboard.CurrentLocationsUnderPlayerAttack = LocationsUnderPlayerAttackResults.Last();
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

void UEnemyStrategicAIComponent::DebugBlackboardBasePoints() const
{
	for (auto EachLocation : M_StrategicAIBlackboard.EnemyBasePoints)
	{
		DrawDebugSphere(GetWorld(), EachLocation, EnemyAISettings::Debugging::BaseLocationDebuggingRadius, 20,
		                FColor::Red, false, EnemyAISettings::Debugging::BaseLocationDebugDuration, 0, 5.f);
		DrawDebugString(
			GetWorld(), EachLocation + FVector(0, 0, EnemyAISettings::Debugging::BaseLocationDebuggingRadius),
			TEXT("Enemy Base Location"), nullptr, FColor::Red, EnemyAISettings::Debugging::BaseLocationDebugDuration,
			false);
	}
}

void UEnemyStrategicAIComponent::DebugBlackboardUnitCounts() const
{
	FString DebugString = "\n-------- Player Counts --------";
	DebugString += FString::Printf(
		TEXT("\n Armored Cars: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerArmoredCars);
	DebugString += FString::Printf(
		TEXT("\n Light Tanks: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerLightTanks);
	DebugString += FString::Printf(
		TEXT("\n Medium Tanks: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerMediumTanks);
	DebugString += FString::Printf(
		TEXT("\n Heavy Tanks: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerHeavyTanks);
	DebugString += FString::Printf(
		TEXT("\n Squads: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerSquads);
	DebugString += FString::Printf(
		TEXT("\n Nomads: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerNomadicVehicles);
	const bool bFoundHQ = M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerHQLocation != FVector::ZeroVector;
	DebugString += FString::Printf(
		TEXT("\n Found Player HQ: %d"), bFoundHQ ? 1 : 0);
	DebugString += FString::Printf(
		TEXT("\n Resource Buildings: %d"), M_StrategicAIBlackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings.Num());
	RTSFunctionLibrary::PrintString(
		DebugString,  FColor::Green, EnemyAISettings::Debugging::PlayerCountsDuration);
}
