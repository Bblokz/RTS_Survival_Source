// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerStartLocationManager.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/StartLocation/PlayerStartLocation/PlayerStartLocation.h"
#include "RTS_Survival/StartLocation/W_ChooseStartLocation/W_ChoosePlayerStartLocation.h"
#include "RTS_Survival/Procedural/LandscapeDivider/RTSLandscapeDivider.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


UPlayerStartLocationManager::UPlayerStartLocationManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerStartLocationManager::SetPlayerController(ACPPController* PlayerController)
{
	M_PlayerController = PlayerController;
}

void UPlayerStartLocationManager::SetWidgetLocationClass(const TSubclassOf<UW_ChoosePlayerStartLocation> WidgetClass)
{
	M_ChoosePlayerStartLocationWidgetClass = WidgetClass;
}

void UPlayerStartLocationManager::RegisterStartLocation(APlayerStartLocation* StartLocation)
{
	if (not IsValid(StartLocation))
	{
		return;
	}
	if (not M_StartLocations.Contains(StartLocation))
	{
		M_StartLocations.Add(StartLocation);
		CheckIfAllStartLocationsAreRegistered();
		return;
	}
	RTSFunctionLibrary::ReportError("StartLocation already registered: " + StartLocation->GetName());
}

void UPlayerStartLocationManager::OnClickedLocation(const bool bGoToNextLocation)
{
	if(bGoToNextLocation)
	{
		const bool bCanStillIncrement = M_CurrentStartLocationIndex < M_StartLocations.Num() - 1;
		bCanStillIncrement ? M_CurrentStartLocationIndex++ : M_CurrentStartLocationIndex = 0;
	}
	else
	{
		const bool bCanStillDecrement = M_CurrentStartLocationIndex > 0;
		bCanStillDecrement ? M_CurrentStartLocationIndex-- : M_CurrentStartLocationIndex = M_StartLocations.Num() - 1;
	}
	NavigateCameraToLocation(M_CurrentStartLocationIndex);
}

void UPlayerStartLocationManager::OnDecidedToStartAtLocation()
{
	if(not GetIsValidStartLocationIndex(M_CurrentStartLocationIndex))
	{
		return;
	}
	OnStartLocationChosen(M_StartLocations[M_CurrentStartLocationIndex].Get()->GetActorLocation());
}


void UPlayerStartLocationManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerStartLocationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	M_StartLocations.Empty();
	Super::EndPlay(EndPlayReason);
}

void UPlayerStartLocationManager::CheckIfAllStartLocationsAreRegistered()
{
	if (not EnsureValidLandscapeDivider())
	{
		RTSFunctionLibrary::ReportError("Could not find LandscapeDivider for UPlayerStartLocationManager"
			"\n At function CheckIfAllStartLocationsAreRegistered() in PlayerStartLocationManager.cpp");
		return;
	}
	Debug_Message("Amount of Start locations registered: " + FString::FromInt(M_StartLocations.Num()));
	if (M_StartLocations.Num() == M_LandscapeDivider->GetAmountOfPlayerStartRegions())
	{
		OnAllPlayerStartLocationsRegistered();
	}
}

bool UPlayerStartLocationManager::EnsureValidLandscapeDivider()
{
	if (M_LandscapeDivider.IsValid())
	{
		return true;
	}
	M_LandscapeDivider = FRTS_Statics::GetRTSLandscapeDivider(this);
	return M_LandscapeDivider.IsValid();
}

bool UPlayerStartLocationManager::EnsureValidPlayerController()
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	M_PlayerController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0));
	return M_PlayerController.IsValid();
}

bool UPlayerStartLocationManager::GetIsValidStartingLocationUI() const
{
	if(IsValid(M_ChoosePlayerStartLocationWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("StartingLocationUI is not valid in UPlayerStartLocationManager"
								 "\n At function GetIsValidStartingLocationUI() in PlayerStartLocationManager.cpp");
	return false;
}

void UPlayerStartLocationManager::OnAllPlayerStartLocationsRegistered()
{
	if (not EnsureValidPlayerController())
	{
		RTSFunctionLibrary::ReportError("Could not access valid player controller on UPlayerStartLocationManager"
			"\n At function OnAllPlayerStartLocationsRegistered() in PlayerStartLocationManager.cpp");
		return;
	}
	// Either invokes immediately if the main menu is already loaded or adds the callback to the main menu ui delegate.
	M_PlayerController->OnMainMenuCallbacks.CallbackOnMenuReady(&UPlayerStartLocationManager::OnMainMenuReady, this);
}

void UPlayerStartLocationManager::OnMainMenuReady()
{
	if (not EnsureValidPlayerController())
	{
		RTSFunctionLibrary::ReportError("Could not access valid player controller on UPlayerStartLocationManager"
			"\n At function OnMainMenuReady() in PlayerStartLocationManager.cpp");
		return;
	}
	M_PlayerController->GetMainMenuUI()->SetMainMenuVisiblity(false);
	M_PlayerController->SetCameraMovementDisabled(true);
	// Create widget class and add to viewport.
	M_ChoosePlayerStartLocationWidget = CreateWidget<UW_ChoosePlayerStartLocation>(
		M_PlayerController.Get(), M_ChoosePlayerStartLocationWidgetClass);
	if (not IsValid(M_ChoosePlayerStartLocationWidget))
	{
		OnCouldNotInitStartLocationUI();
		return;
	}
	M_ChoosePlayerStartLocationWidget->AddToViewport(10);
	M_ChoosePlayerStartLocationWidget->SetPlayerStartLocationManager(this);
	// Make sure that input only goes to the widget.
	SetInputToFocusWidget(M_ChoosePlayerStartLocationWidget);
	// Go to the first location.
	NavigateCameraToLocation(M_CurrentStartLocationIndex);
	Debug_MenuReady(M_ChoosePlayerStartLocationWidget);

}

void UPlayerStartLocationManager::OnStartLocationChosen(const FVector& StartLocation)
{
	if(not EnsureValidPlayerController())
	{
		return;
	}
	LocationChosen_DestroyWidget();
	// Set back to game and ui with focus on main game UI widget.
	LocationChosen_RestoreInputMode();
	// Ask the player controller to start at the location; load profile and start the game.
	M_PlayerController->StartGameAtLocation(StartLocation, false);
	M_PlayerController->SetCameraMovementDisabled(false);
	LocationChosen_CleanUpStartLocations();
}

void UPlayerStartLocationManager::SetInputToFocusWidget(const TObjectPtr<UUserWidget>& WidgetToFocus) const
{
	FInputModeUIOnly InputModeData;
	InputModeData.SetWidgetToFocus(WidgetToFocus->TakeWidget());
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	M_PlayerController->SetInputMode(InputModeData);
	M_PlayerController->bShowMouseCursor = true;
	
}

void UPlayerStartLocationManager::NavigateCameraToLocation(const int32 Index)
{
	if(not GetIsValidStartLocationIndex(Index) || not EnsureValidPlayerController())
	{
		return;
	}
	// Get the location and navigate the camera to it.
	const FVector StartLocation = M_StartLocations[Index].Get()->GetActorLocation();
	M_PlayerController->FocusCameraOnStartLocation(StartLocation);
	Debug_PickedLocation(StartLocation);
}

bool UPlayerStartLocationManager::GetIsValidStartLocationIndex(const int32 Index) const
{
	if(not M_StartLocations.IsValidIndex(Index))
	{
		RTSFunctionLibrary::ReportError("Invalid index for start location: " + FString::FromInt(Index) +
			"\n At function EnsureIsValidStartLocationIndex() in PlayerStartLocationManager.cpp");
		return false;
	}
	return true;
}

void UPlayerStartLocationManager::OnCouldNotInitStartLocationUI() const
{
	RTSFunctionLibrary::ReportError("Could not initialize StartLocation UI"
		"\n At function OnCouldNotInitStartLocationUI() in PlayerStartLocationManager.cpp");
}

void UPlayerStartLocationManager::LocationChosen_DestroyWidget()
{
	if(not GetIsValidStartingLocationUI())
	{
		return;
	}
	M_ChoosePlayerStartLocationWidget->RemoveFromParent();
	// Causes no references left to the widget; it will be destroyed by the garbage collector.
	M_ChoosePlayerStartLocationWidget = nullptr;	
}

void UPlayerStartLocationManager::LocationChosen_RestoreInputMode()
{
	if(not EnsureValidPlayerController())
	{
		return;
	}
	FInputModeGameAndUI InputModeData;
	InputModeData.SetHideCursorDuringCapture(false);
	UMainGameUI* MainMenuUI = M_PlayerController->GetMainMenuUI();
	MainMenuUI->SetMainMenuVisiblity(true);
	InputModeData.SetWidgetToFocus(MainMenuUI->TakeWidget());
	M_PlayerController->SetInputMode(InputModeData);
	M_PlayerController->bShowMouseCursor = true;
}

void UPlayerStartLocationManager::LocationChosen_CleanUpStartLocations()
{
	for(auto EachLocation : M_StartLocations)
	{
		if(EachLocation.IsValid())
		{
			EachLocation->Destroy();	
		}
	}
	M_StartLocations.Empty();
	Debug_Message("Emptied all start locations in the UPlayerStartLocationManager.");
}

void UPlayerStartLocationManager::Debug_PickedLocation(const FVector& Location) const
{
	if constexpr (DeveloperSettings::Debugging::GPlayerStartLocations_Compile_DebugSymbols)
	{
			RTSFunctionLibrary::PrintString("Picked location: " + Location.ToString());
	}
}

void UPlayerStartLocationManager::Debug_MenuReady(
	const TObjectPtr<UW_ChoosePlayerStartLocation>& StartLocationsWidget) const
{
	if constexpr (DeveloperSettings::Debugging::GPlayerStartLocations_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Init StartLocation UI: " + StartLocationsWidget->GetName() +
			"\n started at index: " + FString::FromInt(M_CurrentStartLocationIndex) +
			"\n Input mode has been focused on widget.");
	}
}

void UPlayerStartLocationManager::Debug_Message(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GPlayerStartLocations_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message);
	}
}
