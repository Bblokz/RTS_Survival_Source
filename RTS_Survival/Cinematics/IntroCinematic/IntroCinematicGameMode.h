// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "IntroCinematicGameMode.generated.h"

/**
 * @brief Game mode of the intro cinematic map (RTS_IntroCinematic_1976): no RTS player, pawn or HUD, but the RTS game
 * state, so the actors of the streamed mission maps find the state class they cast to.
 * Set as the map's GameMode Override and as the Movie Render Queue game mode override.
 */
UCLASS()
class RTS_SURVIVAL_API AIntroCinematicGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIntroCinematicGameMode();
};
