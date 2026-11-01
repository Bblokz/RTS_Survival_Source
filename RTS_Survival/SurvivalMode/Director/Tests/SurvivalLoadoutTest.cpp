// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalLoadoutTest.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyLibrary.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"
#include "RTS_Survival/SurvivalMode/Deployment/SurvivalDeploymentComponent.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalArmySelection.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalRewardPicker.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurvivalLoadoutTest, Log, All);

namespace SurvivalLoadoutTestConstants
{
	constexpr int32 PlayerOwner = 1;
	constexpr double SettleSeconds = 2.0;
	constexpr double ShotTimeoutSeconds = 15.0;
	constexpr double AfterDeploymentSeconds = 6.0;
	constexpr double AfterWavesStartSeconds = 3.0;
	constexpr double OpenShotSeconds = 2.0;
	constexpr double NoChoiceShotSeconds = 20.0;
	constexpr double ClickSeconds = 22.0;
	constexpr double AfterClickShotSeconds = 1.0;
	constexpr double BeforeIntermissionEndSeconds = 3.0;
	constexpr double AfterIntermissionEndSeconds = 2.0;
	constexpr double MaxRunSeconds = 900.0;
	constexpr int32 RosterUnitsToAdd = 3;
}

class FSurvivalLoadoutTestRunner : public TSharedFromThis<FSurvivalLoadoutTestRunner>
{
public:
	void Start(ASurvivalModeDirector* Director);

private:
	enum class EStep : uint8
	{
		WaitArmySelection,
		ArmyChecks,
		ConfirmArmy,
		WaitDeployment,
		SpawnShot,
		WaitWaves,
		RewardOpen,
		RewardNoChoice,
		RewardClick,
		RewardAfterClick,
		WaitFirstIntermissionEnd,
		UnclaimedBeforeEnd,
		UnclaimedAfterEnd,
		Finished
	};

	TWeakObjectPtr<ASurvivalModeDirector> M_Director;
	FTSTicker::FDelegateHandle M_TickerHandle;
	EStep M_Step = EStep::WaitArmySelection;
	double M_StepStartReal = 0.0;
	double M_RunStartReal = 0.0;
	FString M_Directory;
	FString M_PendingShotPath;
	double M_PendingShotStartReal = 0.0;
	bool bM_PressedStart = false;

	bool Tick(float DeltaTime);
	bool TickStep(double StepSeconds);
	void AdvanceTo(EStep Step);
	double GetRealSeconds() const;
	void TryPressStart();
	bool GetIsWaitingForShot();
	void RequestShot(const FString& FileName);

	UW_SurvivalArmySelection* FindArmySelection() const;
	TArray<UW_SurvivalRewardPicker*> FindRewardPickers() const;
	void RunArmyChecks(UW_SurvivalArmySelection& Selection) const;
	void LogPicks(const TCHAR* Label, const TArray<FSurvivalArmyPick>& Picks) const;
	void LogArmyValidity(const TArray<FSurvivalArmyPick>& Picks) const;
	void ConfirmArmyAndStart();
	void LogSpawnedUnits() const;
	void ClickRewardCard();
	void LogRewardState(const TCHAR* Label) const;
	void Finish();
};

void FSurvivalLoadoutTestRunner::Start(ASurvivalModeDirector* Director)
{
	M_Director = Director;
	M_Directory = FPaths::ProjectSavedDir() / TEXT("AgentNotes/start_loadout_review");
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalLoadoutTestDir="), M_Directory);
	IFileManager::Get().MakeDirectory(*M_Directory, true);
	// An RTS error is a modal dialog that would freeze the unattended run.
	RTSFunctionLibrary::PushModalDialogSuppression();
	M_RunStartReal = FPlatformTime::Seconds();
	AdvanceTo(EStep::WaitArmySelection);
	M_TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FSurvivalLoadoutTestRunner::Tick));
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST started dir=%s"), *M_Directory);
}

double FSurvivalLoadoutTestRunner::GetRealSeconds() const
{
	return FPlatformTime::Seconds();
}

void FSurvivalLoadoutTestRunner::AdvanceTo(const EStep Step)
{
	M_Step = Step;
	M_StepStartReal = GetRealSeconds();
}

void FSurvivalLoadoutTestRunner::TryPressStart()
{
	if (bM_PressedStart or not M_Director.IsValid())
	{
		return;
	}
	ACPPController* Controller = Cast<ACPPController>(UGameplayStatics::GetPlayerController(M_Director.Get(), 0));
	UPlayerStartGameControl* StartControl = IsValid(Controller)
		                                        ? Controller->FindComponentByClass<UPlayerStartGameControl>()
		                                        : nullptr;
	if (IsValid(StartControl) and StartControl->GetIsWaitingForPlayerToStart())
	{
		// As the player's "Press To Start" does (MapSmokeTestSubsystem::TryStartGame).
		StartControl->PlayerStartedGame();
		bM_PressedStart = true;
		UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST pressed start"));
	}
}

void FSurvivalLoadoutTestRunner::RequestShot(const FString& FileName)
{
	M_PendingShotPath = M_Directory / FileName;
	IFileManager::Get().Delete(*M_PendingShotPath, false, true, true);
	M_PendingShotStartReal = GetRealSeconds();
	FScreenshotRequest::RequestScreenshot(M_PendingShotPath, true, false);
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST shot requested %s"), *FileName);
}

bool FSurvivalLoadoutTestRunner::GetIsWaitingForShot()
{
	if (M_PendingShotPath.IsEmpty())
	{
		return false;
	}
	const bool bWritten = IFileManager::Get().FileExists(*M_PendingShotPath);
	if (not bWritten and GetRealSeconds() - M_PendingShotStartReal < SurvivalLoadoutTestConstants::ShotTimeoutSeconds)
	{
		return true;
	}
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST shot %s %s"), *FPaths::GetCleanFilename(M_PendingShotPath),
	       bWritten ? TEXT("written") : TEXT("TIMED OUT"));
	M_PendingShotPath.Reset();
	return false;
}

bool FSurvivalLoadoutTestRunner::Tick(float)
{
	if (not M_Director.IsValid())
	{
		return false;
	}
	TryPressStart();
	if (GetIsWaitingForShot())
	{
		return true;
	}
	if (GetRealSeconds() - M_RunStartReal > SurvivalLoadoutTestConstants::MaxRunSeconds)
	{
		UE_LOG(LogSurvivalLoadoutTest, Error, TEXT("LOADOUT_TEST timed out in step %d"), static_cast<int32>(M_Step));
		Finish();
		return false;
	}
	return TickStep(GetRealSeconds() - M_StepStartReal);
}

bool FSurvivalLoadoutTestRunner::TickStep(const double StepSeconds)
{
	using namespace SurvivalLoadoutTestConstants;
	ASurvivalModeDirector* Director = M_Director.Get();
	switch (M_Step)
	{
	case EStep::WaitArmySelection:
		if (IsValid(FindArmySelection()))
		{
			AdvanceTo(EStep::ArmyChecks);
		}
		return true;
	case EStep::ArmyChecks:
		if (StepSeconds >= SettleSeconds and IsValid(FindArmySelection()))
		{
			RunArmyChecks(*FindArmySelection());
			RequestShot(TEXT("01_army_selection_locked.png"));
			AdvanceTo(EStep::ConfirmArmy);
		}
		return true;
	case EStep::ConfirmArmy:
		ConfirmArmyAndStart();
		AdvanceTo(EStep::WaitDeployment);
		return true;
	case EStep::WaitDeployment:
		if (Director->GetPhase() >= ESurvivalPhase::GracePeriod and not Director->GetDeployment()->GetIsDeploying())
		{
			AdvanceTo(EStep::SpawnShot);
		}
		return true;
	case EStep::SpawnShot:
		if (StepSeconds >= AfterDeploymentSeconds)
		{
			LogSpawnedUnits();
			RequestShot(TEXT("02_spawned_at_start.png"));
			AdvanceTo(EStep::WaitWaves);
		}
		return true;
	case EStep::WaitWaves:
		if (Director->GetPhase() == ESurvivalPhase::Waves and StepSeconds >= AfterWavesStartSeconds)
		{
			Director->DebugOpenTestIntermission(1);
			AdvanceTo(EStep::RewardOpen);
		}
		return true;
	case EStep::RewardOpen:
		if (StepSeconds >= OpenShotSeconds)
		{
			LogRewardState(TEXT("open"));
			RequestShot(TEXT("03_reward_open.png"));
			AdvanceTo(EStep::RewardNoChoice);
		}
		return true;
	case EStep::RewardNoChoice:
		if (StepSeconds >= NoChoiceShotSeconds - OpenShotSeconds)
		{
			LogRewardState(TEXT("no_choice_after_20s"));
			RequestShot(TEXT("04_reward_still_open_no_choice.png"));
			AdvanceTo(EStep::RewardClick);
		}
		return true;
	case EStep::RewardClick:
		if (StepSeconds >= ClickSeconds - NoChoiceShotSeconds)
		{
			ClickRewardCard();
			AdvanceTo(EStep::RewardAfterClick);
		}
		return true;
	case EStep::RewardAfterClick:
		if (StepSeconds >= AfterClickShotSeconds)
		{
			LogRewardState(TEXT("after_click"));
			RequestShot(TEXT("05_reward_closed_after_click.png"));
			AdvanceTo(EStep::WaitFirstIntermissionEnd);
		}
		return true;
	case EStep::WaitFirstIntermissionEnd:
		if (not Director->GetIsInIntermission() and StepSeconds >= AfterIntermissionEndSeconds)
		{
			// Second intermission: nobody picks, the wave starts with the choice still open.
			Director->DebugOpenTestIntermission(2);
			AdvanceTo(EStep::UnclaimedBeforeEnd);
		}
		return true;
	case EStep::UnclaimedBeforeEnd:
		if (Director->GetIntermissionSecondsRemaining() <= BeforeIntermissionEndSeconds and StepSeconds >= 1.0)
		{
			LogRewardState(TEXT("unclaimed_before_wave"));
			RequestShot(TEXT("06_unclaimed_before_wave.png"));
			AdvanceTo(EStep::UnclaimedAfterEnd);
		}
		return true;
	case EStep::UnclaimedAfterEnd:
		if (not Director->GetIsInIntermission() and StepSeconds >= BeforeIntermissionEndSeconds
			+ AfterIntermissionEndSeconds)
		{
			LogRewardState(TEXT("unclaimed_after_wave_start"));
			RequestShot(TEXT("07_unclaimed_after_wave_start.png"));
			AdvanceTo(EStep::Finished);
		}
		return true;
	case EStep::Finished:
	default:
		Finish();
		return false;
	}
}

UW_SurvivalArmySelection* FSurvivalLoadoutTestRunner::FindArmySelection() const
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(M_Director.Get(), Widgets, UW_SurvivalArmySelection::StaticClass(),
	                                              true);
	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget) and Widget->IsInViewport())
		{
			return Cast<UW_SurvivalArmySelection>(Widget);
		}
	}
	return nullptr;
}

TArray<UW_SurvivalRewardPicker*> FSurvivalLoadoutTestRunner::FindRewardPickers() const
{
	TArray<UW_SurvivalRewardPicker*> Pickers;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(M_Director.Get(), Widgets, UW_SurvivalRewardPicker::StaticClass(),
	                                              true);
	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget) and Widget->IsInViewport())
		{
			Pickers.Add(Cast<UW_SurvivalRewardPicker>(Widget));
		}
	}
	return Pickers;
}

void FSurvivalLoadoutTestRunner::LogPicks(const TCHAR* Label, const TArray<FSurvivalArmyPick>& Picks) const
{
	int32 LockedCount = 0;
	for (const FSurvivalArmyPick& Pick : Picks)
	{
		LockedCount += Pick.Entry.bIsLocked ? 1 : 0;
		UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST %s slot %d %s (%s) locked=%d hq=%d supply=%d"), Label,
		       Pick.SlotIndex, *Pick.Entry.DisplayName.ToString(), *Pick.Entry.Unit.GetTrainingName(),
		       Pick.Entry.bIsLocked ? 1 : 0, Pick.Entry.bIsHQ ? 1 : 0, Pick.Entry.SupplyCost);
	}
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST %s picks=%d locked=%d supply=%d"), Label, Picks.Num(),
	       LockedCount, USurvivalArmyLibrary::GetTotalSupply(Picks));
}

void FSurvivalLoadoutTestRunner::RunArmyChecks(UW_SurvivalArmySelection& Selection) const
{
	const ASurvivalModeDirector* Director = M_Director.Get();
	const int32 LockedCount = USurvivalArmyLibrary::BuildLockedEntries(Director->GetSettings(),
	                                                                   Director->GetPlayerFaction()).Num();
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST faction=%s slots=%d locked=%d free=%d budget=%d"),
	       *UEnum::GetValueAsString(Director->GetPlayerFaction()), Selection.GetSlotCount(), LockedCount,
	       Selection.GetSlotCount() - LockedCount, Selection.GetSupplyBudget());
	LogPicks(TEXT("initial"), Selection.BuildPicks());
	// Every way the player can take a unit out of a slot: right-click remove, drag out, CLEAR.
	int32 DragSources = 0;
	for (int32 SlotIndex = 0; SlotIndex < LockedCount; ++SlotIndex)
	{
		Selection.HandleSlotRemoveRequested(SlotIndex);
		DragSources += IsValid(Selection.CreateDragOperationFromSlot(SlotIndex)) ? 1 : 0;
	}
	Selection.ClearArmy();
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST locked slots that gave a drag operation: %d"),
	       DragSources);
	for (int32 RosterIndex = 0; RosterIndex < SurvivalLoadoutTestConstants::RosterUnitsToAdd; ++RosterIndex)
	{
		Selection.HandleRosterEntryActivated(RosterIndex);
	}
	const TArray<FSurvivalArmyPick> Picks = Selection.BuildPicks();
	LogPicks(TEXT("after_remove_clear_add"), Picks);
	LogArmyValidity(Picks);
}

void FSurvivalLoadoutTestRunner::LogArmyValidity(const TArray<FSurvivalArmyPick>& Picks) const
{
	const ASurvivalModeDirector* Director = M_Director.Get();
	FText Reason;
	const bool bValid = USurvivalArmyLibrary::GetIsArmyValid(Director->GetSettings(), Director->GetPlayerFaction(),
	                                                         Picks, Reason);
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST army valid=%d %s"), bValid ? 1 : 0,
	       *Reason.ToString());
	// A pick set without one harvester (as a tampered UI would send) must be refused.
	TArray<FSurvivalArmyPick> WithoutHarvester = Picks;
	WithoutHarvester.RemoveAll([](const FSurvivalArmyPick& Pick) { return Pick.SlotIndex == 3; });
	const bool bValidWithout = USurvivalArmyLibrary::GetIsArmyValid(Director->GetSettings(),
	                                                                Director->GetPlayerFaction(), WithoutHarvester,
	                                                                Reason);
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST army without slot 3 valid=%d reason=%s"),
	       bValidWithout ? 1 : 0, *Reason.ToString());
}

void FSurvivalLoadoutTestRunner::ConfirmArmyAndStart()
{
	ASurvivalModeDirector* Director = M_Director.Get();
	UW_SurvivalArmySelection* Selection = FindArmySelection();
	if (IsValid(Selection))
	{
		// The CONFIRM button's own path.
		Selection->ConfirmArmy();
	}
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST confirmed army: %d picks, phase %s"),
	       Director->GetConfirmedArmy().Num(), *Global_GetSurvivalPhaseAsString(Director->GetPhase()));
	if (Director->GetPhase() != ESurvivalPhase::StartLocationSelection or Director->GetStartLocations().IsEmpty())
	{
		return;
	}
	int32 StartIndex = 0;
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalStartIndex="), StartIndex);
	StartIndex = FMath::Clamp(StartIndex, 0, Director->GetStartLocations().Num() - 1);
	Director->ConfirmStartLocation(Director->GetStartLocations()[StartIndex].Get());
}

void FSurvivalLoadoutTestRunner::LogSpawnedUnits() const
{
	const ASurvivalModeDirector* Director = M_Director.Get();
	const ASurvivalStartLocation* Start = Director->GetChosenStartLocation();
	const FVector Center = IsValid(Start) ? Start->GetDeployCenter() : FVector::ZeroVector;
	int32 Harvesters = 0;
	int32 Scavengers = 0;
	int32 Units = 0;
	float MaxDistance = 0.f;
	for (TActorIterator<AActor> It(Director->GetWorld()); It; ++It)
	{
		const bool bIsUnit = It->IsA<ATankMaster>() or It->IsA<ASquadController>();
		const URTSComponent* RTSComponent = bIsUnit ? It->FindComponentByClass<URTSComponent>() : nullptr;
		if (not RTSComponent or RTSComponent->GetOwningPlayer() != SurvivalLoadoutTestConstants::PlayerOwner)
		{
			continue;
		}
		const FString ClassName = It->GetClass()->GetName();
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Center);
		Harvesters += ClassName.Contains(TEXT("Harvester")) ? 1 : 0;
		Scavengers += ClassName.Contains(TEXT("Scaveng")) ? 1 : 0;
		MaxDistance = FMath::Max(MaxDistance, Distance);
		++Units;
		UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST unit %s at %.0f cm from the start centre"),
		       *ClassName, Distance);
	}
	UE_LOG(LogSurvivalLoadoutTest, Display,
	       TEXT("LOADOUT_TEST spawned units=%d harvesters=%d scavenger squads=%d max distance=%.0f cm start=%s"),
	       Units, Harvesters, Scavengers, MaxDistance, IsValid(Start) ? *Start->GetName() : TEXT("none"));
	Director->PreviewStartLocation(Start);
}

void FSurvivalLoadoutTestRunner::ClickRewardCard()
{
	const TArray<UW_SurvivalRewardPicker*> Pickers = FindRewardPickers();
	if (Pickers.IsEmpty() or not IsValid(Pickers[0]) or not Pickers[0]->WidgetTree)
	{
		UE_LOG(LogSurvivalLoadoutTest, Error, TEXT("LOADOUT_TEST no reward picker to click"));
		return;
	}
	TArray<UButton*> Cards;
	Pickers[0]->WidgetTree->ForEachWidget([&Cards](UWidget* Widget)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			Cards.Add(Button);
		}
	});
	TInlineComponentArray<USurvivalDeploymentComponent*> DropsBefore(M_Director.Get());
	// A double click on card 0 and a late click on card 1: only the first may count.
	for (const int32 CardIndex : {0, 0, 1})
	{
		if (Cards.IsValidIndex(CardIndex) and IsValid(Cards[CardIndex]))
		{
			Cards[CardIndex]->OnClicked.Broadcast();
		}
	}
	TInlineComponentArray<USurvivalDeploymentComponent*> DropsAfter(M_Director.Get());
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST clicked cards 0,0,1 of %d: reward drops started=%d"),
	       Cards.Num(), DropsAfter.Num() - DropsBefore.Num());
}

void FSurvivalLoadoutTestRunner::LogRewardState(const TCHAR* Label) const
{
	const TArray<UW_SurvivalRewardPicker*> Pickers = FindRewardPickers();
	FString FocusedWidget = TEXT("none");
	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget())
		{
			FocusedWidget = Focused->GetTypeAsString() + TEXT(" ") + Focused->GetReadableLocation();
		}
	}
	UE_LOG(LogSurvivalLoadoutTest, Display,
	       TEXT("LOADOUT_TEST reward %s: pickers on screen=%d chosen=%d intermission=%d (%.1f s left) phase=%s "
		       "keyboard focus=%s"),
	       Label, Pickers.Num(), M_Director->GetHasChosenReward() ? 1 : 0,
	       M_Director->GetIsInIntermission() ? 1 : 0, M_Director->GetIntermissionSecondsRemaining(),
	       *Global_GetSurvivalPhaseAsString(M_Director->GetPhase()), *FocusedWidget);
}

void FSurvivalLoadoutTestRunner::Finish()
{
	UE_LOG(LogSurvivalLoadoutTest, Display, TEXT("LOADOUT_TEST finished"));
	FTSTicker::GetCoreTicker().RemoveTicker(M_TickerHandle);
	FPlatformMisc::RequestExit(false, TEXT("SurvivalLoadoutTest"));
}

void FSurvivalLoadoutTest::StartIfRequested(ASurvivalModeDirector* Director)
{
	if (not IsValid(Director) or not FParse::Param(FCommandLine::Get(), TEXT("SurvivalLoadoutTest")))
	{
		return;
	}
	// The ticker holds a weak reference (CreateSP); a static keeps the runner alive for the run.
	static TSharedPtr<FSurvivalLoadoutTestRunner> Runner;
	Runner = MakeShared<FSurvivalLoadoutTestRunner>();
	Runner->Start(Director);
}

#endif
