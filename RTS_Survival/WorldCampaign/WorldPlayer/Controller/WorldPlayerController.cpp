// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "WorldPlayerOutliner/PlayerWorldOutliner.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/PinnedPlaces/WorldCampaignPinnedPlaces.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechManager.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKit.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnDirector.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "WorldCameraController/WorldCameraController.h"
#include "WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldStrategicSupportArea.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionInfluenceComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerAudioController/WorldPlayerAudioController.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/ConnectionSpline/WorldConnectionSplineRenderer.h"

namespace WorldPlayerAsyncGenerationUIConstants
{
	constexpr float AnchorPlacementStartedPercentage = 0.f;
	constexpr float AnchorPlacementCompletePercentage = 2.f;
	constexpr float ConnectionGenerationCompletePercentage = 4.f;
	constexpr float AsyncGenerationCompletePercentage = 80.f;
	constexpr float PruningCompletedPercentage= 90.f;
	constexpr float TimedProgressTargetSeconds = 720.f;
	constexpr float TimedProgressIntervalSeconds = 1.f;
}

namespace WorldPlayerOperationMapConstants
{
	constexpr int32 EnemyProceduralMapSeedOffset = 23017;

	// Same tag the generator puts on every anchor it spawns, including the anchors of a restored campaign.
	const FName GeneratedAnchorTag(TEXT("WC_GeneratedAnchor"));

	int32 BuildOperationSeed(const int32 CampaignSeed,
	                         const FGuid& SourceAnchorKey,
	                         const uint8 OperationTypeValue)
	{
		uint32 OperationSeedHash = HashCombine(GetTypeHash(CampaignSeed), GetTypeHash(SourceAnchorKey));
		OperationSeedHash = HashCombine(OperationSeedHash, GetTypeHash(OperationTypeValue));
		return static_cast<int32>(OperationSeedHash & static_cast<uint32>(MAX_int32));
	}

	// A division has no map item type of its own; the operation map is told the enemy item it resembles most.
	EMapEnemyItem GetEnemyItemProxyForDivision(const EWorldFieldDivisions DivisionType)
	{
		switch (DivisionType)
		{
		case EWorldFieldDivisions::SovietSupplyConvoy:
			return EMapEnemyItem::SupplyDepo;
		case EWorldFieldDivisions::SovietInfantryDivision:
			return EMapEnemyItem::Barracks;
		default:
			return EMapEnemyItem::Factory;
		}
	}
}

AWorldPlayerController::AWorldPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	M_PlayerWorldOutliner = CreateDefaultSubobject<UPlayerWorldOutliner>(TEXT("PlayerWorldOutliner"));
	M_WorldStateAndSaveManager = CreateDefaultSubobject<UWorldStateAndSaveManager>(TEXT("WorldStateAndSaveManager"));
	M_WorldCampaignProgressManager = CreateDefaultSubobject<UWorldCampaignProgressManager>(
		TEXT("WorldCampaignProgressManager"));
	M_WorldEnemyTurnDirector = CreateDefaultSubobject<UWorldEnemyTurnDirector>(TEXT("WorldEnemyTurnDirector"));
	M_WorldCampaignTechManager = CreateDefaultSubobject<UWorldCampaignTechManager>(TEXT("WorldCampaignTechManager"));
	M_WorldCommanderMapKit = CreateDefaultSubobject<UWorldCommanderMapKit>(TEXT("WorldCommanderMapKit"));
	M_WorldBuildNodeManager = CreateDefaultSubobject<UWorldBuildNodeManager>(TEXT("WorldBuildNodeManager"));
	M_WorldDivisionManager = CreateDefaultSubobject<UWorldDivisionManager>(TEXT("WorldDivisionManager"));
	M_PlayerResourceManager = CreateDefaultSubobject<UPlayerResourceManager>(TEXT("PlayerResourceManager"));
	M_WorldPlayerAudioController = CreateDefaultSubobject<UWorldPlayerAudioController>(TEXT("WorldPlayerAudioController"));
}

AGeneratorWorldCampaign* AWorldPlayerController::GetWorldGenerator() const
{
	if (not GetIsValidWorldGenerator())
	{
		return nullptr;
	}

	return M_WorldGenerator.Get();
}

UWorldStateAndSaveManager* AWorldPlayerController::GetWorldStateAndSaveManager() const
{
	if (not GetIsValidWorldStateAndSaveManager())
	{
		return nullptr;
	}

	return M_WorldStateAndSaveManager;
}

UPlayerResourceManager* AWorldPlayerController::GetPlayerResourceManager() const
{
	if (not GetIsValidPlayerResourceManager())
	{
		return nullptr;
	}

	return M_PlayerResourceManager;
}

UWorldCampaignProgressManager* AWorldPlayerController::GetWorldCampaignProgressManager() const
{
	if (not GetIsValidWorldCampaignProgressManager())
	{
		return nullptr;
	}

	return M_WorldCampaignProgressManager;
}

AWorldMapObject* AWorldPlayerController::FindWorldMapObjectByAnchorKey(const FGuid& AnchorKey) const
{
	if (not AnchorKey.IsValid())
	{
		return nullptr;
	}

	for (TActorIterator<AWorldMapObject> MapObjectIterator(GetWorld()); MapObjectIterator; ++MapObjectIterator)
	{
		AWorldMapObject* MapObject = *MapObjectIterator;
		const bool bIsOperationTarget = MapObject->IsA(AWorldMissionObject::StaticClass())
			|| MapObject->IsA(AWorldEnemyObject::StaticClass());
		if (bIsOperationTarget && MapObject->GetAnchorKey() == AnchorKey)
		{
			return MapObject;
		}
	}
	return nullptr;
}

float AWorldPlayerController::GetWallProximityForLocation(const FVector& WorldLocation)
{
	const FVector PlayerHQLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(this);
	// Without a wall on the map the enemy HQ marks the far end of the campaign.
	FVector WallLocation = FRTS_WorldStatics::GetEnemyHQWorldLocation(this);
	for (TActorIterator<AWorldEnemyObject> EnemyObjectIterator(GetWorld()); EnemyObjectIterator; ++EnemyObjectIterator)
	{
		if (EnemyObjectIterator->GetEnemyItemType() == EMapEnemyItem::EnemyWall)
		{
			WallLocation = EnemyObjectIterator->GetActorLocation();
			break;
		}
	}

	const float DistanceToWall = FVector::Dist2D(PlayerHQLocation, WallLocation);
	if (DistanceToWall <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp(FVector::Dist2D(PlayerHQLocation, WorldLocation) / DistanceToWall, 0.f, 1.f);
}

FPlayerTurnContext AWorldPlayerController::GetPlayerTurnContext() const
{
	FPlayerTurnContext PlayerTurnContext;
	if (not GetIsValidWorldStateAndSaveManager())
	{
		return PlayerTurnContext;
	}

	PlayerTurnContext.CurrentTurn = M_WorldStateAndSaveManager->GetCurrentTurn();
	return PlayerTurnContext;
}

UWorldDivisionManager* AWorldPlayerController::GetWorldDivisionManager() const
{
	if (not GetIsValidWorldDivisionManager())
	{
		return nullptr;
	}

	return M_WorldDivisionManager;
}

void AWorldPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_WorldCameraController = FindComponentByClass<UWorldCameraController>();
	M_WorldProfileAndUIManager = FindComponentByClass<UWorldProfileAndUIManager>();
}

void AWorldPlayerController::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_BindCommanderMapKit();
	BeginPlay_SetupWorldGenerator();
	BeginPlay_SetupWorldMenu();
	BeginPlay_GameState_Faction_CampaignSettings();
	const bool bShouldLoadSavedCampaign = GetShouldLoadSavedCampaign();
	if (bShouldLoadSavedCampaign)
	{
		BeginPlay_RestoreSessionSettingsFromSave();
	}
	BeginPlay_InitializeWorldGenerator();
	if (not bShouldLoadSavedCampaign)
	{
		BeginPlay_GenerateNewWorld();
		return;
	}
	BeginPlay_LoadSavedWorld();
}

bool AWorldPlayerController::GetShouldLoadSavedCampaign() const
{
#if !UE_BUILD_SHIPPING
	// -RTSWorldCampaignForceLoad: headless checks of the load path start the level directly, without a mission.
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSWorldCampaignForceLoad")))
	{
		return UWorldStateAndSaveManager::GetDoesCampaignSaveExist();
	}
#endif
	// Returning from an operation and "continue campaign" both clear the generate flag on the game instance.
	return not M_CampaignSettings.bNeedsToGenerateCampaign && UWorldStateAndSaveManager::GetDoesCampaignSaveExist();
}

void AWorldPlayerController::BeginPlay_RestoreSessionSettingsFromSave()
{
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance) || not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	FWorldCampaignState SavedWorldCampaignState;
	FPlayerProfileSaveData SavedPlayerProfileSaveData;
	if (not M_WorldStateAndSaveManager->LoadCampaignState(SavedWorldCampaignState, SavedPlayerProfileSaveData))
	{
		return;
	}

	// Operation seeds are derived from the campaign seed, so a continued campaign must keep the saved one.
	M_CampaignSettings.GenerationSeed = SavedWorldCampaignState.WorldGenerationSeed;
	const FWorldCampaignProgressState& SavedProgress = SavedWorldCampaignState.Progress;
	if (not GameInstance->GetHasPlayerCommander() && SavedProgress.CommanderMap.Commander != ERTSCommander::NoCommander)
	{
		GameInstance->SetPlayerCommander(SavedProgress.CommanderMap.Commander);
	}

	if (not GameInstance->GetHasPlayerFaction())
	{
		GameInstance->SetPlayerFaction(SavedPlayerProfileSaveData.PlayerFaction);
		M_PlayerFaction = SavedPlayerProfileSaveData.PlayerFaction;
	}

	if (not M_SelectedDifficulty.bIsInitialized && SavedProgress.Difficulty.bIsInitialized)
	{
		M_SelectedDifficulty = SavedProgress.Difficulty;
		GameInstance->SetSelectedGameDifficulty(SavedProgress.Difficulty);
	}
}

void AWorldPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not GetIsValidPlayerWorldOutliner())
	{
		return;
	}

	FHitResult HitResultCursorProjection;
	const bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResultCursorProjection);
	AActor* HitActor = bHit ? HitResultCursorProjection.GetActor() : nullptr;
	M_PlayerWorldOutliner->OnPlayerTick(HitActor, HitResultCursorProjection.Location);
	Tick_UpdateHoveredCountry();
	Tick_UpdateHoveredBuildNode(HitActor);
}

void AWorldPlayerController::Tick_UpdateHoveredBuildNode(AActor* ActorUnderCursor)
{
	AWorldBuildNode* BuildNodeUnderCursor = Cast<AWorldBuildNode>(ActorUnderCursor);
	if (M_HoveredBuildNode.Get() == BuildNodeUnderCursor)
	{
		return;
	}

	if (M_HoveredBuildNode.IsValid())
	{
		M_HoveredBuildNode->SetIsHovered(false);
	}

	M_HoveredBuildNode = BuildNodeUnderCursor;
	if (IsValid(BuildNodeUnderCursor))
	{
		BuildNodeUnderCursor->SetIsHovered(true);
	}
}

void AWorldPlayerController::Tick_UpdateHoveredCountry()
{
	if (not M_WorldGenerator.IsValid())
	{
		return;
	}

	UWorldCountryOccupationRegulator* CountryRegulator =
		M_WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>();
	if (not IsValid(CountryRegulator))
	{
		return;
	}

	// Country meshes have no collision, so the cursor is projected onto the map plane instead of traced.
	FVector CursorOrigin;
	FVector CursorDirection;
	const bool bHasCursorRay = DeprojectMousePositionToWorld(CursorOrigin, CursorDirection)
		&& not FMath::IsNearlyZero(CursorDirection.Z);
	const float DistanceToMapPlane = bHasCursorRay ? -CursorOrigin.Z / CursorDirection.Z : -1.f;
	const bool bIsCursorOverMap = DistanceToMapPlane > 0.f;
	const FVector MapLocation = CursorOrigin + CursorDirection * FMath::Max(DistanceToMapPlane, 0.f);
	CountryRegulator->SetHoveredCountryAtMapLocation(FVector2D(MapLocation), bIsCursorOverMap);
}

void AWorldPlayerController::PrimaryClick()
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	switch (M_PrimaryClickContext)
	{
	case None:
		PrimaryClick_Regular();
		break;
	case MissionItemActive:
		PrimaryClick_ActiveMissionItem();
		break;
	}
}

void AWorldPlayerController::SecondaryClick()
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}
}

void AWorldPlayerController::PlayTurn(const EWorldTurnType TurnType)
{
	if (TurnType == EWorldTurnType::Enemy)
	{
		EnemyTurn();
	}
	else
	{
		PlayerTurn();
	}
}

void AWorldPlayerController::PlayerTurn()
{
	if (not GetIsValidWorldGenerator() || not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	if (bM_IsStartOfNewPlayerTurn)
	{
		bM_IsStartOfNewPlayerTurn = false;
		PlayerTurn_ApplyStartOfNewTurn();
	}

	M_WorldGenerator->AdjustDifficultyPercentagesForStrategicSupport(M_SelectedDifficulty.DifficultyLevel);
	// The generator pass rebuilds strategic support from scratch, so the player's buildings are added again after it.
	if (GetIsValidWorldBuildNodeManager())
	{
		M_WorldBuildNodeManager->ApplyPlayerBuildingSupport();
	}
	RefreshWorldDivisionInfluence();
	const FPlayerTurnContext PlayerTurnContext = GetPlayerTurnContext();
	BP_OnPlayerTurnStarted(PlayerTurnContext);
}

void AWorldPlayerController::PlayerTurn_ApplyStartOfNewTurn()
{
	if (GetIsValidWorldCommanderMapKit())
	{
		M_WorldCommanderMapKit->TickCooldownAtStartOfPlayerTurn();
	}

	if (GetIsValidWorldBuildNodeManager())
	{
		M_WorldBuildNodeManager->ApplyStartOfPlayerTurnIncome();
	}
}

void AWorldPlayerController::EnemyTurn()
{
	if (not GetIsValidWorldGenerator() || not GetIsValidWorldEnemyTurnDirector())
	{
		return;
	}

	BP_OnEnemyTurnStarted();
	M_WorldGenerator->AdjustDifficultyPercentagesForStrategicSupport(M_SelectedDifficulty.DifficultyLevel);
	if (GetIsValidWorldBuildNodeManager())
	{
		M_WorldBuildNodeManager->ApplyPlayerBuildingSupport();
	}
	// The director spends the enemy budget, moves the enemy pieces and calls back once all movement has ended.
	M_WorldEnemyTurnDirector->PlayEnemyTurn(
		M_SelectedDifficulty.DifficultyLevel,
		FSimpleDelegate::CreateUObject(this, &AWorldPlayerController::OnEndEnemyTurn));
}

void AWorldPlayerController::OnEndEnemyTurn()
{
	if (not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	const int32 CurrentTurn = M_WorldStateAndSaveManager->AdvanceCurrentTurn();
	bM_IsStartOfNewPlayerTurn = true;
	UpdateTurnCounter(CurrentTurn);
	M_WorldStateAndSaveManager->SaveCampaignState();
	PlayTurn(EWorldTurnType::Player);
	LogWorldCampaignSummary(TEXT("PlayerTurnStarted"));
#if !UE_BUILD_SHIPPING
	DebugCommitOperationOnPinFromCommandLine();
#endif
}

void AWorldPlayerController::UpdateTurnCounter(const int32 CurrentTurn) const
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	WorldMenu->UpdateTurnCounter(CurrentTurn);
}

void AWorldPlayerController::MovePlayerDivisions()
{
	if (not GetIsValidWorldDivisionManager())
	{
		return;
	}

	M_WorldDivisionManager->MovePlayerDivisions(M_SelectedDifficulty.DifficultyLevel);
}

void AWorldPlayerController::MoveEnemyDivisions()
{
	if (not GetIsValidWorldDivisionManager())
	{
		return;
	}

	M_WorldDivisionManager->MoveEnemyDivisions(M_SelectedDifficulty.DifficultyLevel);
}

bool AWorldPlayerController::PrimaryClick_Regular()
{
	FHitResult HitUnderCursor;
	if (not GetHitResultUnderCursor(ECC_Visibility, false, HitUnderCursor))
	{
		CollapseMissionMapItemDesc();
		return true;
	}
	AActor* ClickedActor = HitUnderCursor.GetActor();
	if (not IsValid(ClickedActor))
	{
		CollapseMissionMapItemDesc();
		return true;
	}
	if (not GetCanPrimaryClickActor(ClickedActor))
	{
		return false;
	}
	if (ClickedActor->IsA(AWorldEnemyObject::StaticClass()))
	{
		OnClicked_EnemyMapObj(Cast<AWorldEnemyObject>(ClickedActor));	
		return true;
	}
	if (ClickedActor->IsA(AWorldMissionObject::StaticClass()))
	{
		OnClicked_MissionMapObj(Cast<AWorldMissionObject>(ClickedActor));	
		return true;
	}
	if (ClickedActor->IsA(AWorldPlayerObject::StaticClass()))
	{
		OnClicked_PlayerMapObj(Cast<AWorldPlayerObject>(ClickedActor));	
		return true;
	}
	if (ClickedActor->IsA(AWorldNeutralObject::StaticClass()))
	{
		OnClicked_NeutralMapObj(Cast<AWorldNeutralObject>(ClickedActor));	
		return true;
	}
	if (ClickedActor->IsA(AWorldDivisionBase::StaticClass()))
	{
		OnClicked_EnemyDivision(Cast<AWorldDivisionBase>(ClickedActor));
		return true;
	}
	if (ClickedActor->IsA(AWorldBuildNode::StaticClass()))
	{
		CollapseMissionMapItemDesc();
		BP_OnBuildNodeClicked(Cast<AWorldBuildNode>(ClickedActor));
		return true;
	}
	CollapseMissionMapItemDesc();
	return true;
}

void AWorldPlayerController::OnClicked_EnemyMapObj(AWorldEnemyObject* EnemyMapObj)
{
	if (not IsValid(EnemyMapObj) || not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	WorldMenu->ShowMissionMapItemDesc(
		EnemyMapObj,
		EnemyMapObj->GetMapItemUIData(),
		EnemyMapObj->GetPrimaryReward(),
		EnemyMapObj->GetSecondaryReward()
	);
	ShowClickedDifficultyInfluenceRadiiForActor(EnemyMapObj);
	M_PrimaryClickContext = EWorldPrimaryClickContext::MissionItemActive;
}

void AWorldPlayerController::OnClicked_MissionMapObj(AWorldMissionObject* MissionMapObj)
{
	if (not IsValid(MissionMapObj) || not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	WorldMenu->ShowMissionMapItemDesc(
		MissionMapObj,
		MissionMapObj->GetMapItemUIData(),
		MissionMapObj->GetPrimaryReward(),
		MissionMapObj->GetSecondaryReward()
	);
	ShowClickedDifficultyInfluenceRadiiForActor(MissionMapObj);
	M_PrimaryClickContext = EWorldPrimaryClickContext::MissionItemActive;
}

void AWorldPlayerController::OnClicked_EnemyDivision(AWorldDivisionBase* EnemyDivision)
{
	CollapseMissionMapItemDesc();
	if (not IsValid(EnemyDivision) || EnemyDivision->GetOwningPlayer() != WorldDivisionOwners::EnemyOwner)
	{
		return;
	}

	ShowClickedDifficultyInfluenceRadiiForActor(EnemyDivision);
	BP_OnEnemyDivisionClicked(EnemyDivision);
}

void AWorldPlayerController::OnClicked_PlayerMapObj(AWorldPlayerObject* PlayerMapObj)
{
	CollapseMissionMapItemDesc();
}

void AWorldPlayerController::OnClicked_NeutralMapObj(AWorldNeutralObject* NeutralMapObj)
{
	CollapseMissionMapItemDesc();
}

void AWorldPlayerController::PrimaryClick_ActiveMissionItem()
{
	const EWorldPrimaryClickContext PreviousPrimaryClickContext = M_PrimaryClickContext;
	M_PrimaryClickContext = EWorldPrimaryClickContext::None;
	if (PrimaryClick_Regular())
	{
		return;
	}

	M_PrimaryClickContext = PreviousPrimaryClickContext;
}

void AWorldPlayerController::CollapseMissionMapItemDesc()
{
	M_PrimaryClickContext = EWorldPrimaryClickContext::None;
	HideClickedDifficultyInfluenceRadii();
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	WorldMenu->CollapseMissionMapItemDesc();
}

void AWorldPlayerController::ShowClickedDifficultyInfluenceRadiiForActor(AActor* Actor)
{
	if (not IsValid(Actor))
	{
		HideClickedDifficultyInfluenceRadii();
		return;
	}

	AActor* ClickedDifficultyInfluenceRadiusActor = M_ClickedDifficultyInfluenceRadiusActor.Get();
	if (IsValid(ClickedDifficultyInfluenceRadiusActor) && ClickedDifficultyInfluenceRadiusActor != Actor)
	{
		UWorldStrategicSupportArea::HideSelectedRadiiOnActor(ClickedDifficultyInfluenceRadiusActor);
		UWorldDivisionInfluenceComponent::HideSelectedRadiiOnActor(ClickedDifficultyInfluenceRadiusActor);
	}

	M_ClickedDifficultyInfluenceRadiusActor = Actor;
	UWorldStrategicSupportArea::ShowSelectedRadiiOnActor(Actor);
	UWorldDivisionInfluenceComponent::ShowSelectedRadiiOnActor(Actor);
}

void AWorldPlayerController::HideClickedDifficultyInfluenceRadii()
{
	AActor* ClickedDifficultyInfluenceRadiusActor = M_ClickedDifficultyInfluenceRadiusActor.Get();
	if (IsValid(ClickedDifficultyInfluenceRadiusActor))
	{
		UWorldStrategicSupportArea::HideSelectedRadiiOnActor(ClickedDifficultyInfluenceRadiusActor);
		UWorldDivisionInfluenceComponent::HideSelectedRadiiOnActor(ClickedDifficultyInfluenceRadiusActor);
	}

	M_ClickedDifficultyInfluenceRadiusActor = nullptr;
}

void AWorldPlayerController::SetIsWorldCameraMovementDisabled(const bool bIsDisabled)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->SetIsCameraMovementDisabled(bIsDisabled);
}

bool AWorldPlayerController::GetIsWorldCameraMovementDisabled() const
{
	if (not GetIsValidWorldCameraController())
	{
		return false;
	}

	return M_WorldCameraController->GetIsCameraMovementDisabled();
}

void AWorldPlayerController::WorldCamera_ZoomIn()
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomIn();
}

void AWorldPlayerController::WorldCamera_ZoomOut()
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomOut();
}

void AWorldPlayerController::WorldCamera_ForwardMovement(const float AxisValue)
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ForwardMovement(AxisValue);
}

void AWorldPlayerController::WorldCamera_RightMovement(const float AxisValue)
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->RightMovement(AxisValue);
}

bool AWorldPlayerController::IssueWorldDivisionMoveOrder(AWorldDivisionBase* WorldDivision,
                                                         const FVector& TargetLocation)
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return false;
	}

	if (not GetIsValidWorldDivisionManager())
	{
		return false;
	}

	return M_WorldDivisionManager->IssueMoveOrderToDivision(WorldDivision, TargetLocation);
}

void AWorldPlayerController::WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->MoveCameraTo(MoveRequest);
}

void AWorldPlayerController::WorldCamera_StartCameraOvertake(
	const FCameraOvertakeSettings& CameraOvertakeSettings,
	FWorldCameraOvertakeFinishedDelegate OnCameraOvertakeFinished)
{
	if (not GetIsValidWorldCameraController())
	{
		OnCameraOvertakeFinished.ExecuteIfBound(FVector::ZeroVector);
		return;
	}

	if (not GetIsValidWorldProfileAndUIManager())
	{
		OnCameraOvertakeFinished.ExecuteIfBound(M_WorldCameraController->GetCurrentCameraLocation());
		return;
	}

	if (CameraOvertakeSettings.Points.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Camera overtake needs at least one point.")
			TEXT("\n See function: AWorldPlayerController::WorldCamera_StartCameraOvertake()"));
		OnCameraOvertakeFinished.ExecuteIfBound(M_WorldCameraController->GetCurrentCameraLocation());
		return;
	}

	BeginCameraOvertakeControl(CameraOvertakeSettings, OnCameraOvertakeFinished);
	PlayCameraOvertakeSound(CameraOvertakeSettings.SoundStart);
	FWorldCameraOvertakePointReachedNativeDelegate PointReachedDelegate;
	PointReachedDelegate.BindUObject(this, &AWorldPlayerController::OnCameraOvertakePointReached);
	FWorldCameraOvertakeFinishedNativeDelegate FinishedDelegate;
	FinishedDelegate.BindUObject(this, &AWorldPlayerController::OnCameraOvertakeFinished);
	if (M_WorldCameraController->StartCameraOvertake(CameraOvertakeSettings, PointReachedDelegate, FinishedDelegate))
	{
		return;
	}

	this->OnCameraOvertakeFinished(M_WorldCameraController->GetCurrentCameraLocation());
}

void AWorldPlayerController::LaunchOperationForWorldObject(AWorldMapObject* OperationWorldObject)
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not IsValid(OperationWorldObject))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot launch world operation because the initiating world object is invalid."));
		return;
	}

	// A won mission or destroyed enemy item stays cleared; it cannot be attacked a second time.
	if (OperationWorldObject->GetIsClearedByPlayer())
	{
		return;
	}

	if (AWorldMissionObject* MissionMapObject = Cast<AWorldMissionObject>(OperationWorldObject))
	{
		LaunchMissionMap(MissionMapObject);
		return;
	}

	if (AWorldEnemyObject* EnemyMapObject = Cast<AWorldEnemyObject>(OperationWorldObject))
	{
		LaunchEnemyMap(EnemyMapObject);
		return;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("Cannot launch world operation because the initiating object is not a mission or enemy object."));
}

void AWorldPlayerController::LaunchOperationAgainstDivision(AWorldDivisionBase* EnemyDivision)
{
	if (bM_IsWorldPlayerInputDisabled)
	{
		return;
	}

	if (not IsValid(EnemyDivision) || EnemyDivision->GetOwningPlayer() != WorldDivisionOwners::EnemyOwner)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot launch a division operation because the target is not a valid enemy division."));
		return;
	}

	TSoftObjectPtr<UWorld> EnemyMap;
	if (not TryGetEnemyMap(EnemyMap))
	{
		return;
	}

	if (not PopulateWorldMissionContextForDivision(EnemyDivision))
	{
		return;
	}

	const int32 PreviousMapIndex = M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex();
	M_WorldStateAndSaveManager->AdvanceEnemyObjectProceduralMapIndex(M_ShuffledEnemyObjectProceduralMaps.Num());
	if (not BeginCampaignOperation(EWorldOperationTarget::EnemyDivision, FGuid(), EMapMission::None,
	                               EnemyDivision->GetDivisionKey()))
	{
		M_WorldStateAndSaveManager->SetEnemyObjectProceduralMapIndex(PreviousMapIndex);
		return;
	}

	OpenOperationMap(EnemyMap);
}

bool AWorldPlayerController::PopulateWorldMissionContextForDivision(const AWorldDivisionBase* EnemyDivision) const
{
	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not IsValid(EnemyDivision) || not IsValid(RTSGameInstance) || not GetIsValidWorldDivisionManager())
	{
		return false;
	}

	FWorldMissionContext WorldMissionContext;
	WorldMissionContext.OperationType = EMapItemType::EnemyItem;
	WorldMissionContext.EnemyObjectType = WorldPlayerOperationMapConstants::GetEnemyItemProxyForDivision(
		EnemyDivision->GetDivisionType());
	WorldMissionContext.SourceDivisionKey = EnemyDivision->GetDivisionKey();
	WorldMissionContext.OperationSeed = WorldPlayerOperationMapConstants::BuildOperationSeed(
		M_CampaignSettings.GenerationSeed,
		WorldMissionContext.SourceDivisionKey,
		static_cast<uint8>(EnemyDivision->GetDivisionType()));
	WorldMissionContext.StrengthContributions =
		M_WorldDivisionManager->BuildOperationStrengthContributionsForDivision(EnemyDivision->GetDivisionKey());
	WorldMissionContext.bIsInitialized = true;
	if (not WorldMissionContext.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("The populated division operation context is invalid."));
		return false;
	}

	FillCampaignFieldsOnMissionContext(WorldMissionContext);
	RTSGameInstance->SetWorldMissionContext(WorldMissionContext);
	return true;
}

void AWorldPlayerController::BeginPlay_BindCommanderMapKit()
{
	if (not GetIsValidWorldCommanderMapKit())
	{
		return;
	}

	M_WorldCommanderMapKit->OnDamageDivisionRequested.AddUObject(
		this, &AWorldPlayerController::OnCommanderDamageDivisionRequested);
	M_WorldCommanderMapKit->OnRevealRegionRequested.AddUObject(
		this, &AWorldPlayerController::OnCommanderRevealRegionRequested);
}

void AWorldPlayerController::OnCommanderRevealRegionRequested(const FVector& Location, const float Radius,
                                                              const int32 Turns)
{
	AWorldFowManager* WorldFowManager = M_WorldFowManager.Get();
	if (not IsValid(WorldFowManager))
	{
		return;
	}

	// Fog only ever opens, so a timed reveal stays open as well; the turn count is not used yet.
	FWorldFowRevealSource RevealSource;
	RevealSource.SourceId = FName(*FString::Printf(TEXT("CommanderReveal_%d_%d"),
	                                               FMath::RoundToInt(Location.X), FMath::RoundToInt(Location.Y)));
	RevealSource.WorldLocation = Location;
	RevealSource.RevealRadius = Radius;
	WorldFowManager->SetRevealSource(RevealSource);
	WorldFowManager->RefreshWorldFow();
}

void AWorldPlayerController::OnCommanderDamageDivisionRequested(const FGuid& DivisionKey, const int32 Damage)
{
	if (not GetIsValidWorldDivisionManager())
	{
		return;
	}

	AWorldDivisionBase* EnemyDivision = M_WorldDivisionManager->FindDivisionByKey(DivisionKey);
	if (not IsValid(EnemyDivision))
	{
		return;
	}

	EnemyDivision->ApplyWorldDivisionDamage(Damage);
	if (EnemyDivision->GetRemainingEntryCount() == 0)
	{
		M_WorldDivisionManager->RemoveDivisionByKey(DivisionKey);
	}
}

void AWorldPlayerController::InitializeOperationMapsForCampaignSeed(const int32 WorldGenerationSeed)
{
	M_ShuffledEnemyObjectProceduralMaps = M_WorldOperationMapSettings.EnemyObjectProceduralMaps;
	FRandomStream RandomStream(WorldGenerationSeed + WorldPlayerOperationMapConstants::EnemyProceduralMapSeedOffset);
	CampaignGenerationHelper::DeterministicShuffle(M_ShuffledEnemyObjectProceduralMaps, RandomStream);

	if (not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	if (M_ShuffledEnemyObjectProceduralMaps.Num() == 0)
	{
		M_WorldStateAndSaveManager->ResetEnemyObjectProceduralMapIndex();
		return;
	}

	if (M_ShuffledEnemyObjectProceduralMaps.IsValidIndex(
		M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex()))
	{
		return;
	}

	M_WorldStateAndSaveManager->ResetEnemyObjectProceduralMapIndex();
}

void AWorldPlayerController::LaunchMissionMap(AWorldMissionObject* MissionMapObject)
{
	if (not IsValid(MissionMapObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot launch mission operation because the mission object is invalid."));
		return;
	}

	TSoftObjectPtr<UWorld> MissionMap;
	if (not TryGetMissionMap(MissionMapObject->GetMissionType(), MissionMap))
	{
		return;
	}

	if (not PopulateWorldMissionContextForMissionMap(MissionMapObject))
	{
		return;
	}

	if (not BeginCampaignOperation(EWorldOperationTarget::Mission, MissionMapObject->GetAnchorKey(),
	                               MissionMapObject->GetMissionType()))
	{
		return;
	}

	BP_OnMissionMapLaunch(MissionMapObject, MissionMap);
	OpenOperationMap(MissionMap);
}

void AWorldPlayerController::LaunchEnemyMap(AWorldEnemyObject* EnemyMapObject)
{
	if (not IsValid(EnemyMapObject))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot launch enemy operation because the enemy object is invalid."));
		return;
	}

	TSoftObjectPtr<UWorld> EnemyMap;
	if (not TryGetEnemyMap(EnemyMap))
	{
		return;
	}

	if (not PopulateWorldMissionContextForEnemyMap(EnemyMapObject))
	{
		return;
	}

	const int32 PreviousMapIndex = M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex();
	M_WorldStateAndSaveManager->AdvanceEnemyObjectProceduralMapIndex(M_ShuffledEnemyObjectProceduralMaps.Num());
	if (not BeginCampaignOperation(EWorldOperationTarget::EnemyItem, EnemyMapObject->GetAnchorKey(),
	                               EMapMission::None))
	{
		M_WorldStateAndSaveManager->SetEnemyObjectProceduralMapIndex(PreviousMapIndex);
		return;
	}

	BP_OnEnemyMapLaunch(EnemyMapObject, EnemyMap);
	OpenOperationMap(EnemyMap);
}

bool AWorldPlayerController::PopulateWorldMissionContextForMissionMap(
	const AWorldMissionObject* MissionMapObject) const
{
	if (not IsValid(MissionMapObject))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the mission object is invalid."));
		return false;
	}

	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the RTS game instance is invalid."));
		return false;
	}

	const UWorldStrengthEstimationComponent* StrengthEstimationComponent =
		MissionMapObject->GetWorldStrengthEstimationComponent();
	if (not IsValid(StrengthEstimationComponent))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the mission strength component is invalid."));
		return false;
	}

	FWorldMissionContext WorldMissionContext;
	WorldMissionContext.OperationType = EMapItemType::Mission;
	WorldMissionContext.MissionType = MissionMapObject->GetMissionType();
	WorldMissionContext.SourceAnchorKey = MissionMapObject->GetAnchorKey();
	WorldMissionContext.OperationSeed = WorldPlayerOperationMapConstants::BuildOperationSeed(
		M_CampaignSettings.GenerationSeed,
		WorldMissionContext.SourceAnchorKey,
		static_cast<uint8>(WorldMissionContext.MissionType));
	WorldMissionContext.StrengthContributions = StrengthEstimationComponent->GetStrengthContributions();
	WorldMissionContext.bIsInitialized = true;

	if (not WorldMissionContext.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("The populated mission operation context is invalid."));
		return false;
	}

	FillCampaignFieldsOnMissionContext(WorldMissionContext);
	RTSGameInstance->SetWorldMissionContext(WorldMissionContext);
	return true;
}

bool AWorldPlayerController::PopulateWorldMissionContextForEnemyMap(
	const AWorldEnemyObject* EnemyMapObject) const
{
	if (not IsValid(EnemyMapObject))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the enemy object is invalid."));
		return false;
	}

	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the RTS game instance is invalid."));
		return false;
	}

	const UWorldStrengthEstimationComponent* StrengthEstimationComponent =
		EnemyMapObject->GetWorldStrengthEstimationComponent();
	if (not IsValid(StrengthEstimationComponent))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Cannot populate world mission context because the enemy strength component is invalid."));
		return false;
	}

	FWorldMissionContext WorldMissionContext;
	WorldMissionContext.OperationType = EMapItemType::EnemyItem;
	WorldMissionContext.EnemyObjectType = EnemyMapObject->GetEnemyItemType();
	WorldMissionContext.SourceAnchorKey = EnemyMapObject->GetAnchorKey();
	WorldMissionContext.OperationSeed = WorldPlayerOperationMapConstants::BuildOperationSeed(
		M_CampaignSettings.GenerationSeed,
		WorldMissionContext.SourceAnchorKey,
		static_cast<uint8>(WorldMissionContext.EnemyObjectType));
	WorldMissionContext.StrengthContributions = StrengthEstimationComponent->GetStrengthContributions();
	WorldMissionContext.bIsInitialized = true;

	if (not WorldMissionContext.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("The populated enemy operation context is invalid."));
		return false;
	}

	FillCampaignFieldsOnMissionContext(WorldMissionContext);
	RTSGameInstance->SetWorldMissionContext(WorldMissionContext);
	return true;
}

bool AWorldPlayerController::TryGetMissionMap(const EMapMission MissionType,
                                              TSoftObjectPtr<UWorld>& OutMissionMap) const
{
	OutMissionMap.Reset();
	const TSoftObjectPtr<UWorld>* MissionMap = M_WorldOperationMapSettings.MissionMaps.Find(MissionType);
	if (MissionMap != nullptr && not MissionMap->IsNull())
	{
		OutMissionMap = *MissionMap;
		return true;
	}

	// Missions the controller Blueprint does not know yet can be mapped in the map content settings.
	const FSoftObjectPath* ConfiguredMissionMap = UWorldCampaignMapContentSettings::Get()->MissionMaps.Find(MissionType);
	if (ConfiguredMissionMap != nullptr && not ConfiguredMissionMap->IsNull())
	{
		OutMissionMap = TSoftObjectPtr<UWorld>(*ConfiguredMissionMap);
		return true;
	}

	const UEnum* MissionEnum = StaticEnum<EMapMission>();
	const FString MissionName = MissionEnum != nullptr
		                            ? MissionEnum->GetNameStringByValue(static_cast<int64>(MissionType))
		                            : FString::FromInt(static_cast<int32>(MissionType));
	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("No operation map is configured for mission type: %s."), *MissionName));
	return false;
}

bool AWorldPlayerController::TryGetEnemyMap(TSoftObjectPtr<UWorld>& OutEnemyMap)
{
	OutEnemyMap.Reset();
	if (M_ShuffledEnemyObjectProceduralMaps.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("No enemy object procedural maps are configured."));
		return false;
	}

	if (not GetIsValidWorldStateAndSaveManager())
	{
		return false;
	}

	int32 EnemyMapIndex = M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex();
	if (not M_ShuffledEnemyObjectProceduralMaps.IsValidIndex(EnemyMapIndex))
	{
		M_WorldStateAndSaveManager->ResetEnemyObjectProceduralMapIndex();
		EnemyMapIndex = M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex();
	}

	OutEnemyMap = M_ShuffledEnemyObjectProceduralMaps[EnemyMapIndex];
	if (not OutEnemyMap.IsNull())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("Enemy object procedural map at shuffled index %d is not configured."), EnemyMapIndex));
	return false;
}

void AWorldPlayerController::OpenOperationMap(const TSoftObjectPtr<UWorld>& OperationMap) const
{
	if (OperationMap.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot open operation map because no map was configured."));
		return;
	}

	if (URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance()))
	{
		RTSGameInstance->SetWorldMissionOperationLevel(OperationMap);
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, OperationMap, true, FString());
}

void AWorldPlayerController::LogWorldCampaignSummary(const TCHAR* Moment) const
{
	if (not GetIsValidWorldCampaignProgressManager() || not GetIsValidWorldStateAndSaveManager()
		|| not GetIsValidWorldGenerator())
	{
		return;
	}

	const FWorldCampaignProgressState& ProgressState = M_WorldCampaignProgressManager->GetProgressState();
	const FWorldCampaignState& WorldCampaignState = M_WorldStateAndSaveManager->GetCachedWorldCampaignState();
	const UWorldCountryOccupationRegulator* CountryRegulator =
		M_WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>();
	int32 AnchorsInCountry = 0;
	int32 CountryCount = 0;
	if (IsValid(CountryRegulator))
	{
		CountryCount = CountryRegulator->GetCountryIds().Num();
		for (const FWorldCampaignAnchorSaveData& AnchorSaveData : WorldCampaignState.Anchors)
		{
			AnchorsInCountry += CountryRegulator->GetCountryIdForAnchorKey(AnchorSaveData.AnchorKey).IsNone() ? 0 : 1;
		}
	}

	int32 BuildNodeCount = 0;
	int32 OpenBuildNodeCount = 0;
	if (IsValid(M_WorldBuildNodeManager))
	{
		const TArray<AWorldBuildNode*> BuildNodes = M_WorldBuildNodeManager->GetBuildNodes();
		BuildNodeCount = BuildNodes.Num();
		for (const AWorldBuildNode* BuildNode : BuildNodes)
		{
			OpenBuildNodeCount += IsValid(BuildNode) && BuildNode->GetNodeState() != EWorldBuildNodeState::Locked ? 1 : 0;
		}
	}

	int32 LevelAnchorCount = 0;
	for (TActorIterator<AAnchorPoint> AnchorIterator(GetWorld()); AnchorIterator; ++AnchorIterator)
	{
		LevelAnchorCount++;
	}

	UE_LOG(LogTemp, Display,
	       TEXT("RTS_WORLDCAMPAIGN_SUMMARY moment=%s turn=%d missionsCompleted=%d missionsLost=%d cleared=%d pending=%d ")
	       TEXT("alert=%.2f freezeStage=%d freezePoints=%d radixite=%d blueprints=%d pins=%d countries=%d ")
	       TEXT("anchorsSaved=%d anchorsInLevel=%d anchorsInCountry=%d mapItems=%d divisions=%d buildNodes=%d ")
	       TEXT("openBuildNodes=%d deckCards=%d"),
	       Moment, WorldCampaignState.CurrentTurn, ProgressState.MissionsCompleted, ProgressState.MissionsLost,
	       ProgressState.ClearedAnchorKeys.Num(), ProgressState.PendingOperation.bIsPending ? 1 : 0,
	       ProgressState.EnemyTurn.Alert, ProgressState.EnemyTurn.FreezeStage,
	       ProgressState.EnemyTurn.FreezeProgressPoints, ProgressState.Resources.Radixite,
	       ProgressState.Resources.Blueprints, M_WorldGenerator->GetResolvedPinnedPlaces().GetPins().Num(),
	       CountryCount, WorldCampaignState.Anchors.Num(), LevelAnchorCount, AnchorsInCountry,
	       WorldCampaignState.MapItems.Num(), WorldCampaignState.WorldDivisions.Num(), BuildNodeCount,
	       OpenBuildNodeCount,
	       M_WorldStateAndSaveManager->GetCachedPlayerProfileSaveData().PlayerData.CardData.SelectedCards.Num());
}

#if !UE_BUILD_SHIPPING
void AWorldPlayerController::DebugCommitOperationOnPinFromCommandLine()
{
	FString PinIdText;
	if (not FParse::Value(FCommandLine::Get(), TEXT("-RTSWorldCampaignTestLaunchPin="), PinIdText)
		|| not GetIsValidWorldGenerator() || not GetIsValidWorldCampaignProgressManager())
	{
		return;
	}

	if (M_WorldCampaignProgressManager->GetProgressState().PendingOperation.bIsPending)
	{
		return;
	}

	FGuid PinAnchorKey;
	if (not M_WorldGenerator->TryGetAnchorKeyForPinId(FName(*PinIdText), PinAnchorKey))
	{
		UE_LOG(LogTemp, Error, TEXT("RTS_WORLDCAMPAIGN_TEST pin %s was not resolved."), *PinIdText);
		return;
	}

	const AWorldMissionObject* MissionMapObject = Cast<AWorldMissionObject>(
		FindWorldMapObjectByAnchorKey(PinAnchorKey));
	if (not IsValid(MissionMapObject) || not PopulateWorldMissionContextForMissionMap(MissionMapObject))
	{
		UE_LOG(LogTemp, Error, TEXT("RTS_WORLDCAMPAIGN_TEST pin %s holds no launchable mission."), *PinIdText);
		return;
	}

	const bool bDidCommit = BeginCampaignOperation(EWorldOperationTarget::Mission, PinAnchorKey,
	                                               MissionMapObject->GetMissionType());
	UE_LOG(LogTemp, Display, TEXT("RTS_WORLDCAMPAIGN_TEST committed operation on pin %s: %s"), *PinIdText,
	       bDidCommit ? TEXT("yes") : TEXT("no"));
	LogWorldCampaignSummary(TEXT("OperationCommitted"));
}
#endif

void AWorldPlayerController::FillCampaignFieldsOnMissionContext(FWorldMissionContext& WorldMissionContext) const
{
	if (not GetIsValidWorldCampaignProgressManager() || not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	const FWorldCampaignProgressState& ProgressState = M_WorldCampaignProgressManager->GetProgressState();
	WorldMissionContext.bLaunchedFromWorldMap = true;
	WorldMissionContext.CampaignGlobalAbilityTierCap = M_WorldCampaignProgressManager->GetGlobalAbilityTierCap();
	WorldMissionContext.CampaignResearchedTechnologies = ProgressState.ResearchedTechnologies;
	WorldMissionContext.CampaignCardData =
		M_WorldStateAndSaveManager->GetCachedPlayerProfileSaveData().PlayerData.CardData;
	WorldMissionContext.FreezeStage = ProgressState.EnemyTurn.FreezeStage;
}

bool AWorldPlayerController::BeginCampaignOperation(const EWorldOperationTarget Target, const FGuid& AnchorKey,
                                                    const EMapMission MissionType, const FGuid& DivisionKey)
{
	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not IsValid(RTSGameInstance) || not GetIsValidWorldCampaignProgressManager()
		|| not GetIsValidWorldStateAndSaveManager())
	{
		return false;
	}

	FWorldPendingOperation PendingOperation;
	PendingOperation.Target = Target;
	PendingOperation.AnchorKey = AnchorKey;
	PendingOperation.DivisionKey = DivisionKey;
	PendingOperation.MissionType = MissionType;
	PendingOperation.LaunchedOnTurn = M_WorldStateAndSaveManager->GetCurrentTurn();
	M_WorldCampaignProgressManager->BeginOperation(PendingOperation);
	if (not M_WorldStateAndSaveManager->SaveCampaignState())
	{
		M_WorldCampaignProgressManager->DiscardPendingOperation();
		RTSGameInstance->ResetWorldMissionContext();
		return false;
	}

	// The world map must restore this campaign when the operation ends, never generate over it.
	RTSGameInstance->RequestLoadSavedCampaign();
	return true;
}

bool AWorldPlayerController::GetIsValidWorldCameraController() const
{
	if (M_WorldCameraController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCameraController"),
		TEXT("GetIsValidWorldCameraController"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldPlayerAudioController() const
{
	if (IsValid(M_WorldPlayerAudioController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldPlayerAudioController"),
		TEXT("GetIsValidWorldPlayerAudioController"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidPlayerWorldOutliner() const
{
	if (IsValid(M_PlayerWorldOutliner))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_PlayerWorldOutliner"),
		TEXT("GetIsValidPlayerWorldOutliner"),
		this
	);
	return false;
}

void AWorldPlayerController::BeginPlay_SetupWorldGenerator()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	AGeneratorWorldCampaign* WorldGenerator = nullptr;
	// Find the first instance of AGeneratorWorldCampaign in the world
	for (TActorIterator<AGeneratorWorldCampaign> It(World); It; ++It)
	{
		WorldGenerator = *It;
		if (IsValid(WorldGenerator))
		{
			break;
		}
	}
	if (not IsValid(WorldGenerator))
	{
		RTSFunctionLibrary::ReportError("did not find a valid world generator."
			"for BeginPlay_SetupWorldGenerator in WorldPlayerController");
		return;
	}
	M_WorldGenerator = WorldGenerator;
}

void AWorldPlayerController::BeginPlay_SetupWorldMenu()
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}
	M_WorldProfileAndUIManager->SetupWorldMenu(this);
	UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(
		this,
		nullptr,
		EMouseLockMode::LockAlways,
		false,
		false
	);
	bShowMouseCursor = true;
}

void AWorldPlayerController::BeginPlay_GameState_Faction_CampaignSettings()
{
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not GameInstance)
	{
		RTSFunctionLibrary::ReportError(
			"GameInstance not valid in WorldPlayerController::BeginPlay_InitWorldGeneratorWithGameInstance");
		return;
	}

	const FCampaignGenerationSettings CampaignSettings = GameInstance->GetCampaignGenerationSettings();
	const FRTSGameDifficulty SelectedDifficulty = GameInstance->GetSelectedGameDifficulty();
	const ERTSFaction PlayerFaction = GameInstance->GetPlayerFaction();
	M_CampaignSettings = CampaignSettings;
	M_SelectedDifficulty = SelectedDifficulty;
	M_PlayerFaction = PlayerFaction;
}

void AWorldPlayerController::BeginPlay_InitializeWorldGenerator()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	M_WorldGenerator->InitializeWorldGenerator(
		this,
		M_CampaignSettings,
		M_SelectedDifficulty
	);
}

void AWorldPlayerController::BeginPlay_GenerateNewWorld()
{
	if (not GetIsValidWorldProfileAndUIManager() || not GetIsValidWorldGenerator() || not
		GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	ShowAsyncWorldGenerationWidget();
	M_WorldGenerator->OnGenerationFinished().RemoveAll(this);
	M_WorldGenerator->OnGenerationFinished().AddUObject(this, &AWorldPlayerController::OnGeneratedCampaignAsyncWorkFinished);
	M_WorldGenerator->StartWorldGeneration();
	if (M_WorldGenerator->GetIsGenerationFinished() && not bM_HasCompletedInitialWorldSetup)
	{
		OnGeneratedCampaignAsyncWorkFinished();
	}
}

void AWorldPlayerController::BeginPlay_LoadSavedWorld()
{
	if (not GetIsValidWorldProfileAndUIManager() || not GetIsValidWorldGenerator() || not
		GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	FWorldCampaignState LoadedWorldCampaignState;
	FPlayerProfileSaveData LoadedPlayerProfileSaveData;
	if (not M_WorldStateAndSaveManager->LoadCampaignState(LoadedWorldCampaignState, LoadedPlayerProfileSaveData))
	{
		return;
	}

	if (GetIsValidWorldCampaignProgressManager())
	{
		M_WorldCampaignProgressManager->RestoreFromSave(LoadedWorldCampaignState.Progress);
	}
	InitializeOperationMapsForCampaignSeed(LoadedWorldCampaignState.WorldGenerationSeed);
	M_WorldGenerator->RestoreWorldStateFromSave(LoadedWorldCampaignState);
	BeginPlay_LoadSavedWorld_RemoveUnusedLevelAnchors();
	// Rewards and bonus objectives are not saved; they are dealt again from the campaign seed.
	LoadWorldDataIntoObjects();
	M_WorldProfileAndUIManager->SetupUIForLoadedCampaign(LoadedPlayerProfileSaveData);
	RestoreWorldDivisionsFromSave(LoadedWorldCampaignState);
	OnAllWorldObjectsAndTheirDataReady();
}

void AWorldPlayerController::BeginPlay_LoadSavedWorld_RemoveUnusedLevelAnchors()
{
	TArray<AAnchorPoint*> UnusedLevelAnchors;
	for (TActorIterator<AAnchorPoint> AnchorIterator(GetWorld()); AnchorIterator; ++AnchorIterator)
	{
		if (not AnchorIterator->Tags.Contains(WorldPlayerOperationMapConstants::GeneratedAnchorTag))
		{
			UnusedLevelAnchors.Add(*AnchorIterator);
		}
	}

	for (AAnchorPoint* UnusedLevelAnchor : UnusedLevelAnchors)
	{
		UnusedLevelAnchor->Destroy();
	}
}

void AWorldPlayerController::OnGeneratedCampaignAsyncWorkFinished()
{
	if (not GetIsValidWorldProfileAndUIManager()
		|| not GetIsValidWorldGenerator()
		|| not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	M_WorldGenerator->OnGenerationFinished().RemoveAll(this);
	M_WorldGenerator->PruneUnusedAnchorsAndRepairConnectivity();
	UpdateAsyncWorldGenerationWidget_PruningCompleted();
	M_WorldGenerator->EnsureMissionObjectsAreReachableBeforeEnemyHQ();
	LoadWorldDataIntoObjects();
	M_WorldGenerator->InitMapObjectsBaseFortificationStrength(M_SelectedDifficulty.DifficultyLevel);
	bM_IsNewlyGeneratedCampaign = true;
	if (GetIsValidWorldCampaignProgressManager())
	{
		const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
		const ERTSCommander Commander = IsValid(GameInstance) && GameInstance->GetHasPlayerCommander()
			                                ? GameInstance->GetPlayerCommander()
			                                : ERTSCommander::NoCommander;
		M_WorldCampaignProgressManager->InitializeForNewCampaign(Commander, M_SelectedDifficulty);
	}
	M_WorldStateAndSaveManager->CacheCurrentWorldState(*M_WorldGenerator.Get());
	M_WorldStateAndSaveManager->ResetEnemyObjectProceduralMapIndex();
	InitializeOperationMapsForCampaignSeed(M_WorldStateAndSaveManager->GetWorldGenerationSeed());
	const FPlayerProfileSaveData PlayerProfileSaveData =
		M_WorldProfileAndUIManager->OnSetupUIForNewCampaign(M_PlayerFaction);
	M_WorldStateAndSaveManager->CachePlayerProfileSaveData(PlayerProfileSaveData);
	InitializeWorldDivisionsForNewCampaign();
	OnAllWorldObjectsAndTheirDataReady();
}

void AWorldPlayerController::LoadWorldDataIntoObjects()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	M_WorldGenerator->LoadWorldDataIntoObjects();
}

void AWorldPlayerController::InitializeWorldDivisionsForNewCampaign()
{
	if (not GetIsValidWorldDivisionManager() || not GetIsValidWorldGenerator())
	{
		return;
	}

	M_WorldDivisionManager->InitializeNewCampaignDivisions(
		this,
		M_WorldGenerator.Get(),
		M_SelectedDifficulty.DifficultyLevel);
	if (GetIsValidWorldEnemyTurnDirector())
	{
		M_WorldEnemyTurnDirector->InitializeEnemyTurnDirector(this);
	}
}

void AWorldPlayerController::RestoreWorldDivisionsFromSave(
	const FWorldCampaignState& LoadedWorldCampaignState)
{
	if (not GetIsValidWorldDivisionManager() || not GetIsValidWorldGenerator())
	{
		return;
	}

	M_WorldDivisionManager->RestoreSavedCampaignDivisions(
		this,
		M_WorldGenerator.Get(),
		M_SelectedDifficulty.DifficultyLevel,
		LoadedWorldCampaignState.WorldDivisions);
	if (GetIsValidWorldEnemyTurnDirector())
	{
		M_WorldEnemyTurnDirector->InitializeEnemyTurnDirector(this);
	}
}

void AWorldPlayerController::RefreshWorldDivisionInfluence()
{
	if (not GetIsValidWorldDivisionManager())
	{
		return;
	}

	M_WorldDivisionManager->RefreshDivisionInfluence(M_SelectedDifficulty.DifficultyLevel);
}

void AWorldPlayerController::OnAllWorldObjectsAndTheirDataReady()
{
	if (bM_HasCompletedInitialWorldSetup)
	{
		return;
	}

	bM_HasCompletedInitialWorldSetup = true;
	HideAsyncWorldGenerationWidget();
	WorldGenerated_InitCountryOccupationRegulator();
	WorldGenerated_SpawnWorldFowManager();
	// Build nodes need the progress state, the country regulator and the fog manager, which all exist by now.
	if (GetIsValidWorldBuildNodeManager())
	{
		M_WorldBuildNodeManager->InitializeBuildNodes(this);
	}
	WorldGenerated_SpawnConnectionSplineRenderer();
	OnInitialWorldSetupComplete();
}

void AWorldPlayerController::WorldGenerated_InitCountryOccupationRegulator()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	M_WorldGenerator->InitializeCountryOccupationRegulator();
	UWorldCountryOccupationRegulator* CountryRegulator =
		M_WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>();
	if (IsValid(CountryRegulator))
	{
		CountryRegulator->InitializeCampaignProgress(this);
	}
}

void AWorldPlayerController::WorldGenerated_SpawnWorldFowManager()
{
	if (not GetIsValidWorldGenerator() || not IsValid(M_WorldFowManagerClass))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	AWorldFowManager* SpawnedManager = World->SpawnActor<AWorldFowManager>(M_WorldFowManagerClass);
	if (not IsValid(SpawnedManager))
	{
		return;
	}

	M_WorldFowManager = SpawnedManager;
	SpawnedManager->InitializeWorldFow(this, M_WorldGenerator.Get());
}

void AWorldPlayerController::WorldGenerated_SpawnConnectionSplineRenderer()
{
	if (not GetIsValidWorldGenerator() || not IsValid(M_ConnectionSplineRendererClass))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	AWorldConnectionSplineRenderer* SpawnedRenderer =
		World->SpawnActor<AWorldConnectionSplineRenderer>(M_ConnectionSplineRendererClass);
	if (not IsValid(SpawnedRenderer))
	{
		return;
	}

	M_ConnectionSplineRenderer = SpawnedRenderer;
	SpawnedRenderer->InitializeConnectionSplines(this, M_WorldGenerator.Get());
}

bool AWorldPlayerController::GetCanPrimaryClickActor(AActor* ClickedActor) const
{
	if (not IsValid(ClickedActor))
	{
		return false;
	}

	if (not ClickedActor->GetClass()->ImplementsInterface(UWorldFowParticipant::StaticClass()))
	{
		return true;
	}

	const IWorldFowParticipant* FowParticipant = Cast<IWorldFowParticipant>(ClickedActor);
	return FowParticipant != nullptr && FowParticipant->GetCanPrimaryClickInteract();
}

void AWorldPlayerController::OnInitialWorldSetupComplete()
{
	WorldSetupComplete_MovePlayerToHQ();
	WorldSetupComplete_ApplyClearedAnchors();
	if (bM_IsNewlyGeneratedCampaign)
	{
		// A new campaign opens on the player's first turn: count it, save it and start it.
		OnEndEnemyTurn();
		return;
	}

	if (WorldSetupComplete_ResolveReturningOperation())
	{
		// Saved at once, so a win is kept even when the game closes while the enemy pieces are still moving.
		if (GetIsValidWorldStateAndSaveManager())
		{
			M_WorldStateAndSaveManager->SaveCampaignState();
		}
		LogWorldCampaignSummary(TEXT("OperationResolved"));
		// One operation is one turn, won or lost: the enemy answers now.
		PlayTurn(EWorldTurnType::Enemy);
		return;
	}

	LogWorldCampaignSummary(TEXT("CampaignLoaded"));

	// The campaign was only loaded: the player's turn goes on where it was saved.
	if (GetIsValidWorldStateAndSaveManager())
	{
		UpdateTurnCounter(M_WorldStateAndSaveManager->GetCurrentTurn());
	}
	PlayTurn(EWorldTurnType::Player);
}

void AWorldPlayerController::WorldSetupComplete_ApplyClearedAnchors()
{
	if (not GetIsValidWorldCampaignProgressManager())
	{
		return;
	}

	for (TActorIterator<AWorldMapObject> MapObjectIterator(GetWorld()); MapObjectIterator; ++MapObjectIterator)
	{
		AWorldMapObject* MapObject = *MapObjectIterator;
		const bool bIsOperationTarget = MapObject->IsA(AWorldMissionObject::StaticClass())
			|| MapObject->IsA(AWorldEnemyObject::StaticClass());
		if (bIsOperationTarget && M_WorldCampaignProgressManager->GetIsAnchorCleared(MapObject->GetAnchorKey()))
		{
			MapObject->SetIsClearedByPlayer();
		}
	}
}

bool AWorldPlayerController::WorldSetupComplete_ResolveReturningOperation()
{
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance) || not GetIsValidWorldCampaignProgressManager())
	{
		return false;
	}

	FWorldMissionResult MissionResult = GameInstance->ConsumeWorldMissionResult();
	const FWorldPendingOperation PendingOperation =
		M_WorldCampaignProgressManager->GetProgressState().PendingOperation;
#if !UE_BUILD_SHIPPING
	// -RTSWorldCampaignSimulateResult=Victory|Defeat stands in for the mission when the load path is checked headless.
	FString SimulatedOutcome;
	if (PendingOperation.bIsPending && not MissionResult.GetHasResult()
		&& FParse::Value(FCommandLine::Get(), TEXT("-RTSWorldCampaignSimulateResult="), SimulatedOutcome))
	{
		MissionResult.Outcome = SimulatedOutcome.Equals(TEXT("Victory"), ESearchCase::IgnoreCase)
			                        ? EWorldMissionOutcome::Victory
			                        : EWorldMissionOutcome::Defeat;
		MissionResult.SourceAnchorKey = PendingOperation.AnchorKey;
		MissionResult.SourceDivisionKey = PendingOperation.DivisionKey;
	}
#endif
	if (not PendingOperation.bIsPending)
	{
		return false;
	}

	AWorldMapObject* TargetMapObject = FindWorldMapObjectByAnchorKey(PendingOperation.AnchorKey);
	const FPrimaryReward* RewardOnVictory = nullptr;
	if (const AWorldMissionObject* MissionMapObject = Cast<AWorldMissionObject>(TargetMapObject))
	{
		RewardOnVictory = &MissionMapObject->GetPrimaryReward();
	}
	else if (const AWorldEnemyObject* EnemyMapObject = Cast<AWorldEnemyObject>(TargetMapObject))
	{
		RewardOnVictory = &EnemyMapObject->GetPrimaryReward();
	}

	const float WallProximity = IsValid(TargetMapObject)
		                            ? GetWallProximityForLocation(TargetMapObject->GetActorLocation())
		                            : 0.f;
	const FWorldOperationResolution Resolution = M_WorldCampaignProgressManager->ResolvePendingOperation(
		MissionResult, RewardOnVictory, WallProximity);
	if (not Resolution.bWasResolved)
	{
		// The game was closed during the operation: it never happened, and the turn is not spent.
		M_WorldCampaignProgressManager->DiscardPendingOperation();
		return false;
	}

	if (Resolution.bWasVictory && IsValid(TargetMapObject))
	{
		TargetMapObject->SetIsClearedByPlayer();
	}

	const bool bDefeatedEnemyDivision = Resolution.bWasVictory
		&& Resolution.Operation.Target == EWorldOperationTarget::EnemyDivision;
	if (bDefeatedEnemyDivision && GetIsValidWorldEnemyTurnDirector())
	{
		M_WorldEnemyTurnDirector->RemoveEnemyPieceDefeatedByPlayer(Resolution.Operation.DivisionKey);
	}
	return true;
}

void AWorldPlayerController::WorldSetupComplete_MovePlayerToHQ()
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	FMovePlayerCamera MoveRequest;
	MoveRequest.MoveToLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(this);
	MoveRequest.TimeCameraInputDisabled = 2.f;
	MoveRequest.TimeToMove = 2.f;
	M_WorldCameraController->MoveCameraTo(MoveRequest);
}

void AWorldPlayerController::ShowAsyncWorldGenerationWidget()
{
	if (IsValid(M_AsyncWorldGenerationWidget))
	{
		SetIsWorldCameraMovementDisabled(true);
		return;
	}

	if (not IsValid(M_AsyncWorldGenerationWidgetClass.Get()))
	{
		RTSFunctionLibrary::ReportError(TEXT("M_AsyncWorldGenerationWidgetClass is not set."));
		return;
	}

	M_AsyncWorldGenerationWidget = CreateWidget<UW_AsyncWorldGeneration>(this, M_AsyncWorldGenerationWidgetClass);
	if (not GetIsValidAsyncWorldGenerationWidget())
	{
		return;
	}

	M_AsyncWorldGenerationWidget->AddToViewport();
	SetIsWorldCameraMovementDisabled(true);
}

void AWorldPlayerController::HideAsyncWorldGenerationWidget()
{
	StopAsyncWorldGenerationProgressTimer();
	if (IsValid(M_AsyncWorldGenerationWidget))
	{
		M_AsyncWorldGenerationWidget->RemoveFromParent();
		M_AsyncWorldGenerationWidget = nullptr;
	}

	SetIsWorldCameraMovementDisabled(false);
}

void AWorldPlayerController::UpdateAsyncWorldGenerationWidget_AnchorPlacementStarted()
{
	const FText DescriptionText = FText::FromString(TEXT("Placing World Anchors"));
	SetAsyncWorldGenerationWidgetProgress(
		DescriptionText,
		WorldPlayerAsyncGenerationUIConstants::AnchorPlacementStartedPercentage
	);
}

void AWorldPlayerController::UpdateAsyncWorldGenerationWidget_AnchorPlacementComplete()
{
	const FText DescriptionText = FText::FromString(TEXT("Generating Edges"));
	SetAsyncWorldGenerationWidgetProgress(
		DescriptionText,
		WorldPlayerAsyncGenerationUIConstants::AnchorPlacementCompletePercentage
	);
}

void AWorldPlayerController::UpdateAsyncWorldGenerationWidget_ConnectionGenerationComplete()
{
	const FText DescriptionText = FText::FromString(TEXT("Generating World with Backtracking"));
	SetAsyncWorldGenerationWidgetProgress(
		DescriptionText,
		WorldPlayerAsyncGenerationUIConstants::ConnectionGenerationCompletePercentage
	);
	StartAsyncWorldGenerationProgressTimer();
}

void AWorldPlayerController::UpdateAsyncWorldGenerationWidget_AsyncGenerationComplete()
{
	StopAsyncWorldGenerationProgressTimer();
	const FText DescriptionText = FText::FromString(TEXT("Pruning unused anchor points..."));
	SetAsyncWorldGenerationWidgetProgress(
		DescriptionText,
		WorldPlayerAsyncGenerationUIConstants::AsyncGenerationCompletePercentage
	);
}

void AWorldPlayerController::UpdateAsyncWorldGenerationWidget_PruningCompleted()
{
	
	const FText DescriptionText = FText::FromString(TEXT("Finalizing World"));
	SetAsyncWorldGenerationWidgetProgress(
		DescriptionText,
		WorldPlayerAsyncGenerationUIConstants::PruningCompletedPercentage
	);
}

void AWorldPlayerController::StartAsyncWorldGenerationProgressTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_AsyncWorldGenerationProgressElapsedSeconds = 0.f;
	World->GetTimerManager().SetTimer(
		M_AsyncWorldGenerationProgressTimerHandle,
		this,
		&AWorldPlayerController::UpdateAsyncWorldGenerationTimedProgress,
		WorldPlayerAsyncGenerationUIConstants::TimedProgressIntervalSeconds,
		true
	);
}

void AWorldPlayerController::StopAsyncWorldGenerationProgressTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_AsyncWorldGenerationProgressTimerHandle);
}

void AWorldPlayerController::UpdateAsyncWorldGenerationTimedProgress()
{
	M_AsyncWorldGenerationProgressElapsedSeconds += WorldPlayerAsyncGenerationUIConstants::TimedProgressIntervalSeconds;
	const float ProgressAlpha = FMath::Clamp(
		M_AsyncWorldGenerationProgressElapsedSeconds / WorldPlayerAsyncGenerationUIConstants::TimedProgressTargetSeconds,
		0.f,
		1.f
	);
	const float PercentageValue = FMath::Lerp(
		WorldPlayerAsyncGenerationUIConstants::ConnectionGenerationCompletePercentage,
		WorldPlayerAsyncGenerationUIConstants::AsyncGenerationCompletePercentage,
		ProgressAlpha
	);
	const FText DescriptionText = FText::FromString(TEXT("  Generating World with Backtracking</>"));
	SetAsyncWorldGenerationWidgetProgress(DescriptionText, PercentageValue);
	if (ProgressAlpha >= 1.f)
	{
		StopAsyncWorldGenerationProgressTimer();
	}
}

void AWorldPlayerController::SetAsyncWorldGenerationWidgetProgress(const FText& DescriptionText,
                                                                   const float PercentageValue)
{
	if (not GetIsValidAsyncWorldGenerationWidget())
	{
		return;
	}

	M_AsyncWorldGenerationWidget->SetGenerationProgress(DescriptionText, PercentageValue);
}

void AWorldPlayerController::BeginCameraOvertakeControl(
	const FCameraOvertakeSettings& CameraOvertakeSettings,
	FWorldCameraOvertakeFinishedDelegate OnCameraOvertakeFinished)
{
	if (not bM_IsCameraOvertakeControlActive)
	{
		HideWorldUIForCameraOvertake();
	}

	bM_IsCameraOvertakeControlActive = true;
	M_CameraOvertakeFinishedDelegate = OnCameraOvertakeFinished;
	M_CameraOvertakeSoundPerSequentialPoint = CameraOvertakeSettings.SoundPerSequentialPoint;
	SetIsWorldPlayerInputDisabled(true);
}

void AWorldPlayerController::OnCameraOvertakePointReached(
	const int32 ReachedPointIndex,
	const FVector& ReachedCameraLocation)
{
	(void)ReachedCameraLocation;
	constexpr int32 FirstSequentialPointIndex = 1;
	if (ReachedPointIndex < FirstSequentialPointIndex)
	{
		return;
	}

	PlayCameraOvertakeSound(M_CameraOvertakeSoundPerSequentialPoint);
}

void AWorldPlayerController::OnCameraOvertakeFinished(const FVector& LastCameraPosition)
{
	bM_IsCameraOvertakeControlActive = false;
	M_CameraOvertakeSoundPerSequentialPoint = nullptr;
	RestoreWorldUIAfterCameraOvertake();
	SetIsWorldPlayerInputDisabled(false);

	FWorldCameraOvertakeFinishedDelegate FinishedDelegate = M_CameraOvertakeFinishedDelegate;
	M_CameraOvertakeFinishedDelegate.Clear();
	FinishedDelegate.ExecuteIfBound(LastCameraPosition);
}

void AWorldPlayerController::HideWorldUIForCameraOvertake()
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	M_WorldMenuVisibilityBeforeCameraOvertake = WorldMenu->GetVisibility();
	WorldMenu->SetVisibility(ESlateVisibility::Collapsed);
}

void AWorldPlayerController::RestoreWorldUIAfterCameraOvertake()
{
	if (not GetIsValidWorldProfileAndUIManager())
	{
		return;
	}

	UW_WorldMenu* WorldMenu = M_WorldProfileAndUIManager->GetWorldMenu();
	if (not IsValid(WorldMenu))
	{
		return;
	}

	WorldMenu->SetVisibility(M_WorldMenuVisibilityBeforeCameraOvertake);
}

void AWorldPlayerController::SetIsWorldPlayerInputDisabled(const bool bIsDisabled)
{
	if (bM_IsWorldPlayerInputDisabled == bIsDisabled)
	{
		return;
	}

	bM_IsWorldPlayerInputDisabled = bIsDisabled;
	SetIgnoreMoveInput(bIsDisabled);
	SetIgnoreLookInput(bIsDisabled);
	SetIsWorldCameraMovementDisabled(bIsDisabled);
	if (bM_IsWorldPlayerInputDisabled)
	{
		FlushPressedKeys();
	}
}

void AWorldPlayerController::PlayCameraOvertakeSound(USoundBase* Sound) const
{
	if (not IsValid(Sound))
	{
		return;
	}

	if (not GetIsValidWorldPlayerAudioController())
	{
		return;
	}

	M_WorldPlayerAudioController->PlayUISound(Sound);
}

bool AWorldPlayerController::GetIsValidAsyncWorldGenerationWidget() const
{
	if (IsValid(M_AsyncWorldGenerationWidget))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_AsyncWorldGenerationWidget"),
		TEXT("GetIsValidAsyncWorldGenerationWidget"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldProfileAndUIManager() const
{
	if (not M_WorldProfileAndUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_WorldProfileAndUIManager"),
			TEXT("GetIsValidWorldProfileAndUIManager"),
			this
		);
		return false;
	}
	return true;
}

bool AWorldPlayerController::GetIsValidWorldCampaignProgressManager() const
{
	if (IsValid(M_WorldCampaignProgressManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCampaignProgressManager"),
		TEXT("GetIsValidWorldCampaignProgressManager"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldEnemyTurnDirector() const
{
	if (IsValid(M_WorldEnemyTurnDirector))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldEnemyTurnDirector"),
		TEXT("GetIsValidWorldEnemyTurnDirector"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldBuildNodeManager() const
{
	if (IsValid(M_WorldBuildNodeManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldBuildNodeManager"),
		TEXT("GetIsValidWorldBuildNodeManager"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldCommanderMapKit() const
{
	if (IsValid(M_WorldCommanderMapKit))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCommanderMapKit"),
		TEXT("GetIsValidWorldCommanderMapKit"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldStateAndSaveManager() const
{
	if (IsValid(M_WorldStateAndSaveManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldStateAndSaveManager"),
		TEXT("GetIsValidWorldStateAndSaveManager"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldDivisionManager() const
{
	if (IsValid(M_WorldDivisionManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldDivisionManager"),
		TEXT("GetIsValidWorldDivisionManager"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidPlayerResourceManager() const
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_PlayerResourceManager"),
		TEXT("GetIsValidPlayerResourceManager"),
		this
	);
	return false;
}

bool AWorldPlayerController::GetIsValidWorldGenerator() const
{
	if (not M_WorldGenerator.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			TEXT("M_WorldGenerator"),
			TEXT("GetIsValidWorldGenerator"),
			this
		);
		return false;
	}
	return true;
}
