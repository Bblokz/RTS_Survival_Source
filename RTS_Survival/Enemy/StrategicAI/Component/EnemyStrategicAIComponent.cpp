// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrategicAIComponent.h"

#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
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

const FStrategicAIResultBatch& UEnemyStrategicAIComponent::GetLatestStrategicAIResults() const
{
	return M_LatestResults;
}

void UEnemyStrategicAIComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheGenerationSeedFromGameInstance();
	StartStrategicAIThinkingTimer();
}

void UEnemyStrategicAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStrategicAIThinkingTimer();
	Super::EndPlay(EndPlayReason);
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
		&UEnemyStrategicAIComponent::ProcessStrategicAIRequests,
		EnemyAISettings::EnemyStrategicAIThinkingSpeed,
		true);
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
		&& M_PendingRequests.FindAlliedTanksToRetreatRequests.IsEmpty())
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
}
