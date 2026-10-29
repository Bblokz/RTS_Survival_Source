// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMainMenuGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void ARTSMainMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// InitGame runs before the level's actors initialize, and placed or spawned units query the faction on init.
	PrimeGameInstanceFaction();
}

APlayerController* ARTSMainMenuGameMode::SpawnPlayerControllerCommon(const ENetRole InRemoteRole,
                                                                     FVector const& SpawnLocation,
                                                                     FRotator const& SpawnRotation,
                                                                     const TSubclassOf<APlayerController>
                                                                     InPlayerControllerClass)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	// Player controllers are never saved into a map.
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;
	APlayerController* NewPlayerController = GetWorld()->SpawnActor<APlayerController>(
		InPlayerControllerClass, SpawnLocation, SpawnRotation, SpawnInfo);
	if (not IsValid(NewPlayerController))
	{
		return nullptr;
	}
	if (InRemoteRole == ROLE_SimulatedProxy)
	{
		// A local player: it has no authority or autonomous remote role.
		NewPlayerController->SetAsLocalPlayerController();
	}
	if (ACPPController* RTSController = Cast<ACPPController>(NewPlayerController))
	{
		RTSController->SetIsMainMenuController(true);
	}
	UGameplayStatics::FinishSpawningActor(NewPlayerController, FTransform(SpawnRotation, SpawnLocation));
	return NewPlayerController;
}

void ARTSMainMenuGameMode::PrimeGameInstanceFaction() const
{
	URTSGameInstance* RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSMainMenuGameMode: the game instance is not a URTSGameInstance."));
		return;
	}
	if (RTSGameInstance->GetHasPlayerFaction())
	{
		return;
	}
	RTSGameInstance->SetPlayerFaction(M_MenuPlayerFaction);
}
