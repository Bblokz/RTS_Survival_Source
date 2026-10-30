// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalModeDirector.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyLibrary.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalRewardLibrary.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeSave.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Deployment/SurvivalDeploymentComponent.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseComponent.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalArmySelection.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalHud.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalRewardPicker.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalStartLocationPicker.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalEdgeSpawnZone.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveDirectorComponent.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveTypes.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalModeDirectorConstants
{
	constexpr int32 PlayerOwner = 1;
	constexpr float DefeatCheckInterval = 2.f;
	constexpr float WaitForControllerInterval = 0.25f;
	constexpr int32 SetupWidgetZOrder = 20;
	constexpr int32 HudWidgetZOrder = 5;
}

ASurvivalModeDirector::ASurvivalModeDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	M_Deployment = CreateDefaultSubobject<USurvivalDeploymentComponent>(TEXT("Deployment"));
	M_WaveDirector = CreateDefaultSubobject<USurvivalWaveDirectorComponent>(TEXT("WaveDirector"));
	M_SovietBases = CreateDefaultSubobject<USurvivalSovietBaseComponent>(TEXT("SovietBases"));
	RewardPickerWidgetClass = UW_SurvivalRewardPicker::StaticClass();
}

ASurvivalModeDirector* ASurvivalModeDirector::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not World)
	{
		return nullptr;
	}
	for (TActorIterator<ASurvivalModeDirector> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

void ASurvivalModeDirector::BeginPlay()
{
	Super::BeginPlay();
	if (not GetIsValidSettings())
	{
		return;
	}
	M_GraceSeconds = Settings->GracePeriodSeconds;
#if RTS_WITH_MAP_TESTS
	// Headless test runs: -SurvivalAutoStart deploys the debug army at the first start location, -SurvivalGrace=N.
	if (FParse::Param(FCommandLine::Get(), TEXT("SurvivalAutoStart")))
	{
		bDebugAutoStart = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalGrace="), DebugGraceSecondsOverride);
	// Unattended test runs log RTS errors instead of stopping the game on a modal dialog per error.
	if (bDebugAutoStart)
	{
		RTSFunctionLibrary::PushModalDialogSuppression();
		bM_HasPushedDialogSuppression = true;
	}
#endif
	BeginPlay_GatherLevelActors();
	M_Deployment->InitDeployment(this);
	M_WaveDirector->InitWaves(this);
	M_WaveIncomingHandle = M_WaveDirector->OnWaveIncoming.AddUObject(this, &ASurvivalModeDirector::OnWaveIncoming);
	M_RoundSurvivedHandle = M_WaveDirector->OnRoundSurvived.AddUObject(this, &ASurvivalModeDirector::OnRoundSurvived);
	M_RewardRandom.Initialize(FMath::Rand());
	M_SovietBases->InitSovietBases(this);
	BeginPlay_WaitForPlayerController();
}

void ASurvivalModeDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_GraceTimerHandle);
		World->GetTimerManager().ClearTimer(M_DefeatCheckTimerHandle);
		World->GetTimerManager().ClearTimer(M_WaitForControllerHandle);
		World->GetTimerManager().ClearTimer(M_IntermissionTimerHandle);
	}
	if (bM_HasPushedDialogSuppression)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_HasPushedDialogSuppression = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ASurvivalModeDirector::BeginPlay_GatherLevelActors()
{
	M_StartLocations.Reset();
	M_EdgeSpawnZones.Reset();
	for (TActorIterator<ASurvivalStartLocation> It(GetWorld()); It; ++It)
	{
		M_StartLocations.Add(*It);
	}
	// A stable order so the picker cycles the same way every run.
	M_StartLocations.Sort([](const TWeakObjectPtr<ASurvivalStartLocation>& A,
	                         const TWeakObjectPtr<ASurvivalStartLocation>& B)
	{
		return A->GetName() < B->GetName();
	});
	for (TActorIterator<ASurvivalEdgeSpawnZone> It(GetWorld()); It; ++It)
	{
		M_EdgeSpawnZones.Add(*It);
	}
	if (M_StartLocations.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("ASurvivalModeDirector: the level has no ASurvivalStartLocation."));
	}
	if (M_EdgeSpawnZones.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("ASurvivalModeDirector: the level has no ASurvivalEdgeSpawnZone."));
	}
}

void ASurvivalModeDirector::BeginPlay_WaitForPlayerController()
{
	ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController))
	{
		// The controller spawns after the level actors; try again shortly.
		GetWorld()->GetTimerManager().SetTimer(M_WaitForControllerHandle, this,
		                                       &ASurvivalModeDirector::BeginPlay_WaitForPlayerController,
		                                       SurvivalModeDirectorConstants::WaitForControllerInterval, false);
		return;
	}
	PlayerController->OnMainMenuCallbacks.CallbackOnMenuReady(&ASurvivalModeDirector::StartSetup, this);
}

void ASurvivalModeDirector::StartSetup()
{
	if (M_Phase != ESurvivalPhase::None)
	{
		return;
	}
	if (bDebugAutoStart)
	{
		DebugAutoStart();
		return;
	}
	ShowArmySelection();
}

void ASurvivalModeDirector::SetPhase(const ESurvivalPhase NewPhase)
{
	if (NewPhase == M_Phase)
	{
		return;
	}
	const ESurvivalPhase OldPhase = M_Phase;
	M_Phase = NewPhase;
	UE_LOG(LogTemp, Log, TEXT("Survival phase %s -> %s"), *Global_GetSurvivalPhaseAsString(OldPhase),
	       *Global_GetSurvivalPhaseAsString(NewPhase));
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);
}

float ASurvivalModeDirector::GetElapsedSurvivalSeconds() const
{
	if (M_WavesStartTime < 0.0 or not GetWorld())
	{
		return 0.f;
	}
	const double EndTime = M_DefeatTime >= 0.0 ? M_DefeatTime : GetWorld()->GetTimeSeconds();
	return static_cast<float>(EndTime - M_WavesStartTime);
}

float ASurvivalModeDirector::GetGraceSecondsRemaining() const
{
	if (M_Phase != ESurvivalPhase::GracePeriod or M_GraceStartTime < 0.0 or not GetWorld())
	{
		return 0.f;
	}
	const double Elapsed = GetWorld()->GetTimeSeconds() - M_GraceStartTime;
	return FMath::Max(0.f, M_GraceSeconds - static_cast<float>(Elapsed));
}

FVector ASurvivalModeDirector::GetCurrentPlayerBaseLocation() const
{
	if (M_HQ.IsValid() and RTSFunctionLibrary::RTSIsValid(M_HQ.Get()))
	{
		return M_HQ->GetActorLocation();
	}
	FVector BuildingSum = FVector::ZeroVector;
	int32 BuildingCount = 0;
	FVector UnitSum = FVector::ZeroVector;
	int32 UnitCount = 0;
	for (TActorIterator<ASelectablePawnMaster> It(GetWorld()); It; ++It)
	{
		ASelectablePawnMaster* Pawn = *It;
		const URTSComponent* RTSComponent = IsValid(Pawn) ? Pawn->GetRTSComponent() : nullptr;
		if (not RTSComponent or RTSComponent->GetOwningPlayer() != SurvivalModeDirectorConstants::PlayerOwner
			or not RTSFunctionLibrary::RTSIsValid(Pawn))
		{
			continue;
		}
		// Aircraft circle the map (and drop ships are only passing through); they do not mark the base.
		if (Pawn->IsA<AAircraftMaster>())
		{
			continue;
		}
		if (Pawn->IsA<ANomadicVehicle>())
		{
			BuildingSum += Pawn->GetActorLocation();
			++BuildingCount;
			continue;
		}
		UnitSum += Pawn->GetActorLocation();
		++UnitCount;
	}
	if (BuildingCount > 0)
	{
		return BuildingSum / BuildingCount;
	}
	if (UnitCount > 0)
	{
		return UnitSum / UnitCount;
	}
	if (M_ChosenStartLocation.IsValid())
	{
		return M_ChosenStartLocation->GetDeployCenter();
	}
	return GetActorLocation();
}

ARTSAsyncSpawner* ASurvivalModeDirector::FindAsyncSpawner() const
{
	if (ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this))
	{
		return AsyncSpawner;
	}
	for (TActorIterator<ARTSAsyncSpawner> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

FRTSGameDifficulty ASurvivalModeDirector::GetDifficulty() const
{
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		return FRTSGameDifficulty();
	}
	FRTSGameDifficulty Difficulty = GameInstance->GetSelectedGameDifficulty();
	if (not Difficulty.bIsInitialized)
	{
		// PIE straight into the map: play Normal.
		Difficulty.DifficultyLevel = ERTSGameDifficulty::Normal;
		Difficulty.DifficultyPercentage = 100;
		Difficulty.bIsInitialized = true;
	}
	return Difficulty;
}

ERTSFaction ASurvivalModeDirector::GetPlayerFaction() const
{
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance) or not GameInstance->GetHasPlayerFaction())
	{
		return ERTSFaction::GerBreakthroughDoctrine;
	}
	return GameInstance->GetPlayerFaction();
}

bool ASurvivalModeDirector::ConfirmArmy(const TArray<FSurvivalArmyPick>& Picks, FText& OutReason)
{
	if (M_Phase != ESurvivalPhase::ArmySelection)
	{
		OutReason = NSLOCTEXT("Survival", "NotInArmySelection", "The army can only be changed before deployment.");
		return false;
	}
	if (not USurvivalArmyLibrary::GetIsArmyValid(Settings, Picks, OutReason))
	{
		return false;
	}
	M_ConfirmedArmy = Picks;
	M_ConfirmedArmy.Sort([](const FSurvivalArmyPick& A, const FSurvivalArmyPick& B)
	{
		return A.SlotIndex < B.SlotIndex;
	});
	RemoveActiveSetupWidget();
	ShowStartLocationPicker();
	return true;
}

void ASurvivalModeDirector::ConfirmStartLocation(ASurvivalStartLocation* StartLocation)
{
	if (M_Phase != ESurvivalPhase::StartLocationSelection or not IsValid(StartLocation))
	{
		return;
	}
	M_ChosenStartLocation = StartLocation;
	RemoveActiveSetupWidget();
	SetSetupInputMode(nullptr, false);
	BeginDeployment();
}

void ASurvivalModeDirector::PreviewStartLocation(const ASurvivalStartLocation* StartLocation) const
{
	ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(StartLocation) or not IsValid(PlayerController))
	{
		return;
	}
	PlayerController->FocusCameraOnStartLocation(StartLocation->GetDeployCenter());
}

void ASurvivalModeDirector::ShowArmySelection()
{
	ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController) or not ArmySelectionWidgetClass)
	{
		RTSFunctionLibrary::ReportError(TEXT("ASurvivalModeDirector: no ArmySelectionWidgetClass; deploying the "
			"debug army."));
		DebugAutoStart();
		return;
	}
	SetPhase(ESurvivalPhase::ArmySelection);
	UW_SurvivalArmySelection* ArmySelection = CreateWidget<UW_SurvivalArmySelection>(
		PlayerController, ArmySelectionWidgetClass);
	if (not IsValid(ArmySelection))
	{
		return;
	}
	ArmySelection->AddToViewport(SurvivalModeDirectorConstants::SetupWidgetZOrder);
	ArmySelection->InitArmySelection(this);
	M_ActiveSetupWidget = ArmySelection;
	SetSetupInputMode(ArmySelection, true);
}

void ASurvivalModeDirector::ShowStartLocationPicker()
{
	SetPhase(ESurvivalPhase::StartLocationSelection);
	ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController) or not StartLocationPickerWidgetClass)
	{
		// Without a picker the first start location is used.
		if (not M_StartLocations.IsEmpty() and M_StartLocations[0].IsValid())
		{
			ConfirmStartLocation(M_StartLocations[0].Get());
		}
		return;
	}
	UW_SurvivalStartLocationPicker* Picker = CreateWidget<UW_SurvivalStartLocationPicker>(
		PlayerController, StartLocationPickerWidgetClass);
	if (not IsValid(Picker))
	{
		return;
	}
	Picker->AddToViewport(SurvivalModeDirectorConstants::SetupWidgetZOrder);
	Picker->InitStartLocationPicker(this);
	M_ActiveSetupWidget = Picker;
	SetSetupInputMode(Picker, true);
	if (not M_StartLocations.IsEmpty())
	{
		PreviewStartLocation(M_StartLocations[0].Get());
	}
}

void ASurvivalModeDirector::RemoveActiveSetupWidget()
{
	if (IsValid(M_ActiveSetupWidget))
	{
		M_ActiveSetupWidget->RemoveFromParent();
	}
	M_ActiveSetupWidget = nullptr;
}

void ASurvivalModeDirector::SetSetupInputMode(UUserWidget* WidgetToFocus, const bool bSetupActive) const
{
	ACPPController* PlayerController = GetPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	UMainGameUI* MainGameUI = PlayerController->GetMainMenuUI();
	if (IsValid(MainGameUI))
	{
		MainGameUI->SetMainMenuVisiblity(not bSetupActive);
	}
	PlayerController->SetCameraMovementDisabled(bSetupActive);
	PlayerController->bShowMouseCursor = true;
	if (bSetupActive and IsValid(WidgetToFocus))
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		return;
	}
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	if (IsValid(MainGameUI))
	{
		InputMode.SetWidgetToFocus(MainGameUI->TakeWidget());
	}
	PlayerController->SetInputMode(InputMode);
}

void ASurvivalModeDirector::ShowHud()
{
	ACPPController* PlayerController = GetPlayerController();
	if (IsValid(M_HudWidget) or not IsValid(PlayerController) or not HudWidgetClass)
	{
		return;
	}
	M_HudWidget = CreateWidget<UW_SurvivalHud>(PlayerController, HudWidgetClass);
	if (not IsValid(M_HudWidget))
	{
		return;
	}
	M_HudWidget->AddToViewport(SurvivalModeDirectorConstants::HudWidgetZOrder);
	M_HudWidget->InitHud(this);
}

void ASurvivalModeDirector::BeginDeployment()
{
	ACPPController* PlayerController = GetPlayerController();
	ASurvivalStartLocation* StartLocation = M_ChosenStartLocation.Get();
	if (not IsValid(PlayerController) or not IsValid(StartLocation))
	{
		return;
	}
	SetPhase(ESurvivalPhase::Deploying);
	const FVector DeployCenter = StartLocation->GetDeployCenter();
	// Applies technologies, resource cards and the saved variants without spawning profile units or an HQ.
	PlayerController->bM_IsHQDeliveredAfterStart = true;
	PlayerController->StartGameAtLocation(DeployCenter, true);
	GenerateStartLocationContent();
	ShowHud();
	if (IsValid(Settings->Deployment.DeploymentStartedSound))
	{
		UGameplayStatics::PlaySound2D(this, Settings->Deployment.DeploymentStartedSound);
	}

	// Aircraft come in from the map edge nearest to the start location.
	FBox LandscapeBounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		LandscapeBounds += It->GetComponentsBoundingBox();
	}
	const FVector MapCenter = LandscapeBounds.IsValid ? LandscapeBounds.GetCenter() : FVector::ZeroVector;
	FVector ApproachDirection = (DeployCenter - MapCenter).GetSafeNormal2D();
	if (ApproachDirection.IsNearlyZero())
	{
		ApproachDirection = FVector::ForwardVector;
	}
	M_Deployment->StartDeployment(M_ConfirmedArmy, DeployCenter, ApproachDirection);
}

void ASurvivalModeDirector::GenerateStartLocationContent()
{
	ASurvivalStartLocation* ChosenLocation = M_ChosenStartLocation.Get();
	int32 BaseIndex = 0;
	for (const TWeakObjectPtr<ASurvivalStartLocation>& StartLocation : M_StartLocations)
	{
		if (not StartLocation.IsValid())
		{
			continue;
		}
		if (StartLocation.Get() == ChosenLocation)
		{
			StartLocation->GeneratePlayerStartContent(Settings->StartContent);
			continue;
		}
		M_SovietBases->GenerateSovietBaseAt(StartLocation.Get(), BaseIndex++);
	}
}

void ASurvivalModeDirector::OnHQLanded(ANomadicVehicle* HQ)
{
	ACPPController* PlayerController = GetPlayerController();
	M_HQ = HQ;
	UE_LOG(LogTemp, Log, TEXT("Survival HQ landed: %s"), IsValid(HQ) ? *HQ->GetName() : TEXT("none"));
	for (const TWeakObjectPtr<ASurvivalStartLocation>& StartLocation : M_StartLocations)
	{
		if (StartLocation.IsValid())
		{
			StartLocation->SetRevealFogOfWar(false);
		}
	}
	if (IsValid(PlayerController))
	{
		// Registers the HQ as the resource drop-off and applies the mission starting resources.
		PlayerController->SetupPlayerHQReference();
	}
	StartGracePeriod();
}

void ASurvivalModeDirector::OnDeploymentFinished()
{
	UE_LOG(LogTemp, Log, TEXT("Survival deployment finished."));
}

void ASurvivalModeDirector::StartGracePeriod()
{
	if (M_Phase != ESurvivalPhase::Deploying)
	{
		return;
	}
	SetPhase(ESurvivalPhase::GracePeriod);
	M_GraceStartTime = GetWorld()->GetTimeSeconds();
	if (M_GraceSeconds <= 0.f)
	{
		OnGracePeriodFinished();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(M_GraceTimerHandle, this, &ASurvivalModeDirector::OnGracePeriodFinished,
	                                       M_GraceSeconds, false);
}

void ASurvivalModeDirector::OnGracePeriodFinished()
{
	StartWaves();
}

void ASurvivalModeDirector::StartWaves()
{
	if (M_Phase != ESurvivalPhase::GracePeriod)
	{
		return;
	}
	SetPhase(ESurvivalPhase::Waves);
	M_WavesStartTime = GetWorld()->GetTimeSeconds();
	M_WaveDirector->StartWaves();
	GetWorld()->GetTimerManager().SetTimer(M_DefeatCheckTimerHandle, this, &ASurvivalModeDirector::CheckDefeat,
	                                       SurvivalModeDirectorConstants::DefeatCheckInterval, true);
}

int32 ASurvivalModeDirector::GetWavesLaunched() const
{
	return IsValid(M_WaveDirector) ? M_WaveDirector->GetWavesLaunched() : 0;
}

void ASurvivalModeDirector::OnWaveIncoming(const FSurvivalWaveAnnouncement& Announcement)
{
	if (not IsValid(M_HudWidget))
	{
		return;
	}
	FText EdgeLabel;
	if (not Announcement.EdgeNames.IsEmpty())
	{
		EdgeLabel = FText::Join(NSLOCTEXT("Survival", "EdgeJoin", " and the "), Announcement.EdgeNames);
	}
	else if (not Announcement.EdgeLocations.IsEmpty())
	{
		EdgeLabel = UW_SurvivalHud::GetCompassDirectionText(Announcement.EdgeLocations[0],
		                                                    GetCurrentPlayerBaseLocation());
	}
	M_HudWidget->ShowIncomingWaveBanner(Announcement.WaveNumber, EdgeLabel, 0.f);
}

void ASurvivalModeDirector::CheckDefeat()
{
	if (M_Phase != ESurvivalPhase::Waves)
	{
		return;
	}
	if (GetHasPlayerLost())
	{
		OnDefeat();
	}
}

bool ASurvivalModeDirector::GetHasPlayerLost() const
{
	const bool bHQAlive = M_HQ.IsValid() and RTSFunctionLibrary::RTSIsValid(M_HQ.Get());
	if (Settings->DefeatRule == ESurvivalDefeatRule::HQDestroyed)
	{
		return not bHQAlive;
	}
	if (bHQAlive)
	{
		return false;
	}
	for (TActorIterator<ASelectablePawnMaster> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = IsValid(*It) ? It->GetRTSComponent() : nullptr;
		if (RTSComponent and RTSComponent->GetOwningPlayer() == SurvivalModeDirectorConstants::PlayerOwner
			and RTSFunctionLibrary::RTSIsValid(*It))
		{
			return false;
		}
	}
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = IsValid(*It) ? It->FindComponentByClass<URTSComponent>() : nullptr;
		if (RTSComponent and RTSComponent->GetOwningPlayer() == SurvivalModeDirectorConstants::PlayerOwner
			and RTSFunctionLibrary::RTSIsValid(*It))
		{
			return false;
		}
	}
	return true;
}

void ASurvivalModeDirector::OnDefeat()
{
	M_DefeatTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().ClearTimer(M_DefeatCheckTimerHandle);
	M_WaveDirector->StopWaves();
	GetWorld()->GetTimerManager().ClearTimer(M_IntermissionTimerHandle);
	M_IntermissionEndTime = -1.0;
	CloseRewardPicker();
	const float PreviousBest = GetBestSurvivalSeconds();
	const float Survived = GetElapsedSurvivalSeconds();
	SaveBestSurvivalTime();
	SetPhase(ESurvivalPhase::Defeated);
	if (IsValid(M_HudWidget))
	{
		M_HudWidget->ShowDefeat(Survived, GetRoundsSurvived(), FMath::Max(PreviousBest, Survived),
		                        Survived > PreviousBest);
	}
}

FString ASurvivalModeDirector::GetBestTimeSaveSlotName() const
{
	const FString MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	const FString DifficultyName = StaticEnum<ERTSGameDifficulty>()->GetNameStringByValue(
		static_cast<int64>(GetDifficulty().DifficultyLevel));
	return MapName + TEXT("|") + DifficultyName;
}

float ASurvivalModeDirector::GetBestSurvivalSeconds() const
{
	const USurvivalModeSave* Save = Cast<USurvivalModeSave>(
		UGameplayStatics::LoadGameFromSlot(USurvivalModeSave::SlotName, 0));
	if (not IsValid(Save))
	{
		return 0.f;
	}
	const float* BestSeconds = Save->BestSecondsPerKey.Find(GetBestTimeSaveSlotName());
	return BestSeconds ? *BestSeconds : 0.f;
}

void ASurvivalModeDirector::SaveBestSurvivalTime() const
{
	USurvivalModeSave* Save = Cast<USurvivalModeSave>(UGameplayStatics::LoadGameFromSlot(USurvivalModeSave::SlotName, 0));
	if (not IsValid(Save))
	{
		Save = Cast<USurvivalModeSave>(UGameplayStatics::CreateSaveGameObject(USurvivalModeSave::StaticClass()));
	}
	if (not IsValid(Save))
	{
		return;
	}
	const FString Key = GetBestTimeSaveSlotName();
	const float Survived = GetElapsedSurvivalSeconds();
	float& BestSeconds = Save->BestSecondsPerKey.FindOrAdd(Key);
	BestSeconds = FMath::Max(BestSeconds, Survived);
	int32& BestWaves = Save->BestWavesPerKey.FindOrAdd(Key);
	BestWaves = FMath::Max(BestWaves, GetRoundsSurvived());
	UGameplayStatics::SaveGameToSlot(Save, USurvivalModeSave::SlotName, 0);
}

void ASurvivalModeDirector::DebugAutoStart()
{
	SetPhase(ESurvivalPhase::ArmySelection);
	M_ConfirmedArmy = BuildDebugArmy();
	if (DebugGraceSecondsOverride >= 0.f)
	{
		M_GraceSeconds = DebugGraceSecondsOverride;
	}
	SetPhase(ESurvivalPhase::StartLocationSelection);
	if (M_StartLocations.IsEmpty() or not M_StartLocations[0].IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("ASurvivalModeDirector debug start: no start location."));
		return;
	}
	ConfirmStartLocation(M_StartLocations[0].Get());
}

TArray<FSurvivalArmyPick> ASurvivalModeDirector::BuildDebugArmy() const
{
	TArray<FSurvivalArmyPick> Army;
	const TArray<FSurvivalArmyEntry> Roster = USurvivalArmyLibrary::BuildArmyRoster(
		Settings, GetPlayerFaction(), FindAsyncSpawner());
	int32 SupplyLeft = Settings->Army.SupplyBudget;
	for (const FSurvivalArmyEntry& Entry : Roster)
	{
		if (Army.Num() >= Settings->Army.SlotCount)
		{
			break;
		}
		if (not Entry.bIsHQ and (Entry.SupplyCost > SupplyLeft or Entry.Unit.UnitType == EAllUnitType::UNType_Nomadic))
		{
			continue;
		}
		FSurvivalArmyPick Pick;
		Pick.SlotIndex = Army.Num();
		Pick.Entry = Entry;
		Army.Add(Pick);
		if (not Entry.bIsHQ)
		{
			SupplyLeft -= Entry.SupplyCost;
		}
	}
	return Army;
}

ACPPController* ASurvivalModeDirector::GetPlayerController() const
{
	return Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0));
}

bool ASurvivalModeDirector::GetIsValidSettings() const
{
	if (IsValid(Settings))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "Settings", "GetIsValidSettings", this);
	return false;
}

// ---------------------------------------------------------------- Rounds, intermission and rewards

int32 ASurvivalModeDirector::GetEnemiesRemaining() const
{
	return IsValid(M_WaveDirector) ? M_WaveDirector->GetEnemiesRemaining() : 0;
}

int32 ASurvivalModeDirector::GetRoundsSurvived() const
{
	return IsValid(M_WaveDirector) ? M_WaveDirector->GetRoundsSurvived() : 0;
}

ESurvivalRoundState ASurvivalModeDirector::GetRoundState() const
{
	return IsValid(M_WaveDirector) ? M_WaveDirector->GetRoundState() : ESurvivalRoundState::Idle;
}

float ASurvivalModeDirector::GetIntermissionSecondsRemaining() const
{
	if (M_IntermissionEndTime < 0.0 or not GetWorld())
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(M_IntermissionEndTime - GetWorld()->GetTimeSeconds()));
}

void ASurvivalModeDirector::OnRoundSurvived(const int32 RoundNumber)
{
	if (M_Phase != ESurvivalPhase::Waves)
	{
		return;
	}
	const FSurvivalRewardSettings& Rewards = Settings->Rewards;
	const float Seconds = FMath::Min(
		Rewards.IntermissionSeconds + Rewards.IntermissionSecondsPerRound * FMath::Max(0, RoundNumber - 1),
		Rewards.MaxIntermissionSeconds);
	M_IntermissionRound = RoundNumber;
	M_IntermissionEndTime = GetWorld()->GetTimeSeconds() + Seconds;
	bM_HasChosenReward = false;
	M_RewardOptions = FSurvivalRewardLibrary::BuildRewardOptions(Settings, GetPlayerFaction(), RoundNumber,
	                                                             M_RewardRandom,
	                                                             FindAsyncSpawner());
	UE_LOG(LogTemp, Log, TEXT("Survival intermission after wave %d: %.0f s, %d reward options."), RoundNumber,
	       Seconds, M_RewardOptions.Num());
	for (int32 Index = 0; Index < M_RewardOptions.Num(); ++Index)
	{
		FString Units;
		for (const FSurvivalArmyEntry& Entry : M_RewardOptions[Index].Units)
		{
			Units += Entry.DisplayName.ToString() + TEXT(", ");
		}
		UE_LOG(LogTemp, Log, TEXT("  option %d %s (%d supply): %s"), Index, *M_RewardOptions[Index].Title.ToString(),
		       M_RewardOptions[Index].TotalSupply, *Units);
	}
	ShowRewardPicker();
	GetWorld()->GetTimerManager().SetTimer(M_IntermissionTimerHandle, this,
	                                       &ASurvivalModeDirector::OnIntermissionFinished, Seconds, false);
}

void ASurvivalModeDirector::ShowRewardPicker()
{
	ACPPController* PlayerController = GetPlayerController();
	if (M_RewardOptions.IsEmpty() or not IsValid(PlayerController) or not RewardPickerWidgetClass)
	{
		return;
	}
	CloseRewardPicker();
	M_RewardPicker = CreateWidget<UW_SurvivalRewardPicker>(PlayerController, RewardPickerWidgetClass);
	if (not IsValid(M_RewardPicker))
	{
		return;
	}
	M_RewardPicker->AddToViewport(SurvivalModeDirectorConstants::SetupWidgetZOrder);
	M_RewardPicker->InitRewardPicker(this, M_IntermissionRound, M_RewardOptions);
}

void ASurvivalModeDirector::CloseRewardPicker()
{
	if (IsValid(M_RewardPicker))
	{
		M_RewardPicker->RemoveFromParent();
	}
	M_RewardPicker = nullptr;
}

void ASurvivalModeDirector::ChooseReward(const int32 OptionIndex)
{
	if (bM_HasChosenReward or not GetIsInIntermission() or not M_RewardOptions.IsValidIndex(OptionIndex))
	{
		return;
	}
	bM_HasChosenReward = true;
	if (IsValid(M_RewardPicker))
	{
		M_RewardPicker->ShowChosen(OptionIndex);
	}
	DeliverReward(M_RewardOptions[OptionIndex]);
}

void ASurvivalModeDirector::DeliverReward(const FSurvivalRewardOption& Option)
{
	TArray<FSurvivalArmyPick> Picks;
	for (const FSurvivalArmyEntry& Entry : Option.Units)
	{
		FSurvivalArmyPick Pick;
		Pick.SlotIndex = Picks.Num();
		Pick.Entry = Entry;
		Pick.Entry.bIsHQ = false;
		Picks.Add(Pick);
	}
	if (Picks.IsEmpty())
	{
		return;
	}
	// Land between the base and the nearest map edge so the drop does not bury the buildings.
	FBox LandscapeBounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		LandscapeBounds += It->GetComponentsBoundingBox();
	}
	const FVector MapCenter = LandscapeBounds.IsValid ? LandscapeBounds.GetCenter() : FVector::ZeroVector;
	const FVector BaseLocation = GetCurrentPlayerBaseLocation();
	FVector ApproachDirection = (BaseLocation - MapCenter).GetSafeNormal2D();
	if (ApproachDirection.IsNearlyZero())
	{
		ApproachDirection = FVector::ForwardVector;
	}
	constexpr float RewardLandingOffset = 3000.f;
	const FVector LandingCenter = BaseLocation + ApproachDirection * RewardLandingOffset;

	USurvivalDeploymentComponent* RewardDrop = NewObject<USurvivalDeploymentComponent>(this);
	RewardDrop->RegisterComponent();
	RewardDrop->InitDeployment(this, true);
	RewardDrop->StartDeployment(Picks, LandingCenter, ApproachDirection);
	UE_LOG(LogTemp, Log, TEXT("Survival reward %s: %d units inbound."), *Option.Title.ToString(), Picks.Num());
}

void ASurvivalModeDirector::OnIntermissionFinished()
{
	if (not bM_HasChosenReward and Settings->Rewards.bAutoPickWhenTimeRunsOut and not M_RewardOptions.IsEmpty())
	{
		ChooseReward(0);
	}
	M_IntermissionEndTime = -1.0;
	CloseRewardPicker();
	if (M_Phase == ESurvivalPhase::Waves)
	{
		M_WaveDirector->StartNextRound();
	}
}
