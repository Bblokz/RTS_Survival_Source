// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCampaignUICaptureSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitTypes.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnDirector.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildSitePanel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/W_WorldCampaignHUD.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/W_WorldCommanderPanel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/W_WorldTechWheel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldCameraController/WorldCameraController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWorldCampaignUICapture, Log, All);

namespace WorldCampaignUICaptureConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("WorldCampaignUICapture");
	const TCHAR* const SaveSlotSwitch = TEXT("-RTSWorldCampaignSaveSlot=");
	const TCHAR* const OutputFolder = TEXT("WorldCampaignUICapture");
	// The same seed the deck builder capture uses, so both look at the same campaign.
	constexpr int32 WorldCampaignGenerationSeed = 1337;
	constexpr double MenuBuildSeconds = 3.0;
	constexpr double SettleSeconds = 1.5;
	constexpr double CaptureSeconds = 0.7;
	const FIntPoint CaptureResolution(1920, 1080);
	// Enough to buy the buildings and tech nodes the steps buy.
	constexpr int32 CaptureRadixite = 2000;
	constexpr int32 CaptureBlueprints = 12;
	constexpr int32 CaptureFreezeStage = 2;
	constexpr int32 CaptureFreezePoints = 20;
	const FName HeavyRimNodeId(TEXT("Hvy_4"));
	const FName FirstInfantryPathNodeId(TEXT("Inf_0"));
	const FName FirstInfantryTechNodeId(TEXT("Inf_1"));
}

bool UWorldCampaignUICaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), WorldCampaignUICaptureConstants::CommandLineSwitch);
#endif
}

void UWorldCampaignUICaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace WorldCampaignUICaptureConstants;
	Super::OnWorldBeginPlay(InWorld);
	FString SaveSlotName;
	if (not FParse::Value(FCommandLine::Get(), SaveSlotSwitch, SaveSlotName) || SaveSlotName.IsEmpty())
	{
		// A new campaign saves on its first turn; without its own slot that would replace the real campaign.
		UE_LOG(LogRTSWorldCampaignUICapture, Error, TEXT("UICAPTURE refused: pass %s<name>"), SaveSlotSwitch);
		FPlatformMisc::RequestExit(false, TEXT("WorldCampaignUICapture"));
		return;
	}

	RTSFunctionLibrary::PushModalDialogSuppression();
	SeedGameInstanceForWorldCampaign();
	FSystemResolution::RequestResolutionChange(CaptureResolution.X, CaptureResolution.Y, EWindowMode::Windowed);
	EnterPhase(EPhase::WaitingForCampaign);
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE started"));
}

void UWorldCampaignUICaptureSubsystem::SeedGameInstanceForWorldCampaign() const
{
	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		UE_LOG(LogRTSWorldCampaignUICapture, Warning, TEXT("UICAPTURE no RTS game instance to seed"));
		return;
	}

	// The map is started directly, so the main menu never set these.
	FCampaignGenerationSettings GenerationSettings;
	GenerationSettings.bNeedsToGenerateCampaign = true;
	GenerationSettings.GenerationSeed = WorldCampaignUICaptureConstants::WorldCampaignGenerationSeed;
	GenerationSettings.bAreSettingsLoaded = true;
	GameInstance->SetCampaignGenerationSettings(GenerationSettings);
	FRTSGameDifficulty Difficulty;
	Difficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficulty(Difficulty);
	GameInstance->SetPlayerFaction(ERTSFaction::GerBreakthroughDoctrine);
	GameInstance->SetPlayerCommander(ERTSCommander::LuftwaffeLiaison);
}

AWorldPlayerController* UWorldCampaignUICaptureSubsystem::GetWorldController() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? Cast<AWorldPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

UW_WorldMenu* UWorldCampaignUICaptureSubsystem::FindReadyWorldMenu() const
{
	UWorld* const World = GetWorld();
	const AWorldPlayerController* const WorldController = GetWorldController();
	if (not IsValid(World) || not IsValid(WorldController))
	{
		return nullptr;
	}

	// The loading screen is removed once the campaign is generated.
	TArray<UUserWidget*> LoadingScreens;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, LoadingScreens, UW_AsyncWorldGeneration::StaticClass(), false);
	if (LoadingScreens.Num() > 0)
	{
		return nullptr;
	}

	const UWorldProfileAndUIManager* const ProfileManager = WorldController->FindComponentByClass<
		UWorldProfileAndUIManager>();
	return IsValid(ProfileManager) ? ProfileManager->GetWorldMenu() : nullptr;
}

void UWorldCampaignUICaptureSubsystem::HoldCameraAgainstEdgeScroll() const
{
	const AWorldPlayerController* const WorldController = GetWorldController();
	UWorldCameraController* const CameraController = IsValid(WorldController)
		                                                 ? WorldController->FindComponentByClass<UWorldCameraController>()
		                                                 : nullptr;
	if (IsValid(CameraController) && not CameraController->GetIsCameraMovementDisabled())
	{
		CameraController->SetIsCameraMovementDisabled(true);
	}
}

void UWorldCampaignUICaptureSubsystem::Tick(float DeltaTime)
{
	using namespace WorldCampaignUICaptureConstants;
	switch (M_Phase)
	{
	case EPhase::WaitingForCampaign:
		HoldCameraAgainstEdgeScroll();
		if (GetSecondsInPhase() >= MenuBuildSeconds && IsValid(FindReadyWorldMenu()))
		{
			EnterPhase(EPhase::ApplyingStep);
		}
		break;
	case EPhase::ApplyingStep:
		M_CurrentStepName = ApplyStep(M_StepIndex);
		if (M_CurrentStepName.IsEmpty())
		{
			Finish();
			break;
		}
		EnterPhase(EPhase::Settling);
		break;
	case EPhase::Settling:
		if (GetSecondsInPhase() >= SettleSeconds)
		{
			CaptureStep();
			EnterPhase(EPhase::WaitingForCapture);
		}
		break;
	case EPhase::WaitingForCapture:
		if (GetSecondsInPhase() >= CaptureSeconds)
		{
			++M_StepIndex;
			EnterPhase(EPhase::ApplyingStep);
		}
		break;
	default:
		break;
	}
}

FString UWorldCampaignUICaptureSubsystem::ApplyStep(const int32 StepIndex)
{
	using namespace WorldCampaignUICaptureConstants;
	AWorldPlayerController* const Controller = GetWorldController();
	UW_WorldMenu* const WorldMenu = FindReadyWorldMenu();
	UW_WorldCampaignHUD* const CampaignHUD = IsValid(WorldMenu) ? WorldMenu->GetCampaignHUD() : nullptr;
	if (not IsValid(Controller) || not IsValid(CampaignHUD))
	{
		UE_LOG(LogRTSWorldCampaignUICapture, Error, TEXT("UICAPTURE the campaign HUD is missing"));
		return FString();
	}

	UW_WorldCommanderPanel* const CommanderPanel = CampaignHUD->GetCommanderPanel();
	UW_WorldTechWheel* const TechWheel = WorldMenu->GetTechWheel();
	switch (StepIndex)
	{
	case 0:
		if (UWorldCampaignProgressManager* ProgressManager = Controller->GetWorldCampaignProgressManager())
		{
			FWorldMapResources CaptureResources;
			CaptureResources.Radixite = CaptureRadixite;
			CaptureResources.Blueprints = CaptureBlueprints;
			ProgressManager->AddResources(CaptureResources);
		}
		CampaignHUD->RefreshAtTurnStart();
		return TEXT("01_map");
	case 1:
		ApplyStep_OpenBuildSite(*Controller, *CampaignHUD);
		return TEXT("02_build_site");
	case 2:
		ApplyStep_BuildOnSite(*CampaignHUD);
		return TEXT("03_build_site_built");
	case 3:
		CampaignHUD->CloseBuildSite();
		if (IsValid(CommanderPanel))
		{
			CommanderPanel->ShowAbilityTooltipForCapture(true);
		}
		return TEXT("04_commander_tooltip");
	case 4:
		if (IsValid(CommanderPanel))
		{
			CommanderPanel->ShowAbilityTooltipForCapture(false);
		}
		WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::TechTree);
		return TEXT("05_tech_wheel");
	case 5:
		if (IsValid(TechWheel))
		{
			TechWheel->SelectNodeForCapture(HeavyRimNodeId);
		}
		return TEXT("06_tech_wheel_key_selected");
	case 6:
		ApplyStep_BuyTechNodes(*WorldMenu);
		return TEXT("07_tech_wheel_bought");
	case 7:
		WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::World);
		ApplyStep_PlayEnemyTurn(*Controller);
		return TEXT("08_enemy_reaction");
	case 8:
		ApplyStep_ShowFreezeAndCooldown(*Controller, *CampaignHUD);
		return TEXT("09_freeze_and_cooldown");
	default:
		return FString();
	}
}

void UWorldCampaignUICaptureSubsystem::ApplyStep_OpenBuildSite(AWorldPlayerController& Controller,
                                                               UW_WorldCampaignHUD& CampaignHUD)
{
	AWorldBuildNode* const BuildNode = FindOpenBuildNodeNearHQ(Controller);
	M_CaptureBuildNode = BuildNode;
	if (not IsValid(BuildNode))
	{
		UE_LOG(LogRTSWorldCampaignUICapture, Warning, TEXT("UICAPTURE no open build site to show"));
		return;
	}

	FMovePlayerCamera MoveRequest;
	MoveRequest.MoveToLocation = BuildNode->GetActorLocation();
	Controller.WorldCamera_MoveTo(MoveRequest);
	CampaignHUD.OpenBuildSite(BuildNode);
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE opened build site %s"),
	       *BuildNode->GetBuildNodeId().ToString());
}

void UWorldCampaignUICaptureSubsystem::ApplyStep_BuildOnSite(UW_WorldCampaignHUD& CampaignHUD) const
{
	UW_WorldBuildSitePanel* const BuildSitePanel = CampaignHUD.GetBuildSitePanel();
	if (not IsValid(BuildSitePanel))
	{
		return;
	}

	BuildSitePanel->SelectBuildingForCapture(EWorldCampaignBuilding::SupplyDepot);
	const bool bDidBuild = BuildSitePanel->BuildSelectedForCapture();
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE build supply depot: %s"),
	       bDidBuild ? TEXT("done") : TEXT("refused"));
	// The panel closes after a build; open it again to show the built state.
	CampaignHUD.OpenBuildSite(M_CaptureBuildNode.Get());
}

void UWorldCampaignUICaptureSubsystem::ApplyStep_BuyTechNodes(UW_WorldMenu& WorldMenu) const
{
	using namespace WorldCampaignUICaptureConstants;
	UW_WorldTechWheel* const TechWheel = WorldMenu.GetTechWheel();
	if (not IsValid(TechWheel))
	{
		return;
	}

	const bool bBoughtPath = TechWheel->BuyNodeForCapture(FirstInfantryPathNodeId);
	const bool bBoughtTech = TechWheel->BuyNodeForCapture(FirstInfantryTechNodeId);
	TechWheel->SelectNodeForCapture(FirstInfantryTechNodeId);
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE bought path node: %s, technology: %s"),
	       bBoughtPath ? TEXT("yes") : TEXT("no"), bBoughtTech ? TEXT("yes") : TEXT("no"));
}

void UWorldCampaignUICaptureSubsystem::ApplyStep_PlayEnemyTurn(AWorldPlayerController& Controller) const
{
	UWorldCampaignProgressManager* const ProgressManager = Controller.GetWorldCampaignProgressManager();
	UWorldEnemyTurnDirector* const EnemyTurnDirector = Controller.GetWorldEnemyTurnDirector();
	if (not IsValid(ProgressManager) || not IsValid(EnemyTurnDirector))
	{
		return;
	}

	// A fully alarmed enemy spends its whole budget on convoys and divisions, which is what the map tags show.
	FWorldEnemyTurnState& EnemyTurnState = ProgressManager->GetMutableProgressState().EnemyTurn;
	EnemyTurnState.Alert = 1.f;
	EnemyTurnState.BankedReactionPoints += 12;
	EnemyTurnState.RecentlyStruckLocations.Add(FRTS_WorldStatics::GetPlayerHQWorldLocation(&Controller));
	FMovePlayerCamera MoveRequest;
	MoveRequest.MoveToLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(&Controller);
	Controller.WorldCamera_MoveTo(MoveRequest);
	EnemyTurnDirector->PlayEnemyTurn(ERTSGameDifficulty::Normal, FSimpleDelegate());
	const FWorldEnemyTurnSummary Summary = EnemyTurnDirector->GetLastEnemyTurnSummary();
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE enemy turn: convoys=%d divisions=%d freeze=%d"),
	       Summary.ConvoysSpawned, Summary.DivisionsSpawned, Summary.PointsToFreeze);
}

void UWorldCampaignUICaptureSubsystem::ApplyStep_ShowFreezeAndCooldown(AWorldPlayerController& Controller,
                                                                       UW_WorldCampaignHUD& CampaignHUD) const
{
	using namespace WorldCampaignUICaptureConstants;
	UWorldCampaignProgressManager* const ProgressManager = Controller.GetWorldCampaignProgressManager();
	if (not IsValid(ProgressManager))
	{
		return;
	}

	// The freeze never moves in a one-turn capture, so the bar is set by hand to show a part-filled track.
	FWorldEnemyTurnState& EnemyTurnState = ProgressManager->GetMutableProgressState().EnemyTurn;
	EnemyTurnState.FreezeStage = CaptureFreezeStage;
	EnemyTurnState.FreezeProgressPoints = CaptureFreezePoints;

	bool bDidUseAbility = false;
	if (UW_WorldCommanderPanel* const CommanderPanel = CampaignHUD.GetCommanderPanel())
	{
		FWorldCommanderMapAbilityTarget AbilityTarget;
		AbilityTarget.WorldLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(&Controller);
		AbilityTarget.bHasWorldLocation = true;
		bDidUseAbility = CommanderPanel->UseAbilityForCapture(AbilityTarget);
	}
	CampaignHUD.RefreshAtTurnStart();
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE freeze set, map ability used: %s"),
	       bDidUseAbility ? TEXT("yes") : TEXT("no"));
}

AWorldBuildNode* UWorldCampaignUICaptureSubsystem::FindOpenBuildNodeNearHQ(AWorldPlayerController& Controller) const
{
	const UWorldBuildNodeManager* const BuildNodeManager = Controller.GetWorldBuildNodeManager();
	if (not IsValid(BuildNodeManager))
	{
		return nullptr;
	}

	const FVector PlayerHQLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(&Controller);
	AWorldBuildNode* NearestBuildNode = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	const TArray<AWorldBuildNode*> BuildNodes = BuildNodeManager->GetBuildNodes();
	for (AWorldBuildNode* BuildNode : BuildNodes)
	{
		if (not IsValid(BuildNode) || BuildNode->GetNodeState() != EWorldBuildNodeState::Unlocked)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(BuildNode->GetActorLocation(), PlayerHQLocation);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestBuildNode = BuildNode;
		}
	}
	return NearestBuildNode;
}

void UWorldCampaignUICaptureSubsystem::LogMissionPieces() const
{
	for (TActorIterator<AWorldMissionObject> MissionIterator(GetWorld()); MissionIterator; ++MissionIterator)
	{
		const AWorldMissionObject* const MissionObject = *MissionIterator;
		UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE mission piece name='%s' hidden=%d outline=%d"),
		       *MissionObject->GetMissionDisplayName().ToString(), MissionObject->IsHidden() ? 1 : 0,
		       MissionObject->GetCanBeOutlined() ? 1 : 0);
	}
}

void UWorldCampaignUICaptureSubsystem::CaptureStep() const
{
	if (M_StepIndex == 0)
	{
		LogMissionPieces();
	}

	const FString ScreenshotPath = FPaths::ProjectSavedDir() / WorldCampaignUICaptureConstants::OutputFolder
		/ (TEXT("campaignui_") + M_CurrentStepName + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE capture %s"), *ScreenshotPath);
	// Where the player looks from, so a picture without the map can be told apart from a camera in the wrong place.
	AWorldPlayerController* const PlayerController = GetWorldController();
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE camera at %s looking %s, HQ at %s"),
	       *PlayerController->PlayerCameraManager->GetCameraLocation().ToCompactString(),
	       *PlayerController->PlayerCameraManager->GetCameraRotation().ToCompactString(),
	       *FRTS_WorldStatics::GetPlayerHQWorldLocation(PlayerController).ToCompactString());
}

void UWorldCampaignUICaptureSubsystem::EnterPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UWorldCampaignUICaptureSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UWorldCampaignUICaptureSubsystem::Finish()
{
	EnterPhase(EPhase::Done);
	UE_LOG(LogRTSWorldCampaignUICapture, Display, TEXT("UICAPTURE done"));
	FPlatformMisc::RequestExit(false, TEXT("WorldCampaignUICapture"));
}

TStatId UWorldCampaignUICaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldCampaignUICaptureSubsystem, STATGROUP_Tickables);
}
