// Copyright (C) Bas Blokzijl - All rights reserved.

#include "IntroCinematicGameMode.h"

#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"

AIntroCinematicGameMode::AIntroCinematicGameMode()
{
	GameStateClass = ACPPGameState::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}
