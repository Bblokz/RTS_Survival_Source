
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UT_ErrorReportManager.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/UnitTests/UT_ErrorReporting/UT_ErrorReportingActor/UT_ErrorReportingActor.h"
#include "RTS_Survival/UnitTests/UT_ErrorReporting/W_UT_ErrorReport/W_UT_ErrorReport.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundBase.h"

AUT_ErrorReportManager::AUT_ErrorReportManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AUT_ErrorReportManager::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitFindAndLinkReportingActors();
	BeginPlay_InitCreateAndSetupWidget();
}

void AUT_ErrorReportManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AUT_ErrorReportManager::GetIsValidErrorReportWidgetClass() const
{
	if (M_ErrorReportWidgetClass)
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_ErrorReportWidgetClass"),
	                                                      TEXT("GetIsValidErrorReportWidgetClass"), this);
	return false;
}

bool AUT_ErrorReportManager::GetIsValidErrorReportWidget() const
{
	if (IsValid(M_ErrorReportWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_ErrorReportWidget"),
	                                                      TEXT("GetIsValidErrorReportWidget"), this);
	return false;
}

void AUT_ErrorReportManager::BeginPlay_InitFindAndLinkReportingActors()
{
	UWorld* const World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(TEXT("UT_ErrorReportManager has no valid UWorld in BeginPlay."));
		return;
	}

	for (TActorIterator<AErrorReportingActor> It(World); It; ++It)
	{
		if (AErrorReportingActor* const Reporter = *It)
		{
			Reporter->SetErrorReportManager(this);
		}
	}
}

void AUT_ErrorReportManager::BeginPlay_InitCreateAndSetupWidget()
{
	if (not GetIsValidErrorReportWidgetClass())
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(TEXT("UT_ErrorReportManager has no valid UWorld for widget creation."));
		return;
	}

	M_ErrorReportWidget = CreateWidget<UW_UT_ErrorReport>(World, M_ErrorReportWidgetClass);
	if (not GetIsValidErrorReportWidget())
	{
		return;
	}

	M_ErrorReportWidget->SetErrorReportManager(this);
	M_ErrorReportWidget->AddToViewport();

	// Make sure the player can interact with it using the mouse.
	if (APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		if (TSharedPtr<SWidget> SlateWidget = M_ErrorReportWidget->TakeWidget())
		{
			Mode.SetWidgetToFocus(SlateWidget);
		}
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

void AUT_ErrorReportManager::ReceiveErrorReport(const FString& Message, const FVector& WorldLocation)
{
	// Play notification (optional).
	if (M_ReportSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_ReportSound, WorldLocation);
	}

	if (not GetIsValidErrorReportWidget())
	{
		return;
	}

	M_ErrorReportWidget->AddError(Message, WorldLocation);
}

void AUT_ErrorReportManager::MoveTo(const FVector& WorldLocation) const
{
	// Get the player camera controller via RTS statics and perform a short move.
	if (UPlayerCameraController* const CameraCtrl = FRTS_Statics::GetPlayerCameraController(this))
	{
		FMovePlayerCamera MoveParams;
		MoveParams.MoveToLocation = WorldLocation;
		MoveParams.TimeToMove = 0.5f;
		MoveParams.TimeCameraInputDisabled = 0.6f;
		MoveParams.MoveSound = nullptr;

		CameraCtrl->MoveCameraOverTime(MoveParams);
		return;
	}

	RTSFunctionLibrary::ReportError(TEXT("Failed to get PlayerCameraController to move camera to error location."));
}
