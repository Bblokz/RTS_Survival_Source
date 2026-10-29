// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"

#include "RTSMainMenuGameMode.generated.h"

/**
 * @brief Game mode of the start map: parent of BP_GM_StartGame. Units on the menu (the background battles) ask the
 * game instance for the player faction while it is only chosen later, in faction selection; this mode primes it.
 * Its player controller is marked as the main menu controller, which skips the in-game controller's gameplay setup.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** Same as the engine's, but marks an ACPPController as the main menu controller before it initializes. */
	virtual APlayerController* SpawnPlayerControllerCommon(ENetRole InRemoteRole, FVector const& SpawnLocation,
	                                                       FRotator const& SpawnRotation,
	                                                       TSubclassOf<APlayerController> InPlayerControllerClass) override;

private:
	// Faction the game instance reports while on the menu; faction selection overwrites it before any mission.
	UPROPERTY(EditDefaultsOnly, Category = "MainMenu")
	ERTSFaction M_MenuPlayerFaction = ERTSFaction::GerStrikeDivision;

	void PrimeGameInstanceFaction() const;
};
