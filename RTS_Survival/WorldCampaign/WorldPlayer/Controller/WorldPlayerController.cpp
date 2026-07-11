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
#include "WorldCameraController/WorldCameraController.h"
#include "WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldStrategicSupportArea.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionInfluenceComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
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
}

AWorldPlayerController::AWorldPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	M_PlayerWorldOutliner = CreateDefaultSubobject<UPlayerWorldOutliner>(TEXT("PlayerWorldOutliner"));
	M_WorldStateAndSaveManager = CreateDefaultSubobject<UWorldStateAndSaveManager>(TEXT("WorldStateAndSaveManager"));
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
	BeginPlay_SetupWorldGenerator();
	BeginPlay_SetupWorldMenu();
	BeginPlay_GameState_Faction_CampaignSettings();
	BeginPlay_InitializeWorldGenerator();
	if (M_CampaignSettings.bNeedsToGenerateCampaign)
	{
		BeginPlay_GenerateNewWorld();
		return;
	}
	BeginPlay_LoadSavedWorld();
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

	M_WorldGenerator->AdjustDifficultyPercentagesForStrategicSupport(M_SelectedDifficulty.DifficultyLevel);
	RefreshWorldDivisionInfluence();
	const FPlayerTurnContext PlayerTurnContext = GetPlayerTurnContext();
	BP_OnPlayerTurnStarted(PlayerTurnContext);
}

void AWorldPlayerController::EnemyTurn()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}
	MovePlayerDivisions();

	M_WorldGenerator->AdjustDifficultyPercentagesForStrategicSupport(M_SelectedDifficulty.DifficultyLevel);
	RefreshWorldDivisionInfluence();
	MoveEnemyDivisions();
	OnEndEnemyTurn();
}

void AWorldPlayerController::OnEndEnemyTurn()
{
	if (not GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	const int32 CurrentTurn = M_WorldStateAndSaveManager->AdvanceCurrentTurn();
	UpdateTurnCounter(CurrentTurn);
	M_WorldStateAndSaveManager->SaveCampaignState();
	PlayTurn(EWorldTurnType::Player);
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

	BP_OnEnemyMapLaunch(EnemyMapObject, EnemyMap);
	const int32 PreviousMapIndex = M_WorldStateAndSaveManager->GetEnemyObjectProceduralMapIndex();
	M_WorldStateAndSaveManager->AdvanceEnemyObjectProceduralMapIndex(M_ShuffledEnemyObjectProceduralMaps.Num());
	if (not M_WorldStateAndSaveManager->SaveCampaignState())
	{
		M_WorldStateAndSaveManager->SetEnemyObjectProceduralMapIndex(PreviousMapIndex);
		return;
	}

	OpenOperationMap(EnemyMap);
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

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, OperationMap, true, FString());
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

	InitializeOperationMapsForCampaignSeed(LoadedWorldCampaignState.WorldGenerationSeed);
	M_WorldGenerator->RestoreWorldStateFromSave(LoadedWorldCampaignState);
	M_WorldProfileAndUIManager->SetupUIForLoadedCampaign(LoadedPlayerProfileSaveData);
	RestoreWorldDivisionsFromSave(LoadedWorldCampaignState);
	OnAllWorldObjectsAndTheirDataReady();
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
	PlayTurn(EWorldTurnType::Enemy);
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
