// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/WorldCampaign/SaveSystem/Defaults/FPlayerFactionDefaultProfiles.h"
#include "WorldProfileAndUIManager.generated.h"


class AWorldPlayerController;
class UW_WorldMenu;

/**
 * @brief Used by the world player controller to configure and drive the world menu UI.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldProfileAndUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWorldProfileAndUIManager();

	void SetupWorldMenu(AWorldPlayerController* PlayerController);
	void OnSetupUIForNewCampaign(ERTSFaction PlayerFaction);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UW_WorldMenu> WorldMenuClass;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;
	bool GetIsValidWorldMenu() const;
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	// Starter/default player profiles for NEW campaigns (no existing save).
	UPROPERTY(EditDefaultsOnly, Category="Player Profile|Defaults", meta=(ShowOnlyInnerProperties))
	FPlayerFactionDefaultProfiles M_DefaultPlayerProfiles;

	FPlayerData GetDefaultPlayerDataForFaction(const ERTSFaction PlayerFaction) const;
 
};
