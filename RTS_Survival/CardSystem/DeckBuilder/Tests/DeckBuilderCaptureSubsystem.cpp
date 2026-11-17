// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DeckBuilderCaptureSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "RTS_Survival/CardSystem/DeckBuilder/W_DeckBuilderMenu.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/AsyncWorldGeneration/W_AsyncWorldGeneration.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerProfileAndUIManager/WorldProfileAndUIManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDeckBuilderCapture, Log, All);

namespace DeckBuilderCaptureConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("DeckBuilderCapture");
	const TCHAR* const OutputFolder = TEXT("DeckBuilderCapture");
	// The same seed the UI snapshot uses, so both look at the same campaign.
	constexpr int32 WorldCampaignGenerationSeed = 1337;
	// Time for the world menu to build after the campaign is generated.
	constexpr double MenuBuildSeconds = 2.0;
	// Time for a step's widgets to lay out and for its textures to stream in.
	constexpr double SettleSeconds = 1.0;
	// Time for the screenshot request to be written before the next step changes the screen.
	constexpr double CaptureSeconds = 0.7;
}

bool UDeckBuilderCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), DeckBuilderCaptureConstants::CommandLineSwitch);
#endif
}

void UDeckBuilderCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	SeedGameInstanceForWorldCampaign();
	EnterPhase(EPhase::WaitingForCampaign);
	UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE started"));
}

void UDeckBuilderCaptureSubsystem::SeedGameInstanceForWorldCampaign() const
{
	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		UE_LOG(LogRTSDeckBuilderCapture, Warning, TEXT("DECKCAPTURE no RTS game instance to seed"));
		return;
	}
	// The map is started directly, so the main menu never set these.
	FCampaignGenerationSettings GenerationSettings;
	GenerationSettings.bNeedsToGenerateCampaign = true;
	GenerationSettings.GenerationSeed = DeckBuilderCaptureConstants::WorldCampaignGenerationSeed;
	GenerationSettings.bAreSettingsLoaded = true;
	GameInstance->SetCampaignGenerationSettings(GenerationSettings);
	FRTSGameDifficulty Difficulty;
	Difficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficulty(Difficulty);
	GameInstance->SetPlayerFaction(ERTSFaction::GerBreakthroughDoctrine);
}

UW_WorldMenu* UDeckBuilderCaptureSubsystem::FindReadyWorldMenu() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	const AWorldPlayerController* const WorldController = Cast<AWorldPlayerController>(
		World->GetFirstPlayerController());
	if (not IsValid(WorldController))
	{
		return nullptr;
	}
	// The card data is cached once the campaign is generated, which also removes the loading screen.
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

UW_DeckBuilderMenu* UDeckBuilderCaptureSubsystem::FindDeckBuilderMenu() const
{
	const UW_WorldMenu* const WorldMenu = FindReadyWorldMenu();
	return IsValid(WorldMenu) ? WorldMenu->GetDeckBuilderMenu() : nullptr;
}

void UDeckBuilderCaptureSubsystem::Tick(float DeltaTime)
{
	using namespace DeckBuilderCaptureConstants;
	switch (M_Phase)
	{
	case EPhase::WaitingForCampaign:
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

FString UDeckBuilderCaptureSubsystem::ApplyStep(const int32 StepIndex)
{
	UW_WorldMenu* const WorldMenu = FindReadyWorldMenu();
	if (not IsValid(WorldMenu))
	{
		UE_LOG(LogRTSDeckBuilderCapture, Error, TEXT("DECKCAPTURE the world menu is gone"));
		return FString();
	}
	if (StepIndex == 0)
	{
		WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::CardMenu);
	}
	UW_DeckBuilderMenu* const Menu = WorldMenu->GetDeckBuilderMenu();
	if (not IsValid(Menu))
	{
		UE_LOG(LogRTSDeckBuilderCapture, Error, TEXT("DECKCAPTURE the world menu has no deck builder"));
		return FString();
	}
	FString StepName;
	switch (StepIndex)
	{
	case 0:
		StepName = TEXT("01_opened");
		break;
	case 1:
		Menu->HandleSectionClicked(EDeckSection::Forge);
		StepName = TEXT("02_forge");
		break;
	case 2:
		UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE slot first fitting card: %s"),
		       Menu->Debug_SlotFirstFittingCard() ? TEXT("done") : TEXT("nothing to slot"));
		StepName = TEXT("03_card_slotted");
		break;
	case 3:
		UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE attach first loose variant: %s"),
		       Menu->Debug_AttachFirstLooseVariant() ? TEXT("done") : TEXT("nothing to attach"));
		StepName = TEXT("04_variant_attached");
		break;
	case 4:
		Menu->Debug_ShowWholeCollection(true);
		StepName = TEXT("05_everything");
		break;
	case 5:
		Menu->Debug_ShowWholeCollection(false);
		Menu->HandleSectionClicked(EDeckSection::Technologies);
		StepName = TEXT("06_technologies");
		break;
	case 6:
		StepName = ApplyStep_SaveAndReopen(*Menu);
		break;
	default:
		return FString();
	}
	UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE step %s: %s"), *StepName, *Menu->Debug_DescribeState());
	return StepName;
}

FString UDeckBuilderCaptureSubsystem::ApplyStep_SaveAndReopen(UW_DeckBuilderMenu& Menu) const
{
	const AWorldPlayerController* const WorldController = Cast<AWorldPlayerController>(
		GetWorld()->GetFirstPlayerController());
	UWorldStateAndSaveManager* const SaveManager = IsValid(WorldController)
		                                               ? WorldController->GetWorldStateAndSaveManager()
		                                               : nullptr;
	UW_WorldMenu* const WorldMenu = FindReadyWorldMenu();
	if (not IsValid(SaveManager) || not IsValid(WorldMenu))
	{
		UE_LOG(LogRTSDeckBuilderCapture, Error, TEXT("DECKCAPTURE no save manager to round-trip through"));
		return TEXT("07_reopened");
	}
	Menu.HandleSectionClicked(EDeckSection::Forge);
	const FString DeckBeforeSave = Menu.Debug_DescribeState();
	// The cache only, never the save file: a capture must not replace the player's campaign save.
	SaveManager->CachePlayerCardData(Menu.Debug_BuildCardData());
	WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::World);
	WorldMenu->UpdateMenuForNewFocus(EWorldUIFocusState::CardMenu);
	const FString DeckAfterReopen = Menu.Debug_DescribeState();
	UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE roundtrip=%s"),
	       DeckBeforeSave == DeckAfterReopen ? TEXT("OK") : TEXT("MISMATCH"));
	if (DeckBeforeSave != DeckAfterReopen)
	{
		UE_LOG(LogRTSDeckBuilderCapture, Error, TEXT("DECKCAPTURE before save: %s"), *DeckBeforeSave);
		UE_LOG(LogRTSDeckBuilderCapture, Error, TEXT("DECKCAPTURE after reopen: %s"), *DeckAfterReopen);
	}
	return TEXT("07_reopened");
}

void UDeckBuilderCaptureSubsystem::CaptureStep() const
{
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / DeckBuilderCaptureConstants::OutputFolder
		/ (TEXT("deckbuilder_") + M_CurrentStepName + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE capture %s"), *ScreenshotPath);
}

void UDeckBuilderCaptureSubsystem::EnterPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UDeckBuilderCaptureSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UDeckBuilderCaptureSubsystem::Finish()
{
	EnterPhase(EPhase::Done);
	UE_LOG(LogRTSDeckBuilderCapture, Display, TEXT("DECKCAPTURE done"));
	FPlatformMisc::RequestExit(false, TEXT("DeckBuilderCapture"));
}

TStatId UDeckBuilderCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeckBuilderCaptureSubsystem, STATGROUP_Tickables);
}
