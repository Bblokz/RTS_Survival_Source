// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildingSettings.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

#define LOCTEXT_NAMESPACE "WorldBuildNodeManager"

namespace WorldBuildNodeManagerHelpers
{
	bool GetIsPlayerSupportTargetObject(const AWorldMapObject* WorldMapObject)
	{
		return Cast<AWorldEnemyObject>(WorldMapObject) != nullptr || Cast<AWorldMissionObject>(WorldMapObject) != nullptr;
	}

	bool GetHasStrategicSupportFromSource(const UWorldStrengthEstimationComponent& StrengthEstimationComponent,
	                                      const FGuid& SourceKey)
	{
		const TArray<FWorldStrengthContribution> StrengthContributions =
			StrengthEstimationComponent.GetStrengthContributions();
		for (const FWorldStrengthContribution& StrengthContribution : StrengthContributions)
		{
			if (StrengthContribution.StrengthType == EWorldStrengthTypes::StrategicSupport
				&& StrengthContribution.SourceKey == SourceKey)
			{
				return true;
			}
		}
		return false;
	}
}

UWorldBuildNodeManager::UWorldBuildNodeManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_BuildNodeClass = AWorldBuildNode::StaticClass();
}

void UWorldBuildNodeManager::InitializeBuildNodes(AWorldPlayerController* WorldPlayerController)
{
	if (not IsValid(WorldPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("UWorldBuildNodeManager::InitializeBuildNodes: controller is invalid."));
		return;
	}

	M_WorldPlayerController = WorldPlayerController;
	M_CampaignProgressManager = WorldPlayerController->FindComponentByClass<UWorldCampaignProgressManager>();
	M_WorldGenerator = WorldPlayerController->GetWorldGenerator();
	if (GetIsValidWorldGenerator())
	{
		M_CountryRegulator = M_WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>();
	}

	/*
	 * Hand-placed nodes are gathered first so a config entry with the same id never spawns a duplicate.
	 * Buildings are restored before the unlock pass so a built node is Built whatever the country's liberation says.
	 */
	InitializeBuildNodes_GatherPlacedNodes();
	InitializeBuildNodes_SpawnConfigNodes();
	InitializeBuildNodes_RestoreBuildings();
	InitializeBuildNodes_BindToCountryRegulator();
	RecomputeBuildNodeUnlocks();
	ApplyWatchPostFowReveal();
}

bool UWorldBuildNodeManager::TryBuildOnNode(const FName BuildNodeId, const EWorldCampaignBuilding Building,
                                            FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	AWorldBuildNode* BuildNode = GetBuildNode(BuildNodeId);
	if (not IsValid(BuildNode))
	{
		OutFailReason = WorldBuildNodeRules::GetBuildResultText(EWorldBuildNodeBuildResult::UnknownNode);
		return false;
	}

	if (not GetIsValidCampaignProgressManager())
	{
		OutFailReason = LOCTEXT("NoCampaignProgress", "The campaign is not ready to build.");
		return false;
	}

	const FWorldMapResources* CostEntry = UWorldCampaignRulesSettings::GetNumbers().BuildingCosts.Find(Building);
	const EWorldBuildNodeBuildResult BuildResult = WorldBuildNodeRules::GetBuildResult(
		BuildNode->GetNodeState(),
		M_CampaignProgressManager->GetBuildingOnNode(BuildNodeId),
		Building,
		CostEntry,
		M_CampaignProgressManager->GetResources());
	if (BuildResult != EWorldBuildNodeBuildResult::Success)
	{
		OutFailReason = WorldBuildNodeRules::GetBuildResultText(BuildResult);
		return false;
	}

	// Success guarantees a cost entry; copy it so nothing holds a pointer into the settings map while paying.
	const FWorldMapResources Cost = *CostEntry;
	if (not M_CampaignProgressManager->TrySpendResources(Cost))
	{
		OutFailReason = WorldBuildNodeRules::GetBuildResultText(EWorldBuildNodeBuildResult::CannotAfford);
		return false;
	}

	M_CampaignProgressManager->SetBuildingOnNode(BuildNodeId, Building, GetCurrentTurn());
	BuildNode->SetBuilding(Building);
	OnBuildNodeBuildingPlaced.Broadcast(BuildNodeId, Building);
	ApplyBuildingEffectsAfterBuild(Building);
	return true;
}

TArray<EWorldCampaignBuilding> UWorldBuildNodeManager::GetBuildOptionsForNode(const FName BuildNodeId) const
{
	const AWorldBuildNode* BuildNode = GetBuildNode(BuildNodeId);
	if (not IsValid(BuildNode) || BuildNode->GetNodeState() != EWorldBuildNodeState::Unlocked)
	{
		return TArray<EWorldCampaignBuilding>();
	}

	return WorldBuildNodeRules::GetBuildOptions(UWorldCampaignRulesSettings::GetNumbers().BuildingCosts);
}

FWorldMapResources UWorldBuildNodeManager::GetBuildingCost(const EWorldCampaignBuilding Building) const
{
	const FWorldMapResources* Cost = UWorldCampaignRulesSettings::GetNumbers().BuildingCosts.Find(Building);
	return Cost != nullptr ? *Cost : FWorldMapResources();
}

AWorldBuildNode* UWorldBuildNodeManager::GetBuildNode(const FName BuildNodeId) const
{
	const TWeakObjectPtr<AWorldBuildNode>* BuildNode = M_BuildNodesById.Find(BuildNodeId);
	return BuildNode != nullptr ? BuildNode->Get() : nullptr;
}

TArray<AWorldBuildNode*> UWorldBuildNodeManager::GetBuildNodes() const
{
	TArray<AWorldBuildNode*> BuildNodes;
	BuildNodes.Reserve(M_BuildNodesById.Num());
	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		if (AWorldBuildNode* BuildNode = BuildNodeById.Value.Get())
		{
			BuildNodes.Add(BuildNode);
		}
	}
	return BuildNodes;
}

void UWorldBuildNodeManager::ApplyStartOfPlayerTurnIncome()
{
	if (not GetIsValidCampaignProgressManager())
	{
		return;
	}

	const int32 CurrentTurn = GetCurrentTurn();
	if (CurrentTurn == M_LastIncomePaidTurn)
	{
		return;
	}
	M_LastIncomePaidTurn = CurrentTurn;

	int32 PayingBuildingCount = 0;
	for (const FWorldBuildNodeSaveData& BuildingSaveData : M_CampaignProgressManager->GetProgressState().Buildings)
	{
		const FWorldBuildingEffectDefinition* BuildingEffect =
			UWorldBuildingSettings::FindBuildingEffect(BuildingSaveData.Building);
		if (BuildingEffect != nullptr && BuildingEffect->bPaysSupplyDepotIncome)
		{
			PayingBuildingCount++;
		}
	}

	if (PayingBuildingCount == 0)
	{
		return;
	}

	const FWorldMapResources TurnIncome = WorldBuildNodeRules::GetTurnIncome(
		UWorldCampaignRulesSettings::GetNumbers().SupplyDepotIncomePerTurn, PayingBuildingCount);
	M_CampaignProgressManager->AddResources(TurnIncome);
}

void UWorldBuildNodeManager::ApplyPlayerBuildingSupport()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors = M_WorldGenerator->GetPlacementState().CachedAnchors;
	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		const AWorldBuildNode* BuildNode = BuildNodeById.Value.Get();
		if (not IsValid(BuildNode) || BuildNode->GetBuilding() == EWorldCampaignBuilding::None)
		{
			continue;
		}

		const FWorldBuildingEffectDefinition* BuildingEffect =
			UWorldBuildingSettings::FindBuildingEffect(BuildNode->GetBuilding());
		if (BuildingEffect == nullptr || not BuildingEffect->GetReducesEnemyStrength())
		{
			continue;
		}

		ApplySupportFromNode(*BuildNode, *BuildingEffect, CachedAnchors);
	}
}

void UWorldBuildNodeManager::RefreshStrategicSupportWithPlayerBuildings()
{
	if (not GetIsValidWorldGenerator() || not GetIsValidCampaignProgressManager())
	{
		return;
	}

	const ERTSGameDifficulty GameDifficulty =
		M_CampaignProgressManager->GetProgressState().Difficulty.DifficultyLevel;
	M_WorldGenerator->AdjustDifficultyPercentagesForStrategicSupport(GameDifficulty);
	ApplyPlayerBuildingSupport();
}

TArray<FWorldFowRevealSource> UWorldBuildNodeManager::GetWatchPostRevealSources() const
{
	TArray<FWorldFowRevealSource> RevealSources;
	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		const AWorldBuildNode* BuildNode = BuildNodeById.Value.Get();
		if (not IsValid(BuildNode) || BuildNode->GetBuilding() == EWorldCampaignBuilding::None)
		{
			continue;
		}

		const FWorldBuildingEffectDefinition* BuildingEffect =
			UWorldBuildingSettings::FindBuildingEffect(BuildNode->GetBuilding());
		if (BuildingEffect == nullptr || not BuildingEffect->GetRevealsFow())
		{
			continue;
		}

		FWorldFowRevealSource RevealSource;
		RevealSource.SourceId = BuildNode->GetBuildNodeId();
		RevealSource.WorldLocation = BuildNode->GetActorLocation();
		RevealSource.RevealRadius = BuildingEffect->FowRevealRadius;
		RevealSource.RevealFalloff = BuildingEffect->FowRevealFalloff;
		RevealSources.Add(RevealSource);
	}
	return RevealSources;
}

void UWorldBuildNodeManager::ApplyWatchPostFowReveal()
{
	AWorldFowManager* WorldFowManager = GetWorldFowManager();
	if (not IsValid(WorldFowManager))
	{
		return;
	}

	// Per-source updates leave reveal sources that other systems may add untouched.
	const TArray<FWorldFowRevealSource> WatchPostRevealSources = GetWatchPostRevealSources();
	for (const FWorldFowRevealSource& RevealSource : WatchPostRevealSources)
	{
		WorldFowManager->SetRevealSource(RevealSource);
	}
	WorldFowManager->RefreshWorldFow();
}

void UWorldBuildNodeManager::RecomputeBuildNodeUnlocks()
{
	TMap<FName, TArray<FWorldBuildNodeUnlockEntry>> NodesByCountry;
	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		const AWorldBuildNode* BuildNode = BuildNodeById.Value.Get();
		if (not IsValid(BuildNode))
		{
			continue;
		}

		FWorldBuildNodeUnlockEntry UnlockEntry;
		UnlockEntry.BuildNodeId = BuildNode->GetBuildNodeId();
		UnlockEntry.UnlockOrder = BuildNode->GetUnlockOrder();
		NodesByCountry.FindOrAdd(BuildNode->GetCountryId()).Add(UnlockEntry);
	}

	for (const TPair<FName, TArray<FWorldBuildNodeUnlockEntry>>& CountryNodes : NodesByCountry)
	{
		RecomputeUnlocksForCountry(CountryNodes.Key, CountryNodes.Value);
	}
}

void UWorldBuildNodeManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromCountryRegulator();
	Super::EndPlay(EndPlayReason);
}

void UWorldBuildNodeManager::HandleCountryLiberationChanged(const FName CountryId, const float /*Liberation*/)
{
	TArray<FWorldBuildNodeUnlockEntry> NodesOfCountry;
	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		const AWorldBuildNode* BuildNode = BuildNodeById.Value.Get();
		if (not IsValid(BuildNode) || BuildNode->GetCountryId() != CountryId)
		{
			continue;
		}

		FWorldBuildNodeUnlockEntry UnlockEntry;
		UnlockEntry.BuildNodeId = BuildNode->GetBuildNodeId();
		UnlockEntry.UnlockOrder = BuildNode->GetUnlockOrder();
		NodesOfCountry.Add(UnlockEntry);
	}

	if (NodesOfCountry.Num() == 0)
	{
		return;
	}
	RecomputeUnlocksForCountry(CountryId, NodesOfCountry);
}

void UWorldBuildNodeManager::InitializeBuildNodes_GatherPlacedNodes()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (TActorIterator<AWorldBuildNode> BuildNodeIterator(World); BuildNodeIterator; ++BuildNodeIterator)
	{
		AWorldBuildNode* BuildNode = *BuildNodeIterator;
		if (GetBuildNode(BuildNode->GetBuildNodeId()) == BuildNode)
		{
			continue;
		}
		RegisterBuildNode(BuildNode);
	}
}

void UWorldBuildNodeManager::InitializeBuildNodes_SpawnConfigNodes()
{
	const UWorldCampaignMapContentSettings* MapContentSettings = UWorldCampaignMapContentSettings::Get();
	if (MapContentSettings == nullptr)
	{
		return;
	}

	for (const FWorldBuildNodeDefinition& BuildNodeDefinition : MapContentSettings->BuildNodes)
	{
		if (BuildNodeDefinition.BuildNodeId.IsNone() || M_BuildNodesById.Contains(BuildNodeDefinition.BuildNodeId))
		{
			continue;
		}

		AWorldBuildNode* SpawnedBuildNode = SpawnBuildNodeFromDefinition(BuildNodeDefinition);
		if (not IsValid(SpawnedBuildNode))
		{
			continue;
		}
		RegisterBuildNode(SpawnedBuildNode);
	}
}

void UWorldBuildNodeManager::InitializeBuildNodes_RestoreBuildings()
{
	if (not GetIsValidCampaignProgressManager())
	{
		return;
	}

	for (const TPair<FName, TWeakObjectPtr<AWorldBuildNode>>& BuildNodeById : M_BuildNodesById)
	{
		AWorldBuildNode* BuildNode = BuildNodeById.Value.Get();
		if (not IsValid(BuildNode))
		{
			continue;
		}

		const EWorldCampaignBuilding SavedBuilding = M_CampaignProgressManager->GetBuildingOnNode(BuildNodeById.Key);
		if (SavedBuilding != EWorldCampaignBuilding::None)
		{
			BuildNode->SetBuilding(SavedBuilding);
		}
	}
}

void UWorldBuildNodeManager::InitializeBuildNodes_BindToCountryRegulator()
{
	if (not GetIsValidCountryRegulator())
	{
		return;
	}

	M_CountryRegulator->OnCountryLiberationChanged.AddUniqueDynamic(
		this, &UWorldBuildNodeManager::HandleCountryLiberationChanged);
}

void UWorldBuildNodeManager::UnbindFromCountryRegulator()
{
	if (not M_CountryRegulator.IsValid())
	{
		return;
	}

	M_CountryRegulator->OnCountryLiberationChanged.RemoveDynamic(
		this, &UWorldBuildNodeManager::HandleCountryLiberationChanged);
}

bool UWorldBuildNodeManager::RegisterBuildNode(AWorldBuildNode* BuildNode)
{
	if (not IsValid(BuildNode))
	{
		return false;
	}

	const FName BuildNodeId = BuildNode->GetBuildNodeId();
	if (BuildNodeId.IsNone())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("World build node %s has no Build Node Id and is ignored."), *BuildNode->GetName()));
		return false;
	}

	if (M_BuildNodesById.Contains(BuildNodeId))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("World build node id %s is used twice; %s is ignored."), *BuildNodeId.ToString(),
			*BuildNode->GetName()));
		return false;
	}

	if (BuildNode->GetCountryId().IsNone())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("World build node %s has no Country Id; it will stay locked."), *BuildNodeId.ToString()));
	}

	M_BuildNodesById.Add(BuildNodeId, BuildNode);
	return true;
}

AWorldBuildNode* UWorldBuildNodeManager::SpawnBuildNodeFromDefinition(
	const FWorldBuildNodeDefinition& BuildNodeDefinition)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	UClass* BuildNodeClass = IsValid(M_BuildNodeClass.Get())
		                         ? M_BuildNodeClass.Get()
		                         : AWorldBuildNode::StaticClass();
	const FTransform SpawnTransform(BuildNodeDefinition.WorldLocation);
	// Deferred so id and country are set before BeginPlay draws the node.
	AWorldBuildNode* SpawnedBuildNode = World->SpawnActorDeferred<AWorldBuildNode>(
		BuildNodeClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(SpawnedBuildNode))
	{
		return nullptr;
	}

	SpawnedBuildNode->InitializeFromDefinition(BuildNodeDefinition);
	SpawnedBuildNode->FinishSpawning(SpawnTransform);
	return SpawnedBuildNode;
}

void UWorldBuildNodeManager::RecomputeUnlocksForCountry(const FName CountryId,
                                                        const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry)
{
	if (not GetIsValidCountryRegulator())
	{
		return;
	}

	const float Liberation = M_CountryRegulator->GetCountryLiberation(CountryId);
	const int32 ClearedTargets = M_CountryRegulator->GetClearedTargetCount(CountryId);
	const int32 UnlockedNodeCount = WorldCampaignRules::GetUnlockedBuildNodeCount(
		UWorldCampaignRulesSettings::GetNumbers(), NodesOfCountry.Num(), ClearedTargets, Liberation);
	const TArray<FName> UnlockedNodeIds = WorldBuildNodeRules::GetUnlockedNodeIds(NodesOfCountry, UnlockedNodeCount);

	for (const FWorldBuildNodeUnlockEntry& UnlockEntry : NodesOfCountry)
	{
		AWorldBuildNode* BuildNode = GetBuildNode(UnlockEntry.BuildNodeId);
		if (not IsValid(BuildNode))
		{
			continue;
		}

		const EWorldBuildNodeState PreviousState = BuildNode->GetNodeState();
		const EWorldBuildNodeState NewState = WorldBuildNodeRules::GetNodeStateAfterUnlockPass(
			PreviousState, UnlockedNodeIds.Contains(UnlockEntry.BuildNodeId), BuildNode->GetBuilding());
		BuildNode->SetNodeState(NewState);
		if (PreviousState == EWorldBuildNodeState::Locked && NewState == EWorldBuildNodeState::Unlocked)
		{
			OnBuildNodeUnlocked.Broadcast(UnlockEntry.BuildNodeId, CountryId);
		}
	}
}

void UWorldBuildNodeManager::ApplySupportFromNode(const AWorldBuildNode& BuildNode,
                                                  const FWorldBuildingEffectDefinition& BuildingEffect,
                                                  const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors) const
{
	const FGuid SourceKey = WorldBuildNodeRules::GetBuildNodeSourceKey(BuildNode.GetBuildNodeId());
	const FWorldStrengthReason StrengthReason(
		BuildingEffect.EnemyStrengthReductionReason,
		WorldBuildNodeRules::GetPlayerSupportStrengthPercent(BuildingEffect.EnemyStrengthReductionPercent));
	const FVector SourceLocation = BuildNode.GetActorLocation();

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
	{
		ApplySupportToAnchorTarget(AnchorPoint.Get(), SourceLocation, BuildingEffect.EnemyStrengthReductionRadius,
		                           SourceKey, StrengthReason);
	}
}

void UWorldBuildNodeManager::ApplySupportToAnchorTarget(const AAnchorPoint* AnchorPoint,
                                                        const FVector& SourceLocation, const float XYRadius,
                                                        const FGuid& SourceKey,
                                                        const FWorldStrengthReason& StrengthReason) const
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	const AWorldMapObject* TargetObject = AnchorPoint->GetPromotedWorldObject();
	if (not WorldBuildNodeManagerHelpers::GetIsPlayerSupportTargetObject(TargetObject)
		|| TargetObject->GetIsClearedByPlayer())
	{
		return;
	}

	if (not WorldBuildNodeRules::GetIsWithinSupportRadiusXY(SourceLocation, TargetObject->GetActorLocation(),
	                                                        XYRadius))
	{
		return;
	}

	UWorldStrengthEstimationComponent* StrengthEstimationComponent =
		TargetObject->GetWorldStrengthEstimationComponent();
	if (not IsValid(StrengthEstimationComponent)
		|| WorldBuildNodeManagerHelpers::GetHasStrategicSupportFromSource(*StrengthEstimationComponent, SourceKey))
	{
		return;
	}

	// None: this is the player's support, not an enemy support type the mission side might read as hostile.
	StrengthEstimationComponent->AddStrategicSupportReason(EWorldStrategicSupport::None, SourceKey, StrengthReason);
}

void UWorldBuildNodeManager::ApplyBuildingEffectsAfterBuild(const EWorldCampaignBuilding Building)
{
	const FWorldBuildingEffectDefinition* BuildingEffect = UWorldBuildingSettings::FindBuildingEffect(Building);
	if (BuildingEffect == nullptr)
	{
		return;
	}

	if (BuildingEffect->GetRevealsFow())
	{
		ApplyWatchPostFowReveal();
	}

	if (BuildingEffect->GetReducesEnemyStrength())
	{
		RefreshStrategicSupportWithPlayerBuildings();
	}
}

int32 UWorldBuildNodeManager::GetCurrentTurn() const
{
	if (not GetIsValidWorldPlayerController())
	{
		return 0;
	}

	const UWorldStateAndSaveManager* WorldStateAndSaveManager = M_WorldPlayerController->GetWorldStateAndSaveManager();
	return IsValid(WorldStateAndSaveManager) ? WorldStateAndSaveManager->GetCurrentTurn() : 0;
}

AWorldFowManager* UWorldBuildNodeManager::GetWorldFowManager()
{
	if (M_WorldFowManager.IsValid())
	{
		return M_WorldFowManager.Get();
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	// The controller keeps its fog manager private; there is one per campaign map.
	for (TActorIterator<AWorldFowManager> FowManagerIterator(World); FowManagerIterator; ++FowManagerIterator)
	{
		M_WorldFowManager = *FowManagerIterator;
		return M_WorldFowManager.Get();
	}
	return nullptr;
}

bool UWorldBuildNodeManager::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldPlayerController"),
		TEXT("UWorldBuildNodeManager::GetIsValidWorldPlayerController"),
		this);
	return false;
}

bool UWorldBuildNodeManager::GetIsValidCampaignProgressManager() const
{
	if (M_CampaignProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CampaignProgressManager"),
		TEXT("UWorldBuildNodeManager::GetIsValidCampaignProgressManager"),
		this);
	return false;
}

bool UWorldBuildNodeManager::GetIsValidCountryRegulator() const
{
	if (M_CountryRegulator.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CountryRegulator"),
		TEXT("UWorldBuildNodeManager::GetIsValidCountryRegulator"),
		this);
	return false;
}

bool UWorldBuildNodeManager::GetIsValidWorldGenerator() const
{
	if (M_WorldGenerator.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldGenerator"),
		TEXT("UWorldBuildNodeManager::GetIsValidWorldGenerator"),
		this);
	return false;
}

#undef LOCTEXT_NAMESPACE
