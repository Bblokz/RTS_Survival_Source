// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerController.h"

#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "WorldPlayerOutliner/PlayerWorldOutliner.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "WorldCameraController/WorldCameraController.h"
#include "WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"

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
	BeginPlay_GenerateOrLoadWorld();
	OnInitialWorldSetupComplete();
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

void AWorldPlayerController::BeginPlay_GenerateOrLoadWorld()
{
	if (not GetIsValidWorldProfileAndUIManager() || not GetIsValidWorldGenerator() || not
		GetIsValidWorldStateAndSaveManager())
	{
		return;
	}

	M_WorldGenerator->InitializeWorldGenerator(
		this,
		M_CampaignSettings,
		M_SelectedDifficulty
	);

	if (M_CampaignSettings.bNeedsToGenerateCampaign)
	{
		M_WorldStateAndSaveManager->CacheCurrentWorldState(*M_WorldGenerator.Get());
		const FPlayerProfileSaveData PlayerProfileSaveData = M_WorldProfileAndUIManager->OnSetupUIForNewCampaign(
			M_PlayerFaction);
		M_WorldStateAndSaveManager->CachePlayerProfileSaveData(PlayerProfileSaveData);
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
