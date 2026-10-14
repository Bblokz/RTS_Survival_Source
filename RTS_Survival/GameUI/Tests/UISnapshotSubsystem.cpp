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
#include "RTS_Survival/GameUI/Resources/PlayerEnergyBar/W_PlayerEnergyBar.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
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
	AddGalleryWidgetsOnce();
	if (GetSecondsInPhase() >= UISnapshotConstants::AfterStartSeconds)
	{
		EnterPhase(EUISnapshotPhase::ApplyingStep);
	}
}

void UUISnapshotSubsystem::TickSettling()
{
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
	if (not FParse::Param(FCommandLine::Get(), UISnapshotConstants::HoverPortraitSwitch)
		|| not FSlateApplication::IsInitialized())
	{
		return;
	}
	TArray<UUserWidget*> UnitInfos;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), UnitInfos, UW_SelectedUnitInfo::StaticClass(), false);
	for (UUserWidget* const UnitInfo : UnitInfos)
	{
		if (not IsValid(UnitInfo) || not IsValid(UnitInfo->WidgetTree) || not UnitInfo->IsVisible())
		{
			continue;
		}
		const UBorder* const Border = Cast<UBorder>(UnitInfo->WidgetTree->FindWidget(UISnapshotConstants::UnitInfoBorderName));
		if (not IsValid(Border))
		{
			continue;
		}
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		const FVector2D HoverPosition = Border->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5));
		const FVector2D LastPosition = SlateApplication.GetCursorPos();
		SlateApplication.SetCursorPos(HoverPosition);
		const FPointerEvent MouseMove(
			SlateApplication.GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex, HoverPosition, LastPosition,
			TSet<FKey>(), EKeys::Invalid, 0, SlateApplication.GetModifierKeys());
		SlateApplication.ProcessMouseMoveEvent(MouseMove);
		return;
	}
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
		GalleryWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		Position.Y += UISnapshotConstants::GalleryRowHeight;
		UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT gallery added %s"), *ClassPath);
	}
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
		return;
	}
	SelectTrainer();
}

void UUISnapshotSubsystem::CaptureCurrentStep() const
{
	const FString StepName = GetCurrentStepName();
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("UISnapshot") / (StepName + TEXT(".png"));
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
	UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidPlayerController())
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		const bool bIsPlayerTrainer = IsValid(RTSComponent)
			&& RTSComponent->GetOwningPlayer() == UISnapshotConstants::PlayerOwnerIndex
			&& IsValid(It->FindComponentByClass<UTrainerComponent>());
		if (not bIsPlayerTrainer)
		{
			continue;
		}
		M_PlayerController->SelectSingleUnitFromUI(*It);
		UE_LOG(LogTemp, Display, TEXT("UISNAPSHOT selected trainer %s"), *It->GetName());
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("UISNAPSHOT no player trainer on this map"));
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
