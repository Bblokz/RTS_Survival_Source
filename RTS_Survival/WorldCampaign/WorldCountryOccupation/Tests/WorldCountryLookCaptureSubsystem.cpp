// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCountryLookCaptureSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowCloud.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCountryLookCapture, Log, All);

namespace WorldCountryLookCaptureConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("WorldCountryLookCapture");
	const TCHAR* const OutputFolder = TEXT("WorldCountryLookCapture");
	const TCHAR* const SaveSlotName = TEXT("WorldCountryLookCapture");
	constexpr int32 WorldCampaignGenerationSeed = 1337;
	// Time for the world to finish its first turn setup after generation.
	constexpr double CampaignReadySeconds = 3.0;
	// Longer than the front's growth, so a settled picture shows the final shape.
	constexpr double SettledSeconds = 3.4;
	// Short enough to catch a front while it spreads.
	constexpr double SpreadingSeconds = 0.9;
	constexpr double CaptureSeconds = 0.8;
	const FIntPoint CaptureResolution(1920, 1080);
	// Straight down on central Europe: Germany on the left, the Svarog Line on the right. North is up.
	const FVector CameraLocation(2350.f, -6800.f, 1800.f);
	const FRotator CameraRotation(-90.f, -90.f, 0.f);
	// A point inside Poland on the map plane, for the hover picture.
	const FVector2D PolandMapLocation(1900.f, -6800.f);
	const FName PinBerlin(TEXT("Berlin"));
	const FName PinOreMountains(TEXT("OreMountains"));
	const FName PinOderWarthe(TEXT("OderWarthe"));
	const FName CountryGermany(TEXT("Germany"));
	// Low over Germany, close enough to read the signs on its three build sites.
	const FVector BuildSiteCloseUpCameraLocation(1100.f, -6350.f, 650.f);
	const FName HoveredBuildNodeId(TEXT("Germany_01"));
	// Start height of the straight-down trace that stands in for the cursor.
	constexpr float HoverTraceHeight = 500.f;
}

bool UWorldCountryLookCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), WorldCountryLookCaptureConstants::CommandLineSwitch);
#endif
}

void UWorldCountryLookCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	using namespace WorldCountryLookCaptureConstants;
	RTSFunctionLibrary::PushModalDialogSuppression();
	// The capture generates and saves a campaign; it must never do that on the player's own slot.
	UWorldStateAndSaveManager::SetCampaignSaveSlotNameOverride(SaveSlotName);
	SeedGameInstanceForWorldCampaign();
	FSystemResolution::RequestResolutionChange(CaptureResolution.X, CaptureResolution.Y, EWindowMode::Windowed);
	EnterPhase(EPhase::WaitingForCampaign);
	UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK started"));
}

void UWorldCountryLookCaptureSubsystem::SeedGameInstanceForWorldCampaign() const
{
	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		UE_LOG(LogRTSCountryLookCapture, Warning, TEXT("COUNTRYLOOK no RTS game instance to seed"));
		return;
	}

	FCampaignGenerationSettings GenerationSettings;
	GenerationSettings.bNeedsToGenerateCampaign = true;
	GenerationSettings.GenerationSeed = WorldCountryLookCaptureConstants::WorldCampaignGenerationSeed;
	GenerationSettings.bAreSettingsLoaded = true;
	GameInstance->SetCampaignGenerationSettings(GenerationSettings);
	FRTSGameDifficulty Difficulty;
	Difficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficulty(Difficulty);
	GameInstance->SetPlayerFaction(ERTSFaction::GerBreakthroughDoctrine);
}

AWorldPlayerController* UWorldCountryLookCaptureSubsystem::FindReadyWorldController() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	AWorldPlayerController* const WorldController = Cast<AWorldPlayerController>(World->GetFirstPlayerController());
	if (not IsValid(WorldController))
	{
		return nullptr;
	}

	TArray<UUserWidget*> LoadingScreens;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, LoadingScreens, UW_AsyncWorldGeneration::StaticClass(), false);
	const UWorldStateAndSaveManager* SaveManager = WorldController->GetWorldStateAndSaveManager();
	const bool bHasReachedFirstTurn = IsValid(SaveManager) && SaveManager->GetCurrentTurn() >= 1;
	return LoadingScreens.Num() == 0 && bHasReachedFirstTurn ? WorldController : nullptr;
}

UWorldCountryOccupationRegulator* UWorldCountryLookCaptureSubsystem::FindCountryRegulator() const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	AGeneratorWorldCampaign* WorldGenerator = IsValid(WorldController) ? WorldController->GetWorldGenerator() : nullptr;
	return IsValid(WorldGenerator) ? WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>() : nullptr;
}

void UWorldCountryLookCaptureSubsystem::PrepareScene()
{
	using namespace WorldCountryLookCaptureConstants;
	UWorld* const World = GetWorld();
	AWorldPlayerController* WorldController = FindReadyWorldController();
	if (not IsValid(World) || not IsValid(WorldController))
	{
		return;
	}

	// Fog and menus would hide most of the countries; the pictures are about the ground.
	for (TActorIterator<AWorldFowCloud> CloudIterator(World); CloudIterator; ++CloudIterator)
	{
		CloudIterator->SetActorHiddenInGame(true);
	}
	UWidgetLayoutLibrary::RemoveAllWidgets(World);
	// The controller would overwrite the hover picture with the (absent) cursor every frame.
	WorldController->SetActorTickEnabled(false);

	ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(CameraLocation, CameraRotation);
	if (not IsValid(CameraActor))
	{
		return;
	}

	CameraActor->GetCameraComponent()->SetConstraintAspectRatio(false);
	WorldController->SetViewTarget(CameraActor);
}

void UWorldCountryLookCaptureSubsystem::Tick(float DeltaTime)
{
	using namespace WorldCountryLookCaptureConstants;
	switch (M_Phase)
	{
	case EPhase::WaitingForCampaign:
		if (GetSecondsInPhase() >= CampaignReadySeconds && IsValid(FindReadyWorldController()))
		{
			PrepareScene();
			EnterPhase(EPhase::ApplyingStep);
		}
		break;
	case EPhase::ApplyingStep:
		M_CurrentStepName = ApplyStep(M_StepIndex, M_SettleSeconds);
		if (M_CurrentStepName.IsEmpty())
		{
			Finish();
			break;
		}
		EnterPhase(EPhase::Settling);
		break;
	case EPhase::Settling:
		if (GetSecondsInPhase() >= M_SettleSeconds)
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

FString UWorldCountryLookCaptureSubsystem::ApplyStep(const int32 StepIndex, double& OutSettleSeconds)
{
	using namespace WorldCountryLookCaptureConstants;
	OutSettleSeconds = SettledSeconds;
	UWorldCountryOccupationRegulator* CountryRegulator = FindCountryRegulator();
	if (not IsValid(CountryRegulator))
	{
		UE_LOG(LogRTSCountryLookCapture, Error, TEXT("COUNTRYLOOK no country regulator"));
		return FString();
	}

	switch (StepIndex)
	{
	case 0:
		return TEXT("01_campaign_start");
	case 1:
		OutSettleSeconds = SpreadingSeconds;
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK Berlin cleared: %d"), ClearPinnedPlace(PinBerlin));
		return TEXT("02_berlin_front_spreading");
	case 2:
		return TEXT("03_berlin_cleared");
	case 3:
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK Ore Mountains cleared: %d"),
		       ClearPinnedPlace(PinOreMountains));
		return TEXT("04_ore_mountains_cleared");
	case 4:
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK Germany cleared targets: %d"),
		       ClearEveryTargetInCountry(CountryGermany));
		return TEXT("05_germany_free");
	case 5:
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK Oder-Warthe cleared: %d"),
		       ClearPinnedPlace(PinOderWarthe));
		return TEXT("06_poland_contested");
	case 6:
		OutSettleSeconds = SpreadingSeconds;
		CountryRegulator->SetHoveredCountryAtMapLocation(PolandMapLocation, true);
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK hovered country: %s"),
		       *CountryRegulator->GetCountryIdAtMapLocation(PolandMapLocation).ToString());
		return TEXT("07_poland_hovered");
	case 7:
		CountryRegulator->SetHoveredCountryAtMapLocation(PolandMapLocation, false);
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK build sites built: %d"),
		       BuildOnEveryOpenSiteInCountry(CountryGermany));
		return TEXT("08_build_sites_built");
	case 8:
		MoveCamera(BuildSiteCloseUpCameraLocation);
		return TEXT("09_build_sites_close_up");
	case 9:
		UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK build site hovered through a cursor-style trace: %d"),
		       HoverBuildSiteByTrace(HoveredBuildNodeId));
		return TEXT("10_build_site_hovered");
	default:
		return FString();
	}
}

bool UWorldCountryLookCaptureSubsystem::ClearAnchor(const FGuid& AnchorKey) const
{
	AWorldPlayerController* WorldController = FindReadyWorldController();
	UWorldCampaignProgressManager* ProgressManager = IsValid(WorldController)
		                                                 ? WorldController->GetWorldCampaignProgressManager()
		                                                 : nullptr;
	if (not IsValid(ProgressManager) || not AnchorKey.IsValid() || ProgressManager->GetIsAnchorCleared(AnchorKey))
	{
		return false;
	}

	FWorldPendingOperation PendingOperation;
	PendingOperation.Target = EWorldOperationTarget::Mission;
	PendingOperation.AnchorKey = AnchorKey;
	ProgressManager->BeginOperation(PendingOperation);
	FWorldMissionResult MissionResult;
	MissionResult.Outcome = EWorldMissionOutcome::Victory;
	MissionResult.SourceAnchorKey = AnchorKey;
	const FWorldOperationResolution Resolution = ProgressManager->ResolvePendingOperation(MissionResult, nullptr, 0.f);
	AWorldMapObject* MapObject = WorldController->FindWorldMapObjectByAnchorKey(AnchorKey);
	if (IsValid(MapObject))
	{
		MapObject->SetIsClearedByPlayer();
	}
	return Resolution.bWasVictory;
}

bool UWorldCountryLookCaptureSubsystem::ClearPinnedPlace(const FName PinId) const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	const AGeneratorWorldCampaign* WorldGenerator = IsValid(WorldController) ? WorldController->GetWorldGenerator() : nullptr;
	FGuid AnchorKey;
	if (not IsValid(WorldGenerator) || not WorldGenerator->TryGetAnchorKeyForPinId(PinId, AnchorKey))
	{
		UE_LOG(LogRTSCountryLookCapture, Error, TEXT("COUNTRYLOOK pin %s not found"), *PinId.ToString());
		return false;
	}
	return ClearAnchor(AnchorKey);
}

int32 UWorldCountryLookCaptureSubsystem::ClearEveryTargetInCountry(const FName CountryId) const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	const UWorldCountryOccupationRegulator* CountryRegulator = FindCountryRegulator();
	const UWorldStateAndSaveManager* SaveManager = IsValid(WorldController)
		                                               ? WorldController->GetWorldStateAndSaveManager()
		                                               : nullptr;
	if (not IsValid(CountryRegulator) || not IsValid(SaveManager))
	{
		return 0;
	}

	// Copied: clearing an anchor may refresh cached state while the list is walked.
	const TArray<FWorldCampaignAnchorSaveData> SavedAnchors = SaveManager->GetCachedWorldCampaignState().Anchors;
	int32 ClearedCount = 0;
	for (const FWorldCampaignAnchorSaveData& SavedAnchor : SavedAnchors)
	{
		const bool bIsInCountry = CountryRegulator->GetCountryIdForAnchorKey(SavedAnchor.AnchorKey) == CountryId;
		const bool bHoldsTarget = IsValid(WorldController->FindWorldMapObjectByAnchorKey(SavedAnchor.AnchorKey));
		if (bIsInCountry && bHoldsTarget && ClearAnchor(SavedAnchor.AnchorKey))
		{
			ClearedCount++;
		}
	}
	return ClearedCount;
}

int32 UWorldCountryLookCaptureSubsystem::BuildOnEveryOpenSiteInCountry(const FName CountryId) const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	const UWorldBuildNodeManager* BuildNodeManager = IsValid(WorldController)
		                                                 ? WorldController->GetWorldBuildNodeManager()
		                                                 : nullptr;
	if (not IsValid(BuildNodeManager))
	{
		return 0;
	}

	// Look only: the building goes straight on the node, without price or effect, one kind after the other.
	const TArray<EWorldCampaignBuilding> Buildings = {
		EWorldCampaignBuilding::WatchPost, EWorldCampaignBuilding::Airfield,
		EWorldCampaignBuilding::Battery, EWorldCampaignBuilding::SupplyDepot
	};
	int32 BuiltCount = 0;
	for (AWorldBuildNode* BuildNode : BuildNodeManager->GetBuildNodes())
	{
		const bool bIsOpenSiteOfCountry = IsValid(BuildNode) && BuildNode->GetCountryId() == CountryId
			&& BuildNode->GetNodeState() == EWorldBuildNodeState::Unlocked;
		if (bIsOpenSiteOfCountry)
		{
			BuildNode->SetBuilding(Buildings[BuiltCount % Buildings.Num()]);
			BuiltCount++;
		}
	}
	return BuiltCount;
}

bool UWorldCountryLookCaptureSubsystem::HoverBuildSiteByTrace(const FName BuildNodeId) const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	const UWorldBuildNodeManager* BuildNodeManager = IsValid(WorldController)
		                                                 ? WorldController->GetWorldBuildNodeManager()
		                                                 : nullptr;
	const AWorldBuildNode* ExpectedBuildNode = IsValid(BuildNodeManager) ? BuildNodeManager->GetBuildNode(BuildNodeId) : nullptr;
	if (not IsValid(ExpectedBuildNode))
	{
		return false;
	}

	// Same channel as the controller's cursor trace, so a hit here means hover and click find the site.
	const FVector SiteLocation = ExpectedBuildNode->GetActorLocation();
	const FVector TraceStart = SiteLocation + FVector(0.f, 0.f, WorldCountryLookCaptureConstants::HoverTraceHeight);
	const FVector TraceEnd = SiteLocation - FVector(0.f, 0.f, WorldCountryLookCaptureConstants::HoverTraceHeight);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility);
	AWorldBuildNode* HitBuildNode = bHit ? Cast<AWorldBuildNode>(Hit.GetActor()) : nullptr;
	if (HitBuildNode != ExpectedBuildNode)
	{
		return false;
	}

	HitBuildNode->SetIsHovered(true);
	return true;
}

void UWorldCountryLookCaptureSubsystem::MoveCamera(const FVector& CameraLocation) const
{
	const AWorldPlayerController* WorldController = FindReadyWorldController();
	AActor* ViewTarget = IsValid(WorldController) ? WorldController->GetViewTarget() : nullptr;
	if (IsValid(ViewTarget))
	{
		ViewTarget->SetActorLocation(CameraLocation);
	}
}

void UWorldCountryLookCaptureSubsystem::CaptureStep() const
{
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / WorldCountryLookCaptureConstants::OutputFolder
		/ (TEXT("countrylook_") + M_CurrentStepName + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
	UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK capture %s"), *ScreenshotPath);
}

void UWorldCountryLookCaptureSubsystem::EnterPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UWorldCountryLookCaptureSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UWorldCountryLookCaptureSubsystem::Finish()
{
	EnterPhase(EPhase::Done);
	UE_LOG(LogRTSCountryLookCapture, Display, TEXT("COUNTRYLOOK done"));
	FPlatformMisc::RequestExit(false, TEXT("WorldCountryLookCapture"));
}

TStatId UWorldCountryLookCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWorldCountryLookCaptureSubsystem, STATGROUP_Tickables);
}
