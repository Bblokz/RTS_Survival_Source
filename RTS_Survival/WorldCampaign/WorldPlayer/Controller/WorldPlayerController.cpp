// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "WorldPlayerOutliner/PlayerWorldOutliner.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "WorldCameraController/WorldCameraController.h"
#include "WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowManager.h"

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

AWorldPlayerController::AWorldPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	M_PlayerWorldOutliner = CreateDefaultSubobject<UPlayerWorldOutliner>(TEXT("PlayerWorldOutliner"));
	M_WorldStateAndSaveManager = CreateDefaultSubobject<UWorldStateAndSaveManager>(TEXT("WorldStateAndSaveManager"));
}

AGeneratorWorldCampaign* AWorldPlayerController::GetWorldGenerator() const
{
	if (not M_WorldGenerator.IsValid())
	{
		return nullptr;
	}
	return M_WorldGenerator.Get();
}

UWorldStateAndSaveManager* AWorldPlayerController::GetWorldStateAndSaveManager() const
{
	if (not IsValid(M_WorldStateAndSaveManager))
	{
		return nullptr;
	}
	return M_WorldStateAndSaveManager;
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
}

void AWorldPlayerController::PrimaryClick_Regular()
{
	FHitResult HitUnderCursor;
	if (not GetHitResultUnderCursor(ECC_Visibility, false, HitUnderCursor))
	{
		CollapseMissionMapItemDesc();
		return;
	}
	AActor* ClickedActor = HitUnderCursor.GetActor();
	if (not IsValid(ClickedActor))
	{
		CollapseMissionMapItemDesc();
		return;
	}
	if (not GetCanPrimaryClickActor(ClickedActor))
	{
		CollapseMissionMapItemDesc();
		return;
	}
	if (ClickedActor->IsA(AWorldEnemyObject::StaticClass()))
	{
		OnClicked_EnemyMapObj(Cast<AWorldEnemyObject>(ClickedActor));	
		return;
	}
	if (ClickedActor->IsA(AWorldMissionObject::StaticClass()))
	{
		OnClicked_MissionMapObj(Cast<AWorldMissionObject>(ClickedActor));	
		return;
	}
	if (ClickedActor->IsA(AWorldPlayerObject::StaticClass()))
	{
		OnClicked_PlayerMapObj(Cast<AWorldPlayerObject>(ClickedActor));	
		return;
	}
	if (ClickedActor->IsA(AWorldNeutralObject::StaticClass()))
	{
		OnClicked_NeutralMapObj(Cast<AWorldNeutralObject>(ClickedActor));	
		return;
	}
	CollapseMissionMapItemDesc();
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
		EnemyMapObj->GetMapItemUIData(),
		EnemyMapObj->GetPrimaryReward(),
		EnemyMapObj->GetSecondaryReward()
	);
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
		MissionMapObj->GetMapItemUIData(),
		MissionMapObj->GetPrimaryReward(),
		MissionMapObj->GetSecondaryReward()
	);
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
	M_PrimaryClickContext = EWorldPrimaryClickContext::None;
	PrimaryClick_Regular();
}

void AWorldPlayerController::CollapseMissionMapItemDesc()
{
	M_PrimaryClickContext = EWorldPrimaryClickContext::None;
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
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomIn();
}

void AWorldPlayerController::WorldCamera_ZoomOut()
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ZoomOut();
}

void AWorldPlayerController::WorldCamera_ForwardMovement(const float AxisValue)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->ForwardMovement(AxisValue);
}

void AWorldPlayerController::WorldCamera_RightMovement(const float AxisValue)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->RightMovement(AxisValue);
}

void AWorldPlayerController::WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest)
{
	if (not GetIsValidWorldCameraController())
	{
		return;
	}

	M_WorldCameraController->MoveCameraTo(MoveRequest);
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

	M_WorldGenerator->RestoreWorldStateFromSave(LoadedWorldCampaignState);
	M_WorldProfileAndUIManager->SetupUIForLoadedCampaign(LoadedPlayerProfileSaveData);
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
	LoadWorldDataIntoObjects();
	M_WorldStateAndSaveManager->CacheCurrentWorldState(*M_WorldGenerator.Get());
	const FPlayerProfileSaveData PlayerProfileSaveData =
		M_WorldProfileAndUIManager->OnSetupUIForNewCampaign(M_PlayerFaction);
	M_WorldStateAndSaveManager->CachePlayerProfileSaveData(PlayerProfileSaveData);
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

void AWorldPlayerController::OnAllWorldObjectsAndTheirDataReady()
{
	if (bM_HasCompletedInitialWorldSetup)
	{
		return;
	}

	bM_HasCompletedInitialWorldSetup = true;
	HideAsyncWorldGenerationWidget();
	BeginPlay_SpawnWorldFowManager();
	OnInitialWorldSetupComplete();
}

void AWorldPlayerController::BeginPlay_SpawnWorldFowManager()
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
