// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PlayerStartGameControl.generated.h"

class ACPPController;
class UW_StartGameWidget;

/**
 * @brief Gatekeeper component used by the controller to pause the game until the player starts.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerStartGameControl : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerStartGameControl();

	void InitPlayerStartGameControl(ACPPController* PlayerController);

	void PlayerStartedGame();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UW_StartGameWidget> M_StartGameWidgetClass;
	

private:
	void BeginPlay_InitStartGameFlow();

	void PauseGameAndDisableCamera() const;
	void UnpauseGameAndEnableCamera() const;

	bool GetIsValidPlayerController() const;
	bool GetIsValidStartGameWidgetClass() const;
	bool GetIsValidStartGameWidget() const;


	UPROPERTY()
	TObjectPtr<UW_StartGameWidget> M_StartGameWidget;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	bool bM_HasPausedGame = false;
};
