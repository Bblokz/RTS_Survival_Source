// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FactionPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "FactionUnitPreview.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionDifficultyPicker.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionWorldGenerationSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AFactionPlayerController::AFactionPlayerController()
{
	M_AnnouncementAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AnnouncementAudioComponent"));
	M_AnnouncementAudioComponent->bAutoActivate = false;
}

void AFactionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitFactionPopup();
	InitPreviewReferences();

	if (GetIsValidAnnouncementAudioComponent())
	{
		M_AnnouncementAudioComponent->OnAudioFinished.AddUniqueDynamic(
			this,
			&AFactionPlayerController::HandleAnnouncementFinished
		);
	}
}

void AFactionPlayerController::SpawnPreviewForTrainingOption(const FTrainingOption& TrainingOption)
{
	if (not GetIsValidRTSAsyncSpawner())
	{
		return;
	}

	if (not GetIsValidFactionUnitPreview())
	{
		return;
	}

	ClearCurrentPreview();

	const FVector FactionUnitSpawnLocation = M_FactionUnitPreview->GetActorLocation();
	const int32 PreviewSpawnRequestId = 1;
	TWeakObjectPtr<AFactionPlayerController> WeakThis(this);
	const bool bRequestStarted = M_RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption,
		FactionUnitSpawnLocation,
		this,
		PreviewSpawnRequestId,
		[WeakThis](const FTrainingOption& SpawnedOption, AActor* SpawnedActor, const int32 SpawnRequestId)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandlePreviewSpawned(SpawnedOption, SpawnedActor, SpawnRequestId);
		}
	);

	if (not bRequestStarted)
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::SpawnPreviewForTrainingOption - Failed to start async spawn request."
		);
	}
}

void AFactionPlayerController::PlayAnnouncementSound(USoundBase* AnnouncementSound)
{
	if (not GetIsValidAnnouncementAudioComponent())
	{
		return;
	}

	if (AnnouncementSound == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"AnnouncementSound",
			"PlayAnnouncementSound",
			this
		);
		return;
	}

	M_AnnouncementAudioComponent->SetSound(AnnouncementSound);
	M_AnnouncementAudioComponent->Play();
}

void AFactionPlayerController::StopAnnouncementSound()
{
	if (not GetIsValidAnnouncementAudioComponent())
	{
		return;
	}

	if (M_AnnouncementAudioComponent->IsPlaying())
	{
		M_AnnouncementAudioComponent->Stop();
	}
}

void AFactionPlayerController::HandleLaunchCampaignRequested(const ERTSFaction SelectedFaction)
{
	M_SelectedFaction = SelectedFaction;

	if (GetIsValidFactionSelectionMenu())
	{
		M_FactionSelectionMenu->RemoveFromParent();
		M_FactionSelectionMenu.Reset();
	}

	InitFactionDifficultyPicker();
	InitInputModeForWidget(M_FactionDifficultyPicker.Get());
}

void AFactionPlayerController::HandleFactionDifficultyChosen(
	const int32 DifficultyPercentage,
	const ERTSGameDifficulty SelectedDifficulty)
{
	M_SelectedGameDifficulty.DifficultyPercentage = DifficultyPercentage;
	M_SelectedGameDifficulty.DifficultyLevel = SelectedDifficulty;
	M_SelectedGameDifficulty.bIsInitialized = true;

	if (GetIsValidFactionDifficultyPicker())
	{
		M_FactionDifficultyPicker->RemoveFromParent();
		M_FactionDifficultyPicker.Reset();
	}

	InitFactionWorldGenerationSettings();

	if (GetIsValidFactionWorldGenerationSettings())
	{
		M_FactionWorldGenerationSettings->SetDifficultyText(M_SelectedGameDifficulty.DifficultyLevel);
	}

	InitInputModeForWidget(M_FactionWorldGenerationSettings.Get());
}

void AFactionPlayerController::HandleWorldGenerationBackRequested()
{
	if (GetIsValidFactionWorldGenerationSettings())
	{
		M_FactionWorldGenerationSettings->RemoveFromParent();
		M_FactionWorldGenerationSettings.Reset();
	}

	InitFactionDifficultyPicker();
	InitInputModeForWidget(M_FactionDifficultyPicker.Get());
}

void AFactionPlayerController::HandleWorldGenerationSettingsGenerated(const FCampaignGenerationSettings& Settings)
{
	if (M_SelectedFaction == ERTSFaction::NotInitialised)
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::HandleWorldGenerationSettingsGenerated - Faction was not selected."
		);
		return;
	}

	if (not M_SelectedGameDifficulty.bIsInitialized)
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::HandleWorldGenerationSettingsGenerated - Difficulty was not selected."
		);
		return;
	}

	if (GetIsValidFactionWorldGenerationSettings())
	{
		M_FactionWorldGenerationSettings->RemoveFromParent();
		M_FactionWorldGenerationSettings.Reset();
	}

	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (RTSGameInstance == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::HandleWorldGenerationSettingsGenerated - Failed to get RTS game instance."
		);
		return;
	}

	RTSGameInstance->SetCampaignGenerationSettings(Settings);
	RTSGameInstance->SetPlayerFaction(M_SelectedFaction);
	RTSGameInstance->SetSelectedGameDifficulty(M_SelectedGameDifficulty);

	if (M_CampaignWorld.IsNull())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_CampaignWorld",
			"AFactionPlayerController::HandleWorldGenerationSettingsGenerated",
			this
		);
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, M_CampaignWorld);
}

void AFactionPlayerController::HandleAnnouncementFinished()
{
	if (not GetIsValidFactionSelectionMenu())
	{
		return;
	}

	M_FactionSelectionMenu->HandleAnnouncementFinished();
}

void AFactionPlayerController::HandleFactionPopupAccepted()
{
	if (not GetIsValidFactionPopup())
	{
		return;
	}

	M_FactionPopup->RemoveFromParent();
	M_FactionPopup.Reset();

	InitFactionSelectionMenu();
	InitInputModeForWidget(M_FactionSelectionMenu.Get());
}

void AFactionPlayerController::InitFactionPopup()
{
	if (not GetIsValidFactionPopupClass())
	{
		InitFactionSelectionMenu();
		InitInputModeForWidget(M_FactionSelectionMenu.Get());
		return;
	}

	UW_FactionPopup* FactionPopup = CreateWidget<UW_FactionPopup>(this, M_FactionPopupClass);
	if (not IsValid(FactionPopup))
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::InitFactionPopup - Failed to create faction popup widget."
		);
		InitFactionSelectionMenu();
		InitInputModeForWidget(M_FactionSelectionMenu.Get());
		return;
	}

	FactionPopup->AddToViewport();
	FactionPopup->OnPopupAccepted.AddUniqueDynamic(this, &AFactionPlayerController::HandleFactionPopupAccepted);
	M_FactionPopup = FactionPopup;

	InitInputModeForWidget(FactionPopup);
}

void AFactionPlayerController::InitFactionSelectionMenu()
{
	if (not GetIsValidFactionSelectionMenuClass())
	{
		return;
	}

	UW_FactionSelectionMenu* FactionSelectionMenu = CreateWidget<UW_FactionSelectionMenu>(
		this,
		M_FactionSelectionMenuClass
	);
	if (not IsValid(FactionSelectionMenu))
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::InitFactionSelectionMenu - Failed to create faction selection menu widget."
		);
		return;
	}

	FactionSelectionMenu->AddToViewport();
	FactionSelectionMenu->SetFactionPlayerController(this);
	M_FactionSelectionMenu = FactionSelectionMenu;
}

void AFactionPlayerController::InitFactionDifficultyPicker()
{
	if (not GetIsValidFactionDifficultyPickerClass())
	{
		return;
	}

	UW_FactionDifficultyPicker* FactionDifficultyPicker = CreateWidget<UW_FactionDifficultyPicker>(
		this,
		M_FactionDifficultyPickerClass
	);
	if (not IsValid(FactionDifficultyPicker))
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::InitFactionDifficultyPicker - Failed to create faction difficulty picker widget."
		);
		return;
	}

	FactionDifficultyPicker->AddToViewport();
	FactionDifficultyPicker->SetFactionPlayerController(this);
	M_FactionDifficultyPicker = FactionDifficultyPicker;
}

void AFactionPlayerController::InitFactionWorldGenerationSettings()
{
	if (not GetIsValidFactionWorldGenerationSettingsClass())
	{
		return;
	}

	UW_FactionWorldGenerationSettings* WorldGenerationSettings = CreateWidget<UW_FactionWorldGenerationSettings>(
		this,
		M_FactionWorldGenerationSettingsClass
	);
	if (not IsValid(WorldGenerationSettings))
	{
		RTSFunctionLibrary::ReportError(
			"AFactionPlayerController::InitFactionWorldGenerationSettings - Failed to create world generation settings widget."
		);
		return;
	}

	WorldGenerationSettings->AddToViewport();
	WorldGenerationSettings->SetFactionPlayerController(this);
	M_FactionWorldGenerationSettings = WorldGenerationSettings;
}

void AFactionPlayerController::InitPreviewReferences()
{
	AActor* FoundPreviewActor = UGameplayStatics::GetActorOfClass(this, AFactionUnitPreview::StaticClass());
	if (not IsValid(FoundPreviewActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionUnitPreview",
			"InitPreviewReferences",
			this
		);
	}
	else
	{
		M_FactionUnitPreview = Cast<AFactionUnitPreview>(FoundPreviewActor);
	}

	AActor* FoundSpawnerActor = UGameplayStatics::GetActorOfClass(this, ARTSAsyncSpawner::StaticClass());
	if (not IsValid(FoundSpawnerActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_RTSAsyncSpawner",
			"InitPreviewReferences",
			this
		);
	}
	else
	{
		M_RTSAsyncSpawner = Cast<ARTSAsyncSpawner>(FoundSpawnerActor);
	}
}

void AFactionPlayerController::InitInputModeForWidget(UUserWidget* WidgetToFocus)
{
	if (not IsValid(WidgetToFocus))
	{
		return;
	}

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
}

void AFactionPlayerController::HandlePreviewSpawned(
	const FTrainingOption& TrainingOption,
	AActor* SpawnedActor,
	const int32 SpawnRequestId)
{
	(void)TrainingOption;
	(void)SpawnRequestId;

	if (not IsValid(SpawnedActor))
	{
		return;
	}

	SpawnedActor->SetActorRotation(M_PreviewSpawnRotation);
	M_CurrentPreviewActor = SpawnedActor;
	M_CurrentPreviewActorType = EPreviewActorType::None;
	M_NextPreviewAction = EPreviewActionType::None;

	if (ATankMaster* TankMaster = Cast<ATankMaster>(SpawnedActor))
	{
		M_CurrentPreviewActorType = EPreviewActorType::Tank;

		ICommands* CommandsInterface = Cast<ICommands>(TankMaster);
		if (CommandsInterface == nullptr)
		{
			M_CurrentCommandsInterface.SetObject(nullptr);
			M_CurrentCommandsInterface.SetInterface(nullptr);
			return;
		}

		M_CurrentCommandsInterface.SetObject(TankMaster);
		M_CurrentCommandsInterface.SetInterface(CommandsInterface);
		SetupTankPreviewActions();
		return;
	}

	if (ASquadController* SquadController = Cast<ASquadController>(SpawnedActor))
	{
		M_CurrentPreviewActorType = EPreviewActorType::Squad;

		ICommands* CommandsInterface = Cast<ICommands>(SquadController);
		if (CommandsInterface == nullptr)
		{
			M_CurrentCommandsInterface.SetObject(nullptr);
			M_CurrentCommandsInterface.SetInterface(nullptr);
			return;
		}

		M_CurrentCommandsInterface.SetObject(SquadController);
		M_CurrentCommandsInterface.SetInterface(CommandsInterface);
		SetupSquadPreviewActions();
		return;
	}

	if (AAircraftMaster* AircraftMaster = Cast<AAircraftMaster>(SpawnedActor))
	{
		M_CurrentPreviewActorType = EPreviewActorType::Aircraft;
		M_CurrentCommandsInterface.SetObject(nullptr);
		M_CurrentCommandsInterface.SetInterface(nullptr);
		AircraftMaster->SetPreviewTakeOffHeight(M_UnitPreviewActions.ZTakeOffOffset);
		SetupAircraftPreviewActions();
		return;
	}

	M_CurrentCommandsInterface.SetObject(nullptr);
	M_CurrentCommandsInterface.SetInterface(nullptr);
}

void AFactionPlayerController::ResetPreviewGeneralActionTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_PreviewGeneralActionTimerHandle);

	const float MinDelay = M_UnitPreviewActions.MinTimeTillGeneralAction;
	const float MaxDelay = M_UnitPreviewActions.MaxTimeTillGeneralAction;
	const float NextDelay = FMath::FRandRange(MinDelay, MaxDelay);

	FTimerDelegate PreviewActionDelegate;
	TWeakObjectPtr<AFactionPlayerController> WeakThis(this);
	PreviewActionDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->HandlePreviewGeneralActionTimer();
	});

	World->GetTimerManager().SetTimer(M_PreviewGeneralActionTimerHandle, PreviewActionDelegate, NextDelay, false);
}

void AFactionPlayerController::HandlePreviewGeneralActionTimer()
{
	if (M_CurrentPreviewActorType == EPreviewActorType::Tank)
	{
		HandleTankPreviewAction();
		return;
	}

	if (M_CurrentPreviewActorType == EPreviewActorType::Squad)
	{
		HandleSquadPreviewAction();
		return;
	}

	if (M_CurrentPreviewActorType == EPreviewActorType::Aircraft)
	{
		HandleAircraftPreviewTakeOffAction();
	}
}

void AFactionPlayerController::HandlePreviewAircraftComeDownTimer()
{
	AActor* CurrentPreviewActor = M_CurrentPreviewActor.Get();
	AAircraftMaster* AircraftMaster = Cast<AAircraftMaster>(CurrentPreviewActor);
	if (not IsValid(AircraftMaster))
	{
		return;
	}

	AircraftMaster->StartVerticalLanding_Preview();
	ResetPreviewGeneralActionTimer();
}

void AFactionPlayerController::SetupTankPreviewActions()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	if (not GetHasGroundAttackLocations())
	{
		return;
	}

	M_NextPreviewAction = GetRandomPreviewAction(EPreviewActionType::Rotate, EPreviewActionType::Attack);
	ResetPreviewGeneralActionTimer();
}

void AFactionPlayerController::SetupSquadPreviewActions()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	if (not GetHasGroundAttackLocations() || not GetHasSquadMovementLocations())
	{
		return;
	}

	M_NextPreviewAction = GetRandomPreviewAction(EPreviewActionType::Move, EPreviewActionType::Attack);
	ResetPreviewGeneralActionTimer();
}

void AFactionPlayerController::SetupAircraftPreviewActions()
{
	M_NextPreviewAction = EPreviewActionType::TakeOff;
	ResetPreviewGeneralActionTimer();
}

void AFactionPlayerController::HandleTankPreviewAction()
{
	if (M_NextPreviewAction == EPreviewActionType::Rotate)
	{
		HandleTankPreviewRotateAction();
		FlipTankAction();
		ResetPreviewGeneralActionTimer();
		return;
	}

	if (M_NextPreviewAction == EPreviewActionType::Attack)
	{
		HandleTankPreviewAttackAction();
		FlipTankAction();
		ResetPreviewGeneralActionTimer();
	}
}

void AFactionPlayerController::HandleSquadPreviewAction()
{
	if (M_NextPreviewAction == EPreviewActionType::Move)
	{
		HandleSquadPreviewMoveAction();
		FlipSquadAction();
		ResetPreviewGeneralActionTimer();
		return;
	}

	if (M_NextPreviewAction == EPreviewActionType::Attack)
	{
		HandleSquadPreviewAttackAction();
		FlipSquadAction();
		ResetPreviewGeneralActionTimer();
	}
}

void AFactionPlayerController::HandleAircraftPreviewTakeOffAction()
{
	AActor* CurrentPreviewActor = M_CurrentPreviewActor.Get();
	AAircraftMaster* AircraftMaster = Cast<AAircraftMaster>(CurrentPreviewActor);
	if (not IsValid(AircraftMaster))
	{
		return;
	}

	AircraftMaster->TakeOffFromGroundOrOwner();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float TimeTillComeDown = M_UnitPreviewActions.TimeTillOrderAircraftComeDown;
	FTimerDelegate AircraftComeDownDelegate;
	TWeakObjectPtr<AFactionPlayerController> WeakThis(this);
	AircraftComeDownDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->HandlePreviewAircraftComeDownTimer();
	});

	World->GetTimerManager().ClearTimer(M_PreviewAircraftComeDownTimerHandle);
	World->GetTimerManager().SetTimer(M_PreviewAircraftComeDownTimerHandle, AircraftComeDownDelegate, TimeTillComeDown, false);
}

void AFactionPlayerController::HandleTankPreviewAttackAction()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	if (not GetHasGroundAttackLocations())
	{
		return;
	}

	ICommands* CommandsInterface = M_CurrentCommandsInterface.GetInterface();
	if (CommandsInterface == nullptr)
	{
		return;
	}

	const FVector AttackLocation = GetRandomGroundAttackLocation();
	const bool bSetUnitToIdle = true;
	CommandsInterface->AttackGround(AttackLocation, bSetUnitToIdle);
}

void AFactionPlayerController::HandleTankPreviewRotateAction()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	ICommands* CommandsInterface = M_CurrentCommandsInterface.GetInterface();
	if (CommandsInterface == nullptr)
	{
		return;
	}

	const FRotator Rotation = GetRandomTankPreviewRotation();
	const bool bSetUnitToIdle = true;
	CommandsInterface->RotateTowards(Rotation, bSetUnitToIdle);
}

void AFactionPlayerController::HandleSquadPreviewAttackAction()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	if (not GetHasGroundAttackLocations())
	{
		return;
	}

	ICommands* CommandsInterface = M_CurrentCommandsInterface.GetInterface();
	if (CommandsInterface == nullptr)
	{
		return;
	}

	const FVector AttackLocation = GetRandomGroundAttackLocation();
	const bool bSetUnitToIdle = true;
	CommandsInterface->AttackGround(AttackLocation, bSetUnitToIdle);
}

void AFactionPlayerController::HandleSquadPreviewMoveAction()
{
	if (not GetIsValidCurrentCommandsInterface())
	{
		return;
	}

	if (not GetHasSquadMovementLocations())
	{
		return;
	}

	ICommands* CommandsInterface = M_CurrentCommandsInterface.GetInterface();
	if (CommandsInterface == nullptr)
	{
		return;
	}

	const FVector MoveLocation = GetRandomSquadMovementLocation();
	const bool bSetUnitToIdle = true;
	CommandsInterface->MoveToLocation(MoveLocation, bSetUnitToIdle, FRotator::ZeroRotator);
}

void AFactionPlayerController::FlipTankAction()
{
	if (M_NextPreviewAction == EPreviewActionType::Rotate)
	{
		M_NextPreviewAction = EPreviewActionType::Attack;
		return;
	}

	M_NextPreviewAction = EPreviewActionType::Rotate;
}

void AFactionPlayerController::FlipSquadAction()
{
	if (M_NextPreviewAction == EPreviewActionType::Move)
	{
		M_NextPreviewAction = EPreviewActionType::Attack;
		return;
	}

	M_NextPreviewAction = EPreviewActionType::Move;
}

FVector AFactionPlayerController::GetRandomSquadMovementLocation() const
{
	const int32 LocationCount = M_UnitPreviewActions.SquadMovementLocations.Num();
	if (LocationCount == 0)
	{
		return FVector::ZeroVector;
	}

	const int32 FirstIndex = 0;
	const int32 LastIndex = LocationCount - 1;
	const int32 RandomIndex = FMath::RandRange(FirstIndex, LastIndex);
	return M_UnitPreviewActions.SquadMovementLocations[RandomIndex];
}

FRotator AFactionPlayerController::GetRandomTankPreviewRotation() const
{
	if (not GetIsValidCurrentPreviewActor())
	{
		return FRotator::ZeroRotator;
	}

	const AActor* PreviewActor = M_CurrentPreviewActor.Get();
	const FVector PreviewLocation = PreviewActor->GetActorLocation();
	const FRotator CurrentRotation = PreviewActor->GetActorRotation();
	const float RandomYawOffset = FMath::FRandRange(
		M_UnitPreviewActions.MinYawOffsetForRotation,
		M_UnitPreviewActions.MaxYawOffsetForRotation
	);
	const float PreviewForwardDistance = 600.0f;
	const FRotator TargetRotationWithOffset(
		0.0f,
		CurrentRotation.Yaw + RandomYawOffset,
		0.0f
	);
	const FVector RotationTargetLocation =
		PreviewLocation + (TargetRotationWithOffset.Vector() * PreviewForwardDistance);
	return (RotationTargetLocation - PreviewLocation).Rotation();
}

EPreviewActionType AFactionPlayerController::GetRandomPreviewAction(
	const EPreviewActionType FirstAction,
	const EPreviewActionType SecondAction) const
{
	const bool bUseFirstAction = FMath::RandBool();
	if (bUseFirstAction)
	{
		return FirstAction;
	}

	return SecondAction;
}

bool AFactionPlayerController::GetIsValidCurrentCommandsInterface() const
{
	if (M_CurrentCommandsInterface.GetObject() != nullptr && M_CurrentCommandsInterface.GetInterface() != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_CurrentCommandsInterface",
		"GetIsValidCurrentCommandsInterface",
		this
	);
	return false;
}

bool AFactionPlayerController::GetIsValidCurrentPreviewActor() const
{
	if (M_CurrentPreviewActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_CurrentPreviewActor",
		"GetIsValidCurrentPreviewActor",
		this
	);
	return false;
}

bool AFactionPlayerController::GetHasGroundAttackLocations() const
{
	if (M_UnitPreviewActions.GroundAttackLocations.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_UnitPreviewActions.GroundAttackLocations",
		"GetHasGroundAttackLocations",
		this
	);
	return false;
}

bool AFactionPlayerController::GetHasSquadMovementLocations() const
{
	if (M_UnitPreviewActions.SquadMovementLocations.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_UnitPreviewActions.SquadMovementLocations",
		"GetHasSquadMovementLocations",
		this
	);
	return false;
}

FVector AFactionPlayerController::GetRandomGroundAttackLocation() const
{
	const int32 LocationCount = M_UnitPreviewActions.GroundAttackLocations.Num();
	if (LocationCount == 0)
	{
		return FVector::ZeroVector;
	}

	const int32 FirstIndex = 0;
	const int32 LastIndex = LocationCount - 1;
	const int32 RandomIndex = FMath::RandRange(FirstIndex, LastIndex);
	return M_UnitPreviewActions.GroundAttackLocations[RandomIndex];
}

void AFactionPlayerController::ClearCurrentPreview()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_PreviewGeneralActionTimerHandle);
		World->GetTimerManager().ClearTimer(M_PreviewAircraftComeDownTimerHandle);
	}

	AActor* CurrentPreviewActor = M_CurrentPreviewActor.Get();
	ASquadController* PreviewSquadController = Cast<ASquadController>(CurrentPreviewActor);
	if (IsValid(PreviewSquadController))
	{
		TArray<ASquadUnit*> SquadUnits = PreviewSquadController->GetSquadUnitsChecked();
		for (ASquadUnit* SquadUnit : SquadUnits)
		{
			if (not IsValid(SquadUnit))
			{
				continue;
			}

			SquadUnit->Destroy();
		}
	}

	if (IsValid(CurrentPreviewActor))
	{
		CurrentPreviewActor->Destroy();
	}

	M_CurrentPreviewActor.Reset();
	M_CurrentCommandsInterface.SetObject(nullptr);
	M_CurrentCommandsInterface.SetInterface(nullptr);
	M_CurrentPreviewActorType = EPreviewActorType::None;
	M_NextPreviewAction = EPreviewActionType::None;
}

bool AFactionPlayerController::GetIsValidAnnouncementAudioComponent() const
{
	if (not IsValid(M_AnnouncementAudioComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_AnnouncementAudioComponent",
			"GetIsValidAnnouncementAudioComponent",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionPopupClass() const
{
	if (M_FactionPopupClass == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionPopupClass",
			"GetIsValidFactionPopupClass",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionPopup() const
{
	if (not M_FactionPopup.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionPopup",
			"GetIsValidFactionPopup",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionSelectionMenuClass() const
{
	if (M_FactionSelectionMenuClass == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionSelectionMenuClass",
			"GetIsValidFactionSelectionMenuClass",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionSelectionMenu() const
{
	if (not M_FactionSelectionMenu.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionSelectionMenu",
			"GetIsValidFactionSelectionMenu",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionDifficultyPickerClass() const
{
	if (M_FactionDifficultyPickerClass == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionDifficultyPickerClass",
			"GetIsValidFactionDifficultyPickerClass",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionDifficultyPicker() const
{
	if (not M_FactionDifficultyPicker.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionDifficultyPicker",
			"GetIsValidFactionDifficultyPicker",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionWorldGenerationSettingsClass() const
{
	if (M_FactionWorldGenerationSettingsClass == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionWorldGenerationSettingsClass",
			"GetIsValidFactionWorldGenerationSettingsClass",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionWorldGenerationSettings() const
{
	if (not M_FactionWorldGenerationSettings.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionWorldGenerationSettings",
			"GetIsValidFactionWorldGenerationSettings",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidFactionUnitPreview() const
{
	if (not M_FactionUnitPreview.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_FactionUnitPreview",
			"GetIsValidFactionUnitPreview",
			this
		);
		return false;
	}

	return true;
}

bool AFactionPlayerController::GetIsValidRTSAsyncSpawner() const
{
	if (not M_RTSAsyncSpawner.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_RTSAsyncSpawner",
			"GetIsValidRTSAsyncSpawner",
			this
		);
		return false;
	}

	return true;
}
