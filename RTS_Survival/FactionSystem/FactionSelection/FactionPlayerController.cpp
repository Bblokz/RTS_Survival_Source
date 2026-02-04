// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FactionPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "FactionUnitPreview.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
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

	if (not IsValid(SpawnedActor))
	{
		return;
	}

	SpawnedActor->SetActorRotation(M_PreviewSpawnRotation);
	M_CurrentPreviewActor = SpawnedActor;

	ICommands* CommandsInterface = Cast<ICommands>(SpawnedActor);
	if (CommandsInterface == nullptr)
	{
		M_CurrentCommandsInterface.SetObject(nullptr);
		M_CurrentCommandsInterface.SetInterface(nullptr);
		return;
	}

	M_CurrentCommandsInterface.SetObject(SpawnedActor);
	M_CurrentCommandsInterface.SetInterface(CommandsInterface);

	ResetPreviewAttackTimer();
}

void AFactionPlayerController::ResetPreviewAttackTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_PreviewAttackTimerHandle);

	if (M_CurrentCommandsInterface.GetObject() == nullptr)
	{
		return;
	}

	const int32 LocationCount = M_UnitPreviewActions.GroundAttackLocations.Num();
	if (LocationCount == 0)
	{
		return;
	}

	const float MinDelay = M_UnitPreviewActions.MinTimeTillGroundAttack;
	const float MaxDelay = M_UnitPreviewActions.MaxTimeTillGroundAttack;
	const float NextDelay = FMath::FRandRange(MinDelay, MaxDelay);

	FTimerDelegate PreviewAttackDelegate;
	TWeakObjectPtr<AFactionPlayerController> WeakThis(this);
	PreviewAttackDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->HandlePreviewAttackTimer();
	});

	World->GetTimerManager().SetTimer(M_PreviewAttackTimerHandle, PreviewAttackDelegate, NextDelay, false);
}

void AFactionPlayerController::HandlePreviewAttackTimer()
{
	const int32 LocationCount = M_UnitPreviewActions.GroundAttackLocations.Num();
	if (LocationCount == 0)
	{
		return;
	}

	AActor* CommandsActor = Cast<AActor>(M_CurrentCommandsInterface.GetObject());
	if (not IsValid(CommandsActor))
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

	ResetPreviewAttackTimer();
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
		World->GetTimerManager().ClearTimer(M_PreviewAttackTimerHandle);
	}

	AActor* CurrentPreviewActor = M_CurrentPreviewActor.Get();
	if (IsValid(CurrentPreviewActor))
	{
		CurrentPreviewActor->Destroy();
	}

	M_CurrentPreviewActor.Reset();
	M_CurrentCommandsInterface.SetObject(nullptr);
	M_CurrentCommandsInterface.SetInterface(nullptr);
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
