// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MapSmokeTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Game/RTSGameInstance/DeveloperSettings/RTSGameInstancePIEDeveloperSettings.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TutorialOvertakeWidget/W_TutorialOvertake.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Missions/Victory/W_Victory.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMapSmokeTest, Log, All);

namespace MapSmokeTestPrivate
{
	const TCHAR* const SmokeTestSwitch = TEXT("RTSMapSmokeTest");
	const TCHAR* const MenuFlowSwitch = TEXT("RTSMenuFlowTest");
	// Survival through the real menus: SURVIVAL button, faction, variants, difficulty, then the survival map.
	const TCHAR* const SurvivalMenuFlowSwitch = TEXT("RTSSurvivalMenuFlow");
	const FName SurvivalButtonName(TEXT("ButtonSurvival"));
	constexpr float SurvivalFactionStepSeconds = 4.f;
	// The main menu widget class and its Load Campaign button, as the start game map shows them.
	const TCHAR* const StartGameWidgetClassName = TEXT("StartGameUI_C");
	// The main menu map; its controller derives from the RTS controller, so the map name tells the menu apart.
	const TCHAR* const MainMenuMapName = TEXT("StartGame");
	const FName LoadCampaignButtonName(TEXT("LoadCampaignMap"));
	// The campaign map generates its world first; a mission is launched after this and retried until it travels.
	constexpr float CampaignSettleSeconds = 20.f;
	constexpr float MissionLaunchRetrySeconds = 5.f;
	const TCHAR* const RunSecondsSwitch = TEXT("RTSMapSmokeTestSeconds=");
	constexpr float DefaultRunSeconds = 180.f;
	constexpr float HeartbeatIntervalSeconds = 30.f;
	// A run whose game time stays below this fraction of the run while this many runs of real time pass is stalled.
	constexpr float StalledGameTimeFactor = 4.f;
	constexpr float StalledGameTimeFraction = 0.1f;
	constexpr int32 CategoriesToReport = 12;
	// The difficulty picker's Normal button, as the UI snapshot tool presses it.
	const FName NormalDifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const EnsureMarker = TEXT("Ensure condition failed");
	const FName StackDumpCategory(TEXT("LogOutputDevice"));
	const TCHAR* const VictorySwitch = TEXT("RTSMapSmokeTestVictory");
	const TCHAR* const PIESettingsSwitch = TEXT("RTSMapSmokeTestPIESettings");
	// The game instance outlives the worlds; the settings are applied for the first world only.
	bool bGHasAppliedPIESettings = false;
	// The victory widget's Continue button.
	const FName VictoryContinueButtonName(TEXT("M_Continue"));
	constexpr float VictoryWidgetDelaySeconds = 3.f;
	constexpr float VictoryTravelTimeoutSeconds = 60.f;
	constexpr float VictoryArrivalSettleSeconds = 10.f;
	const FName TutorialContinueFunctionName(TEXT("OnClickedContinue"));
	// Set when Continue was pressed, so the campaign world that loads next knows it is the end of the round trip.
	bool bGIsReturningFromVictory = false;
	constexpr uint8 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;

	bool GetIsVehicleCountedUnderItsSubtype(
		const UGameUnitManager& GameUnitManager,
		const URTSComponent& RTSComponent,
		const bool bIsHumanPlayer)
	{
		if (RTSComponent.GetUnitType() == EAllUnitType::UNType_Nomadic)
		{
			const ENomadicSubtype NomadicSubtype = RTSComponent.GetSubtypeAsNomadicSubtype();
			return bIsHumanPlayer
				? GameUnitManager.GetHasPlayerNomadicOfType(NomadicSubtype)
				: GameUnitManager.GetHasEnemyNomadicOfType(NomadicSubtype);
		}
		const ETankSubtype TankSubtype = RTSComponent.GetSubtypeAsTankSubtype();
		return bIsHumanPlayer
			? GameUnitManager.GetHasPlayerTankOfType(TankSubtype)
			: GameUnitManager.GetHasEnemyTankOfType(TankSubtype);
	}

	// Logs and counts the living units that the game unit manager does not report under their own subtype.
	template <typename TUnit, typename TIsCountedUnderSubtype>
	int32 CountAndLogUncountedUnits(
		const TArray<TUnit*>& Units,
		const uint8 Player,
		const TIsCountedUnderSubtype& IsCountedUnderSubtype)
	{
		int32 Mismatches = 0;
		for (const TUnit* Unit : Units)
		{
			if (not IsValid(Unit) || not IsValid(Unit->GetRTSComponent()))
			{
				continue;
			}
			if (IsCountedUnderSubtype(*Unit->GetRTSComponent()))
			{
				continue;
			}
			++Mismatches;
			UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST counter_mismatch player=%d unit=%s"),
			       Player, *Unit->GetName());
		}
		return Mismatches;
	}
}

enum class EMapSmokeVictoryStep : uint8
{
	None,
	VictoryTriggered,
	ContinuePressed
};

/** Counts error lines per category and ensures; log lines arrive from any thread. */
class FMapSmokeTestLogCounter final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, const ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type BaseVerbosity = static_cast<ELogVerbosity::Type>(
			Verbosity & ELogVerbosity::VerbosityMask);
		const bool bIsEnsure = Message != nullptr
			&& FCString::Strstr(Message, MapSmokeTestPrivate::EnsureMarker) != nullptr;
		if (BaseVerbosity > ELogVerbosity::Error && not bIsEnsure)
		{
			return;
		}
		// The stack dump that follows each reported error arrives as many error lines of this category.
		if (Category == MapSmokeTestPrivate::StackDumpCategory && not bIsEnsure)
		{
			return;
		}
		FScopeLock Lock(&M_CountLock);
		if (bIsEnsure)
		{
			++M_EnsureCount;
		}
		if (BaseVerbosity == ELogVerbosity::Fatal)
		{
			++M_FatalCount;
		}
		++M_ErrorCount;
		++M_ErrorsPerCategory.FindOrAdd(Category);
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	void GetCounts(int32& OutErrors, int32& OutEnsures, int32& OutFatals, TMap<FName, int32>& OutPerCategory)
	{
		FScopeLock Lock(&M_CountLock);
		OutErrors = M_ErrorCount;
		OutEnsures = M_EnsureCount;
		OutFatals = M_FatalCount;
		OutPerCategory = M_ErrorsPerCategory;
	}

private:
	FCriticalSection M_CountLock;
	int32 M_ErrorCount = 0;
	int32 M_EnsureCount = 0;
	int32 M_FatalCount = 0;
	TMap<FName, int32> M_ErrorsPerCategory;
};

bool UMapSmokeTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& (FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::SmokeTestSwitch)
			|| FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::MenuFlowSwitch)
			|| FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::SurvivalMenuFlowSwitch));
}

void UMapSmokeTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ApplyPIEStartupSettingsOnce();
}

void UMapSmokeTestSubsystem::ApplyPIEStartupSettingsOnce() const
{
	using namespace MapSmokeTestPrivate;
	if (bGHasAppliedPIESettings || not FParse::Param(FCommandLine::Get(), PIESettingsSwitch))
	{
		return;
	}
	URTSGameInstance* GameInstance = Cast<URTSGameInstance>(GetWorld()->GetGameInstance());
	const URTSGameInstancePIEDeveloperSettings* PIEStartupSettings = GetDefault<URTSGameInstancePIEDeveloperSettings>();
	if (not IsValid(GameInstance) || PIEStartupSettings == nullptr)
	{
		return;
	}
	bGHasAppliedPIESettings = true;
	// The same values URTSGameInstance::ApplyPIEStartupOverrides sets in a play-in-editor session.
	FCampaignGenerationSettings CampaignGenerationSettings;
	CampaignGenerationSettings.bAreSettingsLoaded = true;
	CampaignGenerationSettings.GenerationSeed = PIEStartupSettings->CampaignGenerationSeed;
	CampaignGenerationSettings.EnemyWorldPersonality = PIEStartupSettings->EnemyWorldPersonality;
	GameInstance->SetCampaignGenerationSettings(CampaignGenerationSettings);
	FRTSGameDifficulty GameDifficulty;
	GameDifficulty.DifficultyLevel = PIEStartupSettings->DifficultyLevel;
	GameDifficulty.DifficultyPercentage = PIEStartupSettings->DifficultyPercentage;
	GameDifficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficulty(GameDifficulty);
	GameInstance->SetPlayerFaction(PIEStartupSettings->PlayerFaction);
	GameInstance->SetPlayerCommander(PIEStartupSettings->PlayerCommander);
	GameInstance->SetWorldMissionContext(PIEStartupSettings->WorldMissionContext);
	UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST applied_pie_startup_settings"));
}

void UMapSmokeTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Again here in case the world had no game instance yet when the subsystem initialised.
	ApplyPIEStartupSettingsOnce();
	M_RunSeconds = MapSmokeTestPrivate::DefaultRunSeconds;
	FParse::Value(FCommandLine::Get(), MapSmokeTestPrivate::RunSecondsSwitch, M_RunSeconds);
	M_LogCounter = MakeShared<FMapSmokeTestLogCounter>();
	GLog->AddOutputDevice(M_LogCounter.Get());
	UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST begin map=%s run_seconds=%.0f"),
	       *InWorld.GetMapName(), M_RunSeconds);
}

void UMapSmokeTestSubsystem::Deinitialize()
{
	if (M_LogCounter.IsValid())
	{
		GLog->RemoveOutputDevice(M_LogCounter.Get());
		M_LogCounter.Reset();
	}
	Super::Deinitialize();
}

void UMapSmokeTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (bM_HasFinished || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (TickReturnedFromVictory() || TickMenuFlow() || TickSurvivalMenuFlow())
	{
		return;
	}
	if (not bM_HasStartedGame)
	{
		bM_HasStartedGame = TryStartGame();
		M_GameSecondsAtStart = World->GetTimeSeconds();
		M_RealSecondsAtStart = World->GetRealTimeSeconds();
		return;
	}
	if (M_VictoryStep != static_cast<uint8>(EMapSmokeVictoryStep::None))
	{
		TickVictoryRoundTrip(World->GetRealTimeSeconds());
		return;
	}
	DismissDifficultyPicker();
	ContinueTutorialOvertakes();
	KeepGameUnpaused();
	const float SecondsRun = World->GetTimeSeconds() - M_GameSecondsAtStart;
	// The heartbeat runs on real time so a world whose game time stands still still reports why.
	const float RealSecondsRun = World->GetRealTimeSeconds() - M_RealSecondsAtStart;
	if (RealSecondsRun >= M_NextHeartbeatSeconds)
	{
		LogHeartbeat(SecondsRun, RealSecondsRun);
	}
	if (TickStuckPause(RealSecondsRun))
	{
		return;
	}
	if (SecondsRun >= M_RunSeconds)
	{
		Finish();
		return;
	}
	if (RealSecondsRun >= M_RunSeconds * MapSmokeTestPrivate::StalledGameTimeFactor
		&& SecondsRun < M_RunSeconds * MapSmokeTestPrivate::StalledGameTimeFraction)
	{
		LogResult(TEXT("smoke"), false, FString::Printf(
			          TEXT("game time stalled: %.0fs of game time in %.0fs real time"), SecondsRun, RealSecondsRun));
	}
}

TStatId UMapSmokeTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMapSmokeTestSubsystem, STATGROUP_Tickables);
}

bool UMapSmokeTestSubsystem::TickMenuFlow()
{
	if (not FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::MenuFlowSwitch))
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (Cast<AWorldPlayerController>(World->GetFirstPlayerController()) != nullptr)
	{
		LaunchFirstCampaignMission();
		return true;
	}
	if (World->GetMapName() != MapSmokeTestPrivate::MainMenuMapName)
	{
		return false;
	}
	PressLoadCampaignButton();
	return true;
}

bool UMapSmokeTestSubsystem::TickSurvivalMenuFlow()
{
	if (not FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::SurvivalMenuFlowSwitch))
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (AFactionPlayerController* FactionController = Cast<AFactionPlayerController>(World->GetFirstPlayerController()))
	{
		TickSurvivalFactionFlow(FactionController);
		return true;
	}
	if (World->GetMapName() != MapSmokeTestPrivate::MainMenuMapName)
	{
		return false;
	}
	PressSurvivalButton();
	return true;
}

void UMapSmokeTestSubsystem::PressSurvivalButton()
{
	if (bM_HasPressedSurvival)
	{
		return;
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* const Widget : Widgets)
	{
		if (not IsValid(Widget) || not IsValid(Widget->WidgetTree)
			|| Widget->GetClass()->GetName() != MapSmokeTestPrivate::StartGameWidgetClassName)
		{
			continue;
		}
		UButton* const SurvivalButton = Cast<UButton>(
			Widget->WidgetTree->FindWidget(MapSmokeTestPrivate::SurvivalButtonName));
		if (not IsValid(SurvivalButton))
		{
			continue;
		}
		bM_HasPressedSurvival = true;
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST menu_survival_pressed"));
		SurvivalButton->OnClicked.Broadcast();
		return;
	}
}

void UMapSmokeTestSubsystem::TickSurvivalFactionFlow(AFactionPlayerController* FactionController)
{
	const float RealSeconds = GetWorld()->GetRealTimeSeconds();
	if (RealSeconds < M_NextSurvivalStepSeconds)
	{
		return;
	}
	M_NextSurvivalStepSeconds = RealSeconds + MapSmokeTestPrivate::SurvivalFactionStepSeconds;
	switch (M_SurvivalFactionStep)
	{
	case 0:
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST survival_faction_chosen"));
		FactionController->HandleLaunchCampaignRequested(ERTSFaction::GerBreakthroughDoctrine);
		break;
	case 1:
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST survival_variants_confirmed"));
		FactionController->HandleUnitVariantSelectionConfirmed({});
		break;
	case 2:
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST survival_difficulty_chosen"));
		FactionController->HandleFactionDifficultyChosen(100, ERTSGameDifficulty::Normal);
		break;
	default:
		break;
	}
	++M_SurvivalFactionStep;
}

void UMapSmokeTestSubsystem::PressLoadCampaignButton()
{
	if (bM_HasPressedLoadCampaign)
	{
		return;
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* const Widget : Widgets)
	{
		if (not IsValid(Widget) || not IsValid(Widget->WidgetTree)
			|| Widget->GetClass()->GetName() != MapSmokeTestPrivate::StartGameWidgetClassName)
		{
			continue;
		}
		UButton* const LoadCampaignButton = Cast<UButton>(
			Widget->WidgetTree->FindWidget(MapSmokeTestPrivate::LoadCampaignButtonName));
		if (not IsValid(LoadCampaignButton))
		{
			continue;
		}
		bM_HasPressedLoadCampaign = true;
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST menu_load_campaign_pressed"));
		LoadCampaignButton->OnClicked.Broadcast();
		return;
	}
}

void UMapSmokeTestSubsystem::LaunchFirstCampaignMission()
{
	UWorld* World = GetWorld();
	const float WorldSeconds = World->GetTimeSeconds();
	if (WorldSeconds < MapSmokeTestPrivate::CampaignSettleSeconds || WorldSeconds < M_NextMissionLaunchSeconds)
	{
		return;
	}
	M_NextMissionLaunchSeconds = WorldSeconds + MapSmokeTestPrivate::MissionLaunchRetrySeconds;
	AWorldPlayerController* WorldController = Cast<AWorldPlayerController>(World->GetFirstPlayerController());
	TActorIterator<AWorldMissionObject> MissionIterator(World);
	if (not IsValid(WorldController) || not MissionIterator)
	{
		UE_LOG(LogRTSMapSmokeTest, Warning, TEXT("RTS_MAP_SMOKE_TEST campaign_has_no_mission_object"));
		return;
	}
	UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST campaign_mission_launch mission=%s"),
	       *MissionIterator->GetName());
	WorldController->LaunchOperationForWorldObject(*MissionIterator);
}

bool UMapSmokeTestSubsystem::TryStartGame()
{
	ACPPController* Controller = GetController();
	if (not IsValid(Controller))
	{
		return false;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST game_started"));
	return true;
}

void UMapSmokeTestSubsystem::DismissDifficultyPicker() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(MapSmokeTestPrivate::NormalDifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
			UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST picked_normal_difficulty"));
		}
	}
}

void UMapSmokeTestSubsystem::ContinueTutorialOvertakes() const
{
	TArray<UUserWidget*> Overtakes;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Overtakes, UW_TutorialOvertake::StaticClass(), false);
	for (UUserWidget* const Overtake : Overtakes)
	{
		UW_TutorialOvertake* const TutorialOvertake = Cast<UW_TutorialOvertake>(Overtake);
		if (not IsValid(TutorialOvertake) || not TutorialOvertake->IsInViewport())
		{
			continue;
		}
		// The Continue button's handler; called through reflection like the button does from Blueprint.
		UFunction* const ContinueFunction = TutorialOvertake->FindFunction(MapSmokeTestPrivate::TutorialContinueFunctionName);
		if (ContinueFunction == nullptr)
		{
			continue;
		}
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST tutorial_overtake_continued %s"),
		       *TutorialOvertake->GetClass()->GetName());
		TutorialOvertake->ProcessEvent(ContinueFunction, nullptr);
	}
}

void UMapSmokeTestSubsystem::KeepGameUnpaused() const
{
	ACPPController* Controller = GetController();
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UMapSmokeTestSubsystem::TickStuckPause(const float RealSecondsRun)
{
	constexpr float StuckPauseRealSeconds = 90.f;
	const ACPPController* Controller = GetController();
	if (not IsValid(Controller) || not Controller->GetIsGameOnPause())
	{
		M_PausedSinceRealSeconds = -1.f;
		return false;
	}
	if (M_PausedSinceRealSeconds < 0.f)
	{
		M_PausedSinceRealSeconds = RealSecondsRun;
	}
	if (RealSecondsRun - M_PausedSinceRealSeconds < StuckPauseRealSeconds)
	{
		return false;
	}
	TArray<UUserWidget*> TopLevelWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), TopLevelWidgets, UUserWidget::StaticClass(), true);
	FString WidgetNames;
	for (const UUserWidget* Widget : TopLevelWidgets)
	{
		if (IsValid(Widget) && Widget->IsInViewport() && Widget->IsVisible())
		{
			WidgetNames += Widget->GetClass()->GetName() + TEXT(",");
		}
	}
	LogResult(TEXT("smoke"), false, FString::Printf(
		          TEXT("stuck_paused for %.0fs real at %.0fs game, world_paused=%d, widgets=%s"),
		          RealSecondsRun - M_PausedSinceRealSeconds, GetWorld()->GetTimeSeconds() - M_GameSecondsAtStart,
		          GetWorld()->IsPaused() ? 1 : 0, *WidgetNames));
	return true;
}

void UMapSmokeTestSubsystem::LogHeartbeat(const float SecondsRun, const float RealSecondsRun)
{
	M_NextHeartbeatSeconds = RealSecondsRun + MapSmokeTestPrivate::HeartbeatIntervalSeconds;
	int32 PawnCount = 0;
	for (TActorIterator<APawn> PawnIterator(GetWorld()); PawnIterator; ++PawnIterator)
	{
		++PawnCount;
	}
	int32 Errors = 0;
	int32 Ensures = 0;
	int32 Fatals = 0;
	TMap<FName, int32> PerCategory;
	if (M_LogCounter.IsValid())
	{
		M_LogCounter->GetCounts(Errors, Ensures, Fatals, PerCategory);
	}
	const UWorld* World = GetWorld();
	const ACPPController* Controller = GetController();
	UE_LOG(LogRTSMapSmokeTest, Display,
	       TEXT("RTS_MAP_SMOKE_TEST heartbeat seconds=%.0f real=%.0f paused=%d dilation=%.3f controller_paused=%d pawns=%d errors=%d ensures=%d"),
	       SecondsRun, RealSecondsRun, World->IsPaused(), World->GetWorldSettings()->GetEffectiveTimeDilation(),
	       IsValid(Controller) && Controller->GetIsGameOnPause(), PawnCount, Errors, Ensures);
}

void UMapSmokeTestSubsystem::TickVictoryRoundTrip(const float WorldSeconds)
{
	using namespace MapSmokeTestPrivate;
	const float StepSeconds = WorldSeconds - M_VictoryStepStartSeconds;
	if (M_VictoryStep == static_cast<uint8>(EMapSmokeVictoryStep::VictoryTriggered))
	{
		if (StepSeconds < VictoryWidgetDelaySeconds)
		{
			return;
		}
		TArray<UUserWidget*> VictoryWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), VictoryWidgets, UW_Victory::StaticClass(), false);
		UButton* ContinueButton = nullptr;
		for (UUserWidget* const VictoryWidget : VictoryWidgets)
		{
			if (IsValid(VictoryWidget) && IsValid(VictoryWidget->WidgetTree))
			{
				ContinueButton = Cast<UButton>(VictoryWidget->WidgetTree->FindWidget(VictoryContinueButtonName));
			}
		}
		if (not IsValid(ContinueButton))
		{
			LogResult(TEXT("victory"), false, TEXT("no victory widget with a Continue button"));
			return;
		}
		bGIsReturningFromVictory = true;
		M_VictoryStep = static_cast<uint8>(EMapSmokeVictoryStep::ContinuePressed);
		M_VictoryStepStartSeconds = WorldSeconds;
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST victory_continue_pressed"));
		ContinueButton->OnClicked.Broadcast();
		return;
	}
	if (StepSeconds >= VictoryTravelTimeoutSeconds)
	{
		LogResult(TEXT("victory"), false, TEXT("Continue did not leave the mission map"));
	}
}

bool UMapSmokeTestSubsystem::TickReturnedFromVictory()
{
	using namespace MapSmokeTestPrivate;
	UWorld* World = GetWorld();
	if (not bGIsReturningFromVictory)
	{
		return false;
	}
	// Continue left the mission: the round trip passes once the next map (the campaign, or the main menu when a
	// mission names no next map) has run for a moment.
	if (World->GetRealTimeSeconds() >= VictoryArrivalSettleSeconds)
	{
		LogResult(TEXT("victory_round_trip"), true, FString::Printf(TEXT("continued to %s"), *World->GetMapName()));
	}
	return true;
}

void UMapSmokeTestSubsystem::LogResult(const FString& Scenario, const bool bPassed, const FString& Details)
{
	bM_HasFinished = true;
	int32 Errors = 0;
	int32 Ensures = 0;
	int32 Fatals = 0;
	TMap<FName, int32> PerCategory;
	if (M_LogCounter.IsValid())
	{
		M_LogCounter->GetCounts(Errors, Ensures, Fatals, PerCategory);
	}
	const bool bResultPassed = bPassed && Ensures == 0 && Fatals == 0;
	UE_LOG(LogRTSMapSmokeTest, Display,
	       TEXT("RTS_MAP_SMOKE_TEST RESULT %s scenario=%s map=%s errors=%d ensures=%d fatals=%d details=%s"),
	       bResultPassed ? TEXT("PASS") : TEXT("FAIL"), *Scenario, *GetWorld()->GetMapName(), Errors, Ensures, Fatals,
	       *Details);
	FPlatformMisc::RequestExit(false, TEXT("RTSMapSmokeTest"));
}

void UMapSmokeTestSubsystem::Finish()
{
	if (FParse::Param(FCommandLine::Get(), MapSmokeTestPrivate::VictorySwitch))
	{
		TActorIterator<AMissionManager> MissionManagerIterator(GetWorld());
		if (not MissionManagerIterator)
		{
			LogResult(TEXT("victory"), false, TEXT("no mission manager on the map"));
			return;
		}
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST victory_triggered"));
		MissionManagerIterator->TriggerVictory(false);
		M_VictoryStep = static_cast<uint8>(EMapSmokeVictoryStep::VictoryTriggered);
		M_VictoryStepStartSeconds = GetWorld()->GetRealTimeSeconds();
		return;
	}
	bM_HasFinished = true;
	int32 Errors = 0;
	int32 Ensures = 0;
	int32 Fatals = 0;
	TMap<FName, int32> PerCategory;
	if (M_LogCounter.IsValid())
	{
		M_LogCounter->GetCounts(Errors, Ensures, Fatals, PerCategory);
	}
	PerCategory.ValueSort([](const int32 Left, const int32 Right)
	{
		return Left > Right;
	});
	int32 Reported = 0;
	for (const TPair<FName, int32>& CategoryCount : PerCategory)
	{
		if (Reported++ >= MapSmokeTestPrivate::CategoriesToReport)
		{
			break;
		}
		UE_LOG(LogRTSMapSmokeTest, Display, TEXT("RTS_MAP_SMOKE_TEST category=%s errors=%d"),
		       *CategoryCount.Key.ToString(), CategoryCount.Value);
	}
	const int32 CounterMismatches = CountUnitSubtypeCounterMismatches();
	const bool bPassed = Ensures == 0 && Fatals == 0 && CounterMismatches == 0;
	UE_LOG(LogRTSMapSmokeTest, Display,
	       TEXT("RTS_MAP_SMOKE_TEST RESULT %s map=%s errors=%d ensures=%d fatals=%d counter_mismatches=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), *GetWorld()->GetMapName(), Errors, Ensures, Fatals,
	       CounterMismatches);
	FPlatformMisc::RequestExit(false, TEXT("RTSMapSmokeTest"));
}

int32 UMapSmokeTestSubsystem::CountUnitSubtypeCounterMismatches() const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return 0;
	}

	int32 Mismatches = 0;
	for (const uint8 Player : {MapSmokeTestPrivate::HumanPlayer, MapSmokeTestPrivate::EnemyPlayer})
	{
		const bool bIsHumanPlayer = Player == MapSmokeTestPrivate::HumanPlayer;
		const TArray<ATankMaster*> Vehicles = GameUnitManager->GetPlayerTanks(Player);
		Mismatches += MapSmokeTestPrivate::CountAndLogUncountedUnits(Vehicles, Player,
			[GameUnitManager, bIsHumanPlayer](const URTSComponent& RTSComponent)
			{
				return MapSmokeTestPrivate::GetIsVehicleCountedUnderItsSubtype(
					*GameUnitManager, RTSComponent, bIsHumanPlayer);
			});
		const TArray<AAircraftMaster*> Aircraft = GameUnitManager->ShippingTest_GetAircraftOfPlayer(Player);
		Mismatches += MapSmokeTestPrivate::CountAndLogUncountedUnits(Aircraft, Player,
			[GameUnitManager, bIsHumanPlayer](const URTSComponent& RTSComponent)
			{
				const EAircraftSubtype AircraftSubtype = RTSComponent.GetSubtypeAsAircraftSubtype();
				return bIsHumanPlayer
					? GameUnitManager->GetHasPlayerAircraftOfType(AircraftSubtype)
					: GameUnitManager->GetHasEnemyAircraftOfType(AircraftSubtype);
			});
		const TArray<ABuildingExpansion*> Bxps = GameUnitManager->MapTest_GetBxpsOfPlayer(Player);
		Mismatches += MapSmokeTestPrivate::CountAndLogUncountedUnits(Bxps, Player,
			[GameUnitManager, bIsHumanPlayer](const URTSComponent& RTSComponent)
			{
				const EBuildingExpansionType BxpSubtype = RTSComponent.GetSubtypeAsBxpSubtype();
				return bIsHumanPlayer
					? GameUnitManager->GetHasPlayerBxpOfType(BxpSubtype)
					: GameUnitManager->GetHasEnemyBxpOfType(BxpSubtype);
			});
	}
	return Mismatches;
}

ACPPController* UMapSmokeTestSubsystem::GetController() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? Cast<ACPPController>(World->GetFirstPlayerController()) : nullptr;
}

#else

bool UMapSmokeTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UMapSmokeTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMapSmokeTestSubsystem::ApplyPIEStartupSettingsOnce() const
{
}

void UMapSmokeTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UMapSmokeTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UMapSmokeTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UMapSmokeTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMapSmokeTestSubsystem, STATGROUP_Tickables);
}

bool UMapSmokeTestSubsystem::TickMenuFlow()
{
	return false;
}

void UMapSmokeTestSubsystem::PressLoadCampaignButton()
{
}

bool UMapSmokeTestSubsystem::TickSurvivalMenuFlow()
{
	return false;
}

void UMapSmokeTestSubsystem::PressSurvivalButton()
{
}

void UMapSmokeTestSubsystem::TickSurvivalFactionFlow(AFactionPlayerController* FactionController)
{
}

void UMapSmokeTestSubsystem::LaunchFirstCampaignMission()
{
}

bool UMapSmokeTestSubsystem::TryStartGame()
{
	return false;
}

void UMapSmokeTestSubsystem::DismissDifficultyPicker() const
{
}

void UMapSmokeTestSubsystem::ContinueTutorialOvertakes() const
{
}

void UMapSmokeTestSubsystem::KeepGameUnpaused() const
{
}

bool UMapSmokeTestSubsystem::TickStuckPause(const float RealSecondsRun)
{
	return false;
}

void UMapSmokeTestSubsystem::LogHeartbeat(const float SecondsRun, const float RealSecondsRun)
{
}

void UMapSmokeTestSubsystem::TickVictoryRoundTrip(const float WorldSeconds)
{
}

bool UMapSmokeTestSubsystem::TickReturnedFromVictory()
{
	return false;
}

void UMapSmokeTestSubsystem::LogResult(const FString& Scenario, const bool bPassed, const FString& Details)
{
}

void UMapSmokeTestSubsystem::Finish()
{
}

int32 UMapSmokeTestSubsystem::CountUnitSubtypeCounterMismatches() const
{
	return 0;
}

ACPPController* UMapSmokeTestSubsystem::GetController() const
{
	return nullptr;
}

#endif
