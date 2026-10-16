// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "UISnapshotSubsystem.h"

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Framework/Application/SlateApplication.h"
#include "RTS_Survival/GameUI/ActionUI/SelectedUnitInfo/W_SelectedUnitInfo.h"
#include "RTS_Survival/GameUI/ActionUI/ItemActionUI/W_ItemActionUI.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/W_TrainingItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/GameUI/Resources/PlayerEnergyBar/W_PlayerEnergyBar.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Behaviours/UI/BehaviourDescription/W_BehaviourDescription.h"
#include "RTS_Survival/GameUI/MiniMap/FullMapView/W_FullMapView.h"
#include "RTS_Survival/GameUI/MiniMap/CombatPings/RTSCombatPingSubsystem.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterSubsystem.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/MiniMap/MinimapTerrainCaptureComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace UISnapshotConstants
{
	// Real seconds: the game starts paused, so world time cannot pace the capture.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double AfterStartSeconds = 3.0;
	constexpr double SettleSeconds = 2.0;
	constexpr double CaptureSeconds = 2.0;
	constexpr uint8 PlayerOwnerIndex = 1;
	const FIntPoint Resolutions[] = {FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160)};
	const TCHAR* const CommandLineSwitch = TEXT("UISnapshot");
	const TCHAR* const ReflectorSnapshotCommand = TEXT("WidgetReflector.TakeSnapshot 0.2");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName EnergyProgressBarName(TEXT("EnergyProgressBar"));
	const TCHAR* const EnergyPreviewSwitch = TEXT("UISnapshotEnergyPreview");
	constexpr float EnergyPreviewPercent = 0.62f;
	const TCHAR* const HoverPortraitSwitch = TEXT("UISnapshotHoverPortrait");
	const FName UnitInfoBorderName(TEXT("UnitInfoBorder"));
	const TCHAR* const HoverActionSwitch = TEXT("UISnapshotHoverAction");
	const FName ActionButtonName(TEXT("M_ActionItemButton"));
	const TCHAR* const HoverTrainingSwitch = TEXT("UISnapshotHoverTraining");
	const FName TrainingButtonName(TEXT("M_TrainingItemButton"));
	// Extra real seconds after the start before capturing, e.g. so nomadic trucks finish converting into buildings.
	const TCHAR* const DelaySwitch = TEXT("UISnapshotDelay=");
	const TCHAR* const FocusTrainerSwitch = TEXT("UISnapshotFocusTrainer");
	// Opens this sub section tab of the trainer's training menu, e.g. 1 for the HQ's Field Defenses.
	const TCHAR* const TrainingTabSwitch = TEXT("UISnapshotTrainingTab=");
	// Unrendered skeletal meshes do not advance montages (the unit optimizer keeps resetting the tick option), so
	// nomadic conversions never finish offscreen without this.
	const TCHAR* const TickPosesSwitch = TEXT("UISnapshotTickPoses");
	// Offscreen the truck-unfold montage never ends; this makes the call its blueprint makes when it does.
	const TCHAR* const FinishTruckAnimsSwitch = TEXT("UISnapshotFinishTruckAnims");
	const FName TruckMontageFinishedFunction(TEXT("OnTruckMontageFinished"));
	constexpr double FinishTruckAnimsAfterSeconds = 1.5;
	// Hides the glass panes (ui_glass_panes.py, widget M_GlassPane) so captures with and without them compare.
	const TCHAR* const HideGlassSwitch = TEXT("UISnapshotHideGlass");
	const FName GlassPaneName(TEXT("M_GlassPane"));
	// Starts a magazine reload on every weapon after each selection so the weapon items' reload bars show.
	const TCHAR* const ReloadWeaponsSwitch = TEXT("UISnapshotReloadWeapons");
	// Presses the mission widget's collapse button once (the blueprint's OnClicked), showing the collapsed title.
	const TCHAR* const CollapseMissionSwitch = TEXT("UISnapshotCollapseMission");
	const FName MissionCollapseButtonName(TEXT("DescriptionButton"));
	const TCHAR* const OpenFullMapSwitch = TEXT("UISnapshotOpenFullMap");
	const TCHAR* const CloseFullMapAtStepSwitch = TEXT("UISnapshotCloseFullMapAtStep=");
	const FName OpenFullMapButtonName(TEXT("M_OpenFullMapButton"));
	const FName FullMapCloseButtonName(TEXT("CloseButton"));
	const TCHAR* const CombatPingsSwitch = TEXT("UISnapshotCombatPings");
	const TCHAR* const MapFiltersSwitch = TEXT("UISnapshotMapFilters");
	const TCHAR* const MapFiltersRevealAllSwitch = TEXT("UISnapshotMapFiltersRevealAll");
	// Preview pings as share of the map extent from its centre, with their age when the capture is taken.
	const FVector PreviewPingOffsets[] = {FVector(-0.32, -0.26, 0.0), FVector(0.3, -0.08, 0.0), FVector(0.06, 0.34, 0.0)};
	constexpr float PreviewPingAgesAtCapture[] = {0.3f, 0.9f, 1.7f};
	const TCHAR* const GallerySwitch = TEXT("UISnapshotGallery=");
	const FVector2D GalleryOrigin(40.0, 140.0);
	constexpr double GalleryRowHeight = 260.0;
	constexpr int32 GalleryZOrder = 1000;
}

bool UUISnapshotSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), UISnapshotConstants::CommandLineSwitch);
#endif
}

void UUISnapshotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BuildSteps();
	FModuleManager::Get().LoadModule(TEXT("SlateReflector"));
	EnterPhase(EUISnapshotPhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT started with %d captures"), M_Steps.Num());
}

void UUISnapshotSubsystem::Tick(float DeltaTime)
{
	ForceNomadicMeshesToTickPose();
	switch (M_Phase)
	{
	case EUISnapshotPhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EUISnapshotPhase::WaitingAfterStart:
		TickWaitingAfterStart();
		break;
	case EUISnapshotPhase::ApplyingStep:
		ApplyCurrentStep();
		EnterPhase(EUISnapshotPhase::Settling);
		break;
	case EUISnapshotPhase::Settling:
		TickSettling();
		break;
	case EUISnapshotPhase::WaitingForCapture:
		TickWaitingForCapture();
		break;
	default:
		break;
	}
}

void UUISnapshotSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() >= UISnapshotConstants::HudBuildSeconds && TryStartGame())
	{
		EnterPhase(EUISnapshotPhase::WaitingAfterStart);
	}
}

void UUISnapshotSubsystem::TickWaitingAfterStart()
{
	DismissDifficultyPicker();
	if (not bM_HasFinishedTruckAnims && GetSecondsInPhase() >= UISnapshotConstants::FinishTruckAnimsAfterSeconds)
	{
		bM_HasFinishedTruckAnims = true;
		FinishTruckUnfoldAnimations();
	}
	AddGalleryWidgetsOnce();
	double ExtraDelaySeconds = 0.0;
	FParse::Value(FCommandLine::Get(), UISnapshotConstants::DelaySwitch, ExtraDelaySeconds);
	if (GetSecondsInPhase() >= UISnapshotConstants::AfterStartSeconds + ExtraDelaySeconds)
	{
		EnterPhase(EUISnapshotPhase::ApplyingStep);
	}
}

void UUISnapshotSubsystem::TickSettling()
{
	HideGlassPanes();
	HoldEnergyPreview();
	HoverPortrait();
	if (GetSecondsInPhase() >= UISnapshotConstants::SettleSeconds)
	{
		CaptureCurrentStep();
		EnterPhase(EUISnapshotPhase::WaitingForCapture);
	}
}

void UUISnapshotSubsystem::TickWaitingForCapture()
{
	if (GetSecondsInPhase() >= UISnapshotConstants::CaptureSeconds)
	{
		AdvanceStepOrFinish();
	}
}

TStatId UUISnapshotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUISnapshotSubsystem, STATGROUP_Tickables);
}

void UUISnapshotSubsystem::EnterPhase(const EUISnapshotPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UUISnapshotSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UUISnapshotSubsystem::BuildSteps()
{
	M_Steps.Reset();
	for (const EUISnapshotSelection Selection : {EUISnapshotSelection::PlayerUnits, EUISnapshotSelection::Trainer})
	{
		for (const FIntPoint& Resolution : UISnapshotConstants::Resolutions)
		{
			FSnapshotStep Step;
			Step.Selection = Selection;
			Step.Resolution = Resolution;
			M_Steps.Add(Step);
		}
	}
	M_StepIndex = 0;
}

bool UUISnapshotSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
	if (not GetIsValidPlayerController())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT game started"));
	FocusCameraOnTrainer();
	return true;
}

void UUISnapshotSubsystem::HoldEnergyPreview() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::EnergyPreviewSwitch))
	{
		return;
	}
	TArray<UUserWidget*> EnergyBars;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), EnergyBars, UW_PlayerEnergyBar::StaticClass(), false);
	for (UUserWidget* const EnergyBar : EnergyBars)
	{
		if (not IsValid(EnergyBar) || not IsValid(EnergyBar->WidgetTree))
		{
			continue;
		}
		UProgressBar* const ProgressBar = Cast<UProgressBar>(
			EnergyBar->WidgetTree->FindWidget(UISnapshotConstants::EnergyProgressBarName));
		if (IsValid(ProgressBar))
		{
			ProgressBar->SetPercent(UISnapshotConstants::EnergyPreviewPercent);
		}
	}
}

void UUISnapshotSubsystem::HoverPortrait() const
{
	if (FParse::Param(FCommandLine::Get(), UISnapshotConstants::HoverPortraitSwitch))
	{
		HoverFirstVisibleWidget(UW_SelectedUnitInfo::StaticClass(), UISnapshotConstants::UnitInfoBorderName);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), UISnapshotConstants::HoverActionSwitch))
	{
		HoverFirstVisibleWidget(UW_ItemActionUI::StaticClass(), UISnapshotConstants::ActionButtonName);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), UISnapshotConstants::HoverTrainingSwitch))
	{
		HoverFirstVisibleWidget(UW_TrainingItem::StaticClass(), UISnapshotConstants::TrainingButtonName);
	}
}

void UUISnapshotSubsystem::HoverFirstVisibleWidget(UClass* const UserWidgetClass, const FName InnerWidgetName) const
{
	if (not FSlateApplication::IsInitialized())
	{
		return;
	}
	TArray<UUserWidget*> Candidates;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Candidates, UserWidgetClass, false);
	for (UUserWidget* const Candidate : Candidates)
	{
		if (not IsValid(Candidate) || not IsValid(Candidate->WidgetTree) || not Candidate->IsVisible())
		{
			continue;
		}
		const UWidget* const Target = Candidate->WidgetTree->FindWidget(InnerWidgetName);
		if (not IsValid(Target) || Target->GetCachedGeometry().GetLocalSize().IsNearlyZero())
		{
			continue;
		}
		MoveSlateCursorTo(Target->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5)));
		return;
	}
}

void UUISnapshotSubsystem::MoveSlateCursorTo(const FVector2D& AbsolutePosition)
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const FVector2D LastPosition = SlateApplication.GetCursorPos();
	SlateApplication.SetCursorPos(AbsolutePosition);
	const FPointerEvent MouseMove(
		SlateApplication.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, AbsolutePosition, LastPosition,
		TSet<FKey>(), EKeys::Invalid, 0, SlateApplication.GetModifierKeys());
	SlateApplication.ProcessMouseMoveEvent(MouseMove);
}

void UUISnapshotSubsystem::AddGalleryWidgetsOnce()
{
	FString GalleryList;
	if (bM_HasAddedGallery || not FParse::Value(FCommandLine::Get(), UISnapshotConstants::GallerySwitch, GalleryList))
	{
		return;
	}
	bM_HasAddedGallery = true;
	TArray<FString> WidgetPaths;
	GalleryList.ParseIntoArray(WidgetPaths, TEXT("+"));
	FVector2D Position = UISnapshotConstants::GalleryOrigin;
	for (const FString& WidgetPath : WidgetPaths)
	{
		const FString ClassPath = WidgetPath + TEXT(".") + FPaths::GetBaseFilename(WidgetPath) + TEXT("_C");
		UClass* const WidgetClass = LoadClass<UUserWidget>(nullptr, *ClassPath);
		if (not IsValid(WidgetClass))
		{
			UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT gallery class not found: %s"), *ClassPath);
			continue;
		}
		UUserWidget* const GalleryWidget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
		if (not IsValid(GalleryWidget))
		{
			continue;
		}
		GalleryWidget->AddToViewport(UISnapshotConstants::GalleryZOrder);
		GalleryWidget->SetPositionInViewport(Position, false);
		ShowCollapsedGalleryParts(*GalleryWidget);
		SetupSampleBehaviourDescription(*GalleryWidget);
		Position.Y += UISnapshotConstants::GalleryRowHeight;
		UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT gallery added %s"), *ClassPath);
	}
}

void UUISnapshotSubsystem::SetupSampleBehaviourDescription(UUserWidget& GalleryWidget)
{
	UW_BehaviourDescription* const Description = Cast<UW_BehaviourDescription>(&GalleryWidget);
	if (not IsValid(Description))
	{
		return;
	}
	const bool bIsDebuffSample = M_BehaviourSampleIndex % 2 == 1;
	++M_BehaviourSampleIndex;
	FBehaviourUIData SampleData;
	if (bIsDebuffSample)
	{
		SampleData.TitleText = TEXT("Suppressed");
		SampleData.DescriptionText = TEXT("Loses <Text_Bad>-30%</> movement speed and <Text_Bad>-15%</> weapon accuracy");
		SampleData.LifeTimeType = EBehaviourLifeTime::Timed;
		SampleData.TotalLifeTime = 8.0f;
		SampleData.BuffDebuffType = EBuffDebuffType::Debuff;
	}
	else
	{
		SampleData.TitleText = TEXT("Veterancy I");
		SampleData.DescriptionText = TEXT("Gains <Text_Accent>+20%</> turret rotation speed and <Text_Accent>+10%</> weapon accuracy");
		SampleData.LifeTimeType = EBehaviourLifeTime::Permanent;
		SampleData.BuffDebuffType = EBuffDebuffType::Buff;
	}
	Description->SetupDescription(SampleData);
}

void UUISnapshotSubsystem::ShowCollapsedGalleryParts(UUserWidget& GalleryWidget)
{
	// Widgets such as training items collapse parts of themselves until gameplay data arrives; show their design.
	GalleryWidget.SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (not IsValid(GalleryWidget.WidgetTree))
	{
		return;
	}
	GalleryWidget.WidgetTree->ForEachWidget([](UWidget* const Widget)
	{
		if (IsValid(Widget) && not Widget->IsVisible())
		{
			// buttons stay hoverable (hover captures); everything else must not catch the cursor
			Widget->SetVisibility(Widget->IsA<UButton>() ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
		}
	});
}

void UUISnapshotSubsystem::DismissDifficultyPicker() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree))
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(Picker->WidgetTree->FindWidget(UISnapshotConstants::DifficultyButtonName));
		if (IsValid(NormalButton) && Picker->IsInViewport())
		{
			NormalButton->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT picked normal difficulty"));
		}
	}
}

void UUISnapshotSubsystem::ExportMinimapTerrain() const
{
	const ACPPGameState* const GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		return;
	}
	const UMinimapTerrainCaptureComponent* const TerrainCapture = GameState->GetMinimapTerrainCaptureComponent();
	if (not IsValid(TerrainCapture))
	{
		return;
	}
	UTextureRenderTarget2D* const TerrainRenderTarget = TerrainCapture->GetMinimapTerrainRenderTarget();
	if (not IsValid(TerrainRenderTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT no minimap terrain render target"));
		return;
	}
	UKismetRenderingLibrary::ExportRenderTarget(
		GetWorld(), TerrainRenderTarget, FPaths::ProjectSavedDir() / TEXT("UISnapshot"), TEXT("minimap_terrain"));
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT exported minimap terrain %dx%d"),
	       TerrainRenderTarget->SizeX, TerrainRenderTarget->SizeY);
}

void UUISnapshotSubsystem::ApplyCurrentStep()
{
	if (not M_Steps.IsValidIndex(M_StepIndex))
	{
		Finish();
		return;
	}
	const FSnapshotStep& Step = M_Steps[M_StepIndex];
	FSystemResolution::RequestResolutionChange(Step.Resolution.X, Step.Resolution.Y, EWindowMode::Windowed);
	if (Step.Selection == EUISnapshotSelection::PlayerUnits)
	{
		SelectPlayerUnits();
	}
	else
	{
		SelectTrainer();
	}
	StartWeaponReloads();
	CollapseMissionOnce();
	// Filters first, so the tactical map's toggles show the modes when it opens.
	ShowMapObjectFiltersForStep();
	ToggleFullMapForStep();
	ShowCombatPingsForStep();
}

void UUISnapshotSubsystem::CaptureCurrentStep() const
{
	const FString StepName = GetCurrentStepName();
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("UISnapshot") / (StepName + TEXT(".png"));
	LogMapObjectFilterCounts();
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	IConsoleManager::Get().ProcessUserConsoleInput(UISnapshotConstants::ReflectorSnapshotCommand, *GLog, GetWorld());
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT capture %s -> %s"), *StepName, *ScreenshotPath);
}

void UUISnapshotSubsystem::AdvanceStepOrFinish()
{
	++M_StepIndex;
	if (M_Steps.IsValidIndex(M_StepIndex))
	{
		EnterPhase(EUISnapshotPhase::ApplyingStep);
		return;
	}
	Finish();
}

void UUISnapshotSubsystem::Finish()
{
	EnterPhase(EUISnapshotPhase::Done);
	ExportMinimapTerrain();
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT done"));
	FPlatformMisc::RequestExit(false, TEXT("UISnapshot"));
}

void UUISnapshotSubsystem::SelectPlayerUnits() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidPlayerController())
	{
		return;
	}
	TArray<ASquadUnit*> SquadUnits;
	TArray<ASelectablePawnMaster*> Pawns;
	for (TActorIterator<ASelectablePawnMaster> It(World); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == UISnapshotConstants::PlayerOwnerIndex)
		{
			Pawns.Add(*It);
		}
	}
	for (TActorIterator<ASquadUnit> It(World); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == UISnapshotConstants::PlayerOwnerIndex)
		{
			SquadUnits.Add(*It);
		}
	}
	M_PlayerController->SelectUnitsFromMarquee(SquadUnits, {}, Pawns);
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT selected %d pawns and %d soldiers"), Pawns.Num(), SquadUnits.Num());
}

void UUISnapshotSubsystem::SelectTrainer() const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	AActor* const Trainer = FindPlayerTrainer();
	if (not IsValid(Trainer))
	{
		UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT no player trainer on this map"));
		return;
	}
	M_PlayerController->SelectSingleUnitFromUI(Trainer);
	const ANomadicVehicle* const NomadicTrainer = Cast<ANomadicVehicle>(Trainer);
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT selected trainer %s nomad status %d"), *Trainer->GetName(),
	       NomadicTrainer ? static_cast<int32>(NomadicTrainer->GetNomadicStatus()) : -1);
	ShowTrainingTab();
}

void UUISnapshotSubsystem::ShowTrainingTab() const
{
	int32 TrainingTabIndex = INDEX_NONE;
	if (not FParse::Value(FCommandLine::Get(), UISnapshotConstants::TrainingTabSwitch, TrainingTabIndex)
		|| not GetIsValidPlayerController())
	{
		return;
	}
	const UMainGameUI* const MainGameUI = M_PlayerController->GetMainMenuUI();
	UTrainingMenuManager* const TrainingMenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (not IsValid(TrainingMenuManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT no training menu manager to show tab %d"), TrainingTabIndex);
		return;
	}
	TrainingMenuManager->OnSubSectionTabClicked(TrainingTabIndex);
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT showing training tab %d"), TrainingTabIndex);
}

void UUISnapshotSubsystem::ForceNomadicMeshesToTickPose() const
{
	if (M_Phase == EUISnapshotPhase::Inactive || not FParse::Param(FCommandLine::Get(), UISnapshotConstants::TickPosesSwitch))
	{
		return;
	}
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (TActorIterator<ANomadicVehicle> It(World); It; ++It)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(*It);
		for (USkeletalMeshComponent* const Mesh : Meshes)
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
}

void UUISnapshotSubsystem::ToggleFullMapForStep()
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::OpenFullMapSwitch))
	{
		return;
	}
	if (not bM_HasOpenedFullMap)
	{
		TArray<UUserWidget*> MainGameUIs;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), MainGameUIs, UMainGameUI::StaticClass(), false);
		for (UUserWidget* const MainGameUI : MainGameUIs)
		{
			if (ClickGlassButtonIn(MainGameUI, UISnapshotConstants::OpenFullMapButtonName))
			{
				bM_HasOpenedFullMap = true;
				UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT pressed the map button"));
				break;
			}
		}
	}
	int32 CloseAtStep = INDEX_NONE;
	if (not FParse::Value(FCommandLine::Get(), UISnapshotConstants::CloseFullMapAtStepSwitch, CloseAtStep)
		|| CloseAtStep != M_StepIndex)
	{
		return;
	}
	TArray<UUserWidget*> FullMapViews;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FullMapViews, UW_FullMapView::StaticClass(), false);
	for (UUserWidget* const FullMapView : FullMapViews)
	{
		if (ClickGlassButtonIn(FullMapView, UISnapshotConstants::FullMapCloseButtonName))
		{
			UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT pressed the full map close button"));
		}
	}
}

bool UUISnapshotSubsystem::ClickGlassButtonIn(UUserWidget* const OwnerWidget, const FName& GlassButtonName) const
{
	if (not IsValid(OwnerWidget) || not IsValid(OwnerWidget->WidgetTree))
	{
		return false;
	}
	const UUserWidget* const GlassButton = Cast<UUserWidget>(OwnerWidget->WidgetTree->FindWidget(GlassButtonName));
	if (not IsValid(GlassButton) || not IsValid(GlassButton->WidgetTree))
	{
		return false;
	}
	UButton* InnerButton = nullptr;
	GlassButton->WidgetTree->ForEachWidget([&InnerButton](UWidget* const Widget)
	{
		if (InnerButton == nullptr)
		{
			InnerButton = Cast<UButton>(Widget);
		}
	});
	if (not IsValid(InnerButton))
	{
		return false;
	}
	InnerButton->OnClicked.Broadcast();
	return true;
}

void UUISnapshotSubsystem::CollapseMissionOnce()
{
	if (bM_HasCollapsedMission || not FParse::Param(FCommandLine::Get(), UISnapshotConstants::CollapseMissionSwitch))
	{
		return;
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* const Widget : Widgets)
	{
		if (not IsValid(Widget) || not IsValid(Widget->WidgetTree))
		{
			continue;
		}
		UButton* const CollapseButton = Cast<UButton>(Widget->WidgetTree->FindWidget(UISnapshotConstants::MissionCollapseButtonName));
		if (IsValid(CollapseButton) && CollapseButton->IsVisible())
		{
			CollapseButton->OnClicked.Broadcast();
			bM_HasCollapsedMission = true;
			UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT collapsed mission %s"), *Widget->GetName());
		}
	}
}

void UUISnapshotSubsystem::StartWeaponReloads() const
{
#if !UE_BUILD_SHIPPING
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::ReloadWeaponsSwitch))
	{
		return;
	}
	const UWorld* const World = GetWorld();
	int32 ReloadCount = 0;
	for (TObjectIterator<UWeaponState> It; It; ++It)
	{
		if (It->GetWorld() == World && not It->GetIsReloading())
		{
			It->Debug_StartReload();
			ReloadCount += It->GetIsReloading() ? 1 : 0;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT started %d weapon reloads"), ReloadCount);
#endif
}

void UUISnapshotSubsystem::HideGlassPanes() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::HideGlassSwitch))
	{
		return;
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* const Widget : Widgets)
	{
		if (not IsValid(Widget) || not IsValid(Widget->WidgetTree))
		{
			continue;
		}
		if (UWidget* const GlassPane = Widget->WidgetTree->FindWidget(UISnapshotConstants::GlassPaneName))
		{
			GlassPane->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UUISnapshotSubsystem::FinishTruckUnfoldAnimations() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World) || not FParse::Param(FCommandLine::Get(), UISnapshotConstants::FinishTruckAnimsSwitch))
	{
		return;
	}
	for (TActorIterator<ANomadicVehicle> It(World); It; ++It)
	{
		if (It->GetNomadicStatus() != ENomadStatus::CreatingBuildingTruckAnim)
		{
			continue;
		}
		UFunction* const MontageFinished = It->FindFunction(UISnapshotConstants::TruckMontageFinishedFunction);
		if (MontageFinished == nullptr)
		{
			continue;
		}
		It->ProcessEvent(MontageFinished, nullptr);
		UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT finished truck unfold of %s"), *It->GetName());
	}
}

AActor* UUISnapshotSubsystem::FindPlayerTrainer() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		const bool bIsPlayerTrainer = IsValid(RTSComponent)
			&& RTSComponent->GetOwningPlayer() == UISnapshotConstants::PlayerOwnerIndex
			&& IsValid(It->FindComponentByClass<UTrainerComponent>());
		if (bIsPlayerTrainer)
		{
			return *It;
		}
	}
	return nullptr;
}

void UUISnapshotSubsystem::FocusCameraOnTrainer() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::FocusTrainerSwitch) || not GetIsValidPlayerController())
	{
		return;
	}
	// Nomadic trucks only finish converting into buildings while they are rendered.
	if (const AActor* const Trainer = FindPlayerTrainer())
	{
		M_PlayerController->FocusCameraOnActor(Trainer);
	}
}

FString UUISnapshotSubsystem::GetCurrentStepName() const
{
	if (not M_Steps.IsValidIndex(M_StepIndex))
	{
		return TEXT("invalid");
	}
	const FSnapshotStep& Step = M_Steps[M_StepIndex];
	const TCHAR* const SelectionName = Step.Selection == EUISnapshotSelection::PlayerUnits ? TEXT("units") : TEXT("trainer");
	return FString::Printf(TEXT("%s_%dp"), SelectionName, Step.Resolution.Y);
}

bool UUISnapshotSubsystem::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UUISnapshotSubsystem::GetIsValidPlayerController"),
		this
	);
	return false;
}

void UUISnapshotSubsystem::ShowCombatPingsForStep() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::CombatPingsSwitch))
	{
		return;
	}
	UWorld* const World = GetWorld();
	const AFowManager* const FowManager = FRTS_Statics::GetFowManager(this);
	URTSCombatPingSubsystem* const CombatPingSubsystem = IsValid(World)
		                                                     ? World->GetSubsystem<URTSCombatPingSubsystem>()
		                                                     : nullptr;
	if (not IsValid(CombatPingSubsystem) || not IsValid(FowManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT combat pings: no ping subsystem or fog of war manager"));
		return;
	}
	const int32 PingsBefore = CombatPingSubsystem->GetPings().Num();
	int32 ReportedHits = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const URTSComponent* const RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == UISnapshotConstants::PlayerOwnerIndex)
		{
			CombatPingSubsystem->ReportUnitHit(*RTSComponent);
			++ReportedHits;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT combat pings: %d hits reported, %d pings kept before, %d after"),
	       ReportedHits, PingsBefore, CombatPingSubsystem->GetPings().Num());

	const FVector MapCenter = FowManager->GetActorLocation();
	const float MapExtent = FowManager->GetMapExtent();
	constexpr int32 PreviewPingCount = UE_ARRAY_COUNT(UISnapshotConstants::PreviewPingAgesAtCapture);
	for (int32 PreviewIndex = 0; PreviewIndex < PreviewPingCount; ++PreviewIndex)
	{
		const float AgeNowSeconds = UISnapshotConstants::PreviewPingAgesAtCapture[PreviewIndex]
			- static_cast<float>(UISnapshotConstants::SettleSeconds);
		CombatPingSubsystem->AddPreviewPing(
			MapCenter + UISnapshotConstants::PreviewPingOffsets[PreviewIndex] * MapExtent, AgeNowSeconds);
	}
}

void UUISnapshotSubsystem::ShowMapObjectFiltersForStep() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::MapFiltersSwitch))
	{
		return;
	}
	const UWorld* const World = GetWorld();
	URTSMapObjectFilterSubsystem* const MapObjectFilters = IsValid(World)
		                                                       ? World->GetSubsystem<URTSMapObjectFilterSubsystem>()
		                                                       : nullptr;
	if (not IsValid(MapObjectFilters))
	{
		UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT map filters: no map object filter subsystem"));
		return;
	}
	// -UISnapshotMapFiltersRevealAll shows every object, so the dots can be reviewed before anything is explored.
	if (FParse::Param(FCommandLine::Get(), UISnapshotConstants::MapFiltersRevealAllSwitch))
	{
		IConsoleManager::Get().ProcessUserConsoleInput(TEXT("RTS.MapObjectFilters.RevealAll 1"), *GLog, GetWorld());
	}
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		MapObjectFilters->SetFilterMode(Filter, ERTSMapObjectFilterMode::MiniMapAndTacticalMap);
	}
}

void UUISnapshotSubsystem::LogMapObjectFilterCounts() const
{
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::MapFiltersSwitch))
	{
		return;
	}
	const UWorld* const World = GetWorld();
	const URTSMapObjectFilterSubsystem* const MapObjectFilters = IsValid(World)
		                                                             ? World->GetSubsystem<URTSMapObjectFilterSubsystem>()
		                                                             : nullptr;
	if (not IsValid(MapObjectFilters))
	{
		return;
	}
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		int32 RegisteredCount = 0;
		int32 DiscoveredCount = 0;
		MapObjectFilters->GetObjectCounts(Filter, RegisteredCount, DiscoveredCount);
		UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT map filter %s: %d registered, %d discovered, %d dots"),
		       *UEnum::GetValueAsString(Filter), RegisteredCount, DiscoveredCount,
		       MapObjectFilters->GetDiscoveredObjectUVs(Filter).Num());
	}
}
