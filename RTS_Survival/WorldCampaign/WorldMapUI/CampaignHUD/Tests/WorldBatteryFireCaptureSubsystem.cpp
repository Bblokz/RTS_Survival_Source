// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldBatteryFireCaptureSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Misc/App.h"
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
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireDirector.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionTankFormation.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldCameraController/WorldCameraController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWorldBatteryFireCapture, Log, All);

namespace WorldBatteryFireCaptureConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("WorldCampaignBatteryFireCapture");
	const TCHAR* const SaveSlotSwitch = TEXT("-RTSWorldCampaignSaveSlot=");
	const TCHAR* const OutputFolder = TEXT("WorldCampaignUICapture");
	// The same seed the campaign UI capture uses, so both look at the same campaign.
	constexpr int32 WorldCampaignGenerationSeed = 1337;
	constexpr float MenuBuildSeconds = 3.f;
	constexpr float SettleSeconds = 1.5f;
	constexpr float PictureSeconds = 0.7f;
	const FIntPoint CaptureResolution(1920, 1080);
	// Enough to pay for the battery.
	constexpr int32 CaptureRadixite = 2000;
	constexpr int32 CaptureBlueprints = 12;
	// The target stands this far from the battery, so both fit in one picture with room around them.
	constexpr float TargetDistance = 700.f;
	const FVector TargetDirection(0.8f, 0.6f, 0.f);
	constexpr float RevealRadius = 900.f;
	constexpr float RevealFalloff = 300.f;
	const FName RevealSourceId(TEXT("BatteryFireCapture"));

	struct FTimedPicture
	{
		float SecondsAfterFire;
		const TCHAR* Name;
	};

	const FTimedPicture PicturesAfterFire[] = {
		{0.6f, TEXT("1_after_0s6")},
		{1.5f, TEXT("2_after_1s5")},
		{2.5f, TEXT("3_after_2s5")},
		{4.5f, TEXT("4_after_4s5")}
	};
	const TCHAR* const PictureBeforeFire = TEXT("0_before");
}

bool UWorldBatteryFireCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), WorldBatteryFireCaptureConstants::CommandLineSwitch);
#endif
}

void UWorldBatteryFireCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace WorldBatteryFireCaptureConstants;
	Super::OnWorldBeginPlay(InWorld);
	FString SaveSlotName;
	if (not FParse::Value(FCommandLine::Get(), SaveSlotSwitch, SaveSlotName) || SaveSlotName.IsEmpty())
	{
		// A new campaign saves on its first turn; without its own slot that would replace the real campaign.
		UE_LOG(LogRTSWorldBatteryFireCapture, Error, TEXT("BATTERYFIRECAPTURE refused: pass %s<name>"), SaveSlotSwitch);
		FPlatformMisc::RequestExit(false, TEXT("WorldCampaignBatteryFireCapture"));
		return;
	}

	RTSFunctionLibrary::PushModalDialogSuppression();
	SeedGameInstanceForWorldCampaign();
	if (FApp::CanEverRender())
	{
		FSystemResolution::RequestResolutionChange(CaptureResolution.X, CaptureResolution.Y, EWindowMode::Windowed);
	}
	EnterPhase(EPhase::WaitingForCampaign);
	UE_LOG(LogRTSWorldBatteryFireCapture, Display, TEXT("BATTERYFIRECAPTURE started"));
}

void UWorldBatteryFireCaptureSubsystem::SeedGameInstanceForWorldCampaign() const
{
	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		UE_LOG(LogRTSWorldBatteryFireCapture, Warning, TEXT("BATTERYFIRECAPTURE no RTS game instance to seed"));
		return;
	}

	// The map is started directly, so the main menu never set these.
	FCampaignGenerationSettings GenerationSettings;
	GenerationSettings.bNeedsToGenerateCampaign = true;
	GenerationSettings.GenerationSeed = WorldBatteryFireCaptureConstants::WorldCampaignGenerationSeed;
	GenerationSettings.bAreSettingsLoaded = true;
	GameInstance->SetCampaignGenerationSettings(GenerationSettings);
	FRTSGameDifficulty Difficulty;
	Difficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficulty(Difficulty);
	GameInstance->SetPlayerFaction(ERTSFaction::GerBreakthroughDoctrine);
	GameInstance->SetPlayerCommander(ERTSCommander::LuftwaffeLiaison);
}

AWorldPlayerController* UWorldBatteryFireCaptureSubsystem::GetWorldController() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? Cast<AWorldPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

UW_WorldMenu* UWorldBatteryFireCaptureSubsystem::FindReadyWorldMenu() const
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

void UWorldBatteryFireCaptureSubsystem::HoldCameraAgainstEdgeScroll() const
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

void UWorldBatteryFireCaptureSubsystem::Tick(const float DeltaTime)
{
	using namespace WorldBatteryFireCaptureConstants;
	if (M_Phase == EPhase::Inactive || M_Phase == EPhase::Done)
	{
		return;
	}

	M_SecondsInPhase += DeltaTime;
	HoldCameraAgainstEdgeScroll();
	AWorldPlayerController* const Controller = GetWorldController();
	switch (M_Phase)
	{
	case EPhase::WaitingForCampaign:
		if (M_SecondsInPhase < MenuBuildSeconds || not IsValid(FindReadyWorldMenu()) || not IsValid(Controller))
		{
			break;
		}
		if (not SetUpScene(*Controller))
		{
			Finish();
			break;
		}
		EnterPhase(EPhase::SettlingBeforeFire);
		break;
	case EPhase::SettlingBeforeFire:
		if (M_SecondsInPhase >= SettleSeconds)
		{
			RequestPicture(PictureBeforeFire);
			EnterPhase(EPhase::WaitingToFire);
		}
		break;
	case EPhase::WaitingToFire:
		if (M_SecondsInPhase < PictureSeconds)
		{
			break;
		}
		if (not IsValid(Controller) || not Fire(*Controller))
		{
			Finish();
			break;
		}
		EnterPhase(EPhase::Firing);
		break;
	case EPhase::Firing:
		Tick_Firing(DeltaTime);
		break;
	case EPhase::WaitingToQuit:
		if (M_SecondsInPhase >= PictureSeconds)
		{
			Finish();
		}
		break;
	default:
		break;
	}
}

bool UWorldBatteryFireCaptureSubsystem::SetUpScene(AWorldPlayerController& Controller)
{
	const AWorldBuildNode* const BatteryNode = BuildBatteryNearHQ(Controller);
	if (not IsValid(BatteryNode))
	{
		return false;
	}

	const AWorldDivisionBase* const TargetDivision = FindOrSpawnTargetDivision(Controller, *BatteryNode);
	if (not IsValid(TargetDivision))
	{
		UE_LOG(LogRTSWorldBatteryFireCapture, Error, TEXT("BATTERYFIRECAPTURE no enemy tank division could be placed"));
		return false;
	}

	M_BatteryNodeId = BatteryNode->GetBuildNodeId();
	M_TargetDivisionKey = TargetDivision->GetDivisionKey();
	RevealAndFrame(Controller, *BatteryNode, *TargetDivision);
	const AWorldTankDivision* const TankDivision = Cast<AWorldTankDivision>(TargetDivision);
	const UWorldDivisionTankFormation* const Formation = IsValid(TankDivision) ? TankDivision->GetTankFormation() : nullptr;
	const UWorldBatteryFireDirector* const Director = Controller.GetWorldBatteryFireDirector();
	UE_LOG(LogRTSWorldBatteryFireCapture, Display,
	       TEXT("BATTERYFIRECAPTURE scene: battery=%s at %s, division at %s entries=%d formation_slots=%d readiness=%s"),
	       *M_BatteryNodeId.ToString(), *BatteryNode->GetActorLocation().ToCompactString(),
	       *TargetDivision->GetActorLocation().ToCompactString(), TargetDivision->GetRemainingEntryCount(),
	       IsValid(Formation) ? Formation->GetSlotCount() : 0,
	       IsValid(Director) ? *UEnum::GetValueAsString(Director->GetBatteryReadiness(M_BatteryNodeId)) : TEXT("no director"));
	return true;
}

AWorldBuildNode* UWorldBatteryFireCaptureSubsystem::BuildBatteryNearHQ(AWorldPlayerController& Controller) const
{
	using namespace WorldBatteryFireCaptureConstants;
	UWorldBuildNodeManager* const BuildNodeManager = Controller.GetWorldBuildNodeManager();
	UWorldCampaignProgressManager* const ProgressManager = Controller.GetWorldCampaignProgressManager();
	if (not IsValid(BuildNodeManager) || not IsValid(ProgressManager))
	{
		return nullptr;
	}

	FWorldMapResources CaptureResources;
	CaptureResources.Radixite = CaptureRadixite;
	CaptureResources.Blueprints = CaptureBlueprints;
	ProgressManager->AddResources(CaptureResources);

	const FVector PlayerHQLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(&Controller);
	AWorldBuildNode* NearestBuildNode = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	const TArray<AWorldBuildNode*> BuildNodes = BuildNodeManager->GetBuildNodes();
	for (AWorldBuildNode* const BuildNode : BuildNodes)
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

	if (not IsValid(NearestBuildNode))
	{
		UE_LOG(LogRTSWorldBatteryFireCapture, Error, TEXT("BATTERYFIRECAPTURE no open build site for the battery"));
		return nullptr;
	}

	FText FailReason;
	if (not BuildNodeManager->TryBuildOnNode(NearestBuildNode->GetBuildNodeId(), EWorldCampaignBuilding::Battery,
	                                        FailReason))
	{
		UE_LOG(LogRTSWorldBatteryFireCapture, Error, TEXT("BATTERYFIRECAPTURE battery refused: %s"),
		       *FailReason.ToString());
		return nullptr;
	}
	return NearestBuildNode;
}

AWorldDivisionBase* UWorldBatteryFireCaptureSubsystem::FindOrSpawnTargetDivision(
	AWorldPlayerController& Controller, const AWorldBuildNode& BatteryNode) const
{
	using namespace WorldBatteryFireCaptureConstants;
	UWorldDivisionManager* const DivisionManager = Controller.GetWorldDivisionManager();
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not IsValid(DivisionManager) || Settings == nullptr)
	{
		return nullptr;
	}

	const FVector BatteryLocation = BatteryNode.GetActorLocation();
	for (const TObjectPtr<AWorldDivisionBase>& Division : DivisionManager->GetWorldDivisions())
	{
		const bool bIsEnemyTankDivision = IsValid(Division) && Division->IsA(AWorldTankDivision::StaticClass())
			&& Division->GetOwningPlayer() == WorldDivisionOwners::EnemyOwner;
		if (bIsEnemyTankDivision
			&& WorldBatteryFireRules::GetIsWithinRangeXY(BatteryLocation, Division->GetActorLocation(), Settings->Range))
		{
			return Division.Get();
		}
	}

	const float Distance = FMath::Min(TargetDistance, Settings->Range * 0.5f);
	const FVector SpawnLocation = BatteryLocation + TargetDirection.GetSafeNormal() * Distance;
	return DivisionManager->SpawnEnemyReactionPiece(EWorldFieldDivisions::SovietMediumArmorDivision, SpawnLocation,
	                                                FGuid::NewGuid(), ERTSGameDifficulty::Normal);
}

void UWorldBatteryFireCaptureSubsystem::RevealAndFrame(AWorldPlayerController& Controller,
                                                       const AWorldBuildNode& BatteryNode,
                                                       const AWorldDivisionBase& TargetDivision) const
{
	using namespace WorldBatteryFireCaptureConstants;
	const FVector MidPoint = (BatteryNode.GetActorLocation() + TargetDivision.GetActorLocation()) * 0.5f;
	if (AWorldFowManager* const FowManager = Controller.GetWorldFowManager())
	{
		FWorldFowRevealSource RevealSource;
		RevealSource.SourceId = RevealSourceId;
		RevealSource.WorldLocation = MidPoint;
		RevealSource.RevealRadius = RevealRadius;
		RevealSource.RevealFalloff = RevealFalloff;
		FowManager->SetRevealSource(RevealSource);
		FowManager->RefreshWorldFow();
	}

	UWorldCameraController* const CameraController = Controller.FindComponentByClass<UWorldCameraController>();
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not IsValid(CameraController) || Settings == nullptr)
	{
		return;
	}

	// The same framing the show uses, so the picture before firing and the pictures after it match.
	FMovePlayerCamera MoveRequest;
	MoveRequest.MoveToLocation = FVector(MidPoint.X, MidPoint.Y, CameraController->GetCurrentCameraLocation().Z);
	Controller.WorldCamera_MoveTo(MoveRequest);
	const float Distance = FVector::Dist2D(BatteryNode.GetActorLocation(), TargetDivision.GetActorLocation());
	CameraController->SetZoomDistance(Distance * Settings->CameraZoomPerDistance + Settings->CameraZoomPadding);
}

bool UWorldBatteryFireCaptureSubsystem::Fire(AWorldPlayerController& Controller)
{
	UWorldBatteryFireDirector* const Director = Controller.GetWorldBatteryFireDirector();
	const UWorldDivisionManager* const DivisionManager = Controller.GetWorldDivisionManager();
	if (not IsValid(Director) || not IsValid(DivisionManager))
	{
		return false;
	}

	// Without a renderer the show's timeline still runs, so a headless run exercises the whole mission.
	Director->SetPlaysShowWithoutRenderer(true);
	FText FailReason;
	const bool bDidFire = Director->RequestFireMission(M_BatteryNodeId, M_TargetDivisionKey, FailReason);
	const AWorldDivisionBase* const DivisionAfter = DivisionManager->FindDivisionByKey(M_TargetDivisionKey);
	UE_LOG(LogRTSWorldBatteryFireCapture, Display,
	       TEXT("BATTERYFIRECAPTURE fired=%d reason='%s' entries_after=%d show_playing=%d readiness_after=%s"),
	       bDidFire ? 1 : 0, *FailReason.ToString(),
	       IsValid(DivisionAfter) ? DivisionAfter->GetRemainingEntryCount() : 0, Director->GetIsShowPlaying() ? 1 : 0,
	       *UEnum::GetValueAsString(Director->GetBatteryReadiness(M_BatteryNodeId)));
	M_NextShotIndex = 0;
	return bDidFire && Director->GetIsShowPlaying();
}

void UWorldBatteryFireCaptureSubsystem::Tick_Firing(const float DeltaTime)
{
	using namespace WorldBatteryFireCaptureConstants;
	const int32 PictureCount = UE_ARRAY_COUNT(PicturesAfterFire);
	if (M_NextShotIndex >= PictureCount)
	{
		EnterPhase(EPhase::WaitingToQuit);
		return;
	}

	// One request per frame: a second request in the same frame would replace the first.
	const FTimedPicture& NextPicture = PicturesAfterFire[M_NextShotIndex];
	if (M_SecondsInPhase >= NextPicture.SecondsAfterFire)
	{
		RequestPicture(NextPicture.Name);
		++M_NextShotIndex;
	}
}

void UWorldBatteryFireCaptureSubsystem::RequestPicture(const TCHAR* PictureName) const
{
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / WorldBatteryFireCaptureConstants::OutputFolder
		/ (FString(TEXT("batteryfire_")) + PictureName + TEXT(".png"));
	if (not FApp::CanEverRender())
	{
		UE_LOG(LogRTSWorldBatteryFireCapture, Display, TEXT("BATTERYFIRECAPTURE no renderer, picture skipped: %s"),
		       *ScreenshotPath);
		return;
	}

	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogRTSWorldBatteryFireCapture, Display, TEXT("BATTERYFIRECAPTURE picture %s at %.2f s in phase"),
	       *ScreenshotPath, M_SecondsInPhase);
}

void UWorldBatteryFireCaptureSubsystem::EnterPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_SecondsInPhase = 0.f;
}

void UWorldBatteryFireCaptureSubsystem::Finish()
{
	EnterPhase(EPhase::Done);
	UE_LOG(LogRTSWorldBatteryFireCapture, Display, TEXT("BATTERYFIRECAPTURE done"));
	FPlatformMisc::RequestExit(false, TEXT("WorldCampaignBatteryFireCapture"));
}

TStatId UWorldBatteryFireCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldBatteryFireCaptureSubsystem, STATGROUP_Tickables);
}
